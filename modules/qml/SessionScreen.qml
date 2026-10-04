import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Item {
    id: screen

    property Item canvas
    property bool shown: false
    property int certainty: 0
    property bool revealed: false
    property var candidates: []
    property string followedFocus

    signal done()

    function chooseCertainty(level: int) {
        certainty = level
    }

    function addCandidate(id: string) {
        Session.addRecalled(id, 0, true)
        findField.clear()
        candidates = []
    }

    function reset() {
        certainty = 0
        revealed = false
        answer.clear()
        writtenKey.clear()
        findField.clear()
        candidates = []
    }

    function submitRebuild() {
        if (!Session.submitRebuild(certainty))
            return
        certainty = 0
        findField.clear()
        candidates = []
    }

    function reveal() {
        revealed = true
    }

    function grade(value: int) {
        if (Session.submitExplain(certainty, value, writtenKey.text))
            reset()
    }

    function finish() {
        Session.finish()
        followedFocus = ""
        reset()
        done()
    }

    function follow() {
        if (Session.stage === "idle") {
            followedFocus = ""
            reset()
            return
        }
        if (Session.focusId === followedFocus)
            return
        followedFocus = Session.focusId
        reset()
        if (canvas && followedFocus !== "")
            canvas.centerOn(followedFocus)
    }

    function outcomeColor(outcome) {
        if (outcome === "recalled")
            return Theme.ringStrong
        return outcome === "partial" ? Theme.ringMedium : Theme.ringWeak
    }

    function outcomeLabel(outcome) {
        if (outcome === "recalled")
            return "Recalled"
        if (outcome === "partial")
            return "With help"
        return outcome === "missed" ? "Missed" : "Not linked"
    }

    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Motion.appear } }

    Connections {
        target: Session
        function onChanged() { screen.follow() }
    }

    Connections {
        target: screen.canvas
        enabled: screen.shown && Session.stage === "rebuild"
        function onNodeClicked(id) {
            if (id !== "" && id !== Session.focusId)
                Session.addRecalled(id, 0, true)
            Qt.callLater(() => MapView.selectedId = Session.focusId)
        }
    }

    Rectangle {
        id: card
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.gap * 3
        width: Math.min(640, parent.width - 48)
        height: Math.min(content.implicitHeight + 48, parent.height - 48)
        radius: Theme.radius + 4
        color: Theme.surface
        border.color: Theme.border

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }

        ScrollView {
            id: cardScroll
            anchors.fill: parent
            padding: 24
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                id: content
                width: cardScroll.availableWidth
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    visible: Session.stage !== "summary"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        SectionLabel {
                            text: "Recall " + Session.focusNumber + " of " + Session.focusCount
                                  + (Session.focusIsNew ? "  ·  new" : "")
                        }
                        Text {
                            Layout.fillWidth: true
                            text: Session.focusTitle
                            color: Theme.text
                            font.pixelSize: Theme.fontTitle
                            elide: Text.ElideRight
                        }
                    }
                    AppButton {
                        text: "End session"
                        onClicked: Session.quit()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Session.stage === "rebuild"
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: "Name what " + Session.focusTitle + " connects to. " + Session.hiddenCount
                              + (Session.hiddenCount === 1 ? " link is" : " links are") + " hidden."
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.Wrap
                    }
                    SectionLabel { text: "How sure are you?" }
                    CertaintyPicker {
                        value: screen.certainty
                        onPicked: level => screen.chooseCertainty(level)
                    }
                    AppTextField {
                        id: findField
                        Layout.fillWidth: true
                        placeholderText: "Type a linked concept"
                        onTextEdited: screen.candidates = Session.candidates(text)
                        onAccepted: {
                            if (screen.candidates.length > 0)
                                screen.addCandidate(screen.candidates[0].id)
                        }
                    }
                    Repeater {
                        model: screen.candidates

                        delegate: ItemDelegate {
                            required property var modelData

                            Layout.fillWidth: true
                            text: modelData.title
                            font.pixelSize: Theme.fontBody
                            onClicked: screen.addCandidate(modelData.id)
                        }
                    }
                    Repeater {
                        model: Session.recalled

                        delegate: RowLayout {
                            id: named

                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: named.modelData.title
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                            ComboBox {
                                id: typeBox
                                font.pixelSize: Theme.fontBody
                                model: Session.typeNames
                                currentIndex: named.modelData.typeIndex
                                onActivated: Session.updateRecalled(named.index, currentIndex, direction.checked)
                            }
                            AppSwitch {
                                id: direction
                                checked: named.modelData.focusIsSource
                                text: checked ? "from here" : "to here"
                                onToggled: Session.updateRecalled(named.index, typeBox.currentIndex, checked)
                            }
                            AppButton {
                                text: "Remove"
                                danger: true
                                onClicked: Session.removeRecalled(named.index)
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        AppButton {
                            text: Session.hintsUsed > 0 ? "Hint (" + Session.hintsUsed + ")" : "Hint"
                            onClicked: Session.hint()
                        }
                        Item { Layout.fillWidth: true }
                        AppButton {
                            text: "Check"
                            primary: true
                            enabled: screen.certainty > 0
                            onClicked: screen.submitRebuild()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Session.stage === "feedback"
                    spacing: 8

                    Repeater {
                        model: Session.feedback

                        delegate: RowLayout {
                            id: result

                            required property var modelData

                            Layout.fillWidth: true
                            spacing: 10

                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: result.modelData.outcome === "confused" ? "transparent"
                                                                              : screen.outcomeColor(result.modelData.outcome)
                                border.color: screen.outcomeColor(result.modelData.outcome)
                            }
                            Text {
                                Layout.fillWidth: true
                                text: result.modelData.title
                                      + (result.modelData.typeName !== "" ? "  ·  " + result.modelData.typeName : "")
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                            Text {
                                text: screen.outcomeLabel(result.modelData.outcome)
                                color: Theme.textMuted
                                font.family: Theme.mono
                                font.pixelSize: Theme.fontSmall
                            }
                        }
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignRight
                        text: "Continue"
                        primary: true
                        onClicked: Session.continueToExplain()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Session.stage === "explain"
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: Session.prompt
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        wrapMode: Text.Wrap
                    }
                    SectionLabel { text: "How sure are you?" }
                    CertaintyPicker {
                        value: screen.certainty
                        onPicked: level => screen.chooseCertainty(level)
                    }
                    AppTextArea {
                        id: answer
                        Layout.fillWidth: true
                        placeholderText: "Your answer, in your own words"
                    }
                    AppButton {
                        visible: !screen.revealed
                        enabled: screen.certainty > 0
                        text: "Reveal"
                        onClicked: screen.reveal()
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: screen.revealed
                        spacing: 8

                        SectionLabel { text: "Stored answer" }
                        Text {
                            Layout.fillWidth: true
                            visible: Session.answerKey !== ""
                            text: Session.answerKey
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.Wrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: Session.answerKey === ""
                            text: "Nothing is written here yet. Write it now so the next review has an answer."
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.Wrap
                        }
                        AppTextArea {
                            id: writtenKey
                            objectName: "writtenKey"
                            Layout.fillWidth: true
                            visible: Session.answerKey === ""
                            placeholderText: "Write the answer"
                        }
                        SectionLabel { text: "How did it go?" }
                        RowLayout {
                            spacing: 6

                            Repeater {
                                model: ["Again", "Hard", "Good", "Easy"]

                                delegate: AppButton {
                                    required property string modelData
                                    required property int index

                                    text: modelData
                                    danger: index === 0
                                    onClicked: screen.grade(index + 1)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Session.stage === "summary"
                    spacing: 10

                    SectionLabel { text: "Session done" }
                    Text {
                        text: Session.recalledCount + " of " + Session.reviewedCount + " recalled"
                        color: Theme.text
                        font.pixelSize: Theme.fontHero
                    }
                    Repeater {
                        model: Session.calibration

                        delegate: Text {
                            required property string modelData

                            text: modelData
                            color: Theme.textMuted
                            font.family: Theme.mono
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: Session.tip
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.Wrap
                    }
                    Text {
                        text: Session.tipSource
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignRight
                        text: "Back to the map"
                        primary: true
                        onClicked: screen.finish()
                    }
                }
            }
        }
    }
}
