import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: screen

    property bool shown: false

    color: Theme.background
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 64
        width: Math.min(560, screen.width - 64)
        spacing: 18

        Text {
            text: "Settings"
            color: Theme.text
            font.pixelSize: Theme.fontHero
        }

        SectionLabel { text: "Appearance" }

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text { text: "Dark theme"; color: Theme.text; font.pixelSize: Theme.fontBody }
                Text { text: "Graphite dark, or its light twin."; color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
            }
            AppSwitch {
                objectName: "darkThemeSwitch"
                Layout.alignment: Qt.AlignRight
                checked: AppSettings.darkTheme
                onToggled: AppSettings.darkTheme = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text { text: "Reduce motion"; color: Theme.text; font.pixelSize: Theme.fontBody }
                Text { text: "Movement becomes short fades."; color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
            }
            AppSwitch {
                objectName: "reducedMotionSwitch"
                Layout.alignment: Qt.AlignRight
                checked: AppSettings.reducedMotion
                onToggled: AppSettings.reducedMotion = checked
            }
        }

        SectionLabel { text: "Learning" }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text { text: "New concepts per day"; color: Theme.text; font.pixelSize: Theme.fontBody }
                Text {
                    Layout.fillWidth: true
                    text: "Spreading new material across days beats cramming it into one sitting (Cepeda et al., 2006)."
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                }
            }
            SpinBox {
                objectName: "newPerDayBox"
                Layout.alignment: Qt.AlignRight
                from: 1
                to: 20
                value: AppSettings.newPerDay
                onValueModified: AppSettings.newPerDay = value
            }
        }
    }
}
