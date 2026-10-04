import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

Item {
    id: overlay

    property bool shown: false
    readonly property string headline: Today.empty ? "Your map is empty"
                                     : Today.caughtUp ? "All caught up"
                                     : Today.itemCount + (Today.itemCount === 1 ? " item" : " items")
                                       + ", about " + Today.estimatedMinutes + " min"
    readonly property string action: Today.empty ? "add" : Today.caughtUp ? "map" : "session"
    readonly property string actionText: action === "add" ? "Add your first concept"
                                       : action === "map" ? "Open the map"
                                       : "Start session"

    signal actionTriggered(string action)

    function act() {
        actionTriggered(action)
    }

    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
        opacity: 0.78

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }
    }

    Rectangle {
        id: card
        width: 440
        height: content.implicitHeight + 56
        anchors.centerIn: parent
        anchors.verticalCenterOffset: overlay.shown || Motion.reduced ? 0 : 16
        radius: Theme.radius + 4
        color: Theme.surface
        border.color: Theme.border

        Behavior on anchors.verticalCenterOffset {
            enabled: !Motion.reduced
            NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 28
            spacing: 10

            SectionLabel { text: "Today" }

            Text {
                Layout.fillWidth: true
                text: overlay.headline
                color: Theme.text
                font.pixelSize: Theme.fontHero
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: !Today.empty
                text: Today.dueCount + " due for recall, " + Today.newCount + " new. "
                      + Today.learnedCount + " of " + Today.conceptCount + " concepts learned."
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                text: Today.empty ? "Start with one idea you want to keep. Link it to what you already know."
                                  : "Short daily recall beats one long session."
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }

            AppButton {
                Layout.topMargin: 8
                primary: true
                text: overlay.actionText
                onClicked: overlay.act()
            }
        }
    }
}
