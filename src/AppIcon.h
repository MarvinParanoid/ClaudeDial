#pragma once

#include <QIcon>

namespace claudometer {

/// The application's identity mark, as distinct from the tray indicator.
///
/// These are two different jobs and were briefly conflated, to the detriment of
/// both. The tray mark is generated per poll, depends on usage, and has its
/// stroke weights tuned for 16-24 px on a panel whose colours we cannot know.
/// This is a static icon that has to survive a launcher, a task manager and a
/// window title bar - drawn once, boldly, with no reading on it.
///
/// Resolution order:
///   1. the installed freedesktop icon, by theme name, which is what the
///      desktop itself uses for `Icon=claudometer` in the .desktop entry - and
///      on Wayland it is how the compositor finds a window's icon at all;
///   2. the same artwork bundled in the binary, so running from a build tree
///      looks right too.
///
/// One SVG serves both, so there is no second copy of the drawing to drift.
[[nodiscard]] QIcon applicationIcon();

} // namespace claudometer
