pragma Singleton
import QtQuick
import Atlas.ViewModels

QtObject {
    readonly property bool reduced: AppSettings.reducedMotion
    readonly property var curve: [0.22, 1, 0.36, 1, 1, 1]
    readonly property int fast: reduced ? 0 : 140
    readonly property int normal: reduced ? 0 : 240
    readonly property int slow: reduced ? 0 : 420
    readonly property int decay: reduced ? 0 : 1200
    readonly property int stagger: reduced ? 0 : 40
    readonly property int fade: 120
    readonly property int appear: reduced ? fade : normal
}
