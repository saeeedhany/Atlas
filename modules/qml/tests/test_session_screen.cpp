#include <QQuickItem>
#include <QVariantMap>
#include <chrono>

#include "atlas/core/memory.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "doctest.h"
#include "qml_fixture.hpp"

using namespace atlas::core;
using namespace atlas::viewmodels;

namespace {

void introduce(QmlFixture& f, const QString& id) {
    ReviewEvent event;
    event.id = Uuid::generate();
    event.item = ItemRef::forConcept(*parseId<KnowledgeObjectId>(id));
    event.sessionId = Uuid::generate();
    event.deviceId = "test";
    event.reviewedAt = std::chrono::system_clock::now() - std::chrono::hours(1);
    event.grade = Grade::Good;
    REQUIRE(f.context().memory().record({event}).hasValue());
}

}  // namespace

TEST_CASE("Today starts a session and the explain step finishes it") {
    QmlFixture f;
    f.context().map().createConcept("Tree");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* today = QmlFixture::child(window.get(), "todayPanel");
    CHECK(today->property("actionText").toString() == "Start session");

    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    CHECK(window->property("mode").toString() == "session");
    CHECK(f.context().session().stage() == "explain");
    CHECK_FALSE(QmlFixture::child(window.get(), "topBar")->property("enabled").toBool());

    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    REQUIRE(QMetaObject::invokeMethod(screen, "reveal"));
    QmlFixture::child(window.get(), "writtenKey")->setProperty("text", "A branching structure");
    REQUIRE(QMetaObject::invokeMethod(screen, "grade", Q_ARG(int, 3)));
    CHECK(f.context().session().stage() == "summary");

    REQUIRE(QMetaObject::invokeMethod(screen, "finish"));
    CHECK(f.context().session().stage() == "idle");
    CHECK(window->property("mode").toString() == "board");
}

TEST_CASE("the rebuild card names concepts and checks them") {
    QmlFixture f;
    QString alpha = f.context().map().createConcept("Alpha");
    QString beta = f.context().map().createConcept("Beta");
    introduce(f, alpha);
    introduce(f, beta);
    f.context()
        .workspace()
        .createRelationship(*parseId<KnowledgeObjectId>(alpha), *parseId<KnowledgeObjectId>(beta),
                            RelationshipType::DependsOn, std::nullopt)
        .value();
    auto window = f.create("Main");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    CHECK(f.context().session().stage() == "rebuild");

    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    REQUIRE(QMetaObject::invokeMethod(screen, "addCandidate", Q_ARG(QString, beta)));
    CHECK(f.context().session().recalled().size() == 1);
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    REQUIRE(QMetaObject::invokeMethod(screen, "submitRebuild"));
    CHECK(f.context().session().stage() == "feedback");
    CHECK(f.context().session().feedback()[0].toMap().value("outcome").toString() == "recalled");
}

TEST_CASE("starting a session saves a valid draft and selects the focus") {
    QmlFixture f;
    QString alpha = f.context().map().createConcept("Alpha");
    f.context().map().createConcept("Beta");
    auto window = f.create("Main");
    QmlFixture::settle();
    f.context().map().setSelectedId(alpha);
    QmlFixture::settle();
    f.context().conceptEditor().setNotes("draft");
    REQUIRE(f.context().conceptEditor().dirty());

    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    QmlFixture::settle();
    CHECK_FALSE(f.context().conceptEditor().dirty());
    CHECK(f.context().workspace().findKnowledgeObject(*parseId<KnowledgeObjectId>(alpha))->notes() == "draft");
    CHECK(f.context().session().stage() != "idle");
    CHECK(f.context().map().selectedId() == f.context().session().focusId());
}

