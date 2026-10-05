import QtQuick
import QtQuick.Controls.Basic
import Atlas.Render
import Atlas.ViewModels

ApplicationWindow {
    id: window

    readonly property string mode: Session.stage === "idle" ? "board" : "session"

    function startSession(): bool {
        panelLayer.closeExpanded()
        mapOverlay.restoreCard()
        notesLayer.flushNotes()
        if (Concept.dirty && !Concept.save())
            return false
        return Session.start()
    }

    function openSettings() {
        mapOverlay.restoreCard()
        settingsPanel.expand()
    }

    function handleEscape() {
        if (panelLayer.anyExpanded)
            panelLayer.closeExpanded()
        else if (mapOverlay.cardExpanded)
            mapOverlay.restoreCard()
        else
            MapView.selectedId = ""
    }

    width: 1280
    height: 820
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: "Atlas"
    color: Theme.background
    font.family: Theme.sans

    palette.window: Theme.surfaceHigh
    palette.windowText: Theme.onSurface
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceHigh
    palette.text: Theme.onSurface
    palette.button: Theme.surface
    palette.buttonText: Theme.onSurface
    palette.highlight: Theme.primary
    palette.highlightedText: Theme.onPrimary
    palette.light: Theme.surfaceHigh
    palette.midlight: Theme.surfaceHigh
    palette.mid: Theme.outline
    palette.dark: Theme.outline
    palette.placeholderText: Theme.onSurfaceFaint

    onActiveChanged: {
        if (active) {
            Today.refresh()
            MapView.refresh()
        }
    }

    onClosing: close => {
        notesLayer.flushNotes()
        if (Concept.dirty && !Concept.save())
            close.accepted = false
    }

    TopBar {
        id: topBar
        objectName: "topBar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 5
        enabled: window.mode === "board"
        onRecallRequested: window.startSession()
        onSettingsRequested: window.openSettings()
        onFocusRequested: id => mapOverlay.focusConcept(id)
        onFitRequested: canvas.fitToContent()
        onNoteRequested: body => notesLayer.createAtScreen(board.width / 2, board.height / 2, body)
    }

    Item {
        id: board
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        clip: true

        GraphCanvas {
            id: canvas
            objectName: "graphCanvas"
            anchors.fill: parent
            animated: !AppSettings.reducedMotion
            Component.onCompleted: MapView.attach(canvas)
            onBackgroundDoubleClicked: (x, y) => {
                if (window.mode === "board")
                    notesLayer.createAtWorld(x, y, "")
            }
        }

        RegionLabels {
            objectName: "regionLabels"
            anchors.fill: parent
            canvas: canvas
            interactive: window.mode === "board"
        }

        NotesLayer {
            id: notesLayer
            objectName: "notesLayer"
            anchors.fill: parent
            canvas: canvas
            interactive: window.mode === "board"
        }

        MapOverlay {
            id: mapOverlay
            objectName: "mapOverlay"
            anchors.fill: parent
            z: cardExpanded ? 1 : 0
            canvas: canvas
            active: window.mode === "board"
            obstacles: panelLayer.occupied
            onNotice: message => toast.show(message)
        }

        SessionScreen {
            objectName: "sessionScreen"
            anchors.fill: parent
            canvas: canvas
            shown: window.mode === "session"
        }

        PanelLayer {
            id: panelLayer
            objectName: "panelLayer"
            anchors.fill: parent
            enabled: window.mode === "board"
            opacity: enabled ? 1 : 0
            visible: opacity > 0

            Behavior on opacity { NumberAnimation { duration: Motion.appear } }

            Panel {
                panelId: "today"
                title: "Today"
                dockedHeight: 230

                TodayPanel {
                    objectName: "todayPanel"
                    anchors.fill: parent
                    onActionTriggered: action => {
                        if (action === "session")
                            window.startSession()
                        else if (action === "add")
                            topBar.focusNewConcept()
                        else
                            canvas.fitToContent()
                    }
                }
            }

            Panel {
                id: settingsPanel
                objectName: "settingsPanel"
                panelId: "settings"
                title: "Settings"
                onDemand: true

                SettingsScreen {
                    objectName: "settingsScreen"
                    anchors.fill: parent
                }
            }
        }

        Toast {
            id: toast
            objectName: "toast"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            z: 40
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: window.mode === "board"
        onActivated: window.handleEscape()
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
    Connections {
        target: Notes
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: Session
        function onErrorOccurred(message) { toast.show(message) }
    }
}
