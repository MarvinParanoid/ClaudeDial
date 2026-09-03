import QtQuick

// Design tokens. The colours with meaning come from `colors`, a C++ object, so
// that the usage ramp has one definition shared with the tray renderer and the
// accent is the user's own Plasma accent rather than one we invented.
//
// Three roles, kept apart:
//   brand   - Claudometer's identity: the header mark, and nothing else.
//   accent  - interaction: switches, the selected segment, the busy spinner.
//   usage*  - how much of a limit is spent.
QtObject {
    readonly property bool isDark: colors.dark

    readonly property color background:  isDark ? "#22262c" : "#ffffff"
    readonly property color separator:   isDark ? "#2a2f36" : "#f3f5f6"
    readonly property color border:      isDark ? "#363b43" : "#e2e4e8"
    readonly property color text:        isDark ? "#e7e9ec" : "#1b1e23"
    readonly property color subtext:     isDark ? "#a8aeb7" : "#5f656d"
    readonly property color track:       isDark ? "#343941" : "#e8eaee"
    readonly property color hover:       isDark ? "#2c3138" : "#f2f3f5"
    readonly property color switchOff:   isDark ? "#3a4048" : "#c9ced6"

    readonly property color brand:  colors.brand
    readonly property color accent: colors.accent

    readonly property int radius: 10
    readonly property int padding: 16
    readonly property int gap: 14

    function levelColor(level) {
        if (level === "limit" || level === "severe")
            return colors.usageSevere
        if (level === "critical")
            return colors.usageCritical
        if (level === "warning")
            return colors.usageWarning
        return colors.usageNormal
    }
}
