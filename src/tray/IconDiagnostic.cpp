#include "IconDiagnostic.h"

#include <QGuiApplication>
#include <QStringList>
#include <QPainter>
#include <QPixmap>
#include <QProcessEnvironment>
#include <QTextStream>

#include <algorithm>

namespace claudedial::tray::diagnostic {
namespace {

/// The same sizes the real icon bakes, so a working square proves the size set
/// is not the problem either - including the 15 px the panel actually asked for,
/// which no entry matches exactly.
constexpr int kSizes[] = { 16, 22, 24, 32, 48, 64 };

/// Magenta: no panel theme uses it, so if this is invisible the colour is not why.
const QColor kProbe(0xff, 0x00, 0xff);

} // namespace

Mode requested()
{
    const QString raw = QProcessEnvironment::systemEnvironment()
                            .value(QStringLiteral("CLAUDEDIAL_DIAGNOSTIC_ICON"))
                            .trimmed()
                            .toLower();
    if (raw.isEmpty())
        return Mode::Off;
    if (raw == QLatin1String("solid") || raw == QLatin1String("1"))
        return Mode::Solid;
    if (raw == QLatin1String("alpha"))
        return Mode::Alpha;
    if (raw == QLatin1String("mono"))
        return Mode::Mono;
    return Mode::Off;
}

QColor monoColour()
{
    return kProbe;
}

QIcon icon(Mode mode)
{
    QIcon result;
    for (const int size : kSizes) {
        QPixmap pixmap(size, size);

        if (mode == Mode::Alpha) {
            // Transparent everywhere except a centred block, so the icon has a
            // genuine alpha channel to composite.
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            const int inset = std::max(1, size / 8);
            painter.fillRect(inset, inset, size - 2 * inset, size - 2 * inset, kProbe);
        } else {
            // No alpha channel in play at all: every pixel opaque.
            pixmap.fill(kProbe);
        }

        result.addPixmap(pixmap);
    }
    return result;
}

void report(Mode mode, const QColor& panelForeground, const QIcon& icon)
{
    static bool done = false;
    if (done)
        return;
    done = true;

    QTextStream err(stderr);
    err << "claudedial: tray icon diagnostic\n";

    const char* name = mode == Mode::Solid   ? "solid (opaque square)"
        : mode == Mode::Alpha                ? "alpha (square inset in transparency)"
        : mode == Mode::Mono                 ? "mono (real icon, forced magenta)"
                                             : "off (the real icon)";
    err << "  mode              " << name << "\n";
    err << "  platform          " << QGuiApplication::platformName() << "\n";

    // The likeliest single cause: a near-black foreground on a black panel.
    err << "  panel foreground  " << panelForeground.name(QColor::HexRgb) << "  (lightness "
        << panelForeground.lightness() << " of 255)\n";
    if (panelForeground.lightness() < 96)
        err << "                    ^ dark. On a dark panel this alone would look blank.\n";

    QStringList sizes;
    const auto available = icon.availableSizes();
    sizes.reserve(available.size());
    for (const QSize& size : available)
        sizes << QStringLiteral("%1").arg(size.width());
    err << "  icon sizes        " << (sizes.isEmpty() ? QStringLiteral("none") : sizes.join(u' '))
        << "\n";

    const QPixmap at16 = icon.pixmap(16, 16);
    err << "  pixmap(16)        " << (at16.isNull() ? QStringLiteral("NULL") : QStringLiteral("%1x%2")
                                                          .arg(at16.width())
                                                          .arg(at16.height()))
        << "\n";
    err << "  set with          CLAUDEDIAL_DIAGNOSTIC_ICON=solid|alpha|mono\n";
}

} // namespace claudedial::tray::diagnostic
