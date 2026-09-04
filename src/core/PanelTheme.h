#pragma once

#include <QString>

#include <optional>

namespace claudedial::core {

/// What a Plasma panel actually looks like, read from configuration.
///
/// This exists because the panel's colour cannot be asked for at runtime and
/// cannot be inferred from QPalette. QPalette describes the *application*
/// colours, and on Plasma the panel is a separate setting: the shipped Breeze
/// Twilight look-and-feel pairs `ColorScheme=BreezeLight` with
/// `plasmarc Theme=breeze-dark`. Guessing from the palette left the tray icon
/// invisible there.
///
/// Plasma does write it down, though, and the value is exact rather than a
/// heuristic: breeze-dark declares `BackgroundNormal=32,35,38`, which is the
/// #202326 measured off the panel with a screenshot.
///
/// Parsing only, so it can be tested without a desktop: the caller reads the
/// files and passes their contents in.

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;

    /// Rec. 709 luma, enough to choose between a light and a dark mark.
    [[nodiscard]] double luminance() const;
    [[nodiscard]] bool isDark() const { return luminance() < 128.0; }
};

/// The Plasma theme in effect. The user's `plasmarc` wins; failing that, the
/// look-and-feel package's defaults, which is where the value lives until
/// somebody changes the theme by hand - it was absent on the machine this was
/// written on, while the desktop was plainly using breeze-dark.
///
/// `lookAndFeelDefaults` groups keys as `[plasmarc][Theme]`, which is its own
/// format rather than plain INI.
[[nodiscard]] QString plasmaThemeName(const QString& plasmarc,
                                      const QString& lookAndFeelDefaults);

/// The look-and-feel package named by kdeglobals, or empty. Read without regard
/// to which group holds it: Plasma writes it under `[KDE]`, but the key is
/// unique in the file and pinning the group buys nothing.
[[nodiscard]] QString lookAndFeelPackage(const QString& kdeglobals);

/// The panel background a Plasma theme declares.
///
/// nullopt when the theme ships no `colors` file - which is how the stock
/// `default` theme says it follows the application colour scheme, and exactly
/// the case where QPalette is the right answer after all.
[[nodiscard]] std::optional<Rgb> plasmaPanelBackground(const QString& themeColors);

} // namespace claudedial::core