TEST_CASE("an invalid draft keeps the session from starting") {
    QmlFixture f;
    QString alpha = f.context().map().createConcept("Alpha");
    f.context().map().createConcept("Beta");
    auto window = f.create("Main");
    QmlFixture::settle();
    f.context().map().setSelectedId(alpha);
    QmlFixture::settle();
    f.context().conceptEditor().setTitle("");
    REQUIRE(f.context().conceptEditor().dirty());

    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    QmlFixture::settle();
    CHECK(f.context().session().stage() == "idle");
    CHECK(window->property("mode").toString() == "board");
    CHECK(QmlFixture::child(window.get(), "toast")->property("shown").toBool());
}

TEST_CASE("ending a session before any focus is done returns to the map") {
    QmlFixture f;
    f.context().map().createConcept("Tree");
    auto window = f.create("Main");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    REQUIRE(window->property("mode").toString() == "session");

    f.context().session().quit();
    QmlFixture::settle();
    CHECK(f.context().session().stage() == "idle");
    CHECK(window->property("mode").toString() == "board");
}

TEST_CASE("the explain prediction is fixed once the answer is revealed") {
    QmlFixture f;
    f.context().map().createConcept("Tree");
    auto window = f.create("Main");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    REQUIRE(f.context().session().stage() == "explain");

    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    auto* picker = QmlFixture::child(window.get(), "explainCertainty");
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 2)));
    CHECK(picker->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(screen, "reveal"));
    CHECK_FALSE(picker->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 4)));
    CHECK(screen->property("certainty").toInt() == 2);
}

TEST_CASE("the rebuild prediction is fixed once a hint is taken") {
    QmlFixture f;
    QString alpha = f.context().map().createConcept("Alpha");
    QString beta = f.context().map().createConcept("Beta");
    introduce(f, alpha);
    introduce(f, beta);
    f.context()
        .workspace()
        .createRelationship(*parseId<KnowledgeObjectId>(alpha), *parseId<KnowledgeObjectId>(beta),
                            RelationshipType::DependsOn, std::nullopt)
        .value();
    auto window = f.create("Main");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    REQUIRE(f.context().session().stage() == "rebuild");

    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    auto* hint = QmlFixture::child(window.get(), "hintButton");
    CHECK_FALSE(hint->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    CHECK(hint->property("enabled").toBool());
    f.context().session().hint();
    QmlFixture::settle();
    CHECK_FALSE(QmlFixture::child(window.get(), "rebuildCertainty")->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 1)));
    CHECK(screen->property("certainty").toInt() == 3);
}

TEST_CASE("a new session starts with a clean screen") {
    QmlFixture f;
    f.context().map().createConcept("Tree");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    REQUIRE(QMetaObject::invokeMethod(screen, "reveal"));
    f.context().session().quit();
    REQUIRE(QMetaObject::invokeMethod(screen, "finish"));

    f.context().map().createConcept("Graph");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    CHECK(f.context().session().stage() != "idle");
    CHECK(screen->property("certainty").toInt() == 0);
    CHECK_FALSE(screen->property("revealed").toBool());
}

TEST_CASE("a session started from one topic centers its first focus") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    auto os = f.context().topics().createTopic("OS");
    f.context().map().setTopicId(os);
    f.context().map().createConcept("Paging");
    f.context().map().createConcept("Segmentation");
    f.context().map().createConcept("Scheduling");
    auto window = f.create("Main");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));
    QmlFixture::settle();
    REQUIRE(f.context().session().stage() != "idle");

    auto* canvas = qobject_cast<QQuickItem*>(QmlFixture::child(window.get(), "graphCanvas"));
    REQUIRE(canvas != nullptr);
    QVariant position;
    REQUIRE(QMetaObject::invokeMethod(canvas, "screenPositionOf", Q_RETURN_ARG(QVariant, position),
                                      Q_ARG(QString, f.context().session().focusId())));
    CHECK(position.toPointF().x() == doctest::Approx(canvas->width() / 2.0));
    CHECK(position.toPointF().y() == doctest::Approx(canvas->height() / 2.0));
}
