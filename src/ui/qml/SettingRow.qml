import QtQuick

// A label on the left and one control on the right. The control is given as a
// child, so the rows in Settings.qml read as what they are rather than as
// layout scaffolding.
Item {
    id: root

    required property Theme theme
    property alias label: labelText.text

    default property alias control: holder.children

    implicitHeight: 34

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        color: root.theme.text
        font.pixelSize: 13
    }

    Item {
        id: holder
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: childrenRect.width
        height: childrenRect.height
    }
}
