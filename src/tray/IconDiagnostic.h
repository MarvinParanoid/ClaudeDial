#pragma once

// TEMPORARY, and meant to be deleted.
//
// On Debian 13 with i3 on X11, Qt's XEmbed path docks the tray icon correctly -
// xwininfo shows the 15x15 tray window at the right place in i3bar - and the
// icon is blank. Docking working rules out the integration, so the question is
// which of four things is at fault, and each run on a test machine is
// expensive. These modes and the report below are arranged to answer all four
// in as few runs as possible.
//
// Delete this file, IconDiagnostic.cpp, and the three call sites in
// Application.cpp once the answer is in.

#include <QColor>
#include <QIcon>

namespace claudedial::tray::diagnostic {

enum class Mode {
    Off,

    /// A fully opaque square, drawn at the same sizes as the real icon. Can any
    /// pixel of ours reach the panel at all? If this shows, XEmbed and the size
    /// set are both fine and the fault is in what we draw.
    Solid,

    /// The same square inset in transparency. An XEmbed tray that does not
    /// advertise _NET_SYSTEM_TRAY_VISUAL may have no ARGB visual to composite
    /// into, which turns a transparent background into nothing or into black.
    /// Solid visible + Alpha blank means exactly that.
    Alpha,

    /// The real icon, with the panel foreground replaced by a colour no panel
    /// could match. The neutral colour comes from QPalette::WindowText, which
    /// on i3 with a generic platform theme is near black - and i3bar's default
    /// background is black. Black on black looks precisely like a blank icon.
    Mono,
};

/// Reads CLAUDEDIAL_DIAGNOSTIC_ICON: solid, alpha, mono - or 1 for solid.
[[nodiscard]] Mode requested();

/// The stand-in icon for Solid and Alpha.
[[nodiscard]] QIcon icon(Mode mode);

/// The colour Mono substitutes for the sampled panel foreground.
[[nodiscard]] QColor monoColour();

/// Prints what we know to stderr, once. Includes the sampled foreground, which
/// may settle the Mono question without anyone having to run that mode.
void report(Mode mode, const QColor& panelForeground, const QIcon& icon);

} // namespace claudedial::tray::diagnostic
