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
    property var obstacles: []
    readonly property bool cardExpanded: conceptCard.expanded
    onPinnedIdsChanged: scheduleLayout()
    onPinnedItemsChanged: scheduleLayout()
    onWidthChanged: scheduleLayout()
    onHeightChanged: scheduleLayout()
    property var pinnedItems: []

    signal notice(string message)

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

    function overlaps(a, b) {
        return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height
    }

    function placeClear(point, cardWidth, cardHeight, obstacles) {
        const gap = 8
        const edge = 16
        const base = besideRect(point, cardWidth, cardHeight, width, height)
        const otherX = base.x > point.x ? point.x - 28 - cardWidth : point.x + 28
        let columns = [base.x, Math.max(edge, Math.min(otherX, width - cardWidth - edge))]
        for (let columnX = edge; columnX + cardWidth <= width - edge; columnX += cardWidth + gap)
            columns.push(columnX)
        for (const columnX of columns) {
            let y = base.y
            for (let pass = 0; pass <= obstacles.length; ++pass) {
                const rect = Qt.rect(columnX, y, cardWidth, cardHeight)
                const hit = obstacles.find(obstacle => overlaps(rect, obstacle))
                if (!hit)
                    return rect
                y = hit.y + hit.height + gap
                if (y + cardHeight > height - edge)
                    break
            }
        }
        return base
    }

    function cardSpot(point, cardWidth, cardHeight) {
        return placeClear(point, cardWidth, cardHeight, obstacles)
    }

    function restoreCard() {
        conceptCard.restore()
    }

    function scheduleLayout() {
        Qt.callLater(layoutPinned)
    }

    function layoutPinned() {
        let taken = obstacles.slice()
        if (conceptCard.visible)
            taken.push(Qt.rect(conceptCard.x, conceptCard.y, conceptCard.width, conceptCard.height))
        for (const id of pinnedIds) {
            const item = pinnedItems.find(candidate => candidate.conceptId === id)
            if (!item)
                continue
            const point = canvas ? canvas.screenPositionOf(id) : undefined
            const rect = point !== undefined
                ? placeClear(point, item.width, item.height, taken)
                : Qt.rect(16, 16, item.width, item.height)
            item.x = rect.x
            item.y = rect.y
            taken.push(rect)
        }
    }

    function pinCurrent(): bool {
        if (!Concept.exists || pinnedIds.includes(Concept.conceptId))
            return false
        if (pinnedIds.length >= 3) {
            notice("Up to three concepts can stay pinned")
            return false
        }
        pinnedIds = pinnedIds.concat([Concept.conceptId])
        return true
    }

    function togglePin() {
        if (pinnedIds.includes(Concept.conceptId))
            unpin(Concept.conceptId)
        else
            pinCurrent()
    }

    function unpin(id: string) {
        pinnedIds = pinnedIds.filter(pinnedId => pinnedId !== id)
    }

    function dropMissingPins() {
        let kept = pinnedIds.filter(id => Object.keys(MapView.conceptInfo(id)).length > 0)
        if (kept.length !== pinnedIds.length)
            pinnedIds = kept
    }

    onObstaclesChanged: scheduleLayout()
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
            overlay.scheduleLayout()
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
        function onViewChanged() {
            hoverCard.place()
            overlay.scheduleLayout()
        }
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

    Rectangle {
        anchors.fill: parent
        z: 25
        color: Theme.background
        opacity: conceptCard.expanded ? 0.72 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: Motion.appear } }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: conceptCard.restore()
            onWheel: wheel => wheel.accepted = true
        }
    }

    ConceptCard {
        id: conceptCard
        objectName: "conceptCard"
        canvas: overlay.canvas
        area: overlay
        pinned: overlay.pinnedIds.includes(Concept.conceptId)
        onFocusRequested: id => overlay.focusConcept(id)
        onPinRequested: overlay.togglePin()
    }

    Connections {
        target: Concept
        function onLoaded() { overlay.scheduleLayout() }
    }

    Connections {
        target: conceptCard
        function onXChanged() { overlay.scheduleLayout() }
        function onYChanged() { overlay.scheduleLayout() }
        function onWidthChanged() { overlay.scheduleLayout() }
        function onHeightChanged() { overlay.scheduleLayout() }
        function onVisibleChanged() { overlay.scheduleLayout() }
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
        onObjectAdded: (index, object) => overlay.pinnedItems = overlay.pinnedItems.concat([object])
        onObjectRemoved: (index, object) => overlay.pinnedItems = overlay.pinnedItems.filter(item => item !== object)
    }
}
