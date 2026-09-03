#pragma once

#include "core/Config.h"

#include <QColor>
#include <QIcon>

#include <optional>

namespace claudometer::tray {

/// Draws the tray icon.
///
/// Every decision here comes from looking at the result in a real Plasma panel
/// at 22 px next to Chrome and Teams, not from a magnified mock-up. At that size
/// a mock-up tells you nothing: the first version looked like a speedometer when
/// enlarged and like a grey loading spinner in the panel.
class IconRenderer
{
public:
    struct Options {
        /// Needle or number. Both draw the same Claudometer arc, and the arc
        /// fills with usage either way - so the two are variants of one mark and
        /// a full ring is never drawn. A full ring was tried first and read as a
        /// notification badge or a battery indicator.
        core::Config::TrayStyle style = core::Config::TrayStyle::Percentage;

        /// Neutral colour, taken from the application palette so the icon
        /// follows the panel's light/dark theme.
        QColor foreground = QColor(220, 220, 220);

        int warningThreshold = 75;
        int criticalThreshold = 90;

        /// Dim the whole mark. The number on screen is the last one we managed
        /// to fetch, and until now the tray gave no sign of that at all - only
        /// the tooltip and the popup said so, which is no use to a glance.
        bool stale = false;
    };

    /// The application's own mark, with no reading on it - for the window icon
    /// and anywhere else a logotype rather than a measurement is wanted.
    [[nodiscard]] static QIcon logo(const QColor& foreground);

    /// A multi-resolution icon; the panel picks the size it needs.
    /// `percentage` of nullopt renders the "no data" mark - an empty dial, so
    /// the icon never implies 0%.
    [[nodiscard]] static QIcon render(std::optional<double> percentage, const Options& options);

private:
    [[nodiscard]] static QColor colorFor(double percentage, const Options& options);
    [[nodiscard]] static QPixmap renderPixmap(int size, std::optional<double> percentage,
                                              const Options& options);
    static void paintNumberInArc(QPainter& painter, int size, double percentage,
                                 const Options& options);
};

} // namespace claudometer::tray
