pragma Singleton
import QtQuick
import Atlas.ViewModels

QtObject {
    id: theme

    readonly property color background: Palette.background
    readonly property color surface: Palette.surface
    readonly property color surfaceHigh: Palette.surfaceRaised
    readonly property color outline: Palette.border
    readonly property color outlineStrong: Palette.outlineStrong
    property color onSurface
    readonly property color onSurfaceMuted: Palette.textMuted
    readonly property color onSurfaceFaint: Palette.textFaint
    readonly property color primary: Palette.accent
    property color onPrimary
    readonly property color secondary: Palette.secondary
    readonly property color tertiary: Palette.tertiary
    readonly property color danger: Palette.danger
    readonly property color link: Palette.link
    readonly property color nodeFill: Palette.nodeFill
    readonly property color ringStrong: Palette.ringStrong
    readonly property color ringMedium: Palette.ringMedium
    readonly property color ringWeak: Palette.ringWeak
    readonly property color ringNew: Palette.ringNew
    readonly property color ringTrack: Palette.ringTrack

    readonly property color surfaceRaised: surfaceHigh
    readonly property color border: outline
    readonly property color text: onSurface
    readonly property color textMuted: onSurfaceMuted
    readonly property color accent: primary

    readonly property string serif: "Newsreader"
    readonly property string sans: "Inter"
    readonly property string mono: "IBM Plex Mono"
    readonly property int fontCaption: 10
    readonly property int fontSmall: 11
    readonly property int fontBody: 13
    readonly property int fontTitle: 17
    readonly property int fontHeadline: 24
    readonly property int fontHero: 32

    readonly property int gap: 8
    readonly property int radius: 12
    readonly property int radiusPanel: 16
    readonly property int radiusChip: 8
    readonly property int radiusRegion: 28
    readonly property int panelWidth: 380

    // Bound here because "onSurface:" or "onPrimary:" in a declaration would be read as a signal handler.
    Component.onCompleted: {
        theme.onSurface = Qt.binding(() => Palette.text)
        theme.onPrimary = Qt.binding(() => Palette.onPrimary)
    }

    function ringColor(recall: real): color {
        if (recall < 0)
            return ringNew
        if (recall >= 0.8)
            return ringStrong
        return recall >= 0.5 ? ringMedium : ringWeak
    }

    function regionTint(hue: int): color {
        return Palette.regionTint(hue)
    }

    function noteFill(name: string): color {
        return Palette.noteFill(name)
    }

    function noteText(name: string): color {
        return Palette.noteText(name)
    }
}
