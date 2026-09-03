#include "Notifier.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>

namespace claudometer::tray {
namespace {

constexpr auto kService = "org.freedesktop.Notifications";
constexpr auto kPath = "/org/freedesktop/Notifications";
constexpr auto kInterface = "org.freedesktop.Notifications";

} // namespace

Notifier::Notifier(QObject* parent)
    : QObject(parent)
{
}

void Notifier::notifyThreshold(core::PeriodKind kind, int threshold)
{
    const QString window = kind == core::PeriodKind::FiveHour ? tr("5-hour limit") : tr("7-day limit");

    QString title;
    QString urgency = QStringLiteral("normal");

    if (threshold >= 100) {
        title = tr("Limit reached");
        urgency = QStringLiteral("critical");
    } else if (threshold >= 95) {
        title = tr("Almost at limit");
        urgency = QStringLiteral("critical");
    } else if (threshold >= 90) {
        title = tr("High usage");
    } else {
        title = tr("Usage warning");
    }

    send(kind, title, tr("%1 at %2%").arg(window).arg(threshold), urgency);
}

void Notifier::send(core::PeriodKind kind, const QString& title, const QString& body,
                    const QString& urgencyHint)
{
    QDBusInterface interface(QLatin1String(kService), QLatin1String(kPath),
                             QLatin1String(kInterface), QDBusConnection::sessionBus());
    if (!interface.isValid())
        return; // No notification daemon; not worth surfacing as an error.

    QVariantMap hints;
    hints[QStringLiteral("urgency")] = urgencyHint == QLatin1String("critical")
        ? uchar(2)
        : uchar(1);
    // Lets the shell group our notifications under the desktop entry.
    hints[QStringLiteral("desktop-entry")] = QStringLiteral("claudometer");

    quint32& id = kind == core::PeriodKind::FiveHour ? m_fiveHourId : m_sevenDayId;

    const QDBusReply<quint32> reply = interface.call(
        QStringLiteral("Notify"),
        QStringLiteral("Claudometer"),
        id, // replaces_id: update this window's banner rather than stacking
        QStringLiteral("claudometer"),
        title,
        body,
        QStringList {},
        hints,
        -1);

    if (reply.isValid())
        id = reply.value();
}

} // namespace claudometer::tray
