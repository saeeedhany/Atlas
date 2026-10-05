#include "atlas/viewmodels/palette.hpp"

namespace atlas::viewmodels {

using atlas::render::ThemeMode;

namespace {

size_t noteIndex(const QString& color) {
    if (color == QLatin1String("olive")) return 1;
    if (color == QLatin1String("rose")) return 2;
    return 0;
}

}  // namespace

Palette::Palette(AppSettings& settings, QObject* parent) : QObject(parent), settings_(&settings) {
    connect(settings_, &AppSettings::darkThemeChanged, this, &Palette::changed);
}

ThemeMode Palette::mode() const { return settings_->darkTheme() ? ThemeMode::Dark : ThemeMode::Light; }

const atlas::render::Theme& Palette::theme() const { return atlas::render::themeFor(mode()); }

QColor Palette::ringColor(double recall) const { return atlas::render::ringColor(theme(), recall); }

QColor Palette::regionTint(int hue) const {
    const auto& hues = theme().regionHues;
    return hues[static_cast<size_t>(((hue % 6) + 6) % 6)];
}

QColor Palette::noteFill(const QString& color) const { return theme().noteFills[noteIndex(color)]; }
QColor Palette::noteText(const QString& color) const { return theme().noteTexts[noteIndex(color)]; }

}  // namespace atlas::viewmodels
