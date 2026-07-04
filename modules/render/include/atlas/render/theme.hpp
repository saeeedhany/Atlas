#pragma once

#include <QColor>
#include <array>

namespace atlas::render {

// Two modes only, for now — this isn't a general theming system, just
// enough to stop every color being a magic literal scattered across
// GraphCanvasItem and graph_window.cpp. Dark is the default (see
// MainWindow::MainWindow / QSettings key "theme").
enum class ThemeMode {
    Dark,
    Light,
};

// Every color the canvas and the Qt Widgets shell need, derived from
// one fixed four-color palette (Black #000000, Brown #1F150C, Coffee
// #412D15, Beige #E1DCC9) rather than each surface inventing its own.
// Difficulty colors are the one deliberate exception: they're a
// semantic mapping (see graph_window.cpp), not decoration, so they
// don't come from the four-color palette — only their contrast is
// tuned per mode.
struct Theme {
    // --- Canvas ---
    QColor background;
    QColor dot;
    QColor edge;
    QColor edgeDimmed;
    QColor selectedRing;
    QColor neighborRing;
    QColor hoverRing;
    QColor nodeBorder;
    std::array<QColor, 4> nodeDifficulty;  // indexed by atlas::core::Difficulty

    // --- Qt Widgets shell (panel, buttons, list) ---
    QColor panelBackground;
    QColor panelAlternateBackground;
    QColor panelText;
    QColor panelBorder;
    QColor accent;
};

// Returns a reference to a static, immutable Theme — cheap to call
// repeatedly (e.g. once per refreshGraph()), no allocation.
const Theme& themeFor(ThemeMode mode);

}  // namespace atlas::render
