import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    color: Theme.onSurface
    placeholderTextColor: Theme.onSurfaceFaint
    selectionColor: Theme.primary
    selectedTextColor: Theme.onPrimary
    font.family: Theme.sans
    font.pixelSize: Theme.fontBody
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 34
        radius: Theme.radius
        color: Theme.surface
        border.color: control.activeFocus ? Theme.primary : Theme.outline

        Behavior on border.color { ColorAnimation { duration: Motion.fast } }
    }
}
