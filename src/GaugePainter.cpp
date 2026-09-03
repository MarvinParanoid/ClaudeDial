#include "GaugePainter.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>

namespace claudometer::gauge {
namespace {

/// Qt measures angles counter-clockwise from 3 o'clock, so sweeping -240 from
/// 210 runs down-left, over the top, to down-right - leaving the gap at the
/// bottom, where a dial's gap belongs.
constexpr double kStartAngle = 210.0;
constexpr double kSweep = -240.0;

/// Proportions of the bounding box.
///
/// These were chosen by rendering the mark into a real Plasma panel at 22 px and
/// comparing, not by looking at a magnified mock-up - at this size a thin dial
/// dissolves into a grey smudge and reads as a loading spinner. Hence a dial
/// this thick, and a needle held well clear of it: at a 1.5 px gap the needle
/// and the fill merge into a single lump at low percentages.
constexpr double kDialThickness = 0.17;

/// Margin between the arc's outer edge and the icon's edge. Small on purpose:
/// the arc is the mark, and the panel already surrounds the icon with its own
/// spacing. Shared by both styles, which is what keeps their silhouettes equal.
constexpr double kMargin = 0.005;
constexpr double kNeedleThickness = 0.12;
constexpr double kNeedleLength = 0.44; ///< of the dial radius
constexpr double kTrackAlpha = 0.20;   ///< the unfilled dial must stay quiet

} // namespace

void paint(QPainter& painter, const QRectF& bounds, std::optional<double> percentage,
           const Colors& colors, double thicknessScale, Center center, Fill fill)
{
    const double size = std::min(bounds.width(), bounds.height());
    if (size <= 0)
        return;

    const double scale = std::max(0.1, thicknessScale);
    const double dialWidth = std::max(1.5, size * kDialThickness * scale);
    const double inset = dialWidth / 2.0 + size * kMargin;
    const QRectF square(bounds.center().x() - size / 2.0, bounds.center().y() - size / 2.0,
                        size, size);
    const QRectF dial = square.adjusted(inset, inset, -inset, -inset);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);

    QColor track = colors.dial;
    track.setAlphaF(static_cast<float>(kTrackAlpha));
    painter.setPen(QPen(track, dialWidth, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(dial, static_cast<int>(kStartAngle * 16), static_cast<int>(kSweep * 16));

    if (!percentage) {
        painter.restore();
        return;
    }

    const double clamped = std::clamp(*percentage, 0.0, 100.0);

    // The fill is the magnitude cue: at a glance, how much of the dial is used.
    if (fill == Fill::Usage && clamped > 0.0) {
        painter.setPen(QPen(colors.value, dialWidth, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(dial, static_cast<int>(kStartAngle * 16),
                        static_cast<int>(kSweep * clamped / 100.0 * 16));
    }

    if (center == Center::Empty) {
        painter.restore();
        return; // the caller fills the middle - with the number, in practice
    }

    // The needle is the identity: it makes the mark a meter rather than a
    // progress ring, and its angle is legible even where the fill is not.
    const QPointF centre = dial.center();
    const double radians = qDegreesToRadians(kStartAngle + kSweep * clamped / 100.0);
    const double length = dial.width() / 2.0 * kNeedleLength;

    painter.setPen(QPen(colors.value, std::max(1.5, size * kNeedleThickness * scale),
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(centre, QPointF(centre.x() + std::cos(radians) * length,
                                     centre.y() - std::sin(radians) * length));

    painter.restore();
}

double outerRadiusFraction()
{
    return 0.5 - kMargin;
}

} // namespace claudometer::gauge
