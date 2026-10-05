import QtQuick
import QtQml.Models
import Atlas.ViewModels

Item {
    id: labels

    property Item canvas
    property bool interactive: true
    property int viewTick: 0
    property var tints: tintList()

    function tintList(): var {
        return [0, 1, 2, 3, 4, 5].map(hue => Theme.regionTint(hue))
    }

    visible: canvas !== null && !canvas.collapsed

    Connections {
        target: labels.canvas
        function onViewChanged() { labels.viewTick++ }
        function onZoomChanged() { labels.viewTick++ }
    }
    Connections {
        target: MapView
        function onSceneChanged() { labels.viewTick++ }
    }
    Connections {
        target: Palette
        function onChanged() { labels.tints = labels.tintList() }
    }

    Instantiator {
        model: MapView.regions

        delegate: Item {
            id: label
            objectName: "regionLabel"
            parent: labels

            required property var modelData
            readonly property string topicName: modelData.name
            readonly property point anchor: {
                labels.viewTick
                return labels.canvas ? labels.canvas.mapToScreen(modelData.x, modelData.y) : Qt.point(0, 0)
            }
            property real dragX: 0
            property real dragY: 0

            x: anchor.x + 20 + dragX
            y: anchor.y + 14 + dragY
            width: column.implicitWidth
            height: column.implicitHeight

            Column {
                id: column
                spacing: 2

                Text {
                    text: label.modelData.name
                    color: labels.tints[label.modelData.hue]
                    font.family: Theme.serif
                    font.weight: Font.Medium
                    font.pixelSize: Theme.fontTitle
                }
                Text {
                    text: label.modelData.caption
                    color: Theme.onSurfaceFaint
                    font.family: Theme.mono
                    font.pixelSize: Theme.fontCaption
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: labels.interactive
                cursorShape: Qt.PointingHandCursor
                property point pressAt
                property bool moved: false
                onPressed: mouse => {
                    pressAt = mapToItem(labels, mouse.x, mouse.y)
                    moved = false
                }
                onPositionChanged: mouse => {
                    let now = mapToItem(labels, mouse.x, mouse.y)
                    label.dragX = now.x - pressAt.x
                    label.dragY = now.y - pressAt.y
                    moved = moved || Math.abs(label.dragX) + Math.abs(label.dragY) > 4
                }
                onReleased: {
                    let scale = labels.canvas.zoom
                    if (moved)
                        MapView.moveTopic(label.modelData.topicId, label.dragX / scale, label.dragY / scale)
                    else
                        labels.canvas.fitWorldRect(label.modelData.x, label.modelData.y,
                                                   label.modelData.width, label.modelData.height)
                    label.dragX = 0
                    label.dragY = 0
                }
                onCanceled: {
                    label.dragX = 0
                    label.dragY = 0
                }
            }
        }
    }
}
