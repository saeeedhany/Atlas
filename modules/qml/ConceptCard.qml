import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

Item {
    id: card

    property Item canvas
    property Item area
    property bool detached: false
    property bool expanded: false
    property real detachedX: 0
    property real detachedY: 0
    property int viewTick: 0
    readonly property real cardWidth: Theme.panelWidth
    readonly property real cardHeight: Math.min(area ? area.height - 32 : 600, 620)
    readonly property var nodePoint: {
        viewTick
        return canvas && Concept.exists ? canvas.screenPositionOf(Concept.conceptId) : undefined
    }
    readonly property rect besideRect: area && nodePoint !== undefined
        ? area.besideRect(nodePoint, cardWidth, cardHeight, area.width, area.height)
        : Qt.rect(16, 16, cardWidth, cardHeight)

    signal focusRequested(string id)
    signal pinRequested()

    function keptX(px: real): real {
        return area ? Math.max(0, Math.min(px, area.width - width)) : px
    }

    function keptY(py: real): real {
        return area ? Math.max(0, Math.min(py, area.height - height)) : py
    }

    function detachTo(px: real, py: real) {
        detachedX = keptX(px)
        detachedY = keptY(py)
        detached = true
    }

    function attach() {
        detached = false
    }

    function expand() {
        expanded = true
    }

    function restore() {
        expanded = false
    }

    x: expanded ? 48 : detached ? keptX(detachedX) : besideRect.x
    y: expanded ? 24 : detached ? keptY(detachedY) : besideRect.y
    width: expanded && area ? area.width - 96 : cardWidth
    height: expanded && area ? area.height - 48 : cardHeight
    visible: editor.shown
    z: expanded ? 30 : 15

    Connections {
        target: card.canvas
        function onViewChanged() { card.viewTick++ }
    }
    Connections {
        target: MapView
        function onSceneChanged() { card.viewTick++ }
    }
    property string shownId

    Connections {
        target: Concept
        function onLoaded() {
            if (Concept.conceptId === card.shownId) {
                card.viewTick++
                return
            }
            card.shownId = Concept.conceptId
            card.detached = false
            card.expanded = false
        }
    }

    Rectangle {
        id: strip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 32
        radius: Theme.radiusPanel
        color: Theme.surface
        border.color: Theme.outline

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.OpenHandCursor
            onWheel: wheel => wheel.accepted = true
            property point pressAt
            property point cardAt
            onPressed: mouse => {
                pressAt = mapToItem(card.parent, mouse.x, mouse.y)
                cardAt = Qt.point(card.x, card.y)
            }
            onPositionChanged: mouse => {
                let now = mapToItem(card.parent, mouse.x, mouse.y)
                if (Math.abs(now.x - pressAt.x) + Math.abs(now.y - pressAt.y) > 4 && !card.expanded)
                    card.detachTo(cardAt.x + now.x - pressAt.x, cardAt.y + now.y - pressAt.y)
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 4
            spacing: 2

            Rectangle {
                width: 28
                height: 4
                radius: 2
                color: Theme.outlineStrong
            }
            Item { Layout.fillWidth: true }
            IconButton {
                visible: card.detached
                text: "\u2316"
                tip: "Back to its node"
                onClicked: card.attach()
            }
            IconButton {
                text: "\u2020"
                tip: "Pin"
                onClicked: card.pinRequested()
            }
            IconButton {
                text: card.expanded ? "\u2199" : "\u2197"
                tip: card.expanded ? "Restore" : "Expand"
                onClicked: card.expanded ? card.restore() : card.expand()
            }
        }
    }

    ConceptPanel {
        id: editor
        objectName: "conceptPanel"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: strip.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        onFocusRequested: id => card.focusRequested(id)
    }
}
