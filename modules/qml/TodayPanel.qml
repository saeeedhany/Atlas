import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

ColumnLayout {
    id: today

    readonly property string headline: Today.empty ? "Your map is empty"
                                     : Today.caughtUp ? "All caught up"
                                     : Today.itemCount + (Today.itemCount === 1 ? " item" : " items")
                                       + ", about " + Today.estimatedMinutes + " min"
    readonly property string action: Today.empty ? "add" : Today.caughtUp ? "map" : "session"
    readonly property string actionText: action === "add" ? "Add your first concept"
                                       : action === "map" ? "Explore the map"
                                       : "Start session"

    signal actionTriggered(string action)

    function act() {
        actionTriggered(action)
    }

    spacing: 8

    Text {
        Layout.fillWidth: true
        text: today.headline
        color: Theme.onSurface
        font.family: Theme.serif
        font.weight: Font.Medium
        font.pixelSize: Theme.fontHeadline
        wrapMode: Text.Wrap
    }
    Text {
        Layout.fillWidth: true
        visible: !Today.empty
        text: Today.dueCount + " due, " + Today.newCount + " new  ·  " + Today.learnedCount + " of "
              + Today.conceptCount + " learned"
        color: Theme.onSurfaceFaint
        font.family: Theme.mono
        font.pixelSize: Theme.fontSmall
    }
    Text {
        Layout.fillWidth: true
        text: Today.empty ? "Start with one idea you want to keep, then link it to what you know."
                          : "Short daily recall beats one long session."
        color: Theme.onSurfaceMuted
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }
    Item { Layout.fillHeight: true }
    AppButton {
        primary: true
        text: today.actionText
        onClicked: today.act()
    }
}
