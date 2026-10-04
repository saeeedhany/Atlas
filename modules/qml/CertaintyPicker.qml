import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

RowLayout {
    id: picker

    property int value: 0

    signal picked(int level)

    spacing: 6

    Repeater {
        model: Session.certaintyNames

        delegate: AppButton {
            required property string modelData
            required property int index

            text: modelData
            primary: picker.value === index + 1
            onClicked: picker.picked(index + 1)
        }
    }
}
