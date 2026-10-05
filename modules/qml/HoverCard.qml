import QtQuick
import Atlas.ViewModels

Rectangle {
    id: card

    property Item canvas
    property string conceptId
    property string linkId
    property var info: ({})
    readonly property bool shown: conceptId !== "" || linkId !== ""
    readonly property string title: conceptId !== "" ? (info.title || "")
                                  : linkId !== "" ? info.source + " " + info.typeName + " " + info.target
                                  : ""
    readonly property string detail: conceptId !== "" ? recallText() + (info.topic ? "  ·  " + info.topic : "")
                                   : linkId !== "" ? (info.note || "")
                                   : ""

    function recallText() {
        return info.recall >= 0 ? "Recall " + Math.round(info.recall * 100) + "%" : "Not learned yet"
    }

    function showConcept(id: string) {
        linkId = ""
        info = id !== "" ? MapView.conceptInfo(id) : ({})
        conceptId = info.title !== undefined ? id : ""
        place()
    }

    function showLink(id: string) {
        if (id === "" && conceptId !== "")
            return
        conceptId = ""
        info = id !== "" ? MapView.linkInfo(id) : ({})
        linkId = info.typeName !== undefined ? id : ""
        place()
    }

    function anchorPoint() {
        if (!canvas)
            return undefined
        if (conceptId !== "")
            return canvas.screenPositionOf(conceptId)
        let from = canvas.screenPositionOf(info.sourceId)
        let to = canvas.screenPositionOf(info.targetId)
        if (from === undefined || to === undefined)
            return undefined
        return Qt.point((from.x + to.x) / 2, (from.y + to.y) / 2)
    }

    function place() {
        let point = shown ? anchorPoint() : undefined
        if (point === undefined || !parent)
            return
        x = Math.max(8, Math.min(point.x + 18, parent.width - width - 8))
        y = Math.max(8, Math.min(point.y + 18, parent.height - height - 8))
    }

    width: 260
    height: column.implicitHeight + 20
    radius: Theme.radius
    color: Theme.surfaceRaised
    border.color: Theme.border
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.fast } }

    Column {
        id: column
        x: 12
        y: 10
        width: card.width - 24
        spacing: 4

        Text {
            width: parent.width
            text: card.title
            color: Theme.text
            font.family: Theme.serif
            font.weight: Font.Medium
            font.pixelSize: Theme.fontTitle
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: card.detail
            color: Theme.textMuted
            font.family: Theme.mono
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.Wrap
        }
    }
}
