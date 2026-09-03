import QtQuick

// A flat, quiet button. Restrained on purpose: these are secondary actions and
// should not compete with the percentages.
Item {
    id: root

    property string glyph: ""
    property color color: "#6c727a"
    property color hoverColor: "#f2f3f5"
    property string tooltip: ""

    signal clicked()

    implicitWidth: 24
    implicitHeight: 24

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: mouse.containsMouse ? root.hoverColor : "transparent"
    }

    Text {
        anchors.centerIn: parent
        text: root.glyph
        color: root.color
        font.pixelSize: 14
        opacity: mouse.containsMouse ? 1.0 : 0.75
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
