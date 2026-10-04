import QtQuick

Rectangle {
    id: screen

    property bool shown: false

    color: Theme.background
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }
}
