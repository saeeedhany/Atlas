import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: pinned
    objectName: "pinnedCard"

    required property string conceptId
    property Item canvas
    property Item area
    property var info: MapView.conceptInfo(conceptId)

    signal unpinRequested(string id)

    function edit() {
        MapView.selectedId = conceptId
    }

    width: 240
    height: column.implicitHeight + 28
    radius: Theme.radiusPanel
    color: Theme.surface
    border.color: Theme.outline
    z: 14

    onHeightChanged: if (area) area.scheduleLayout()
    Component.onCompleted: if (area) area.scheduleLayout()

    Connections {
        target: MapView
        function onSceneChanged() {
            pinned.info = MapView.conceptInfo(pinned.conceptId)
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Ring {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                thickness: 2
                recall: pinned.info.recall !== undefined ? pinned.info.recall : -1
            }
            Text {
                Layout.fillWidth: true
                text: pinned.info.title || ""
                color: Theme.onSurface
                font.family: Theme.serif
                font.weight: Font.Medium
                font.pixelSize: Theme.fontTitle
                elide: Text.ElideRight
            }
        }
        Text {
            Layout.fillWidth: true
            text: (pinned.info.topic || "") + (pinned.info.recall >= 0 ? "  \u00b7  " + Math.round(pinned.info.recall * 100) + "%" : "")
            color: Theme.onSurfaceFaint
            font.family: Theme.mono
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: pinned.info.definition || ""
            color: Theme.onSurfaceMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }
            AppButton {
                text: "Unpin"
                onClicked: pinned.unpinRequested(pinned.conceptId)
            }
            AppButton {
                text: "Edit"
                primary: true
                onClicked: pinned.edit()
            }
        }
    }
}
