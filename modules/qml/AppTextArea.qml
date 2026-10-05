import QtQuick
import QtQuick.Controls.Basic

TextArea {
    id: control

    color: Theme.onSurface
    placeholderTextColor: Theme.onSurfaceFaint
    selectionColor: Theme.primary
    selectedTextColor: Theme.onPrimary
    font.family: Theme.sans
    font.pixelSize: Theme.fontBody
    wrapMode: TextArea.Wrap
    padding: 10

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 64
        radius: Theme.radius
        color: Theme.surface
        border.color: control.activeFocus ? Theme.primary : Theme.outline

        Behavior on border.color { ColorAnimation { duration: Motion.fast } }
    }
}
