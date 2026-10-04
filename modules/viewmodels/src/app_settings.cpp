#include "atlas/viewmodels/app_settings.hpp"

#include <algorithm>

#include "atlas/core/uuid.hpp"

namespace atlas::viewmodels {

namespace {
constexpr auto kDarkThemeKey = "appearance/darkTheme";
constexpr auto kReducedMotionKey = "appearance/reducedMotion";
constexpr auto kNewPerDayKey = "learning/newPerDay";
constexpr auto kDeviceIdKey = "device/id";
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

QString AppSettings::deviceId() const {
    QString id = store_->value(kDeviceIdKey).toString();
    if (id.isEmpty()) {
        id = QString::fromStdString(atlas::core::Uuid::generate().toString());
        store_->setValue(kDeviceIdKey, id);
    }
    return id;
}

}  // namespace atlas::viewmodels
