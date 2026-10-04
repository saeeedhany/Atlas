import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: bar

    property var results: []
    readonly property bool canEditTopic: {
        let entry = Topics.entries.find(topic => topic.topicId === MapView.topicId)
        return entry !== undefined && !entry.uncategorized
    }

    signal focusRequested(string id)
    signal fitRequested()

    function syncTopic() {
        topicBox.currentIndex = Math.max(0, topicBox.indexOfValue(MapView.topicId))
    }

    function addConcept(title: string) {
        let id = MapView.createConcept(title.trim())
        if (id === "")
            return
        newField.clear()
        focusRequested(id)
    }

    function searchFor(query: string) {
        results = query.trim() === "" ? [] : MapView.search(query)
        if (results.length > 0)
            searchPopup.open()
        else
            searchPopup.close()
    }

    function pick(id: string) {
        searchPopup.close()
        searchField.clear()
        results = []
        focusRequested(id)
    }

    function createTopic(name: string) {
        let id = Topics.createTopic(name.trim())
        if (id === "")
            return
        topicPopup.close()
        MapView.topicId = id
    }

    function renameTopic(name: string) {
        if (Topics.rename(MapView.topicId, name.trim()))
            topicPopup.close()
    }

    function submitTopicName(name: string) {
        if (canEditTopic)
            renameTopic(name)
        else
            createTopic(name)
    }

    function deleteTopic() {
        if (!Topics.remove(MapView.topicId))
            return
        topicPopup.close()
        MapView.topicId = ""
    }

    function focusNewConcept() {
        newField.forceActiveFocus()
    }

    implicitHeight: row.implicitHeight + 16
    radius: Theme.radius + 2
    color: Theme.surface
    border.color: Theme.border

    Connections {
        target: MapView
        function onTopicIdChanged() { bar.syncTopic() }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        ComboBox {
            id: topicBox
            objectName: "topicBox"
            Layout.preferredWidth: 180
            font.pixelSize: Theme.fontBody
            model: [{ topicId: "", name: "All topics" }].concat(Topics.entries)
            textRole: "name"
            valueRole: "topicId"
            onActivated: MapView.topicId = currentValue
            onModelChanged: Qt.callLater(bar.syncTopic)
            Component.onCompleted: bar.syncTopic()
        }

        AppButton {
            id: topicButton
            text: "Topic"
            onClicked: topicPopup.open()
        }

        AppTextField {
            id: searchField
            objectName: "searchField"
            Layout.fillWidth: true
            Layout.minimumWidth: 120
            Layout.maximumWidth: 360
            placeholderText: "Search"
            onTextEdited: bar.searchFor(text)
            onAccepted: {
                if (bar.results.length > 0)
                    bar.pick(bar.results[0].id)
            }
        }

        AppTextField {
            id: newField
            objectName: "newConceptField"
            Layout.fillWidth: true
            Layout.minimumWidth: 120
            Layout.maximumWidth: 360
            placeholderText: "New concept"
            onAccepted: bar.addConcept(text)
        }

        Item { Layout.fillWidth: true }

        AppButton {
            text: "Fit"
            onClicked: bar.fitRequested()
        }

        AppButton {
            text: "Tidy"
            onClicked: MapView.tidy()
        }
    }

    Popup {
        id: topicPopup
        parent: topicButton
        y: topicButton.height + 6
        width: 320
        padding: 16
        onOpened: topicName.text = bar.canEditTopic ? Topics.nameOf(MapView.topicId) : ""

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.appear } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fade } }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceRaised
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            spacing: 10

            SectionLabel { text: "Topic" }

            AppTextField {
                id: topicName
                Layout.fillWidth: true
                placeholderText: "Topic name"
                onAccepted: bar.submitTopicName(text)
            }

            RowLayout {
                spacing: 8

                AppButton {
                    text: "Create"
                    primary: true
                    onClicked: bar.createTopic(topicName.text)
                }
                AppButton {
                    text: "Rename"
                    enabled: bar.canEditTopic
                    onClicked: bar.renameTopic(topicName.text)
                }
                AppButton {
                    text: "Delete"
                    danger: true
                    enabled: bar.canEditTopic
                    onClicked: bar.deleteTopic()
                }
            }
        }
    }

    Popup {
        id: searchPopup
        parent: searchField
        y: searchField.height + 6
        width: Math.max(searchField.width, 260)
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceRaised
            border.color: Theme.border
        }

        contentItem: ListView {
            implicitHeight: Math.min(contentHeight, 280)
            clip: true
            model: bar.results

            delegate: ItemDelegate {
                required property var modelData

                width: ListView.view.width
                text: modelData.title
                font.pixelSize: Theme.fontBody
                onClicked: bar.pick(modelData.id)
            }
        }
    }
}
