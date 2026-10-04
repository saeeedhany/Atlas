#include <QColor>
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>

#include <memory>

#include "doctest.h"
#include "qml_fixture.hpp"

TEST_CASE("Theme follows the palette and recolors when the theme switches") {
    QmlFixture f;
    auto probe = f.createFromData("import QtQuick\nimport Atlas.Ui\n"
                                  "QtObject { property color background: Theme.background\n"
                                  "           property color weak: Theme.ringColor(0.3)\n"
                                  "           property color fresh: Theme.ringColor(-1) }");
    CHECK(probe->property("background").value<QColor>() == f.context().palette().background());
    CHECK(probe->property("weak").value<QColor>() == f.context().palette().ringWeak());
    CHECK(probe->property("fresh").value<QColor>() == f.context().palette().ringNew());
    f.context().settings().setDarkTheme(false);
    CHECK(probe->property("background").value<QColor>() == f.context().palette().background());
}

TEST_CASE("reduced motion zeroes every movement duration") {
    QmlFixture f;
    auto probe = f.createFromData("import QtQuick\nimport Atlas.Ui\n"
                                  "QtObject { property int normal: Motion.normal\n"
                                  "           property int decay: Motion.decay\n"
                                  "           property int appear: Motion.appear }");
    CHECK(probe->property("normal").toInt() == 240);
    CHECK(probe->property("decay").toInt() == 1200);
    f.context().settings().setReducedMotion(true);
    CHECK(probe->property("normal").toInt() == 0);
    CHECK(probe->property("decay").toInt() == 0);
    CHECK(probe->property("appear").toInt() == 120);
}

TEST_CASE("a ring's sweep is its recall, and it eases unless motion is reduced") {
    QmlFixture f;
    auto ring = f.create("Ring", {{"recall", 0.5}});
    CHECK(ring->property("sweep").toDouble() == doctest::Approx(180.0));
    CHECK(ring->property("shownSweep").toDouble() == doctest::Approx(180.0));

    ring->setProperty("recall", 0.25);
    CHECK(ring->property("sweep").toDouble() == doctest::Approx(90.0));
    CHECK(ring->property("shownSweep").toDouble() > 90.0);

    f.context().settings().setReducedMotion(true);
    ring->setProperty("recall", 1.0);
    CHECK(ring->property("shownSweep").toDouble() == doctest::Approx(360.0));

    ring->setProperty("recall", -1.0);
    CHECK(ring->property("sweep").toDouble() == doctest::Approx(360.0));
    CHECK_FALSE(ring->property("learned").toBool());
}

TEST_CASE("the startup error window shows its message") {
    QmlFixture f;
    auto window = f.create("StartupError", {{"message", "The database is locked"}});
    CHECK(QmlFixture::child(window.get(), "startupMessage")->property("text").toString().contains("locked"));
}

TEST_CASE("the startup error window loads in a bare engine without singletons") {
    QQmlEngine engine;
    QStringList warnings;
    engine.setOutputWarningsToStandardError(false);
    QObject::connect(&engine, &QQmlEngine::warnings, [&warnings](const QList<QQmlError>& list) {
        for (const auto& warning : list) warnings.append(warning.toString());
    });
    QQmlComponent component(&engine, "Atlas.Ui", "StartupError");
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> window(component.createWithInitialProperties({{"message", "The database is locked"}}));
    REQUIRE(window != nullptr);
    QCoreApplication::processEvents();
    INFO(warnings.join('\n').toStdString());
    CHECK(warnings.isEmpty());
}
