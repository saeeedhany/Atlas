import QtQuick

Item {
    id: overlay

    property Item canvas
    property bool active: false

    function focusNewConcept() {
    }

    opacity: active ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }
}
