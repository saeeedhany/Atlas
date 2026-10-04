pragma Singleton
import QtQuick
import Atlas.ViewModels

QtObject {
    readonly property color background: Palette.background
    readonly property color surface: Palette.surface
    readonly property color surfaceRaised: Palette.surfaceRaised
    readonly property color border: Palette.border
    readonly property color text: Palette.text
    readonly property color textMuted: Palette.textMuted
    readonly property color accent: Palette.accent
    readonly property color danger: Palette.danger
    readonly property color nodeFill: Palette.nodeFill
    readonly property color ringStrong: Palette.ringStrong
    readonly property color ringMedium: Palette.ringMedium
    readonly property color ringWeak: Palette.ringWeak
    readonly property color ringNew: Palette.ringNew
    readonly property color ringTrack: Palette.ringTrack

    readonly property string mono: "monospace"
    readonly property int fontSmall: 11
    readonly property int fontBody: 13
    readonly property int fontTitle: 17
    readonly property int fontHero: 26

    readonly property int gap: 8
    readonly property int radius: 10
    readonly property int railWidth: 72
    readonly property int panelWidth: 400

    function ringColor(recall: real): color {
        if (recall < 0)
            return ringNew
        if (recall >= 0.8)
            return ringStrong
        return recall >= 0.5 ? ringMedium : ringWeak
    }
}
