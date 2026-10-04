#include <QColor>

#include "doctest.h"
#include "qml_fixture.hpp"

TEST_CASE("first run with an empty map says so and leads to the map") {
    QmlFixture f;
    auto window = f.create("Main");
    CHECK(window->property("page").toString() == "today");
    auto* today = QmlFixture::child(window.get(), "todayOverlay");
    CHECK(today->property("headline").toString() == "Your map is empty");
    CHECK(today->property("actionText").toString() == "Add your first concept");

    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    CHECK(window->property("page").toString() == "map");
}

TEST_CASE("Today counts the work waiting") {
    QmlFixture f;
    f.context().map().createConcept("Recursion");
    f.context().map().createConcept("Stack");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* today = QmlFixture::child(window.get(), "todayOverlay");
    CHECK(today->property("headline").toString().startsWith("2 items"));
    CHECK(today->property("actionText").toString() == "Open the map");
}

TEST_CASE("switching the theme recolors the window at once") {
    QmlFixture f;
    auto window = f.create("Main");
    QColor dark = window->property("color").value<QColor>();
    f.context().settings().setDarkTheme(false);
    QColor light = window->property("color").value<QColor>();
    CHECK(light != dark);
    CHECK(light == f.context().palette().background());
}

TEST_CASE("reduced motion turns off canvas camera animation") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* canvas = QmlFixture::child(window.get(), "graphCanvas");
    CHECK(canvas->property("animated").toBool());
    f.context().settings().setReducedMotion(true);
    CHECK_FALSE(canvas->property("animated").toBool());
}

TEST_CASE("view model errors reach the toast") {
    QmlFixture f;
    auto window = f.create("Main");
    emit f.context().topics().errorOccurred("Could not rename the topic");
    auto* toast = QmlFixture::child(window.get(), "toast");
    CHECK(toast->property("shown").toBool());
    CHECK(toast->property("text").toString() == "Could not rename the topic");
}

TEST_CASE("the rail switches pages") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* rail = QmlFixture::child(window.get(), "navRail");
    REQUIRE(QMetaObject::invokeMethod(rail, "selected", Q_ARG(QString, "settings")));
    CHECK(window->property("page").toString() == "settings");
    CHECK(rail->property("current").toString() == "settings");
}
