#include "doctest.h"
#include "qml_fixture.hpp"

#include <QQuickItem>

TEST_CASE("settings controls write and follow the app settings") {
    QmlFixture f;
    auto screen = f.create("SettingsScreen");

    auto* dark = QmlFixture::child(screen.get(), "darkThemeSwitch");
    CHECK(dark->property("checked").toBool());
    REQUIRE(QMetaObject::invokeMethod(dark, "toggle"));
    REQUIRE(QMetaObject::invokeMethod(dark, "toggled"));
    CHECK_FALSE(f.context().settings().darkTheme());
    f.context().settings().setDarkTheme(true);
    CHECK(dark->property("checked").toBool());

    auto* motion = QmlFixture::child(screen.get(), "reducedMotionSwitch");
    REQUIRE(QMetaObject::invokeMethod(motion, "toggle"));
    REQUIRE(QMetaObject::invokeMethod(motion, "toggled"));
    CHECK(f.context().settings().reducedMotion());

    auto* perDay = QmlFixture::child(screen.get(), "newPerDayBox");
    CHECK(perDay->property("value").toInt() == 5);
    perDay->setProperty("value", 8);
    REQUIRE(QMetaObject::invokeMethod(perDay, "valueModified"));
    CHECK(f.context().settings().newPerDay() == 8);
}

TEST_CASE("settings switches and the spin box share the right edge") {
    QmlFixture f;
    auto screen = f.create("SettingsScreen", {{"width", 900}, {"height", 700}});
    QmlFixture::settle();

    auto rightEdge = [&](const char* name) {
        auto* item = qobject_cast<QQuickItem*>(QmlFixture::child(screen.get(), name));
        REQUIRE(item != nullptr);
        return item->mapToItem(qobject_cast<QQuickItem*>(screen.get()), QPointF(item->width(), 0)).x();
    };

    const double spin = rightEdge("newPerDayBox");
    CHECK(rightEdge("darkThemeSwitch") == doctest::Approx(spin));
    CHECK(rightEdge("reducedMotionSwitch") == doctest::Approx(spin));
}

TEST_CASE("the theme caption names warm stone") {
    QmlFixture f;
    auto screen = f.create("SettingsScreen");
    bool found = false;
    for (QObject* child : screen->findChildren<QObject*>())
        found = found || child->property("text").toString() == "Warm stone, or its light paper twin.";
    CHECK(found);
}
