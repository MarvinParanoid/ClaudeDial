#include "Notifier.h"

#include "core/UsageLevel.h"

#ifdef CLAUDEDIAL_HAVE_DBUS
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#endif
#include <QVariantMap>

namespace claudedial::tray {
namespace {

constexpr auto kService = "org.freedesktop.Notifications";
constexpr auto kPath = "/org/freedesktop/Notifications";
constexpr auto kInterface = "org.freedesktop.Notifications";

/// The `image-data` hint's wire format: (iiibiiay).
#ifdef CLAUDEDIAL_HAVE_DBUS
// The image-data hint's wire format. D-Bus only, and nothing else uses it.
struct HintImage {
    int width = 0;
    int height = 0;
    int rowStride = 0;
    bool hasAlpha = true;
    int bitsPerSample = 8;
    int channels = 4;
    QByteArray data;
};

} // namespace
} // namespace claudedial::tray

Q_DECLARE_METATYPE(claudedial::tray::HintImage)

namespace claudedial::tray {
namespace {

QDBusArgument& operator<<(QDBusArgument& argument, const HintImage& image)
{
    argument.beginStructure();
    argument << image.width << image.height << image.rowStride << image.hasAlpha
             << image.bitsPerSample << image.channels << image.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, HintImage& image)
{
    argument.beginStructure();
    argument >> image.width >> image.height >> image.rowStride >> image.hasAlpha
             >> image.bitsPerSample >> image.channels >> image.data;
    argument.endStructure();
    return argument;
}

HintImage toHintImage(const QImage& source)
{
    // RGBA8888 is byte-ordered R,G,B,A and not premultiplied, which is what the
    // specification asks for.
    const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);

    HintImage image;
    image.width = rgba.width();
    image.height = rgba.height();
    image.rowStride = static_cast<int>(rgba.bytesPerLine());
    image.data = QByteArray(reinterpret_cast<const char*>(rgba.constBits()),
                            static_cast<qsizetype>(rgba.sizeInBytes()));
    return image;
}
#endif // CLAUDEDIAL_HAVE_DBUS

QString windowName(core::PeriodKind kind)
{
    // The same words the popup and Claude Code use, so a banner read on its own
    // names the window the same way the app does.
    return kind == core::PeriodKind::FiveHour ? Notifier::tr("Session") : Notifier::tr("Weekly");
}

} // namespace

Notifier::Notifier(QObject* parent)
    : QObject(parent)
{
#ifdef CLAUDEDIAL_HAVE_DBUS
    qDBusRegisterMetaType<HintImage>();
#endif
}

void Notifier::notifyThreshold(core::PeriodKind kind, int threshold, const QString& resetText,
                               const QImage& icon)
{
    const QString window = windowName(kind);

    QString title;
    QString what;
    bool critical = false;

    if (threshold >= core::kLimitThreshold) {
        title = tr("Limit reached");
        what = tr("%1 limit reached").arg(window);
        critical = true;
    } else if (threshold >= core::kSevereThreshold) {
        title = tr("Almost at the limit");
        what = tr("%1 usage reached %2%").arg(window).arg(threshold);
        critical = true;
    } else {
        // The configured critical threshold reads as "high", the warning one as
        // a warning; there is no third wording to invent between them.
        title = threshold >= 90 ? tr("High usage") : tr("Usage warning");
        what = tr("%1 usage reached %2%").arg(window).arg(threshold);
    }

    const QString body = resetText.isEmpty() ? what
                                             : QStringLiteral("%1\n%2").arg(what, resetText);
    send(kind, title, body, critical, icon);
}

void Notifier::send(core::PeriodKind kind, const QString& title, const QString& body,
                    bool critical, const QImage& icon)
{
#ifndef CLAUDEDIAL_HAVE_DBUS
    // No notification bus. The tray icon can show a message itself, which loses
    // replaces_id - a second warning stacks another balloon rather than
    // updating the first - and that is the platform's behaviour, not something
    // to emulate.
    Q_UNUSED(kind)
    Q_EMIT messageRequested(title, body, critical, icon);
    return;
#else
    QDBusInterface interface(QLatin1String(kService), QLatin1String(kPath),
                             QLatin1String(kInterface), QDBusConnection::sessionBus());
    if (!interface.isValid())
        return; // No notification daemon; not worth surfacing as an error.

    QVariantMap hints;
    hints[QStringLiteral("urgency")] = critical ? uchar(2) : uchar(1);
    // Lets the shell group our notifications under the desktop entry.
    hints[QStringLiteral("desktop-entry")] = QStringLiteral("claudedial");
    if (!icon.isNull())
        hints[QStringLiteral("image-data")] = QVariant::fromValue(toHintImage(icon));

    quint32& id = kind == core::PeriodKind::FiveHour ? m_fiveHourId : m_sevenDayId;

    const QDBusReply<quint32> reply = interface.call(
        QStringLiteral("Notify"),
        QStringLiteral("ClaudeDial"),
        id, // replaces_id: update this window's banner rather than stacking
        QString(), // no icon name: the pixels above are the icon
        title,
        body,
        QStringList {},
        hints,
        -1);

    if (reply.isValid())
        id = reply.value();
#endif
}

} // namespace claudedial::tray
