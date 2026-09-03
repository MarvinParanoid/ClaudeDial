#pragma once

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
        /// Draw the number inside a ring - the default. The number is the whole
        /// point of a tray indicator: it tells you the state without a hover and
        /// without interpreting anything. The ring around it still fills as the
        /// limit is spent, so the icon carries an exact value and a visual state
        /// at once. Clear this to get the dial instead.
        bool showPercentage = true;

        /// Neutral colour, taken from the application palette so the icon
        /// follows the panel's light/dark theme.
        QColor foreground = QColor(220, 220, 220);

        int warningThreshold = 75;
        int criticalThreshold = 90;
    };

    /// A multi-resolution icon; the panel picks the size it needs.
    /// `percentage` of nullopt renders the "no data" mark - an empty dial, so
    /// the icon never implies 0%.
    [[nodiscard]] static QIcon render(std::optional<double> percentage, const Options& options);

private:
    [[nodiscard]] static QColor colorFor(double percentage, const Options& options);
    [[nodiscard]] static QPixmap renderPixmap(int size, std::optional<double> percentage,
                                              const Options& options);
    static void paintRingWithNumber(QPainter& painter, int size, double percentage,
                                    const Options& options);
};

} // namespace claudometer::tray
