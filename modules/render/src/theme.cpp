#include "atlas/render/theme.hpp"

namespace atlas::render {

namespace {

QColor withAlpha(QRgb rgb, int alpha) {
    QColor color(rgb);
    color.setAlpha(alpha);
    return color;
}

Theme makeDark() {
    Theme theme;
    theme.background = QColor(0x18181b);
    theme.dot = withAlpha(0xe4e4e7, 14);
    theme.edge = QColor(0x52525b);
    theme.edgeDimmed = withAlpha(0x52525b, 80);
    theme.selectedRing = QColor(0xe4e4e7);
    theme.neighborRing = withAlpha(0xa1a1aa, 160);
    theme.hoverRing = withAlpha(0xe4e4e7, 120);
    theme.nodeBorder = QColor(0x3f3f46);

    theme.accent = QColor(0xa3e635);

    theme.surface = QColor(0x1f1f23);
    theme.surfaceRaised = QColor(0x27272a);
    theme.border = QColor(0x2e2e33);
    theme.text = QColor(0xe4e4e7);
    theme.textMuted = QColor(0xa1a1aa);
    theme.nodeFill = QColor(0x27272a);
    theme.label = QColor(0xa1a1aa);
    theme.ringStrong = QColor(0xa3e635);
    theme.ringMedium = QColor(0xe4e4e7);
    theme.ringWeak = QColor(0xfb7185);
    theme.ringNew = QColor(0x52525b);
    theme.ringTrack = QColor(0x2e2e33);
    theme.danger = QColor(0xfb7185);
    return theme;
}

Theme makeLight() {
    Theme theme;
    theme.background = QColor(0xfafafa);
    theme.dot = withAlpha(0x18181b, 18);
    theme.edge = QColor(0xa1a1aa);
    theme.edgeDimmed = withAlpha(0xa1a1aa, 70);
    theme.selectedRing = QColor(0x18181b);
    theme.neighborRing = withAlpha(0x52525b, 160);
    theme.hoverRing = withAlpha(0x18181b, 110);
    theme.nodeBorder = QColor(0xd4d4d8);

    theme.accent = QColor(0x4d7c0f);

    theme.surface = QColor(0xf4f4f5);
    theme.surfaceRaised = QColor(0xffffff);
    theme.border = QColor(0xe4e4e7);
    theme.text = QColor(0x18181b);
    theme.textMuted = QColor(0x52525b);
    theme.nodeFill = QColor(0xffffff);
    theme.label = QColor(0x52525b);
    theme.ringStrong = QColor(0x4d7c0f);
    theme.ringMedium = QColor(0x71717a);
    theme.ringWeak = QColor(0xe11d48);
    theme.ringNew = QColor(0xa1a1aa);
    theme.ringTrack = QColor(0xe4e4e7);
    theme.danger = QColor(0xe11d48);
    return theme;
}

}  // namespace

const Theme& themeFor(ThemeMode mode) {
    static const Theme dark = makeDark();
    static const Theme light = makeLight();
    return mode == ThemeMode::Dark ? dark : light;
}

RingBand ringBand(double recall) {
    if (!(recall >= 0.0)) return RingBand::New;
    if (recall >= kStrongRecall) return RingBand::Strong;
    if (recall >= kMediumRecall) return RingBand::Medium;
    return RingBand::Weak;
}

QColor ringColor(const Theme& theme, double recall) {
    switch (ringBand(recall)) {
        case RingBand::Strong: return theme.ringStrong;
        case RingBand::Medium: return theme.ringMedium;
        case RingBand::Weak: return theme.ringWeak;
        case RingBand::New: return theme.ringNew;
    }
    return theme.ringNew;
}

}  // namespace atlas::render
