import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control

    font.family: Theme.sans
    font.pixelSize: Theme.fontBody
    leftPadding: 12
    rightPadding: 8
    opacity: enabled ? 1 : 0.4

    contentItem: Text {
        rightPadding: control.indicator.width + control.spacing
        text: control.displayText
        font: control.font
        color: Theme.onSurface
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Chevron {
        x: control.width - width - control.rightPadding - 4
        y: control.topPadding + (control.availableHeight - height) / 2
    }

    background: Rectangle {
        implicitWidth: 140
        implicitHeight: 34
        radius: Theme.radius
        color: control.hovered ? Theme.surfaceHigh : Theme.surface
        border.color: control.activeFocus || control.popup.visible ? Theme.primary : Theme.outline

        Behavior on color { ColorAnimation { duration: Motion.fast } }
    }

    delegate: ItemDelegate {
        id: entry

        required property int index

        width: ListView.view ? ListView.view.width : implicitWidth
        highlighted: control.highlightedIndex === index
        font: control.font

        contentItem: Text {
            text: control.textAt(entry.index)
            font: entry.font
            color: control.currentIndex === entry.index ? Theme.primary : Theme.onSurface
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Theme.radiusChip
            color: entry.highlighted || entry.hovered ? Theme.surface : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceHigh
            border.color: Theme.outline
        }
    }
}
