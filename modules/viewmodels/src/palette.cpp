#include "atlas/viewmodels/palette.hpp"

namespace atlas::viewmodels {

using atlas::render::ThemeMode;

Palette::Palette(AppSettings& settings, QObject* parent) : QObject(parent), settings_(&settings) {
    connect(settings_, &AppSettings::darkThemeChanged, this, &Palette::changed);
}

ThemeMode Palette::mode() const { return settings_->darkTheme() ? ThemeMode::Dark : ThemeMode::Light; }

const atlas::render::Theme& Palette::theme() const { return atlas::render::themeFor(mode()); }

QColor Palette::ringColor(double recall) const { return atlas::render::ringColor(theme(), recall); }

}  // namespace atlas::viewmodels
