import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property bool primary: false
    property bool danger: false

    font.family: Theme.sans
    font.pixelSize: Theme.fontBody
    font.weight: Font.Medium
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    opacity: enabled ? 1 : 0.4

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primary ? Theme.onPrimary : control.danger ? Theme.danger : Theme.onSurface
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: 34
        radius: Theme.radius
        color: control.primary ? Theme.primary : control.hovered ? Theme.surfaceHigh : Theme.surface
        border.color: control.primary ? Theme.primary : Theme.outline
        scale: control.down ? 0.97 : 1

        Behavior on color { ColorAnimation { duration: Motion.fast } }
        Behavior on scale {
            enabled: !Motion.reduced
            NumberAnimation { duration: Motion.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
        }
    }
}
