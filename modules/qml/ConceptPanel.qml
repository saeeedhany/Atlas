import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: panel

    readonly property bool shown: Concept.exists

    signal focusRequested(string id)

    function close() {
        MapView.selectedId = ""
    }

    function confirmDelete() {
        if (!Concept.remove())
            return
        deletePopup.close()
        MapView.selectedId = ""
    }

    function syncTopic() {
        topicBox.currentIndex = Math.max(0, topicBox.indexOfValue(Concept.topicId))
    }

    width: Theme.panelWidth
    radius: Theme.radius + 4
    color: Theme.surface
    border.color: Theme.border
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    Connections {
        target: Concept
        function onLoaded() { panel.syncTopic() }
        function onEdited() { panel.syncTopic() }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    ScrollView {
        id: scroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.bottomMargin: 8
        padding: 20
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Ring {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    recall: Concept.recall
                    thickness: 4
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    SectionLabel {
                        text: Concept.recall >= 0 ? "Recall " + Math.round(Concept.recall * 100) + "%" : "Not learned yet"
                    }
                    AppTextField {
                        objectName: "titleField"
                        Layout.fillWidth: true
                        text: Concept.title
                        placeholderText: "Title"
                        font.pixelSize: Theme.fontTitle
                        onTextEdited: Concept.title = text
                    }
                }

                AppButton {
                    text: "Close"
                    onClicked: panel.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    SectionLabel { text: "Topic" }
                    ComboBox {
                        id: topicBox
                        Layout.fillWidth: true
                        font.pixelSize: Theme.fontBody
                        model: Topics.entries
                        textRole: "name"
                        valueRole: "topicId"
                        onActivated: Concept.topicId = currentValue
                        onModelChanged: Qt.callLater(panel.syncTopic)
                        Component.onCompleted: panel.syncTopic()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    SectionLabel { text: "Difficulty" }
                    ComboBox {
                        Layout.fillWidth: true
                        font.pixelSize: Theme.fontBody
                        model: Concept.difficultyNames
                        currentIndex: Concept.difficulty
                        onActivated: index => Concept.difficulty = index
                    }
                }
            }

            SectionLabel { text: "Definition" }
            AppTextArea {
                objectName: "definitionArea"
                Layout.fillWidth: true
                text: Concept.definition
                placeholderText: "What is it, in your own words?"
                onTextChanged: if (activeFocus) Concept.definition = text
            }

            SectionLabel { text: "Problem it solves" }
            AppTextArea {
                Layout.fillWidth: true
                text: Concept.problemSolved
                placeholderText: "What goes wrong without it?"
                onTextChanged: if (activeFocus) Concept.problemSolved = text
            }

            SectionLabel { text: "Why it exists" }
            AppTextArea {
                Layout.fillWidth: true
                text: Concept.whyItExists
                placeholderText: "Why was it invented?"
                onTextChanged: if (activeFocus) Concept.whyItExists = text
            }

            SectionLabel { text: "Notes" }
            AppTextArea {
                Layout.fillWidth: true
                text: Concept.notes
                onTextChanged: if (activeFocus) Concept.notes = text
            }

            ListEditor {
                Layout.fillWidth: true
                title: "Examples"
                primaryLabel: "Example"
                secondaryLabel: "Snippet"
                items: Concept.examples
                onItemsEdited: list => Concept.examples = list
            }

            ListEditor {
                Layout.fillWidth: true
                title: "Mini projects"
                primaryLabel: "Project"
                secondaryLabel: "Description"
                items: Concept.miniProjects
                onItemsEdited: list => Concept.miniProjects = list
            }

            ListEditor {
                Layout.fillWidth: true
                title: "References"
                primaryLabel: "Title"
                secondaryLabel: "Link"
                items: Concept.references
                onItemsEdited: list => Concept.references = list
            }

            LinksEditor {
                Layout.fillWidth: true
                onFocusRequested: id => panel.focusRequested(id)
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: "Keep this position on the map"
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                Switch {
                    checked: Concept.pinned
                    onToggled: Concept.pinned = checked
                }
            }
        }
    }

    RowLayout {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        spacing: 8

        AppButton {
            text: "Delete"
            danger: true
            onClicked: deletePopup.open()
        }
        Item { Layout.fillWidth: true }
        AppButton {
            text: "Revert"
            visible: Concept.dirty
            onClicked: Concept.revert()
        }
        AppButton {
            text: "Save"
            primary: true
            enabled: Concept.dirty
            onClicked: Concept.save()
        }
    }

    Popup {
        id: deletePopup
        objectName: "deletePopup"
        anchors.centerIn: parent
        width: panel.width - 48
        modal: true
        padding: 20

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.appear } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fade } }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceRaised
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: "Delete \"" + Concept.title + "\"? This cannot be undone."
                color: Theme.text
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton {
                    text: "Cancel"
                    onClicked: deletePopup.close()
                }
                AppButton {
                    text: "Delete"
                    danger: true
                    onClicked: panel.confirmDelete()
                }
            }
        }
    }
}
