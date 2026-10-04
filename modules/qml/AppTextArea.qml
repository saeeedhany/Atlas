import QtQuick
import QtQuick.Controls.Basic

TextArea {
    id: control

    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: Theme.background
    font.pixelSize: Theme.fontBody
    wrapMode: TextArea.Wrap
    padding: 10

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 64
        radius: Theme.radius - 2
        color: Theme.surface
        border.color: control.activeFocus ? Theme.accent : Theme.border

        Behavior on border.color { ColorAnimation { duration: Motion.fast } }
    }
}
