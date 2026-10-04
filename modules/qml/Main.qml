import QtQuick
import QtQuick.Controls.Basic
import Atlas.Render
import Atlas.ViewModels

ApplicationWindow {
    id: window

    property string page: "today"

    function show(name: string) {
        page = name
        if (name === "today")
            Today.refresh()
    }

    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Atlas"
    color: Theme.background

    palette.window: Theme.surfaceRaised
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceRaised
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.background
    palette.light: Theme.surfaceRaised
    palette.midlight: Theme.surfaceRaised
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.placeholderText: Theme.textMuted

    onActiveChanged: {
        if (active) {
            Today.refresh()
            MapView.refresh()
        }
    }

    onClosing: close => {
        if (Concept.dirty && !Concept.save())
            close.accepted = false
    }

    NavRail {
        id: rail
        objectName: "navRail"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Theme.railWidth
        current: window.page
        onSelected: name => window.show(name)
    }

    Item {
        id: stage
        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        GraphCanvas {
            id: canvas
            objectName: "graphCanvas"
            anchors.fill: parent
            animated: !AppSettings.reducedMotion
            Component.onCompleted: MapView.attach(canvas)
        }

        MapOverlay {
            id: mapOverlay
            objectName: "mapOverlay"
            anchors.fill: parent
            canvas: canvas
            active: window.page === "map"
        }

        TodayOverlay {
            objectName: "todayOverlay"
            anchors.fill: parent
            shown: window.page === "today"
            onActionTriggered: addConcept => {
                window.show("map")
                if (addConcept)
                    mapOverlay.focusNewConcept()
            }
        }

        SettingsScreen {
            objectName: "settingsScreen"
            anchors.fill: parent
            shown: window.page === "settings"
        }

        Toast {
            id: toast
            objectName: "toast"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
        }
    }

    Connections {
        target: MapView
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: Topics
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: Concept
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: ConceptLinks
        function onErrorOccurred(message) { toast.show(message) }
    }
}
