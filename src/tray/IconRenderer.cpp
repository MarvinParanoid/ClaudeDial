#include "IconRenderer.h"

#include "Brand.h"
#include "GaugePainter.h"
#include "core/GaugeGeometry.h"
#include "core/UsageLevel.h"

#include <QFont>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace claudedial::tray {
namespace {

/// Sizes a Linux panel actually asks for. Each is rendered directly so the mark
/// stays crisp instead of being downscaled from one large pixmap.
constexpr int kSizes[] = { 15, 16, 22, 24, 32, 48, 64 };

/// Above this, the mark is drawn as designed. At or below it, the design does
/// not survive the pixels and a simpler one is drawn instead - see
/// paintSmallNumber and the note on kSmallGaugeScale.
///
/// Chosen by the size, never by the desktop. i3bar asks for 15; a
/// StatusNotifierItem host is never asked at all - it receives every size in
/// kSizes and picks one - so baking the simpler mark into the small entries is
/// the only way to reach a host with a small panel. Plasma takes 22 or 24 and
/// is untouched by any of this.
constexpr int kSmallIconMax = 16;

/// The small gauge's stroke, against 0.32 above it. With no number inside, the
/// arc has the whole box and can afford weight - and weight is what survives
/// few pixels. Measured at 15 px: 0.32 reads as grey haze with the needle
/// barely present, and a 1 px arc with antialiasing off turns visibly
/// polygonal. This is the one that stays a dial.
constexpr double kSmallGaugeScale = 0.55;

/// The small number's share of the box, against kDigitSize above it. "99" at
/// this size measures 13.7 px of advance in a 15 px box, which fits; the arc
/// does not fit beside it, which is why the small variant replaces the arc
/// with a bar rather than shrinking the digits back down.
constexpr double kSmallDigitSize = 0.86;
constexpr double kSmallDigitLift = 0.08;

/// The usage bar under the small number: the state indicator, reduced to the
/// one shape that is unambiguous at this size.
constexpr double kSmallBarHeight = 0.14;

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

/// The number at nearly full height with a usage bar beneath it.
///
/// At 15 px the designed mark reads as a number trapped inside a grey ring:
/// reported from GNOME as "an icon inside an icon", and from i3 as the arc
/// turning to noise. Enlarging the digits inside the arc only makes the two
/// collide. So below kSmallIconMax the number becomes the mark and the arc
/// becomes a bar - which loses the dial, and keeps the reading.
void IconRenderer::paintSmallNumber(QPainter& painter, int size, double percentage,
                                    const Options& options)
{
    const QColor value = colorFor(percentage, options);
    const QRectF box(0, 0, size, size);

    QFont font;
    font.setWeight(kDigitWeight);
    font.setPixelSize(static_cast<int>(size * kSmallDigitSize));
    painter.setFont(font);
    painter.setPen(QPen(value));
    painter.drawText(box.translated(0, -size * kSmallDigitLift), Qt::AlignCenter,
                     percentage >= 100.0 ? QString::fromLatin1(kLimitGlyph)
                                         : QString::number(qRound(percentage)));

    // Pixel-aligned and unantialiased: a two-pixel bar either lands on the grid
    // or turns into two rows of grey.
    QColor track = options.foreground;
    track.setAlphaF(static_cast<float>(core::gaugeGeometry::kTrackAlpha));
    const double height = std::max(2.0, std::round(size * kSmallBarHeight));
    const double y = size - height;
    const double width = size - 2.0;

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(track);
    painter.drawRect(QRectF(1.0, y, width, height));
    painter.setBrush(value);
    painter.drawRect(QRectF(1.0, y, width * std::clamp(percentage, 0.0, 100.0) / 100.0, height));
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
        const double clamped = std::clamp(*percentage, 0.0, 100.0);
        if (size <= kSmallIconMax)
            paintSmallNumber(painter, size, clamped, options);
        else
            paintNumberInArc(painter, size, clamped, options);
        return pixmap;
    }

    const gauge::Colors colors {
        options.foreground,
        percentage ? colorFor(*percentage, options) : options.foreground,
    };
    gauge::paint(painter, QRectF(0, 0, size, size), percentage, colors,
                 size <= kSmallIconMax ? kSmallGaugeScale : kTrayArcScale);

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
