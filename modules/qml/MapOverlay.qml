import QtQuick
import Atlas.ViewModels

Item {
    id: overlay

    property Item canvas
    property bool active: false
    property string focusId
    property bool fitPending: false
    property bool focusWaited: false
    property var pinnedIds: []

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

    function besideRect(point, cardWidth, cardHeight, areaWidth, areaHeight) {
        const gapToNode = 28
        const edge = 16
        let x = point.x + gapToNode
        if (x + cardWidth > areaWidth - edge)
            x = point.x - gapToNode - cardWidth
        x = Math.max(edge, Math.min(x, areaWidth - cardWidth - edge))
        let y = Math.max(edge, Math.min(point.y - 80, areaHeight - cardHeight - edge))
        return Qt.rect(x, y, cardWidth, cardHeight)
    }

    function pinCurrent(): bool {
        if (!Concept.exists || pinnedIds.includes(Concept.conceptId) || pinnedIds.length >= 3)
            return false
        pinnedIds = pinnedIds.concat([Concept.conceptId])
        return true
    }

    function unpin(id: string) {
        pinnedIds = pinnedIds.filter(pinnedId => pinnedId !== id)
    }

    function dropMissingPins() {
        let kept = pinnedIds.filter(id => Object.keys(MapView.conceptInfo(id)).length > 0)
        if (kept.length !== pinnedIds.length)
            pinnedIds = kept
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
        function onSceneChanged() {
            overlay.dropMissingPins()
            Qt.callLater(overlay.settle)
        }
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
        text: "Type a concept name in the bar above and press Enter."
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }

    HoverCard {
        id: hoverCard
        objectName: "hoverCard"
        canvas: overlay.canvas
    }

    ConceptCard {
        id: conceptCard
        objectName: "conceptCard"
        canvas: overlay.canvas
        area: overlay
        onFocusRequested: id => overlay.focusConcept(id)
        onPinRequested: overlay.pinCurrent()
    }

    Instantiator {
        model: overlay.pinnedIds.filter(id => id !== Concept.conceptId)

        delegate: PinnedCard {
            required property string modelData
            parent: overlay
            conceptId: modelData
            canvas: overlay.canvas
            area: overlay
            onUnpinRequested: id => overlay.unpin(id)
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: overlay.active
        onActivated: MapView.selectedId = ""
    }
}
