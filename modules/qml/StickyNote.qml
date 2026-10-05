import QtQuick
import QtQuick.Controls.Basic
import Atlas.ViewModels

Rectangle {
    id: note
    objectName: "stickyNote"

    required property string noteId
    required property string body
    required property string colorName
    required property real worldX
    required property real worldY
    required property real worldWidth
    required property real worldHeight
    required property var links
    property Item notesLayer
    property bool dragging: false
    property real dragDX: 0
    property real dragDY: 0
    property real resizeDW: 0
    property real resizeDH: 0
    property bool deleteArmed: false
    property point moveFrom
    readonly property real zoom: notesLayer.canvas ? notesLayer.canvas.zoom : 1
    readonly property int textSize: Math.round(Math.max(9, Math.min(20, Theme.fontBody * zoom)))
    readonly property real uiScale: textSize / Theme.fontBody
    readonly property bool chromeShown: notesLayer.interactive && zoom >= 0.6
    readonly property real minSide: Notes.minSize * zoom
    readonly property var linkTitles: links.map(link => link.kind === "concept"
                                                ? (MapView.conceptInfo(link.targetId).title || "")
                                                : Topics.nameOf(link.targetId))
    readonly property rect screenRect: {
        notesLayer.viewTick
        return notesLayer.screenRectOf(worldX, worldY, worldWidth, worldHeight)
    }

    function saveBody() {
        saveTimer.stop()
        if (bodyArea.text !== body)
            Notes.setBody(noteId, bodyArea.text)
    }

    function focusBody() {
        if (!bodyArea.visible)
            return
        bodyArea.forceActiveFocus()
        bodyArea.cursorPosition = bodyArea.length
    }

    function requestDelete() {
        if (deleteArmed) {
            Notes.remove(noteId)
            return
        }
        deleteArmed = true
        disarmTimer.restart()
    }

    function removeLink(index: int) {
        const link = links[index]
        if (link !== undefined)
            Notes.unlink(noteId, link.kind, link.targetId)
    }

    function beginMove(at: point) {
        moveFrom = at
        dragging = true
    }

    function updateMove(at: point) {
        dragDX = at.x - moveFrom.x
        dragDY = at.y - moveFrom.y
    }

    function endMove(commit: bool) {
        if (commit && (dragDX !== 0 || dragDY !== 0))
            notesLayer.moveNoteTo(noteId, screenRect.x + dragDX, screenRect.y + dragDY)
        dragDX = 0
        dragDY = 0
        dragging = false
    }

    x: screenRect.x + dragDX
    y: screenRect.y + dragDY
    width: Math.max(minSide, screenRect.width + resizeDW)
    height: Math.max(minSide, screenRect.height + resizeDH)
    radius: Theme.radius
    color: notesLayer.fills[colorName]
    border.color: Qt.darker(color, 1.15)
    z: dragging ? 3 : 2

    onDragDXChanged: notesLayer.repaintLinks()
    onDragDYChanged: notesLayer.repaintLinks()
    onWidthChanged: notesLayer.repaintLinks()
    onHeightChanged: notesLayer.repaintLinks()
    onLinksChanged: if (links.length === 0) linksPopup.close()

    Timer {
        id: saveTimer
        interval: 400
        onTriggered: note.saveBody()
    }

    Timer {
        id: disarmTimer
        interval: 3000
        onTriggered: note.deleteArmed = false
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.leftMargin: 4
        anchors.rightMargin: -4
        anchors.bottomMargin: -10
        z: -1
        radius: parent.radius
        color: "#000000"
        opacity: note.dragging ? (AppSettings.darkTheme ? 0.22 : 0.07) : 0
        visible: opacity > 0
    }

    MouseArea {
        anchors.fill: parent
        enabled: note.notesLayer.interactive
        acceptedButtons: Qt.LeftButton
        onPressed: mouse => note.beginMove(mapToItem(note.notesLayer, mouse.x, mouse.y))
        onPositionChanged: mouse => note.updateMove(mapToItem(note.notesLayer, mouse.x, mouse.y))
        onReleased: note.endMove(true)
        onCanceled: note.endMove(false)
        onDoubleClicked: note.focusBody()
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(18 * note.uiScale)

        MouseArea {
            anchors.fill: parent
            enabled: note.notesLayer.interactive
            cursorShape: Qt.OpenHandCursor
            onPressed: mouse => note.beginMove(mapToItem(note.notesLayer, mouse.x, mouse.y))
            onPositionChanged: mouse => note.updateMove(mapToItem(note.notesLayer, mouse.x, mouse.y))
            onReleased: note.endMove(true)
            onCanceled: note.endMove(false)
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8 * note.uiScale
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5 * note.uiScale
            visible: note.chromeShown

            Repeater {
                model: ["clay", "olive", "rose"]

                delegate: Rectangle {
                    required property string modelData
                    width: 8 * note.uiScale
                    height: width
                    radius: width / 2
                    color: note.notesLayer.inks[modelData]
                    opacity: note.colorName === modelData ? 1 : 0.45

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -2
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Notes.recolor(note.noteId, parent.modelData)
                    }
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8 * note.uiScale
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8 * note.uiScale
            visible: note.chromeShown

            Text {
                objectName: "noteLinksButton"
                visible: note.links.length > 0
                text: "→ " + note.links.length
                color: note.notesLayer.inks[note.colorName]
                font.family: Theme.mono
                font.pixelSize: Math.round(Theme.fontSmall * note.uiScale)

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: linksPopup.opened ? linksPopup.close() : linksPopup.open()
                }
            }
            Text {
                text: note.deleteArmed ? "Delete" : "×"
                color: note.deleteArmed ? Theme.danger : note.notesLayer.inks[note.colorName]
                font.family: Theme.sans
                font.weight: note.deleteArmed ? Font.Medium : Font.Normal
                font.pixelSize: Math.round((note.deleteArmed ? Theme.fontSmall : Theme.fontBody) * note.uiScale)

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: note.requestDelete()
                }
            }
        }
    }

    TextArea {
        id: bodyArea
        objectName: "noteBody"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        leftPadding: 10 * note.uiScale
        rightPadding: 10 * note.uiScale
        topPadding: 2 * note.uiScale
        bottomPadding: 10 * note.uiScale
        visible: note.zoom >= 0.45
        enabled: note.notesLayer.interactive
        text: note.body
        wrapMode: TextEdit.Wrap
        color: note.notesLayer.inks[note.colorName]
        selectionColor: Theme.primary
        selectedTextColor: Theme.onPrimary
        font.family: Theme.sans
        font.pixelSize: note.textSize
        background: null
        onTextChanged: if (text !== note.body) saveTimer.restart()
        onEditingFinished: note.saveBody()
    }

    Rectangle {
        width: Math.round(10 * note.uiScale)
        height: width
        radius: width / 2
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -width / 2
        color: note.notesLayer.inks[note.colorName]
        visible: note.notesLayer.interactive

        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.CrossCursor
            onPressed: mouse => note.notesLayer.beginDraft(note.noteId, mapToItem(note.notesLayer, mouse.x, mouse.y))
            onPositionChanged: mouse => note.notesLayer.moveDraft(mapToItem(note.notesLayer, mouse.x, mouse.y))
            onReleased: mouse => note.notesLayer.endDraft(mapToItem(note.notesLayer, mouse.x, mouse.y))
            onCanceled: note.notesLayer.cancelDraft()
        }
    }

    Item {
        objectName: "noteResizeGrip"
        width: Math.round(14 * note.uiScale)
        height: width
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: note.chromeShown

        Rectangle {
            width: Math.round(6 * note.uiScale)
            height: width
            radius: 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Math.round(4 * note.uiScale)
            color: note.notesLayer.inks[note.colorName]
            opacity: 0.55
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            property point pressAt
            onPressed: mouse => {
                pressAt = mapToItem(note.notesLayer, mouse.x, mouse.y)
                note.dragging = true
            }
            onPositionChanged: mouse => {
                let now = mapToItem(note.notesLayer, mouse.x, mouse.y)
                note.resizeDW = now.x - pressAt.x
                note.resizeDH = now.y - pressAt.y
            }
            onReleased: {
                note.notesLayer.resizeNoteTo(note.noteId, note.width, note.height)
                note.resizeDW = 0
                note.resizeDH = 0
                note.dragging = false
            }
            onCanceled: {
                note.resizeDW = 0
                note.resizeDH = 0
                note.dragging = false
            }
        }
    }

    Popup {
        id: linksPopup
        objectName: "noteLinksPopup"
        y: header.height
        x: Math.max(0, note.width - width)
        width: 220
        padding: 8

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceHigh
            border.color: Theme.outline
        }

        contentItem: Column {
            spacing: 2

            Repeater {
                model: note.linkTitles

                delegate: Item {
                    id: linkRow

                    required property string modelData
                    required property int index

                    width: 204
                    height: 28

                    Text {
                        anchors.left: parent.left
                        anchors.right: removeButton.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 4
                        text: linkRow.modelData
                        color: Theme.onSurface
                        font.family: Theme.sans
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    IconButton {
                        id: removeButton
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "×"
                        tip: "Remove link"
                        onClicked: note.removeLink(linkRow.index)
                    }
                }
            }
        }
    }
}
