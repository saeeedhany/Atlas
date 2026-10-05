#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QTest>
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

QQuickWindow* activated(QObject* window) {
    auto* quick = qobject_cast<QQuickWindow*>(window);
    REQUIRE(quick != nullptr);
    quick->requestActivate();
    REQUIRE(QTest::qWaitForWindowActive(quick));
    return quick;
}

QPointF centerInScene(QObject* object) {
    auto* item = qobject_cast<QQuickItem*>(object);
    REQUIRE(item != nullptr);
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
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

TEST_CASE("region names recolor with the theme") {
    QmlFixture f;
    QString os = f.context().topics().createTopic("Operating Systems");
    f.context().map().setTopicId(os);
    f.context().map().createConcept("Paging");
    f.context().map().setTopicId(QString());
    auto window = f.create("Main");
    QmlFixture::settle();
    auto labels = QmlFixture::child(window.get(), "regionLabels")->findChildren<QObject*>("regionLabel");
    REQUIRE(labels.size() == 1);
    QObject* name = nullptr;
    for (QObject* child : labels[0]->findChildren<QObject*>())
        if (child->property("text").toString() == "Operating Systems") name = child;
    REQUIRE(name != nullptr);
    QColor dark = name->property("color").value<QColor>();
    REQUIRE(dark.isValid());
    f.context().settings().setDarkTheme(false);
    QColor light = name->property("color").value<QColor>();
    CHECK(light != dark);
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

TEST_CASE("Escape restores an expanded panel, then an expanded card, then clears the selection") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    QString id = f.context().map().createConcept("Recursion");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* quick = activated(window.get());
    f.context().map().setSelectedId(QString());
    f.context().map().setSelectedId(id);
    QmlFixture::settle();
    REQUIRE(f.context().conceptEditor().exists());
    auto* settings = QmlFixture::child(window.get(), "settingsPanel");
    auto* card = QmlFixture::child(window.get(), "conceptCard");
    REQUIRE(QMetaObject::invokeMethod(card, "expand"));
    REQUIRE(QMetaObject::invokeMethod(window.get(), "openSettings"));
    REQUIRE(settings->property("mode").toString() == "expanded");

    QTest::keyClick(quick, Qt::Key_Escape);
    CHECK(settings->property("mode").toString() == "hidden");
    REQUIRE(QMetaObject::invokeMethod(card, "expand"));
    QTest::keyClick(quick, Qt::Key_Escape);
    CHECK_FALSE(card->property("expanded").toBool());
    CHECK(f.context().map().selectedId() == id);
    QTest::keyClick(quick, Qt::Key_Escape);
    CHECK(f.context().map().selectedId().isEmpty());
}

TEST_CASE("an expanded card sits above the corner stack") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    QString id = f.context().map().createConcept("Recursion");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* quick = activated(window.get());
    f.context().map().setSelectedId(QString());
    f.context().map().setSelectedId(id);
    QmlFixture::settle();
    REQUIRE(f.context().conceptEditor().exists());
    auto* card = QmlFixture::child(window.get(), "conceptCard");
    REQUIRE(QMetaObject::invokeMethod(card, "expand"));
    QmlFixture::settle();
    auto* toggle = QmlFixture::child(card, "cardExpandButton");
    auto* layer = qobject_cast<QQuickItem*>(QmlFixture::child(window.get(), "panelLayer"));
    QVariantList occupied = layer->property("occupied").toList();
    REQUIRE(occupied.size() == 1);
    QRectF today = layer->mapRectToScene(occupied[0].toRectF());
    REQUIRE(waitFor([&] { return today.contains(centerInScene(toggle)); }));
    QPointF at = centerInScene(toggle);
    QTest::mouseClick(quick, Qt::LeftButton, {}, at.toPoint());
    QmlFixture::settle();
    CHECK_FALSE(card->property("expanded").toBool());
    CHECK(layer->property("occupied").toList()[0].toRectF() == occupied[0].toRectF());
}

TEST_CASE("a card beside a node keeps clear of the corner stack") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* overlay = QmlFixture::child(window.get(), "mapOverlay");
    auto* layer = QmlFixture::child(window.get(), "panelLayer");
    QVariantList obstacles = layer->property("occupied").toList();
    REQUIRE(obstacles.size() == 1);
    QRectF today = obstacles[0].toRectF();
    double width = overlay->property("width").toDouble();
    QVariant spot;
    REQUIRE(QMetaObject::invokeMethod(overlay, "cardSpot", Q_RETURN_ARG(QVariant, spot),
                                      Q_ARG(QVariant, QVariant::fromValue(QPointF(width - 420, 60))),
                                      Q_ARG(QVariant, 380), Q_ARG(QVariant, 500)));
    CHECK_FALSE(spot.toRectF().intersects(today));
}

TEST_CASE("unsaved note text is saved before the window closes and before a session") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    f.context().map().createConcept("Recursion");
    QString id = f.context().notes().createNote(0, 0, "");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* body = QmlFixture::child(QmlFixture::child(window.get(), "stickyNote"), "noteBody");
    body->setProperty("text", "Before a session");
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    CHECK(f.context().notes().note(id).value("body").toString() == "Before a session");

    f.context().session().quit();
    f.context().session().finish();
    QmlFixture::settle();
    body->setProperty("text", "Before closing");
    auto* quick = qobject_cast<QQuickWindow*>(window.get());
    REQUIRE(quick != nullptr);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    quick->close();
    CHECK(f.context().notes().note(id).value("body").toString() == "Before closing");
}

TEST_CASE("the top bar is slim and its chevron opens the menu") {
    QmlFixture f;
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* bar = QmlFixture::child(window.get(), "topBar");
    CHECK(bar->property("height").toDouble() == doctest::Approx(44.0));
    auto* menu = QmlFixture::child(bar, "topMenu");
    CHECK_FALSE(menu->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(bar, "menuButton"), "clicked"));
    QmlFixture::settle();
    CHECK(menu->property("visible").toBool());
}

TEST_CASE("theme text roles follow the palette in both themes") {
    QmlFixture f;
    auto probe = f.createFromData("import QtQuick\nimport Atlas.Ui\n"
                                  "QtObject { property color text: Theme.onSurface; property color onAccent: Theme.onPrimary }");
    for (bool dark : {true, false}) {
        f.context().settings().setDarkTheme(dark);
        CHECK(probe->property("text").value<QColor>() == f.context().palette().text());
        CHECK(probe->property("onAccent").value<QColor>() == f.context().palette().onPrimary());
    }
}

TEST_CASE("Escape closes an open menu before anything else") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    QString id = f.context().map().createConcept("Recursion");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* quick = activated(window.get());
    REQUIRE(f.context().map().selectedId() == id);
    auto* menu = QmlFixture::child(window.get(), "topMenu");
    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(window.get(), "topBar"), "openMenu"));
    QmlFixture::settle();
    REQUIRE(menu->property("visible").toBool());
    QTest::keyClick(quick, Qt::Key_Escape);
    QmlFixture::settle();
    CHECK(waitFor([&] { return !menu->property("visible").toBool(); }));
    CHECK(f.context().map().selectedId() == id);
    QTest::keyClick(quick, Qt::Key_Escape);
    CHECK(f.context().map().selectedId().isEmpty());
}
