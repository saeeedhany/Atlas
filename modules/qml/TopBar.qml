import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: bar

    signal recallRequested()
    signal settingsRequested()
    signal focusRequested(string id)
    signal fitRequested()
    signal noteRequested(string body)

    function focusNewConcept() {
        topics.focusNewConcept()
    }

    function openMenu() {
        menu.popup(bar, 12, bar.height + 4)
    }

    implicitHeight: 44
    color: Theme.background
    opacity: enabled ? 1 : 0.55

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.outline
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 12
        spacing: 12

        Row {
            spacing: 2

            Text {
                id: wordmark
                anchors.verticalCenter: parent.verticalCenter
                text: "Atlas"
                color: Theme.onSurface
                font.family: Theme.serif
                font.weight: Font.Medium
                font.pixelSize: Theme.fontHeadline

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: bar.openMenu() }
            }
            IconButton {
                objectName: "menuButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                tip: "Menu"
                onClicked: bar.openMenu()

                Chevron { anchors.centerIn: parent }
            }
        }

        TopicBar {
            id: topics
            objectName: "topicBar"
            Layout.fillWidth: true
            embedded: true
            onFocusRequested: id => bar.focusRequested(id)
            onFitRequested: bar.fitRequested()
            onNoteRequested: body => bar.noteRequested(body)
        }

        AppButton {
            objectName: "recallButton"
            primary: true
            enabled: Today.itemCount > 0
            text: Today.itemCount > 0 ? "Recall " + Today.itemCount : "Recall"
            onClicked: bar.recallRequested()
        }
    }

    Menu {
        id: menu
        objectName: "topMenu"

        MenuItem { text: "Settings"; onTriggered: bar.settingsRequested() }
        MenuItem { text: "Fit the board"; onTriggered: bar.fitRequested() }
        MenuItem { text: "Tidy layout"; onTriggered: MapView.tidy() }
    }
}
