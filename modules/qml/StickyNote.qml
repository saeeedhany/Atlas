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
    readonly property rect screenRect: {
        notesLayer.viewTick
        return notesLayer.screenRectOf(worldX, worldY, worldWidth, worldHeight)
    }

    x: screenRect.x + dragDX
    y: screenRect.y + dragDY
    width: screenRect.width
    height: screenRect.height
    radius: Theme.radius
    color: notesLayer.fills[colorName]
    border.color: Qt.darker(color, 1.15)
    z: dragging ? 3 : 2

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

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 18

        MouseArea {
            anchors.fill: parent
            enabled: note.notesLayer.interactive
            cursorShape: Qt.OpenHandCursor
            property point pressAt
            onPressed: mouse => {
                pressAt = mapToItem(note.notesLayer, mouse.x, mouse.y)
                note.dragging = true
            }
            onPositionChanged: mouse => {
                let now = mapToItem(note.notesLayer, mouse.x, mouse.y)
                note.dragDX = now.x - pressAt.x
                note.dragDY = now.y - pressAt.y
            }
            onReleased: {
                note.notesLayer.moveNoteTo(note.noteId, note.screenRect.x + note.dragDX, note.screenRect.y + note.dragDY)
                note.dragDX = 0
                note.dragDY = 0
                note.dragging = false
            }
            onCanceled: {
                note.dragDX = 0
                note.dragDY = 0
                note.dragging = false
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            visible: note.height > 70 && note.notesLayer.interactive

            Repeater {
                model: ["clay", "olive", "rose"]

                delegate: Rectangle {
                    required property string modelData
                    width: 8
                    height: 8
                    radius: 4
                    color: note.notesLayer.inks[modelData]
                    opacity: note.colorName === modelData ? 1 : 0.45

                    TapHandler { onTapped: Notes.recolor(note.noteId, parent.modelData) }
                }
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "\u00d7"
            color: note.notesLayer.inks[note.colorName]
            font.pixelSize: 13
            visible: note.height > 70 && note.notesLayer.interactive

            TapHandler { onTapped: Notes.remove(note.noteId) }
        }
    }

    TextArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        leftPadding: 10
        rightPadding: 10
        topPadding: 2
        bottomPadding: 10
        visible: note.notesLayer.canvas.zoom >= 0.45
        enabled: note.notesLayer.interactive
        text: note.body
        wrapMode: TextEdit.Wrap
        color: note.notesLayer.inks[note.colorName]
        font.family: Theme.sans
        font.pixelSize: 12
        background: null
        onEditingFinished: if (text !== note.body) Notes.setBody(note.noteId, text)
    }

    Rectangle {
        width: 10
        height: 10
        radius: 5
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -5
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
}
