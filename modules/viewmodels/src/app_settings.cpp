#include "atlas/viewmodels/app_settings.hpp"

#include <algorithm>

namespace atlas::viewmodels {

namespace {
constexpr auto kDarkThemeKey = "appearance/darkTheme";
constexpr auto kReducedMotionKey = "appearance/reducedMotion";
constexpr auto kNewPerDayKey = "learning/newPerDay";
}  // namespace

AppSettings::AppSettings(QSettings& store, QObject* parent) : QObject(parent), store_(&store) {}

bool AppSettings::darkTheme() const { return store_->value(kDarkThemeKey, true).toBool(); }

void AppSettings::setDarkTheme(bool dark) {
    if (dark == darkTheme()) return;
    store_->setValue(kDarkThemeKey, dark);
    emit darkThemeChanged();
}

bool AppSettings::reducedMotion() const { return store_->value(kReducedMotionKey, false).toBool(); }

void AppSettings::setReducedMotion(bool reduced) {
    if (reduced == reducedMotion()) return;
    store_->setValue(kReducedMotionKey, reduced);
    emit reducedMotionChanged();
}

int AppSettings::newPerDay() const {
    return std::clamp(store_->value(kNewPerDayKey, kDefaultNewPerDay).toInt(), kMinNewPerDay, kMaxNewPerDay);
}

void AppSettings::setNewPerDay(int count) {
    int clamped = std::clamp(count, kMinNewPerDay, kMaxNewPerDay);
    if (clamped == newPerDay()) return;
    store_->setValue(kNewPerDayKey, clamped);
    emit newPerDayChanged();
}

}  // namespace atlas::viewmodels
