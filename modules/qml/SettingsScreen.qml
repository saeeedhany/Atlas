import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Item {
    id: screen

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 18

        SectionLabel { text: "Appearance" }

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text { Layout.fillWidth: true; text: "Dark theme"; color: Theme.text; font.pixelSize: Theme.fontBody }
                Text { Layout.fillWidth: true; text: "Warm stone, or its light paper twin."; color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
            }
            AppSwitch {
                objectName: "darkThemeSwitch"
                checked: AppSettings.darkTheme
                onToggled: AppSettings.darkTheme = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text { Layout.fillWidth: true; text: "Reduce motion"; color: Theme.text; font.pixelSize: Theme.fontBody }
                Text { Layout.fillWidth: true; text: "Movement becomes short fades."; color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
            }
            AppSwitch {
                objectName: "reducedMotionSwitch"
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
            AppSpinBox {
                objectName: "newPerDayBox"
                from: 1
                to: 20
                value: AppSettings.newPerDay
                onValueModified: AppSettings.newPerDay = value
            }
        }
    }
}
