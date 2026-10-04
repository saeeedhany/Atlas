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
    auto* today = QmlFixture::child(window.get(), "todayOverlay");
    CHECK(today->property("actionText").toString() == "Start session");

    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    CHECK(window->property("page").toString() == "session");
    CHECK(f.context().session().stage() == "explain");
    CHECK_FALSE(QmlFixture::child(window.get(), "navRail")->property("enabled").toBool());

    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    REQUIRE(QMetaObject::invokeMethod(screen, "reveal"));
    QmlFixture::child(window.get(), "writtenKey")->setProperty("text", "A branching structure");
    REQUIRE(QMetaObject::invokeMethod(screen, "grade", Q_ARG(int, 3)));
    CHECK(f.context().session().stage() == "summary");

    REQUIRE(QMetaObject::invokeMethod(screen, "finish"));
    CHECK(f.context().session().stage() == "idle");
    CHECK(window->property("page").toString() == "map");
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
    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(window.get(), "todayOverlay"), "act"));
    CHECK(f.context().session().stage() == "rebuild");

    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    REQUIRE(QMetaObject::invokeMethod(screen, "addCandidate", Q_ARG(QString, beta)));
    CHECK(f.context().session().recalled().size() == 1);
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    REQUIRE(QMetaObject::invokeMethod(screen, "submitRebuild"));
    CHECK(f.context().session().stage() == "feedback");
    CHECK(f.context().session().feedback()[0].toMap().value("outcome").toString() == "recalled");
}

TEST_CASE("starting a session keeps the focus selected over an unsaved draft") {
    QmlFixture f;
    QString alpha = f.context().map().createConcept("Alpha");
    f.context().map().createConcept("Beta");
    auto window = f.create("Main");
    QmlFixture::settle();
    f.context().map().setSelectedId(alpha);
    QmlFixture::settle();
    f.context().conceptEditor().setTitle("");
    REQUIRE(f.context().conceptEditor().dirty());

    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(window.get(), "todayOverlay"), "act"));
    QmlFixture::settle();
    CHECK(f.context().session().stage() != "idle");
    CHECK(f.context().map().selectedId() == f.context().session().focusId());
}

TEST_CASE("a new session starts with a clean screen") {
    QmlFixture f;
    f.context().map().createConcept("Tree");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* today = QmlFixture::child(window.get(), "todayOverlay");
    auto* screen = QmlFixture::child(window.get(), "sessionScreen");
    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    REQUIRE(QMetaObject::invokeMethod(screen, "chooseCertainty", Q_ARG(int, 3)));
    REQUIRE(QMetaObject::invokeMethod(screen, "reveal"));
    f.context().session().quit();
    REQUIRE(QMetaObject::invokeMethod(screen, "finish"));

    f.context().map().createConcept("Graph");
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    CHECK(f.context().session().stage() != "idle");
    CHECK(screen->property("certainty").toInt() == 0);
    CHECK_FALSE(screen->property("revealed").toBool());
}
