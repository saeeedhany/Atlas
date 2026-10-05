import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: button

    property string text
    property string tip
    property bool checked: false

    signal clicked()

    implicitWidth: 28
    implicitHeight: 28
    radius: Theme.radiusChip
    color: checked ? Theme.primary : area.containsMouse ? Theme.surfaceHigh : "transparent"

    Behavior on color { ColorAnimation { duration: Motion.fast } }

    Text {
        anchors.centerIn: parent
        text: button.text
        color: button.checked ? Theme.onPrimary : Theme.onSurfaceMuted
        font.family: Theme.sans
        font.pixelSize: Theme.fontBody
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }

    ToolTip {
        objectName: "iconTip"
        visible: area.containsMouse && button.tip !== ""
        delay: 500
        closePolicy: Popup.NoAutoClose
        text: button.tip
        padding: 6
        leftPadding: 8
        rightPadding: 8

        contentItem: Text {
            text: button.tip
            color: Theme.onSurface
            font.family: Theme.sans
            font.pixelSize: Theme.fontSmall
        }

        background: Rectangle {
            radius: Theme.radiusChip
            color: Theme.surfaceHigh
            border.color: Theme.outline
        }
    }
}
