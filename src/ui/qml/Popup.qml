import QtQuick
import Claudometer

// The whole UI. Deliberately not a dashboard: two numbers, two bars, two reset
// lines, and when it was last updated. Nothing here scrolls, navigates or
// charts anything.
Rectangle {
    id: root

    readonly property int contentWidth: 340

    implicitWidth: contentWidth
    implicitHeight: layout.implicitHeight + theme.padding * 2

    color: theme.background
    radius: theme.radius
    border.width: 1
    border.color: theme.border

    Theme { id: theme }

    // Works only when the compositor gave the popup keyboard focus; the tray
    // toggle and the close button are the reliable ways out.
    focus: true
    Keys.onEscapePressed: usage.close()

    Column {
        id: layout
        x: theme.padding
        y: theme.padding
        width: root.contentWidth - theme.padding * 2
        spacing: theme.gap

        // --- header -----------------------------------------------------------
        Item {
            width: parent.width
            height: 24

            // The header doubles as a title bar. Declared first so the buttons,
            // which come later, sit on top of it and still receive their clicks.
            MouseArea {
                anchors.fill: parent
                onPressed: popupWindow.beginMove()
            }

            // The same mark as the application icon: same canonical geometry,
            // same drawing function.
            Gauge {
                id: headerGauge
                width: 20
                height: 20
                // A little lighter than the panel weight, which reads as heavy
                // next to 14 px text on a calm card. Same geometry regardless.
                thicknessScale: 0.85
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                hasValue: usage.fiveHourAvailable
                percentage: usage.fiveHourPercent
                // Always the brand colour, never a usage colour: this is the
                // logotype. A terracotta icon in the panel and a grey mark here
                // would read as two identities rather than one - and a solid
                // dial, so the identity does not fade when a fetch fails.
                identity: true
                dialColor: theme.brand
                valueColor: theme.brand
            }

            Text {
                anchors.left: headerGauge.right
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: "Claudometer"
                color: theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                IconButton {
                    glyph: "↻"
                    color: usage.fetching ? theme.accent : theme.subtext
                    hoverColor: theme.hover
                    onClicked: usage.refresh()
                }

                IconButton {
                    glyph: "⚙"
                    color: theme.subtext
                    hoverColor: theme.hover
                    onClicked: usage.openSettings()
                }

                IconButton {
                    glyph: "✕"
                    color: theme.subtext
                    hoverColor: theme.hover
                    onClicked: usage.close()
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: theme.separator
        }

        // --- unavailable ------------------------------------------------------
        Text {
            width: parent.width
            visible: !usage.available
            text: usage.unavailableReason
            color: theme.subtext
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // --- windows ----------------------------------------------------------
        // No separator between the two rows: the gap carries the division, and
        // the card reads better with air than with another line.
        MeterRow {
            width: parent.width
            visible: usage.available && usage.fiveHourAvailable
            theme: theme
            label: qsTr("5-hour limit")
            percentage: usage.fiveHourPercent
            reset: usage.fiveHourReset
            level: usage.fiveHourLevel
        }

        MeterRow {
            width: parent.width
            visible: usage.available && usage.sevenDayAvailable
            theme: theme
            label: qsTr("7-day limit")
            percentage: usage.sevenDayPercent
            reset: usage.sevenDayReset
            level: usage.sevenDayLevel
        }

        // --- footer -----------------------------------------------------------
        Rectangle {
            width: parent.width
            height: 1
            color: theme.separator
        }

        Row {
            spacing: 6

            Text {
                text: qsTr("Stale")
                visible: usage.stale
                color: theme.warning
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Text {
                text: usage.updatedText
                color: theme.subtext
                font.pixelSize: 11
            }
        }
    }
}
