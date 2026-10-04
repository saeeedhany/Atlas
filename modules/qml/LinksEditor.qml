import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

ColumnLayout {
    id: editor

    property string targetId
    property string targetTitle
    property var candidates: []

    signal focusRequested(string id)

    function searchCandidates(query: string) {
        candidates = query.trim() === "" ? [] : ConceptLinks.candidates(query)
    }

    function chooseTarget(id: string, title: string) {
        targetId = id
        targetTitle = title
        candidates = []
        findField.clear()
    }

    function addLink(typeIndex: int, outgoing: bool, note: string): bool {
        if (targetId === "")
            return false
        if (!ConceptLinks.add(targetId, typeIndex, outgoing, note.trim()))
            return false
        cancel()
        noteField.clear()
        return true
    }

    function cancel() {
        targetId = ""
        targetTitle = ""
        candidates = []
    }

    spacing: 8

    SectionLabel { text: "Links" }

    Text {
        Layout.fillWidth: true
        visible: ConceptLinks.count === 0
        text: "No links yet. Tying a new idea to one you know helps it stick."
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }

    Repeater {
        model: ConceptLinks

        delegate: RowLayout {
            id: row

            required property string linkId
            required property string otherId
            required property string otherTitle
            required property string typeName
            required property bool outgoing
            required property bool symmetric
            required property string note
            required property real recall

            Layout.fillWidth: true
            spacing: 10

            Ring {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                thickness: 2.5
                recall: row.recall
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: row.otherTitle
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: editor.focusRequested(row.otherId) }
                }
                Text {
                    Layout.fillWidth: true
                    text: (row.symmetric ? row.typeName : row.outgoing ? "this " + row.typeName + " it" : "it " + row.typeName + " this")
                          + (row.note !== "" ? "  ·  " + row.note : "")
                    color: Theme.textMuted
                    font.family: Theme.mono
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            AppButton {
                text: "Remove"
                danger: true
                onClicked: ConceptLinks.remove(row.linkId)
            }
        }
    }

    AppTextField {
        id: findField
        Layout.fillWidth: true
        visible: editor.targetId === ""
        placeholderText: "Link to..."
        onTextEdited: editor.searchCandidates(text)
    }

    Repeater {
        model: editor.candidates

        delegate: ItemDelegate {
            required property var modelData

            Layout.fillWidth: true
            text: modelData.title + (modelData.topic ? "  ·  " + modelData.topic : "")
            font.pixelSize: Theme.fontBody
            onClicked: editor.chooseTarget(modelData.id, modelData.title)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: editor.targetId !== ""
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: "Link to " + editor.targetTitle
            color: Theme.text
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }
        ComboBox {
            id: typeBox
            Layout.fillWidth: true
            font.pixelSize: Theme.fontBody
            model: ConceptLinks.typeNames
        }
        Switch {
            id: directionSwitch
            checked: true
            text: checked ? "This concept points to it" : "It points to this concept"
            font.pixelSize: Theme.fontBody
        }
        AppTextField {
            id: noteField
            Layout.fillWidth: true
            placeholderText: "Why are they linked? (optional)"
        }
        RowLayout {
            spacing: 8

            Item { Layout.fillWidth: true }
            AppButton {
                text: "Cancel"
                onClicked: editor.cancel()
            }
            AppButton {
                text: "Add link"
                primary: true
                onClicked: editor.addLink(typeBox.currentIndex, directionSwitch.checked, noteField.text)
            }
        }
    }
}
