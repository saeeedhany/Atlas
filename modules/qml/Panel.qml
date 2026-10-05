import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

PanelBase {
    id: panel

    required property string panelId
    property string title
    property int dockedHeight: 240
    property string mode: transient ? "hidden" : "docked"
    property string returnMode: "docked"
    property real floatX: 0
    property real floatY: 0
    property real floatWidth: 320
    property real floatHeight: dockedHeight
    property rect slot: Qt.rect(0, 0, 320, dockedHeight)
    property rect expandedRect: Qt.rect(0, 0, 640, 480)
    property bool dragging: false
    readonly property int titleHeight: 40
    readonly property rect target: mode === "expanded" ? expandedRect
                                 : mode === "floating" ? Qt.rect(floatX, floatY, floatWidth, floatHeight)
                                 : mode === "folded" ? Qt.rect(slot.x, slot.y, slot.width, titleHeight)
                                 : slot
    default property alias content: body.data

    signal layoutChanged()

    function fold() {
        if (mode !== "docked")
            return
        mode = "folded"
        layoutChanged()
    }

    function unfold() {
        if (mode !== "folded")
            return
        mode = "docked"
        layoutChanged()
    }

    function expand() {
        if (mode === "expanded")
            return
        returnMode = mode === "hidden" ? "docked" : mode
        mode = "expanded"
        layoutChanged()
    }

    function restore() {
        if (mode !== "expanded")
            return
        mode = transient ? "hidden" : returnMode
        layoutChanged()
    }

    function floatAt(px: real, py: real) {
        if (mode !== "floating") {
            floatWidth = Math.max(width, 220)
            floatHeight = Math.max(height, 160)
            mode = "floating"
        }
        floatX = px
        floatY = py
        layoutChanged()
    }

    function dock() {
        if (mode !== "floating")
            return
        mode = "docked"
        layoutChanged()
    }

    function resizeTo(w: real, h: real) {
        floatWidth = Math.max(220, w)
        floatHeight = Math.max(120, h)
        layoutChanged()
    }

    function savedLayout() {
        let kept = transient ? "hidden" : mode === "expanded" ? returnMode : mode
        return { mode: kept, x: floatX, y: floatY, width: floatWidth, height: floatHeight }
    }

    x: target.x
    y: target.y
    width: target.width
    height: target.height
    visible: mode !== "hidden"
    z: mode === "expanded" ? 30 : mode === "floating" ? 20 : 10

    Behavior on x { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }
    Behavior on y { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }
    Behavior on width { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }
    Behavior on height { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }

    Rectangle {
        anchors.fill: frame
        anchors.topMargin: 12
        anchors.leftMargin: 6
        anchors.rightMargin: -6
        anchors.bottomMargin: -12
        radius: Theme.radiusPanel
        color: "#000000"
        opacity: panel.mode === "floating" || panel.dragging ? (AppSettings.darkTheme ? 0.22 : 0.07) : 0
        visible: opacity > 0
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusPanel
        color: Theme.surface
        border.color: Theme.outline
        clip: true

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }

        Item {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: panel.titleHeight

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.OpenHandCursor
                property point pressAt
                property point panelAt
                onPressed: mouse => {
                    pressAt = mapToItem(panel.parent, mouse.x, mouse.y)
                    panelAt = Qt.point(panel.x, panel.y)
                }
                onPositionChanged: mouse => {
                    let now = mapToItem(panel.parent, mouse.x, mouse.y)
                    let dx = now.x - pressAt.x
                    let dy = now.y - pressAt.y
                    if (!panel.dragging && Math.abs(dx) + Math.abs(dy) < 4)
                        return
                    if (panel.mode === "expanded")
                        return
                    panel.dragging = true
                    panel.floatAt(panelAt.x + dx, panelAt.y + dy)
                }
                onReleased: panel.dragging = false
                onDoubleClicked: panel.mode === "expanded" ? panel.restore() : panel.expand()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: panel.title
                    color: Theme.onSurface
                    font.family: Theme.serif
                    font.pixelSize: 15
                    elide: Text.ElideRight
                }
                IconButton {
                    visible: panel.mode === "docked" || panel.mode === "folded"
                    text: panel.mode === "folded" ? "+" : "\u2212"
                    tip: panel.mode === "folded" ? "Unfold" : "Fold"
                    onClicked: panel.mode === "folded" ? panel.unfold() : panel.fold()
                }
                IconButton {
                    visible: panel.mode === "floating"
                    text: "\u21f2"
                    tip: "Pin back"
                    onClicked: panel.dock()
                }
                IconButton {
                    text: panel.mode === "expanded" ? "\u2199" : "\u2197"
                    tip: panel.mode === "expanded" ? "Restore" : "Expand"
                    onClicked: panel.mode === "expanded" ? panel.restore() : panel.expand()
                }
            }
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.bottomMargin: 16
            visible: panel.mode !== "folded"
        }

        Rectangle {
            width: 14
            height: 14
            radius: 4
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 4
            color: Theme.outline
            visible: panel.mode === "floating"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeFDiagCursor
                property point pressAt
                property size sizeAt
                onPressed: mouse => {
                    pressAt = mapToItem(panel.parent, mouse.x, mouse.y)
                    sizeAt = Qt.size(panel.floatWidth, panel.floatHeight)
                    panel.dragging = true
                }
                onPositionChanged: mouse => {
                    let now = mapToItem(panel.parent, mouse.x, mouse.y)
                    panel.resizeTo(sizeAt.width + now.x - pressAt.x, sizeAt.height + now.y - pressAt.y)
                }
                onReleased: panel.dragging = false
            }
        }
    }
}
