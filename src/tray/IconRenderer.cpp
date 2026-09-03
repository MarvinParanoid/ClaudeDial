#include "IconRenderer.h"

#include "GaugePainter.h"

#include <QFont>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace claudometer::tray {
namespace {

/// Sizes a Linux panel actually asks for. Each is rendered directly so the mark
/// stays crisp instead of being downscaled from one large pixmap.
constexpr int kSizes[] = { 16, 22, 24, 32, 48, 64 };

/// How much lighter the arc is drawn when it holds a number instead of a needle.
///
/// In the gauge style the arc *is* the reading, so it carries full weight. Here
/// the number is the reading and the arc is context, so it steps back - and it
/// has to: a block of two digits is a rectangle inscribed in the arc's circle,
/// and at full weight the interior radius simply is not large enough to hold one
/// without the digits colliding with the arc walls.
constexpr double kNumberArcScale = 0.45;

/// Digit size as a fraction of the icon. Settled by rendering the combinations
/// at 16, 20, 22 and 24 px: this is the largest that still leaves visible air
/// between the digits and the arc.
constexpr double kDigitSize = 0.52;

/// DemiBold rather than Bold: at 16-22 px Bold fills the counters and looks
/// blunt, while Medium goes thin enough to fade at 16 px, particularly in the
/// warning and critical colours.
constexpr QFont::Weight kDigitWeight = QFont::DemiBold;

/// Three digits do not fit legibly, and "at the limit" is better said than
/// counted.
constexpr auto kLimitGlyph = "!";

/// How far the mark fades once the data behind it is stale. Enough to read as
/// "not current" at a glance, not so far that the number becomes unreadable.
constexpr double kStaleOpacity = 0.45;

// Breeze-adjacent, so the icon does not look foreign on Plasma while staying
// legible on GNOME's and XFCE's panels.
//
// NOTE: the popup uses a longer ramp with an accent step (see Theme.qml). That
// is deliberate, not an oversight: a panel icon is on screen permanently and
// must not be a standing splash of colour, whereas the popup is only visible
// while the user is looking at it.
const QColor kWarning { 0xfd, 0xbc, 0x4b };
const QColor kCritical { 0xda, 0x44, 0x53 };

} // namespace

QColor IconRenderer::colorFor(double percentage, const Options& options)
{
    if (percentage >= options.criticalThreshold)
        return kCritical;
    if (percentage >= options.warningThreshold)
        return kWarning;
    return options.foreground; // monochrome until something needs attention
}

void IconRenderer::paintNumberInArc(QPainter& painter, int size, double percentage,
                                    const Options& options)
{
    const QColor value = colorFor(percentage, options);
    const QRectF bounds(0, 0, size, size);

    // The same arc as the gauge style, drawn by the same function, with its
    // middle left for us.
    gauge::paint(painter, bounds, percentage, { options.foreground, value }, kNumberArcScale,
                 gauge::Center::Empty);

    const QString text = percentage >= 100.0 ? QString::fromLatin1(kLimitGlyph)
                                             : QString::number(qRound(percentage));
    QFont font;
    font.setWeight(kDigitWeight);
    font.setPixelSize(static_cast<int>(size * kDigitSize));
    painter.setFont(font);
    painter.setPen(QPen(value));
    painter.drawText(bounds, Qt::AlignCenter, text);
}

QPixmap IconRenderer::renderPixmap(int size, std::optional<double> percentage,
                                   const Options& options)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (options.stale)
        painter.setOpacity(kStaleOpacity);

    // With no data there is no number to show, so both modes fall back to the
    // empty dial - "unknown" looks the same however the icon is configured.
    if (percentage && options.style == core::Config::TrayStyle::Percentage) {
        paintNumberInArc(painter, size, std::clamp(*percentage, 0.0, 100.0), options);
        return pixmap;
    }

    const gauge::Colors colors {
        options.foreground,
        percentage ? colorFor(*percentage, options) : options.foreground,
    };
    gauge::paint(painter, QRectF(0, 0, size, size), percentage, colors);

    return pixmap;
}

QIcon IconRenderer::render(std::optional<double> percentage, const Options& options)
{
    QIcon icon;
    for (const int size : kSizes)
        icon.addPixmap(renderPixmap(size, percentage, options));
    return icon;
}

} // namespace claudometer::tray
