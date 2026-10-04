#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>
#include <QTemporaryDir>

#include <cmath>
#include <memory>

#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "doctest.h"

using namespace atlas::viewmodels;
using atlas::render::RingBand;

TEST_CASE("settings persist and newPerDay is clamped") {
    QTemporaryDir dir;
    {
        QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
        AppSettings settings(store);
        CHECK(settings.darkTheme());
        CHECK_FALSE(settings.reducedMotion());
        CHECK(settings.newPerDay() == 5);
        settings.setDarkTheme(false);
        settings.setReducedMotion(true);
        settings.setNewPerDay(50);
        CHECK(settings.newPerDay() == 20);
        settings.setNewPerDay(0);
        CHECK(settings.newPerDay() == 1);
    }
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings reopened(store);
    CHECK_FALSE(reopened.darkTheme());
    CHECK(reopened.reducedMotion());
    CHECK(reopened.newPerDay() == 1);
}

TEST_CASE("the palette follows the theme setting") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings settings(store);
    Palette palette(settings);
    int changes = 0;
    QObject::connect(&palette, &Palette::changed, [&] { ++changes; });

    QColor darkBackground = palette.background();
    settings.setDarkTheme(false);
    CHECK(palette.background() != darkBackground);
    CHECK(palette.mode() == atlas::render::ThemeMode::Light);
    CHECK(changes == 1);
}

TEST_CASE("ring bands follow the recall thresholds") {
    CHECK(atlas::render::ringBand(-1.0) == RingBand::New);
    CHECK(atlas::render::ringBand(0.2) == RingBand::Weak);
    CHECK(atlas::render::ringBand(0.5) == RingBand::Medium);
    CHECK(atlas::render::ringBand(0.79) == RingBand::Medium);
    CHECK(atlas::render::ringBand(0.8) == RingBand::Strong);
    CHECK(atlas::render::ringBand(std::nan("")) == RingBand::New);
}

TEST_CASE("QML sees the provided settings and palette instances") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings settings(store);
    Palette palette(settings);
    settings.setNewPerDay(7);
    AppSettings::provide(&settings);
    Palette::provide(&palette);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport Atlas.ViewModels\n"
                      "QtObject { property int perDay: AppSettings.newPerDay; property color bg: Palette.background }",
                      QUrl());
    std::unique_ptr<QObject> object(component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(object != nullptr);
    CHECK(object->property("perDay").toInt() == 7);
    CHECK(object->property("bg").value<QColor>() == palette.background());
}
