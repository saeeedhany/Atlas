import QtQuick

Rectangle {
    id: button

    property string text
    property string tip

    signal clicked()

    implicitWidth: 28
    implicitHeight: 28
    radius: Theme.radiusChip
    color: hover.hovered ? Theme.surfaceHigh : "transparent"

    Behavior on color { ColorAnimation { duration: Motion.fast } }

    Text {
        anchors.centerIn: parent
        text: button.text
        color: Theme.onSurfaceMuted
        font.family: Theme.sans
        font.pixelSize: 14
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: button.clicked() }
}
