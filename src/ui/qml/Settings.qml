import QtQuick

// Three small groups, drawn with the same tokens as the popup.
//
// Nothing here reads QPalette. That is the point: the platform theme may keep a
// dark palette while the user has asked Claudometer for Light, and a form that
// takes its background from one source and its label colours from the other ends
// up with invisible text. One source of truth, everywhere.
Rectangle {
    id: root

    readonly property int contentWidth: 380

    implicitWidth: contentWidth
    implicitHeight: layout.implicitHeight + 36

    color: theme.background

    Theme { id: theme }

    Column {
        id: layout
        x: 18
        y: 18
        width: root.contentWidth - 36
        spacing: 6

        SectionHeader {
            theme: theme
            text: qsTr("General")
            bottomPadding: 4
        }

        Toggle {
            theme: theme
            text: qsTr("Start on login")
            checked: settings.startOnLogin
            onToggled: settings.startOnLogin = checked
        }

        SettingRow {
            width: parent.width
            theme: theme
            label: qsTr("Tray style")

            // Two variants of the same mark: the arc is identical, and only its
            // middle differs. The popup header keeps the needle either way, as
            // the constant logotype.
            Segmented {
                theme: theme
                options: [qsTr("Gauge"), qsTr("Percentage")]
                currentIndex: settings.trayStyleIndex
                onActivated: function(index) { settings.trayStyleIndex = index }
            }
        }

        SectionHeader {
            theme: theme
            text: qsTr("Notifications")
            topPadding: 16
            bottomPadding: 4
        }

        Toggle {
            theme: theme
            text: qsTr("Desktop notifications")
            checked: settings.notificationsEnabled
            onToggled: settings.notificationsEnabled = checked
        }

        SettingRow {
            width: parent.width
            theme: theme
            label: qsTr("Warning")

            Stepper {
                theme: theme
                suffix: "%"
                from: 1
                to: 100
                value: settings.warningThreshold
                onValueModified: settings.warningThreshold = value
            }
        }

        SettingRow {
            width: parent.width
            theme: theme
            label: qsTr("Critical")

            Stepper {
                theme: theme
                suffix: "%"
                from: 1
                to: 100
                value: settings.criticalThreshold
                onValueModified: settings.criticalThreshold = value
            }
        }

        SectionHeader {
            theme: theme
            text: qsTr("Behavior")
            topPadding: 16
            bottomPadding: 4
        }

        SettingRow {
            width: parent.width
            theme: theme
            label: qsTr("Refresh every")

            Stepper {
                theme: theme
                suffix: qsTr(" min")
                from: 1
                to: 60
                value: settings.refreshMinutes
                onValueModified: settings.refreshMinutes = value
            }
        }

        SettingRow {
            width: parent.width
            theme: theme
            label: qsTr("Theme")

            Segmented {
                theme: theme
                options: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                currentIndex: settings.themeIndex
                onActivated: function(index) { settings.themeIndex = index }
            }
        }
    }
}
