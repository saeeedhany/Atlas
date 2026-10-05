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

    function focusNewConcept() {
        topics.focusNewConcept()
    }

    implicitHeight: 52
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
        anchors.rightMargin: 16
        spacing: 12

        Text {
            id: wordmark
            text: "Atlas"
            color: Theme.onSurface
            font.family: Theme.serif
            font.pixelSize: 22

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: menu.popup(wordmark, 0, wordmark.height + 8) }
        }

        TopicBar {
            id: topics
            objectName: "topicBar"
            Layout.fillWidth: true
            embedded: true
            onFocusRequested: id => bar.focusRequested(id)
            onFitRequested: bar.fitRequested()
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

        MenuItem { text: "Settings"; onTriggered: bar.settingsRequested() }
        MenuItem { text: "Fit the board"; onTriggered: bar.fitRequested() }
        MenuItem { text: "Tidy layout"; onTriggered: MapView.tidy() }
    }
}
