#include "IconRenderer.h"

#include "Brand.h"
#include "GaugePainter.h"
#include "core/UsageLevel.h"

#include <QFont>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace claudedial::tray {
namespace {

/// Sizes a Linux panel actually asks for. Each is rendered directly so the mark
/// stays crisp instead of being downscaled from one large pixmap.
constexpr int kSizes[] = { 16, 22, 24, 32, 48, 64 };

/// The arc's stroke weight in the tray, for both styles.
///
/// The number style forced the value. A block of two digits is a rectangle
/// inscribed in the arc's circle, so the interior radius has to cover half its
/// diagonal, and at heavier weights it does not: at 0.45 and even at 0.38, 88
/// and 99 still touch the arc walls in a real panel.
///
/// The gauge style then adopted it, which is the point. Drawn at full weight it
/// read as a donut or a progress widget rather than as the same dial with a
/// different middle - the colour mass dominated the needle from 75% up, and the
/// two styles looked like two families of icon instead of one mark read two
/// ways. Identical geometry, and only the middle differs: a needle, or the
/// number. Checked at a real 16 and 22 px, magnified without interpolation
/// rather than judged from a scaled-up mock-up; at 16 px the stroke floor makes
/// the alternatives indistinguishable anyway.
constexpr double kTrayArcScale = 0.32;

/// Nudge the digits down a little. Below the centre the arc simply is not there
/// - the gap is at the bottom - so this is clearance bought for free, and it is
/// where the top of a two-digit block needs it.
constexpr double kDigitOffset = 0.03;

/// Digit size as a fraction of the icon. Settled by rendering the combinations
/// at 16, 20, 22 and 24 px: this is the largest that still leaves visible air
/// between the digits and the arc.
constexpr double kDigitSize = 0.52;

/// DemiBold rather than Bold: at 16-22 px Bold fills the counters and looks
/// blunt, while Medium goes thin enough to fade at 16 px, particularly in the
/// warning and critical colours.
constexpr QFont::Weight kDigitWeight = QFont::DemiBold;

/// At the limit the icon shows this instead of "100".
///
/// Three digits were tried, at a reduced size so they would fit. In a real panel
/// they came out visibly weaker than "99" - the one reading that most needs to
/// carry. A full red arc around an exclamation mark says "limit reached" more
/// firmly than a cramped number, and the tooltip still gives the exact figure.
constexpr auto kLimitGlyph = "!";

/// How far the mark fades once the data behind it is stale. Enough to read as
/// "not current" at a glance, not so far that the number becomes unreadable.
constexpr double kStaleOpacity = 0.45;

} // namespace

QColor IconRenderer::colorFor(double percentage, const Options& options)
{
    // One ramp, one definition, shared with the popup and with --json. The
    // neutral step is the panel's own foreground: a tray icon stays monochrome
    // until something needs attention, because it is on screen permanently.
    return brand::usageColour(
        core::levelFor(percentage, options.warningThreshold, options.criticalThreshold),
        options.foreground);
}

void IconRenderer::paintNumberInArc(QPainter& painter, int size, double percentage,
                                    const Options& options)
{
    const QColor value = colorFor(percentage, options);
    const QRectF bounds(0, 0, size, size);

    // The same arc as the gauge style, drawn by the same function, with its
    // middle left for us.
    gauge::paint(painter, bounds, percentage, { options.foreground, value }, kTrayArcScale,
                 gauge::Center::Empty);

    const QString text = percentage >= 100.0 ? QString::fromLatin1(kLimitGlyph)
                                             : QString::number(qRound(percentage));

    QFont font;
    font.setWeight(kDigitWeight);
    font.setPixelSize(static_cast<int>(size * kDigitSize));
    painter.setFont(font);
    painter.setPen(QPen(value));
    painter.drawText(bounds.translated(0, size * kDigitOffset), Qt::AlignCenter, text);
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
    gauge::paint(painter, QRectF(0, 0, size, size), percentage, colors, kTrayArcScale);

    return pixmap;
}

QIcon IconRenderer::render(std::optional<double> percentage, const Options& options)
{
    QIcon icon;
    for (const int size : kSizes)
        icon.addPixmap(renderPixmap(size, percentage, options));
    return icon;
}

} // namespace claudedial::tray
