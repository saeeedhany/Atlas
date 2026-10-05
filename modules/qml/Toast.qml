import QtQuick

Rectangle {
    id: toast

    property string text
    property bool shown: false

    function show(message: string) {
        text = message
        shown = true
        hideTimer.restart()
    }

    implicitWidth: Math.min(label.implicitWidth + 32, 520)
    implicitHeight: label.implicitHeight + 20
    radius: Theme.radius
    color: Theme.surfaceRaised
    border.color: Theme.outline
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    Rectangle {
        width: 3
        height: parent.height - 2 * toast.radius
        anchors.verticalCenter: parent.verticalCenter
        x: 1
        radius: 1.5
        color: Theme.danger
    }

    Text {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, 488)
        text: toast.text
        color: Theme.text
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }

    Timer {
        id: hideTimer
        interval: 4000
        onTriggered: toast.shown = false
    }
}
