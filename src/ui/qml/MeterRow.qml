import QtQuick

// One quota window: label, percentage, a thin bar, and the reset line.
// The percentage is the largest thing on the row - it is what the user came for.
Item {
    id: root

    property alias label: labelText.text
    property int percentage: 0
    property string reset: ""
    property string level: "normal"
    property Theme theme

    implicitHeight: column.implicitHeight

    Column {
        id: column
        width: parent.width
        spacing: 9

        Item {
            width: parent.width
            height: Math.max(labelText.implicitHeight, valueText.implicitHeight)

            Text {
                id: labelText
                anchors.verticalCenter: parent.verticalCenter
                color: root.theme.text
                font.pixelSize: 13
            }

            Text {
                id: valueText
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.percentage + "%"
                color: root.theme.levelColor(root.level)
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
        }

        // The bar and its reset line are one unit, so they sit closer together
        // than they do to the label above.
        Column {
            width: parent.width
            spacing: 6

            Rectangle {
                width: parent.width
                height: 4
                radius: 2
                color: root.theme.track

                Rectangle {
                    width: parent.width * Math.min(Math.max(root.percentage, 0), 100) / 100
                    height: parent.height
                    radius: parent.radius
                    color: root.theme.levelColor(root.level)
                    // Below the warning threshold the bar is quieter than the
                    // number, so the row reads as one value rather than two marks.
                    opacity: root.level === "normal" ? 0.55 : 1.0

                    Behavior on width {
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }
                }
            }

            Text {
                text: root.reset
                visible: root.reset !== ""
                color: root.theme.subtext
                font.pixelSize: 11
            }
        }
    }
}
