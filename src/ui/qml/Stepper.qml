import QtQuick
import QtQuick.Controls.Basic

// A number with minus and plus, on Controls' SpinBox so arrow keys and the
// accessibility role still work. Not editable: every value here is a small
// number reached faster by stepping than by typing.
//
// The glyphs are plain ASCII on purpose - a settings form should not depend on
// the user having a font with the typographic minus sign.
SpinBox {
    id: control

    required property Theme theme
    property string suffix: ""

    editable: false
    padding: 0
    implicitWidth: 104
    implicitHeight: 28
    font.pixelSize: 13

    textFromValue: function(value) { return value + control.suffix }

    background: Rectangle {
        radius: 6
        color: control.theme.hover
    }

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    down.indicator: Rectangle {
        x: 0
        height: control.height
        width: 28
        radius: 6
        color: control.down.pressed ? control.theme.track : "transparent"

        Text {
            anchors.centerIn: parent
            text: "-"
            color: control.down.pressed ? control.theme.text : control.theme.subtext
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }

    up.indicator: Rectangle {
        x: control.width - width
        height: control.height
        width: 28
        radius: 6
        color: control.up.pressed ? control.theme.track : "transparent"

        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.up.pressed ? control.theme.text : control.theme.subtext
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }
}
