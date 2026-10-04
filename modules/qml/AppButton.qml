import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property bool primary: false
    property bool danger: false

    font.pixelSize: Theme.fontBody
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    opacity: enabled ? 1 : 0.4

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primary ? Theme.background : control.danger ? Theme.danger : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: 34
        radius: Theme.radius - 2
        color: control.primary ? Theme.accent : control.hovered ? Theme.surfaceRaised : Theme.surface
        border.color: control.primary ? Theme.accent : control.danger ? Theme.danger : Theme.border
        scale: control.down ? 0.97 : 1

        Behavior on color { ColorAnimation { duration: Motion.fast } }
        Behavior on scale {
            enabled: !Motion.reduced
            NumberAnimation { duration: Motion.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
        }
    }
}
