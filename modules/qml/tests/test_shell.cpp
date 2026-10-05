#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <functional>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

bool waitFor(const std::function<bool()>& done) {
    QElapsedTimer clock;
    clock.start();
    while (!done() && clock.elapsed() < 2000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

}  // namespace

TEST_CASE("first run with an empty map says so") {
    QmlFixture f;
    auto window = f.create("Main");
    CHECK(window->property("mode").toString() == "board");
    auto* today = QmlFixture::child(window.get(), "todayPanel");
    CHECK(today->property("headline").toString() == "Your map is empty");
    CHECK(today->property("actionText").toString() == "Add your first concept");
    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    CHECK(window->property("mode").toString() == "board");
}

TEST_CASE("Today and the recall button count the work waiting") {
    QmlFixture f;
    f.context().map().createConcept("Recursion");
    f.context().map().createConcept("Stack");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* today = QmlFixture::child(window.get(), "todayPanel");
    CHECK(today->property("headline").toString().startsWith("2 items"));
    CHECK(today->property("actionText").toString() == "Start session");
    CHECK(QmlFixture::child(window.get(), "recallButton")->property("text").toString() == "Recall 2");
}

TEST_CASE("the recall button starts a session and locks the top bar") {
    QmlFixture f;
    f.context().map().createConcept("Recursion");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* bar = QmlFixture::child(window.get(), "topBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "recallRequested"));
    CHECK(window->property("mode").toString() == "session");
    CHECK_FALSE(bar->property("enabled").toBool());
}

TEST_CASE("settings open as an expanded panel") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* settings = QmlFixture::child(window.get(), "settingsPanel");
    CHECK(settings->property("mode").toString() == "hidden");
    REQUIRE(QMetaObject::invokeMethod(window.get(), "openSettings"));
    CHECK(settings->property("mode").toString() == "expanded");
    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(window.get(), "panelLayer"), "closeExpanded"));
    CHECK(settings->property("mode").toString() == "hidden");
}

TEST_CASE("topics show as labeled regions on the board") {
    QmlFixture f;
    QString os = f.context().topics().createTopic("Operating Systems");
    f.context().map().setTopicId(os);
    f.context().map().createConcept("Paging");
    f.context().map().setTopicId(QString());
    auto window = f.create("Main");
    QmlFixture::settle();
    auto labels = QmlFixture::child(window.get(), "regionLabels")->findChildren<QObject*>("regionLabel");
    REQUIRE(labels.size() == 1);
    CHECK(labels[0]->property("topicName").toString() == "Operating Systems");
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

TEST_CASE("panels step aside during a session and come back after") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    f.context().map().createConcept("Recursion");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* layer = QmlFixture::child(window.get(), "panelLayer");
    auto* settings = QmlFixture::child(window.get(), "settingsPanel");
    REQUIRE(QMetaObject::invokeMethod(window.get(), "openSettings"));
    REQUIRE(settings->property("mode").toString() == "expanded");

    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    QmlFixture::settle();
    CHECK(window->property("mode").toString() == "session");
    CHECK(settings->property("mode").toString() == "hidden");
    CHECK_FALSE(layer->property("enabled").toBool());
    CHECK(waitFor([&] { return !layer->property("visible").toBool(); }));

    f.context().session().quit();
    f.context().session().finish();
    QmlFixture::settle();
    CHECK(window->property("mode").toString() == "board");
    CHECK(layer->property("enabled").toBool());
    CHECK(waitFor([&] { return layer->property("opacity").toDouble() == 1.0; }));
    CHECK(layer->property("visible").toBool());
}

TEST_CASE("exploring the map from Today fits the board") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    f.context().map().createConcept("Recursion");
    f.context().map().createConcept("Stack");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* canvas = QmlFixture::child(window.get(), "graphCanvas");
    canvas->setProperty("zoom", 6.0);
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(window.get(), "todayPanel"), "actionTriggered",
                                      Q_ARG(QString, "map")));
    QmlFixture::settle();
    CHECK(canvas->property("zoom").toDouble() <= 2.5);
}
