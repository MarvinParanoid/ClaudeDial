import QtQuick

// A small segmented picker, used for the three-way theme choice.
//
// Chosen over a drop-down deliberately: all three options are visible at once in
// a form this small, and it avoids styling a Controls popup - the one part of
// Qt Quick Controls whose default colours are hardest to override cleanly, and
// therefore the most likely to reintroduce the mismatch that made light-theme
// labels invisible.
Rectangle {
    id: root

    required property Theme theme
    property var options: []
    property int currentIndex: 0

    signal activated(int index)

    implicitWidth: row.implicitWidth + 4
    implicitHeight: 28
    radius: 6
    color: theme.hover

    Row {
        id: row
        x: 2
        y: 2
        spacing: 0

        Repeater {
            model: root.options

            delegate: Rectangle {
                required property int index
                required property string modelData

                readonly property bool current: root.currentIndex === index

                width: Math.max(46, label.implicitWidth + 18)
                height: root.height - 4
                radius: 5
                color: current ? root.theme.accent
                               : mouse.containsMouse ? root.theme.track : "transparent"

                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checked: current

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 12
                    color: parent.current ? "#ffffff" : root.theme.subtext
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(index)
                }
            }
        }
    }
}
