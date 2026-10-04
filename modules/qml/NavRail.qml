import QtQuick
import QtQuick.Layouts

Rectangle {
    id: rail

    property string current: "today"
    readonly property var pages: [
        { name: "today", label: "Today" },
        { name: "map", label: "Map" },
        { name: "settings", label: "Settings" }
    ]

    signal selected(string name)

    color: Theme.surface

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    Text {
        id: mark
        anchors.horizontalCenter: parent.horizontalCenter
        y: 20
        text: "atlas"
        color: Theme.text
        font.family: Theme.mono
        font.pixelSize: Theme.fontSmall
        font.letterSpacing: 1
    }

    Rectangle {
        id: marker
        x: 0
        width: 3
        height: 24
        radius: 1.5
        color: Theme.accent
        y: column.y + rail.pages.findIndex(page => page.name === rail.current) * 56 + 16

        Behavior on y {
            enabled: !Motion.reduced
            NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
        }
    }

    Column {
        id: column
        anchors.top: mark.bottom
        anchors.topMargin: 28
        width: parent.width

        Repeater {
            model: rail.pages

            delegate: Item {
                id: entry

                required property var modelData

                width: column.width
                height: 56

                Text {
                    anchors.centerIn: parent
                    text: entry.modelData.label
                    color: rail.current === entry.modelData.name ? Theme.text : Theme.textMuted
                    font.family: Theme.mono
                    font.pixelSize: Theme.fontSmall

                    Behavior on color { ColorAnimation { duration: Motion.fast } }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: rail.selected(entry.modelData.name)
                }
            }
        }
    }
}
