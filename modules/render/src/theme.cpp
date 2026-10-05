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
    theme.background = QColor(0x1d1c1a);
    theme.dot = withAlpha(0xe2ddd3, 10);
    theme.edge = QColor(0x57524a);
    theme.edgeDimmed = withAlpha(0x57524a, 90);
    theme.selectedRing = QColor(0xe2ddd3);
    theme.neighborRing = withAlpha(0xa39c8f, 160);
    theme.hoverRing = withAlpha(0xe2ddd3, 110);
    theme.nodeBorder = QColor(0x1d1c1a);
    theme.accent = QColor(0xd29b6c);
    theme.surface = QColor(0x262420);
    theme.surfaceRaised = QColor(0x2e2b26);
    theme.border = QColor(0x34312b);
    theme.text = QColor(0xe2ddd3);
    theme.textMuted = QColor(0xa39c8f);
    theme.nodeFill = QColor(0xc9c2b4);
    theme.label = QColor(0xa39c8f);
    theme.ringStrong = QColor(0xa3b18a);
    theme.ringMedium = QColor(0xc9c2b4);
    theme.ringWeak = QColor(0xc4806b);
    theme.ringNew = QColor(0x57524a);
    theme.ringTrack = QColor(0x34312b);
    theme.danger = QColor(0xd0705f);
    theme.outlineStrong = QColor(0x4b463d);
    theme.textFaint = QColor(0x8c8578);
    theme.secondary = QColor(0xa3b18a);
    theme.tertiary = QColor(0xc4806b);
    theme.onPrimary = QColor(0x1d1c1a);
    theme.link = QColor(0x57524a);
    theme.regionHues = {QColor(0xd29b6c), QColor(0xa3b18a), QColor(0xc4806b),
                        QColor(0xcbb489), QColor(0x8fa89a), QColor(0xb394a0)};
    theme.noteFills = {QColor(0x2e2a23), QColor(0x262b22), QColor(0x2f2422)};
    theme.noteTexts = {QColor(0xead9bf), QColor(0xd3dcc0), QColor(0xecc9bf)};
    return theme;
}

Theme makeLight() {
    Theme theme;
    theme.background = QColor(0xf6f1e7);
    theme.dot = withAlpha(0x2b2722, 14);
    theme.edge = QColor(0xc9bda8);
    theme.edgeDimmed = withAlpha(0xc9bda8, 90);
    theme.selectedRing = QColor(0x2b2722);
    theme.neighborRing = withAlpha(0x5e574c, 160);
    theme.hoverRing = withAlpha(0x2b2722, 110);
    theme.nodeBorder = QColor(0xf6f1e7);
    theme.accent = QColor(0xb8784a);
    theme.surface = QColor(0xfffaf2);
    theme.surfaceRaised = QColor(0xefe7da);
    theme.border = QColor(0xe6dccb);
    theme.text = QColor(0x2b2722);
    theme.textMuted = QColor(0x5e574c);
    theme.nodeFill = QColor(0x6b6357);
    theme.label = QColor(0x5e574c);
    theme.ringStrong = QColor(0x7d8c62);
    theme.ringMedium = QColor(0x6b6357);
    theme.ringWeak = QColor(0xa8583f);
    theme.ringNew = QColor(0xc9bda8);
    theme.ringTrack = QColor(0xe6dccb);
    theme.danger = QColor(0xb3483a);
    theme.outlineStrong = QColor(0xd3c6b1);
    theme.textFaint = QColor(0x8a8070);
    theme.secondary = QColor(0x7d8c62);
    theme.tertiary = QColor(0xa8583f);
    theme.onPrimary = QColor(0xfffaf2);
    theme.link = QColor(0xc9bda8);
    theme.regionHues = {QColor(0xb8784a), QColor(0x7d8c62), QColor(0xa8583f),
                        QColor(0x9c8550), QColor(0x5f7a6b), QColor(0x86677a)};
    theme.noteFills = {QColor(0xf3e2cc), QColor(0xe4ead6), QColor(0xf1d9d0)};
    theme.noteTexts = {QColor(0x4a3622), QColor(0x3b4628), QColor(0x5a2f24)};
    return theme;
}

}

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

}
