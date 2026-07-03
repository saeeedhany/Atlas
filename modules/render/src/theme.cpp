#include "atlas/render/theme.hpp"

namespace atlas::render {

namespace {

// The four palette colors, named once so the two themes below read as
// "which role gets which palette color" rather than repeating hex
// literals.
constexpr QRgb kBlack  = 0x000000;
constexpr QRgb kBrown  = 0x1F150C;
constexpr QRgb kCoffee = 0x412D15;
constexpr QRgb kBeige  = 0xE1DCC9;

Theme makeDark() {
    Theme theme;
    theme.background = QColor(kBlack);
    // Dots are a texture, not a signal — kept low-alpha so they read
    // as "this is a canvas" without competing with nodes/edges.
    theme.dot            = QColor(kBeige);
    theme.dot.setAlpha(28);
    theme.edge            = QColor(kBeige);
    theme.edge.setAlpha(90);
    theme.edgeDimmed      = QColor(kBeige);
    theme.edgeDimmed.setAlpha(28);
    theme.selectedRing    = QColor(kBeige);
    theme.neighborRing    = QColor(kCoffee);
    theme.neighborRing.setAlpha(230);

    // Difficulty stays a semantic traffic-light-ish scale (see
    // graph_window.cpp) — kept saturated/bright since it sits on black.
    theme.nodeDifficulty = {
        QColor(100, 200, 120),  // Beginner
        QColor(100, 160, 220),  // Intermediate
        QColor(230, 170, 60),   // Advanced
        QColor(220, 90, 90),    // Expert
    };

    theme.panelBackground          = QColor(kBrown);
    theme.panelAlternateBackground = QColor(kBrown).lighter(130);
    theme.panelText                = QColor(kBeige);
    theme.panelBorder              = QColor(kCoffee);
    theme.accent                   = QColor(kBeige);
    return theme;
}

Theme makeLight() {
    Theme theme;
    theme.background = QColor(kBeige);
    theme.dot         = QColor(kBrown);
    theme.dot.setAlpha(40);
    theme.edge         = QColor(kBrown);
    theme.edge.setAlpha(110);
    theme.edgeDimmed   = QColor(kBrown);
    theme.edgeDimmed.setAlpha(35);
    theme.selectedRing = QColor(kBlack);
    theme.neighborRing = QColor(kCoffee);

    // Same semantic hues as dark, darkened/desaturated a bit so they
    // hold contrast against the light beige background instead of
    // washing out.
    theme.nodeDifficulty = {
        QColor(60, 140, 80),    // Beginner
        QColor(50, 100, 165),   // Intermediate
        QColor(180, 120, 20),   // Advanced
        QColor(175, 60, 60),    // Expert
    };

    theme.panelBackground          = QColor(kBeige);
    theme.panelAlternateBackground = QColor(kBeige).darker(107);
    theme.panelText                = QColor(kBlack);
    theme.panelBorder              = QColor(kCoffee);
    theme.accent                   = QColor(kCoffee);
    return theme;
}

}  // namespace

const Theme& themeFor(ThemeMode mode) {
    static const Theme dark  = makeDark();
    static const Theme light = makeLight();
    return mode == ThemeMode::Dark ? dark : light;
}

}  // namespace atlas::render
