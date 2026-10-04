#pragma once

#include <QColor>

#include <array>

namespace atlas::render {

enum class ThemeMode {
    Dark,
    Light,
};

struct Theme {
    QColor background;
    QColor dot;
    QColor edge;
    QColor edgeDimmed;
    QColor selectedRing;
    QColor neighborRing;
    QColor hoverRing;
    QColor nodeBorder;
    std::array<QColor, 4> nodeDifficulty;

    QColor panelBackground;
    QColor panelAlternateBackground;
    QColor panelText;
    QColor panelBorder;
    QColor accent;

    QColor surface;
    QColor surfaceRaised;
    QColor border;
    QColor text;
    QColor textMuted;
    QColor nodeFill;
    QColor label;
    QColor ringStrong;
    QColor ringMedium;
    QColor ringWeak;
    QColor ringNew;
    QColor ringTrack;
    QColor danger;
};

const Theme& themeFor(ThemeMode mode);

enum class RingBand { New, Weak, Medium, Strong };

inline constexpr double kStrongRecall = 0.80;
inline constexpr double kMediumRecall = 0.50;

RingBand ringBand(double recall);
QColor ringColor(const Theme& theme, double recall);

}  // namespace atlas::render
