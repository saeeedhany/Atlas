#include <QRectF>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

const QByteArray kBoard = "import QtQuick\nimport Atlas.Ui\n"
                          "PanelLayer { width: 1200; height: 800\n"
                          "  Panel { objectName: \"today\"; panelId: \"today\"; title: \"Today\" }\n"
                          "  Panel { objectName: \"brain\"; panelId: \"brain\"; title: \"Brain\" }\n"
                          "  Panel { objectName: \"settings\"; panelId: \"settings\"; title: \"Settings\"; transient: true }\n"
                          "}";

QRectF targetOf(QObject* panel) { return panel->property("target").toRectF(); }

}  // namespace

TEST_CASE("docked panels stack at the top right and fold") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    auto layer = f.createFromData(kBoard);
    auto* today = QmlFixture::child(layer.get(), "today");
    auto* brain = QmlFixture::child(layer.get(), "brain");
    CHECK(targetOf(today) == QRectF(864, 16, 320, 240));
    CHECK(targetOf(brain).y() == doctest::Approx(266.0));
    REQUIRE(QMetaObject::invokeMethod(today, "fold"));
    CHECK(today->property("mode").toString() == "folded");
    CHECK(targetOf(today).height() == doctest::Approx(40.0));
    CHECK(targetOf(brain).y() == doctest::Approx(66.0));
}

TEST_CASE("a panel floats, expands, restores, and docks back") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    auto layer = f.createFromData(kBoard);
    auto* brain = QmlFixture::child(layer.get(), "brain");
    REQUIRE(QMetaObject::invokeMethod(brain, "floatAt", Q_ARG(double, 100.0), Q_ARG(double, 120.0)));
    CHECK(brain->property("mode").toString() == "floating");
    CHECK(targetOf(brain).topLeft() == QPointF(100, 120));
    REQUIRE(QMetaObject::invokeMethod(brain, "expand"));
    CHECK(targetOf(brain).width() == doctest::Approx(1104.0));
    REQUIRE(QMetaObject::invokeMethod(layer.get(), "closeExpanded"));
    CHECK(brain->property("mode").toString() == "floating");
    REQUIRE(QMetaObject::invokeMethod(brain, "dock"));
    CHECK(brain->property("mode").toString() == "docked");
}

TEST_CASE("transient panels only show while expanded") {
    QmlFixture f;
    auto layer = f.createFromData(kBoard);
    auto* settings = QmlFixture::child(layer.get(), "settings");
    CHECK(settings->property("mode").toString() == "hidden");
    CHECK_FALSE(settings->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(settings, "expand"));
    CHECK(settings->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(layer.get(), "closeExpanded"));
    CHECK(settings->property("mode").toString() == "hidden");
}

TEST_CASE("layouts survive a restart and come back inside the window") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    {
        auto layer = f.createFromData(kBoard);
        auto* today = QmlFixture::child(layer.get(), "today");
        REQUIRE(QMetaObject::invokeMethod(today, "floatAt", Q_ARG(double, 40.0), Q_ARG(double, 50.0)));
    }
    f.context().settings().setPanelLayout("brain", {{"mode", "floating"}, {"x", 5000.0}, {"y", 3000.0},
                                                    {"width", 300.0}, {"height", 200.0}});
    auto layer = f.createFromData(kBoard);
    auto* today = QmlFixture::child(layer.get(), "today");
    auto* brain = QmlFixture::child(layer.get(), "brain");
    CHECK(today->property("mode").toString() == "floating");
    CHECK(targetOf(today).topLeft() == QPointF(40, 50));
    QRectF placed = targetOf(brain);
    CHECK(placed.right() <= 1200.0);
    CHECK(placed.bottom() <= 800.0);
}
