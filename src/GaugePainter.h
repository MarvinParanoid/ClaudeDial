#pragma once

#include <QColor>
#include <QRectF>

#include <optional>

class QPainter;

namespace claudometer::gauge {

/// Colours for one gauge. Supplied by the caller: the tray icon must follow the
/// panel's palette, the popup follows its own.
struct Colors {
    QColor dial;  ///< base colour of the unfilled dial; drawn at low alpha
    QColor value; ///< the used portion of the dial, and the needle
};

/// Draws the Claudometer speedometer: a thick 240-degree dial with the gap at
/// the bottom, the used portion filled, and a needle. 0% points down-left, 100%
/// down-right.
///
/// The fill carries the magnitude and the needle carries the identity. Both are
/// needed: without the needle it is just a progress ring, and without the fill
/// the mark is too faint to read in a panel.
///
/// `percentage` of nullopt means "no data": the dial is drawn with no needle, so
/// the mark never implies 0%.
///
/// This is the only implementation of the mark. The tray icon and the popup
/// header both call it, which is what makes them identical rather than similar.
/// What sits inside the dial.
///
/// The two tray styles are variants of one mark, not two icons: the arc is the
/// same in both, and only its middle differs - a needle, or the exact number.
/// Empty leaves the middle to the caller.
enum class Center {
    Needle,
    Empty,
};

/// Whether the arc shows a reading at all.
///
/// None is for the application icon: a logotype should not appear to be
/// displaying data, and a partially filled arc does exactly that.
enum class Fill {
    Usage,
    None,
};

/// `thicknessScale` thins the strokes without changing the dial's radius.
/// The tray icon uses 1.0 - that weight is what makes it survive at 16-22 px on
/// an unknown panel. The popup header sits on a calm card next to 14 px text,
/// where the same weight reads as heavy, so it asks for a lighter one. The
/// geometry stays shared; only the stroke weight and the colours differ.
void paint(QPainter& painter, const QRectF& bounds, std::optional<double> percentage,
           const Colors& colors, double thicknessScale = 1.0, Center center = Center::Needle,
           Fill fill = Fill::Usage);

/// The outer radius of the arc, as a fraction of the icon.
///
/// Independent of stroke weight by construction - the inset is half the stroke
/// plus a fixed margin, so the arc's outer silhouette is identical however
/// heavily it is drawn. That is what makes the two tray styles the same mark
/// rather than two marks of similar shape.
[[nodiscard]] double outerRadiusFraction();

} // namespace claudometer::gauge
