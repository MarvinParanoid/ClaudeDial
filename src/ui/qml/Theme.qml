import QtQuick

// Design tokens, in one place. `isDark` is a context property set from C++ so
// that the "System" theme setting and an explicit Light/Dark choice arrive
// through the same path.
QtObject {
    readonly property color background:  isDark ? "#22262c" : "#ffffff"
    // Inner rules only. Kept close to the background on purpose: any more
    // contrast and the card starts reading as a table rather than a card.
    readonly property color separator:   isDark ? "#2a2f36" : "#f3f5f6"
    // The card's own edge against the desktop, which does need to be visible.
    readonly property color border:      isDark ? "#363b43" : "#e2e4e8"
    readonly property color text:        isDark ? "#e7e9ec" : "#1b1e23"
    readonly property color subtext:     isDark ? "#a8aeb7" : "#5f656d"
    readonly property color track:       isDark ? "#343941" : "#e8eaee"
    readonly property color hover:       isDark ? "#2c3138" : "#f2f3f5"
    /// Off-state of a switch. Must contrast with the white knob in both themes.
    readonly property color switchOff:   isDark ? "#3a4048" : "#c9ced6"

    // The quiet colour a percentage wears below the warning threshold. Still the
    // largest thing on its row, but carrying no alarm.
    readonly property color quiet:       isDark ? "#c3cad3" : "#4a5058"

    // NOTE: the tray icon uses a shorter ramp - monochrome, then warning, then
    // critical (see IconRenderer.cpp). That is deliberate: a panel icon is on
    // screen permanently and must not be a standing splash of colour, while this
    // popup is only visible while it is being read.
    readonly property color accent:      isDark ? "#5aa2f0" : "#2b7fd4"
    readonly property color warning:     isDark ? "#f09a3c" : "#c2740a"
    readonly property color critical:    isDark ? "#e05c68" : "#c0392b"

    readonly property int radius: 10
    readonly property int padding: 16
    readonly property int gap: 14

    function levelColor(level) {
        if (level === "limit" || level === "severe")
            return critical
        if (level === "critical")
            return warning
        if (level === "warning")
            return accent
        return quiet
    }
}
