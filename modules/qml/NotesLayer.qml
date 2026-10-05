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
    property var noteItems: []
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

    function createAtScreen(sx: real, sy: real, body: string): string {
        let world = canvas.mapToWorld(sx, sy)
        return Notes.create(world.x, world.y, body)
    }

    function moveNoteTo(noteId: string, sx: real, sy: real): bool {
        let world = canvas.mapToWorld(sx, sy)
        return Notes.move(noteId, world.x, world.y)
    }

    function linkNoteAt(noteId: string, sx: real, sy: real): bool {
        let node = canvas.nodeAt(sx, sy)
        if (node !== "")
            return Notes.link(noteId, "concept", node)
        let world = canvas.mapToWorld(sx, sy)
        for (const region of MapView.regions) {
            if (world.x >= region.x && world.x <= region.x + region.width
                    && world.y >= region.y && world.y <= region.y + region.height)
                return Notes.link(noteId, "topic", region.topicId)
        }
        return false
    }

    function targetPoint(link) {
        if (link.kind === "concept")
            return canvas.screenPositionOf(link.targetId)
        let region = MapView.regions.find(r => r.topicId === link.targetId)
        return region ? canvas.mapToScreen(region.x + region.width / 2, region.y + region.height / 2) : undefined
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

    Connections {
        target: layerRoot.canvas
        function onViewChanged() { layerRoot.viewTick++; lines.requestPaint() }
        function onZoomChanged() { layerRoot.viewTick++; lines.requestPaint() }
    }
    Connections {
        target: MapView
        function onSceneChanged() { lines.requestPaint() }
    }
    Connections {
        target: Notes
        function onDataChanged(topLeft, bottomRight, roles) { lines.requestPaint() }
        function onCountChanged() { lines.requestPaint() }
    }
    Connections {
        target: Palette
        function onChanged() {
            layerRoot.fills = layerRoot.toneMap(true)
            layerRoot.inks = layerRoot.toneMap(false)
            lines.requestPaint()
        }
    }

    Canvas {
        id: lines
        anchors.fill: parent

        onPaint: {
            let ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = 1.5
            ctx.setLineDash([4, 4])
            for (const item of layerRoot.noteItems) {
                ctx.strokeStyle = layerRoot.inks[item.colorName]
                for (const link of item.links) {
                    let to = layerRoot.targetPoint(link)
                    if (to === undefined)
                        continue
                    ctx.beginPath()
                    ctx.moveTo(item.x + item.width, item.y + item.height / 2)
                    ctx.lineTo(to.x, to.y)
                    ctx.stroke()
                }
            }
            if (layerRoot.draftNoteId !== "") {
                ctx.strokeStyle = Theme.primary
                ctx.beginPath()
                ctx.moveTo(layerRoot.draftFrom.x, layerRoot.draftFrom.y)
                ctx.lineTo(layerRoot.draftTo.x, layerRoot.draftTo.y)
                ctx.stroke()
            }
        }
    }

    Instantiator {
        model: Notes

        delegate: StickyNote {
            parent: layerRoot
            notesLayer: layerRoot
        }
        onObjectAdded: (index, object) => {
            layerRoot.noteItems = layerRoot.noteItems.concat([object])
            lines.requestPaint()
        }
        onObjectRemoved: (index, object) => {
            layerRoot.noteItems = layerRoot.noteItems.filter(item => item !== object)
            lines.requestPaint()
        }
    }
}
