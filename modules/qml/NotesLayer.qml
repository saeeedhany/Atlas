import QtQuick
import QtQml.Models
import Atlas.ViewModels

Item {
    id: layerRoot

    property Item canvas
    property bool interactive: true
    property int viewTick: 0
    property string draftNoteId
    property point draftFrom
    property point draftTo
    property string pendingFocusId
    property bool linesShown: false
    property var noteItems: []
    property var regions: []
    property var fills: toneMap(true)
    property var inks: toneMap(false)

    function toneMap(fill: bool): var {
        let map = {}
        for (const name of ["clay", "olive", "rose"])
            map[name] = fill ? Theme.noteFill(name) : Theme.noteText(name)
        return map
    }

    function screenRectOf(worldX: real, worldY: real, worldWidth: real, worldHeight: real): rect {
        if (!canvas)
            return Qt.rect(0, 0, 0, 0)
        let topLeft = canvas.mapToScreen(worldX, worldY)
        return Qt.rect(topLeft.x, topLeft.y, worldWidth * canvas.zoom, worldHeight * canvas.zoom)
    }

    function focusNote(noteId: string) {
        const item = noteItems.find(candidate => candidate.noteId === noteId)
        if (item) {
            pendingFocusId = ""
            item.focusBody()
        } else {
            pendingFocusId = noteId
        }
    }

    function createAtWorld(wx: real, wy: real, body: string): string {
        const id = Notes.createNote(wx, wy, body)
        if (id !== "")
            focusNote(id)
        return id
    }

    function createAtScreen(sx: real, sy: real, body: string): string {
        let world = canvas.mapToWorld(sx, sy)
        return createAtWorld(world.x, world.y, body)
    }

    function moveNoteTo(noteId: string, sx: real, sy: real): bool {
        let world = canvas.mapToWorld(sx, sy)
        return Notes.move(noteId, world.x, world.y)
    }

    function resizeNoteTo(noteId: string, screenWidth: real, screenHeight: real): bool {
        return Notes.resize(noteId, screenWidth / canvas.zoom, screenHeight / canvas.zoom)
    }

    function flushNotes() {
        for (const item of noteItems)
            item.saveBody()
    }

    function linkNoteAt(noteId: string, sx: real, sy: real): bool {
        let node = canvas.nodeAt(sx, sy)
        if (node !== "")
            return Notes.link(noteId, "concept", node)
        let world = canvas.mapToWorld(sx, sy)
        for (const region of regions) {
            if (world.x >= region.x && world.x <= region.x + region.width
                    && world.y >= region.y && world.y <= region.y + region.height)
                return Notes.link(noteId, "topic", region.topicId)
        }
        return false
    }

    function targetPoint(link) {
        if (link.kind === "concept")
            return canvas.screenPositionOf(link.targetId)
        let region = regions.find(r => r.topicId === link.targetId)
        return region ? canvas.mapToScreen(region.x + region.width / 2, region.y + region.height / 2) : undefined
    }

    function segments(): var {
        let list = []
        if (!interactive || !canvas)
            return list
        for (const item of noteItems) {
            for (const link of item.links) {
                let to = targetPoint(link)
                if (to !== undefined)
                    list.push({ color: inks[item.colorName], from: Qt.point(item.x + item.width, item.y + item.height / 2), to: to })
            }
        }
        return list
    }

    function hasLinks(): bool {
        return interactive && noteItems.some(item => item.links.length > 0)
    }

    function repaintLinks() {
        if (hasLinks() || draftNoteId !== "" || linesShown)
            lines.requestPaint()
    }

    function beginDraft(noteId: string, at: point) {
        draftNoteId = noteId
        draftFrom = at
        draftTo = at
        lines.requestPaint()
    }

    function moveDraft(at: point) {
        draftTo = at
        lines.requestPaint()
    }

    function endDraft(at: point) {
        if (draftNoteId !== "")
            linkNoteAt(draftNoteId, at.x, at.y)
        cancelDraft()
    }

    function cancelDraft() {
        draftNoteId = ""
        lines.requestPaint()
    }

    opacity: interactive ? 1 : 0.25
    onInteractiveChanged: lines.requestPaint()
    Component.onCompleted: regions = MapView.regions

    Behavior on opacity { NumberAnimation { duration: Motion.fade } }

    Connections {
        target: layerRoot.canvas
        function onViewChanged() { layerRoot.viewTick++; layerRoot.repaintLinks() }
        function onZoomChanged() { layerRoot.viewTick++; layerRoot.repaintLinks() }
    }
    Connections {
        target: MapView
        function onSceneChanged() {
            layerRoot.regions = MapView.regions
            layerRoot.repaintLinks()
        }
    }
    Connections {
        target: Notes
        function onDataChanged(topLeft, bottomRight, roles) { layerRoot.repaintLinks() }
        function onCountChanged() { layerRoot.repaintLinks() }
    }
    Connections {
        target: Palette
        function onChanged() {
            layerRoot.fills = layerRoot.toneMap(true)
            layerRoot.inks = layerRoot.toneMap(false)
            layerRoot.repaintLinks()
        }
    }

    Canvas {
        id: lines
        objectName: "noteLinks"
        anchors.fill: parent

        onPaint: {
            let ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = 1.5
            ctx.setLineDash([4, 4])
            const drawn = layerRoot.segments()
            for (const segment of drawn) {
                ctx.strokeStyle = segment.color
                ctx.beginPath()
                ctx.moveTo(segment.from.x, segment.from.y)
                ctx.lineTo(segment.to.x, segment.to.y)
                ctx.stroke()
            }
            if (layerRoot.draftNoteId !== "") {
                ctx.strokeStyle = Theme.primary
                ctx.beginPath()
                ctx.moveTo(layerRoot.draftFrom.x, layerRoot.draftFrom.y)
                ctx.lineTo(layerRoot.draftTo.x, layerRoot.draftTo.y)
                ctx.stroke()
            }
            layerRoot.linesShown = drawn.length > 0 || layerRoot.draftNoteId !== ""
        }
    }

    Instantiator {
        model: Notes

        delegate: StickyNote {
            parent: layerRoot
            notesLayer: layerRoot
        }
        onObjectAdded: (index, object) => {
            layerRoot.noteItems.push(object)
            if (object.noteId === layerRoot.pendingFocusId)
                layerRoot.focusNote(object.noteId)
            layerRoot.repaintLinks()
        }
        onObjectRemoved: (index, object) => {
            layerRoot.noteItems.splice(layerRoot.noteItems.indexOf(object), 1)
            layerRoot.repaintLinks()
        }
    }
}
