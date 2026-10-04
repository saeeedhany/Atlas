import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: control

    font.pixelSize: Theme.fontBody
    padding: 0
    implicitWidth: text === "" ? indicator.implicitWidth : contentItem.implicitWidth
    implicitHeight: Math.max(indicator.implicitHeight, contentItem.implicitHeight)
    opacity: enabled ? 1 : 0.4

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.text === "" ? control.leftPadding + (control.availableWidth - width) / 2
                               : control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.surfaceRaised
        border.width: 1
        border.color: control.checked ? Theme.accent : Theme.border

        Behavior on color { ColorAnimation { duration: Motion.fast } }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? Theme.background : Theme.textMuted

            Behavior on x {
                enabled: !Motion.reduced
                NumberAnimation { duration: Motion.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
            }
            Behavior on color { ColorAnimation { duration: Motion.fast } }
        }
    }

    contentItem: Text {
        leftPadding: control.text !== "" && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.text !== "" && control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
