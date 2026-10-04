import QtQuick
import Atlas.ViewModels

Item {
    id: overlay

    property Item canvas
    property bool active: false
    property string focusId
    property bool fitPending: false

    function focusNewConcept() {
        topicBar.focusNewConcept()
    }

    function focusConcept(id: string) {
        MapView.selectedId = id
        if (MapView.selectedId !== id) {
            MapView.topicId = ""
            MapView.selectedId = id
        }
        focusId = id
        Qt.callLater(settle)
    }

    function settle() {
        if (!canvas)
            return
        if (focusId !== "") {
            if (canvas.screenPositionOf(focusId) === undefined)
                return
            canvas.centerOn(focusId)
            focusId = ""
            fitPending = false
        } else if (fitPending) {
            canvas.fitToContent()
            fitPending = false
        }
    }

    opacity: active ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    Connections {
        target: MapView
        function onTopicIdChanged() {
            overlay.fitPending = true
            Qt.callLater(overlay.settle)
        }
        function onSceneChanged() { Qt.callLater(overlay.settle) }
    }

    Connections {
        target: overlay.canvas
        function onNodeHovered(id) { hoverCard.showConcept(id) }
        function onLinkHovered(id) { hoverCard.showLink(id) }
        function onViewChanged() { hoverCard.place() }
    }

    Text {
        anchors.centerIn: parent
        visible: MapView.conceptCount === 0
        text: "Type a concept name above and press Enter."
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }

    HoverCard {
        id: hoverCard
        objectName: "hoverCard"
        canvas: overlay.canvas
    }

    TopicBar {
        id: topicBar
        objectName: "topicBar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.gap * 2
        onFocusRequested: id => overlay.focusConcept(id)
        onFitRequested: overlay.canvas.fitToContent()
    }

    Shortcut {
        sequence: "Escape"
        enabled: overlay.active
        onActivated: MapView.selectedId = ""
    }
}
