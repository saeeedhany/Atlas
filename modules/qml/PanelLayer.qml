import QtQuick
import Atlas.ViewModels

Item {
    id: layer

    property int stackWidth: 320
    property int margin: 16
    property int spacing: 10
    property bool restoring: false
    readonly property var panels: {
        let list = []
        for (let i = 0; i < children.length; ++i) {
            if (children[i].panelId !== undefined)
                list.push(children[i])
        }
        return list
    }
    readonly property bool anyExpanded: panels.some(p => p.mode === "expanded")

    function panel(id: string) {
        return panels.find(p => p.panelId === id) || null
    }

    function layoutSlots() {
        let y = margin
        for (const p of panels) {
            p.expandedRect = Qt.rect(48, 24, Math.max(320, width - 96), Math.max(240, height - 48))
            if (p.mode !== "docked" && p.mode !== "folded")
                continue
            p.slot = Qt.rect(width - stackWidth - margin, y, stackWidth, p.dockedHeight)
            y += (p.mode === "folded" ? p.titleHeight : p.dockedHeight) + spacing
        }
    }

    function clampFloating() {
        if (width <= 0 || height <= 0)
            return
        for (const p of panels) {
            if (p.mode !== "floating")
                continue
            p.floatWidth = Math.min(p.floatWidth, width)
            p.floatHeight = Math.min(p.floatHeight, height)
            p.floatX = Math.max(0, Math.min(p.floatX, width - p.floatWidth))
            p.floatY = Math.max(0, Math.min(p.floatY, height - p.floatHeight))
        }
    }

    function save(p) {
        if (!restoring)
            AppSettings.setPanelLayout(p.panelId, p.savedLayout())
    }

    function restoreAll() {
        restoring = true
        for (const p of panels) {
            let saved = AppSettings.panelLayout(p.panelId)
            if (saved.mode === undefined || p.transient)
                continue
            p.floatX = Number(saved.x)
            p.floatY = Number(saved.y)
            p.floatWidth = Number(saved.width)
            p.floatHeight = Number(saved.height)
            p.mode = ["docked", "folded", "floating"].includes(saved.mode) ? saved.mode : "docked"
        }
        clampFloating()
        layoutSlots()
        restoring = false
    }

    function closeExpanded() {
        for (const p of panels)
            p.restore()
    }

    onWidthChanged: { clampFloating(); layoutSlots() }
    onHeightChanged: { clampFloating(); layoutSlots() }

    Component.onCompleted: {
        for (const p of panels) {
            let item = p
            item.layoutChanged.connect(() => {
                layer.layoutSlots()
                layer.save(item)
            })
        }
        restoreAll()
    }

    Rectangle {
        anchors.fill: parent
        z: 25
        color: Theme.background
        opacity: layer.anyExpanded ? 0.72 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: Motion.appear } }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: layer.closeExpanded()
            onWheel: wheel => wheel.accepted = true
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: layer.anyExpanded
        onActivated: layer.closeExpanded()
    }
}
