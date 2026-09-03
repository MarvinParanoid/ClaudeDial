#pragma once

namespace claudometer::core::gaugeGeometry {

/// The canonical Claudometer gauge, as pure numbers.
///
/// Every rendition of the mark comes from these: the application icon, the popup
/// header, the settings window's icon, both tray styles and the notification
/// icon. Only the *content* of the dial changes between them - a needle, a
/// number, or nothing - along with size and semantic colour.
///
/// They live in core, with no Qt drawing types, so that both the painter and the
/// test that checks the shipped SVG against them can read the same values. The
/// SVG had been hand-copied and had drifted: its centre sat 7.8% lower and its
/// outer edge 5% smaller than everything else, which is precisely why the
/// application icon and the popup mark did not look like the same thing.
///
/// Angles are degrees, counter-clockwise from three o'clock, as Qt measures
/// them. Lengths are fractions of the icon's edge.

/// The dial runs from here, sweeping to kStartAngle + kSweep, which leaves the
/// gap at the bottom where a dial's gap belongs.
constexpr double kStartAngle = 210.0;
constexpr double kSweep = -240.0;

/// Distance from the arc's outer edge to the icon's edge. The panel already
/// surrounds an icon with its own spacing.
constexpr double kMargin = 0.005;

/// The outer edge of the arc. Independent of stroke weight by construction,
/// because the inset is half the stroke plus the margin - which is what lets a
/// lighter rendition keep the same silhouette.
constexpr double kOuterRadius = 0.5 - kMargin;

/// Stroke of the dial, at full weight.
constexpr double kDialThickness = 0.17;

/// Stroke of the needle, and how far it reaches as a fraction of the dial's
/// centreline radius. Short enough to stay clear of the arc: at a 1.5 px gap the
/// needle and the fill merge into one lump at low percentages.
constexpr double kNeedleThickness = 0.12;
constexpr double kNeedleLength = 0.44;

/// The unfilled part of the dial, when a fill is drawn over it.
constexpr double kTrackAlpha = 0.20;

/// The centre of the dial, as a fraction of the icon's edge. Both axes.
///
/// A 240-degree arc puts more ink above its centre than below, so it is
/// tempting to shift the circle down to balance it. Don't: that is the drift
/// described above. One number, everywhere.
constexpr double kCentre = 0.5;

/// The needle's angle for a given percentage.
constexpr double needleAngle(double percentage)
{
    return kStartAngle + kSweep * percentage / 100.0;
}

/// Centreline radius for a given stroke weight - the arc's outer edge is fixed,
/// so a thinner stroke sits slightly further out.
constexpr double dialRadius(double thickness)
{
    return kOuterRadius - thickness / 2.0;
}

/// Where the identity mark's needle points when it is not reporting anything.
/// Far enough round to read as "measuring something" rather than empty or full.
constexpr double kIdentityPercentage = 62.0;

} // namespace claudometer::core::gaugeGeometry
