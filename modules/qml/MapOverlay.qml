import QtQuick
import Atlas.ViewModels

Item {
    id: overlay

    property Item canvas
    property bool active: false
    property string focusId
    property bool fitPending: false
    property bool focusWaited: false

    function focusNewConcept() {
        topicBar.focusNewConcept()
    }

    function focusConcept(id: string) {
        if (Object.keys(MapView.conceptInfo(id)).length === 0)
            return
        MapView.selectedId = id
        if (Concept.dirty && Concept.conceptId !== id)
            return
        if (MapView.selectedId !== id) {
            MapView.topicId = ""
            MapView.selectedId = id
        }
        focusId = id
        focusWaited = false
        Qt.callLater(settle)
    }

    function openConcept(id: string) {
        if (id === Concept.conceptId)
            return
        if (Concept.dirty && !Concept.save()) {
            MapView.selectedId = Concept.conceptId
            return
        }
        Concept.conceptId = id
        ConceptLinks.conceptId = id
    }

    function settle() {
        if (!canvas)
            return
        if (focusId !== "" && canvas.screenPositionOf(focusId) === undefined) {
            if (!focusWaited) {
                focusWaited = true
                return
            }
            focusId = ""
        }
        if (focusId !== "") {
            canvas.centerOn(focusId)
            focusId = ""
            fitPending = false
        } else if (fitPending) {
            if (Session.stage === "idle")
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
        function onSelectedIdChanged() {
            if (Session.stage === "idle")
                overlay.openConcept(MapView.selectedId)
        }
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
        onFitRequested: if (overlay.canvas) overlay.canvas.fitToContent()
    }

    ConceptPanel {
        id: panel
        objectName: "conceptPanel"
        anchors.top: topicBar.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: Theme.gap
        anchors.bottomMargin: Theme.gap * 2
        anchors.rightMargin: panel.shown || Motion.reduced ? Theme.gap * 2 : Theme.gap * 2 - 24
        onFocusRequested: id => overlay.focusConcept(id)

        Behavior on anchors.rightMargin {
            enabled: !Motion.reduced
            NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: overlay.active
        onActivated: MapView.selectedId = ""
    }
}
