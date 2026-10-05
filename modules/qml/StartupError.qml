import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Window {
    id: window

    required property string message

    width: 560
    height: 280
    visible: true
    title: "Atlas"
    color: "#1d1c1a"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 12

        Text {
            text: "Atlas could not start"
            color: "#e4e4e7"
            font.pixelSize: 20
        }
        Text {
            objectName: "startupMessage"
            Layout.fillWidth: true
            text: window.message
            color: "#a1a1aa"
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
        Item { Layout.fillHeight: true }
        Button {
            Layout.alignment: Qt.AlignRight
            text: "Quit"
            onClicked: Qt.quit()
        }
    }
}
