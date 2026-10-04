import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: editor

    property string title
    property string primaryLabel: "Title"
    property string secondaryLabel: "Details"
    property var items: []

    signal itemsEdited(var items)

    function copied() {
        return items.map(entry => ({ primary: entry.primary, secondary: entry.secondary }))
    }

    function replaced(index: int, key: string, value: string) {
        let list = copied()
        list[index][key] = value
        return list
    }

    function added() {
        return copied().concat([{ primary: "", secondary: "" }])
    }

    function removed(index: int) {
        let list = copied()
        list.splice(index, 1)
        return list
    }

    spacing: 6

    SectionLabel { text: editor.title }

    Repeater {
        model: editor.items.length

        delegate: RowLayout {
            id: row

            required property int index

            Layout.fillWidth: true
            spacing: 6

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                AppTextField {
                    Layout.fillWidth: true
                    text: editor.items[row.index] ? editor.items[row.index].primary : ""
                    placeholderText: editor.primaryLabel
                    onTextEdited: editor.itemsEdited(editor.replaced(row.index, "primary", text))
                }
                AppTextField {
                    Layout.fillWidth: true
                    text: editor.items[row.index] ? editor.items[row.index].secondary : ""
                    placeholderText: editor.secondaryLabel
                    onTextEdited: editor.itemsEdited(editor.replaced(row.index, "secondary", text))
                }
            }

            AppButton {
                text: "Remove"
                danger: true
                onClicked: editor.itemsEdited(editor.removed(row.index))
            }
        }
    }

    AppButton {
        text: "Add"
        onClicked: editor.itemsEdited(editor.added())
    }
}
