import QtQuick
import QtQuick.Controls.Basic

// A switch, built on Controls' CheckBox so that keyboard focus, space-to-toggle
// and the accessibility role come for free, with every painted part replaced by
// our own tokens.
//
// The Basic style is imported explicitly rather than relying on a style being
// configured: Basic is the template set intended for customisation, and the
// styles that read QPalette are exactly what broke the light theme - the
// platform theme kept a dark palette while our own tokens went light, and the
// labels turned white on white.
CheckBox {
    id: control

    required property Theme theme

    padding: 0
    spacing: 10
    implicitHeight: 24
    font.pixelSize: 13

    indicator: Rectangle {
        implicitWidth: 32
        implicitHeight: 18
        x: 0
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? control.theme.accent : control.theme.switchOff

        Behavior on color {
            ColorAnimation { duration: 130 }
        }

        Rectangle {
            width: 14
            height: 14
            radius: width / 2
            color: "#ffffff"
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 2 : 2

            Behavior on x {
                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
            }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.theme.text
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }

    background: null
}
