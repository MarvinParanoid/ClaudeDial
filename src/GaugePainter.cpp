#include "GaugePainter.h"

#include "core/GaugeGeometry.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>

namespace claudometer::gauge {
namespace {

using namespace claudometer::core::gaugeGeometry;

/// Proportions for a logotype rather than an indicator.
///
/// The dial is shared, but a needle tuned for 16 px - where it only has to say
/// which way it points, with a fill behind it for context - reads as a stub at
/// 48 px and up with no fill. Length and weight are the only things that change.
constexpr double kLogoNeedleThickness = 0.075;
constexpr double kLogoNeedleLength = 0.68;

} // namespace

void paint(QPainter& painter, const QRectF& bounds, std::optional<double> percentage,
           const Colors& colors, double thicknessScale, Center center, Fill fill)
{
    const double size = std::min(bounds.width(), bounds.height());
    if (size <= 0)
        return;

    const double scale = std::max(0.1, thicknessScale);
    const double dialWidth = std::max(1.5, size * kDialThickness * scale);
    const double radius = size * dialRadius(dialWidth / size);
    const QPointF centre(bounds.center().x() - size / 2.0 + size * kCentre,
                         bounds.center().y() - size / 2.0 + size * kCentre);
    const QRectF dial(centre.x() - radius, centre.y() - radius, radius * 2, radius * 2);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);

    // With a fill on top, the dial is the *unused* part and has to stay quiet.
    // With no fill it is the mark itself, and must be solid.
    QColor track = colors.dial;
    if (fill == Fill::Usage)
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
    const bool asLogo = fill == Fill::None;
    const double radians = qDegreesToRadians(needleAngle(clamped));
    const double length = dial.width() / 2.0 * (asLogo ? kLogoNeedleLength : kNeedleLength);
    const double needleWidth =
        size * (asLogo ? kLogoNeedleThickness : kNeedleThickness) * scale;

    painter.setPen(QPen(colors.value, std::max(1.5, needleWidth), Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(centre, QPointF(centre.x() + std::cos(radians) * length,
                                     centre.y() - std::sin(radians) * length));

    painter.restore();
}

} // namespace claudometer::gauge
