import QtQuick

// A quiet group label. Uppercase and small, so it organises the form without
// competing with the settings themselves.
Text {
    property Theme theme

    color: theme.subtext
    font.pixelSize: 10
    font.weight: Font.DemiBold
    font.capitalization: Font.AllUppercase
    font.letterSpacing: 0.8
}
