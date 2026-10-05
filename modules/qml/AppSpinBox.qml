import QtQuick
import QtQuick.Controls.Basic

SpinBox {
    id: control

    font.family: Theme.sans
    font.pixelSize: Theme.fontBody
    opacity: enabled ? 1 : 0.4

    contentItem: Text {
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: Theme.onSurface
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        implicitWidth: 34
        implicitHeight: 34
        radius: Theme.radius
        color: control.up.pressed || control.up.hovered ? Theme.surfaceHigh : "transparent"
        opacity: control.value < control.to ? 1 : 0.4

        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.onSurfaceMuted
            font.family: Theme.sans
            font.pixelSize: Theme.fontBody
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? control.width - width : 0
        height: control.height
        implicitWidth: 34
        implicitHeight: 34
        radius: Theme.radius
        color: control.down.pressed || control.down.hovered ? Theme.surfaceHigh : "transparent"
        opacity: control.value > control.from ? 1 : 0.4

        Text {
            anchors.centerIn: parent
            text: "−"
            color: Theme.onSurfaceMuted
            font.family: Theme.sans
            font.pixelSize: Theme.fontBody
        }
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 34
        radius: Theme.radius
        color: Theme.surface
        border.color: control.activeFocus ? Theme.primary : Theme.outline
    }
}
