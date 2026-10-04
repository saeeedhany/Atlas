#include <string>

#include "atlas/viewmodels/ids.hpp"
#include "doctest.h"
#include "qml_fixture.hpp"

using namespace atlas::core;
using namespace atlas::viewmodels;

TEST_CASE("adding a concept from the bar selects it") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* bar = QmlFixture::child(window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "addConcept", Q_ARG(QString, "  Recursion ")));
    QmlFixture::settle();
    CHECK(f.context().map().conceptCount() == 1);
    REQUIRE_FALSE(f.context().map().selectedId().isEmpty());
    CHECK(f.context().map().conceptInfo(f.context().map().selectedId()).value("title").toString() == "Recursion");
    CHECK(QmlFixture::child(window.get(), "newConceptField")->property("text").toString().isEmpty());
}

TEST_CASE("creating a topic opens it and the picker follows") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* bar = QmlFixture::child(window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "createTopic", Q_ARG(QString, "Databases")));
    QmlFixture::settle();
    CHECK(f.context().topics().count() == 2);
    CHECK(f.context().topics().nameOf(f.context().map().topicId()) == "Databases");
    CHECK(QmlFixture::child(window.get(), "topicBox")->property("currentText").toString() == "Databases");
    CHECK(bar->property("canEditTopic").toBool());

    REQUIRE(QMetaObject::invokeMethod(bar, "deleteTopic"));
    QmlFixture::settle();
    CHECK(f.context().topics().count() == 1);
    CHECK(f.context().map().topicId().isEmpty());
    CHECK(QmlFixture::child(window.get(), "topicBox")->property("currentText").toString() == "All topics");
}

TEST_CASE("picking a search result outside the topic shows all topics and selects it") {
    QmlFixture f;
    auto os = f.context().topics().createTopic("OS");
    auto databases = f.context().topics().createTopic("Databases");
    f.context().map().setTopicId(databases);
    QString tree = f.context().map().createConcept("Binary tree");
    f.context().map().setTopicId(QString());
    auto window = f.create("Main");
    QmlFixture::settle();

    auto* bar = QmlFixture::child(window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "searchFor", Q_ARG(QString, "binary")));
    CHECK(bar->property("results").toList().size() == 1);
    f.context().map().setTopicId(os);
    REQUIRE(QMetaObject::invokeMethod(bar, "pick", Q_ARG(QString, tree)));
    QmlFixture::settle();
    CHECK(f.context().map().topicId().isEmpty());
    CHECK(f.context().map().selectedId() == tree);
}

TEST_CASE("the hover card describes concepts and links") {
    QmlFixture f;
    QString tree = f.context().map().createConcept("Tree");
    QString btree = f.context().map().createConcept("B-Tree");
    auto link = f.context()
                    .workspace()
                    .createRelationship(*parseId<KnowledgeObjectId>(btree), *parseId<KnowledgeObjectId>(tree),
                                        RelationshipType::DependsOn, std::string("balanced"))
                    .value();
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* card = QmlFixture::child(window.get(), "hoverCard");

    REQUIRE(QMetaObject::invokeMethod(card, "showConcept", Q_ARG(QString, tree)));
    CHECK(card->property("shown").toBool());
    CHECK(card->property("title").toString() == "Tree");
    CHECK(card->property("detail").toString().startsWith("Not learned yet"));

    REQUIRE(QMetaObject::invokeMethod(card, "showConcept", Q_ARG(QString, QString())));
    REQUIRE(QMetaObject::invokeMethod(card, "showLink", Q_ARG(QString, idString(link))));
    CHECK(card->property("title").toString() == "B-Tree depends on Tree");
    CHECK(card->property("detail").toString() == "balanced");

    REQUIRE(QMetaObject::invokeMethod(card, "showLink", Q_ARG(QString, QString())));
    CHECK_FALSE(card->property("shown").toBool());
}

TEST_CASE("focusing an unknown concept changes nothing") {
    QmlFixture f;
    auto os = f.context().topics().createTopic("OS");
    f.context().map().setTopicId(os);
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* overlay = QmlFixture::child(window.get(), "mapOverlay");
    REQUIRE(QMetaObject::invokeMethod(overlay, "focusConcept", Q_ARG(QString, "missing")));
    QmlFixture::settle();
    CHECK(f.context().map().topicId() == os);
    CHECK(overlay->property("focusId").toString().isEmpty());
}

TEST_CASE("a focus that never resolves does not block topic fits") {
    QmlFixture f;
    auto os = f.context().topics().createTopic("OS");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* overlay = QmlFixture::child(window.get(), "mapOverlay");
    overlay->setProperty("focusId", "ghost");
    f.context().map().setTopicId(os);
    QmlFixture::settle();
    REQUIRE(QMetaObject::invokeMethod(overlay, "settle"));
    QmlFixture::settle();
    CHECK(overlay->property("focusId").toString().isEmpty());
    CHECK_FALSE(overlay->property("fitPending").toBool());
}

TEST_CASE("enter in the topic popup renames the open topic and creates one otherwise") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* bar = QmlFixture::child(window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "submitTopicName", Q_ARG(QString, "Databases")));
    QmlFixture::settle();
    CHECK(f.context().topics().count() == 2);
    REQUIRE(bar->property("canEditTopic").toBool());

    REQUIRE(QMetaObject::invokeMethod(bar, "submitTopicName", Q_ARG(QString, "Storage")));
    QmlFixture::settle();
    CHECK(f.context().topics().count() == 2);
    CHECK(f.context().topics().nameOf(f.context().map().topicId()) == "Storage");
}

TEST_CASE("focusing another topic's concept keeps the scope when the draft cannot be saved") {
    QmlFixture f;
    auto os = f.context().topics().createTopic("OS");
    auto databases = f.context().topics().createTopic("Databases");
    f.context().map().setTopicId(os);
    QString paging = f.context().map().createConcept("Paging");
    f.context().map().setTopicId(databases);
    QString indexing = f.context().map().createConcept("Indexing");
    f.context().map().setSelectedId(QString());
    auto window = f.create("Main");
    QmlFixture::settle();
    f.context().map().setSelectedId(indexing);
    QmlFixture::settle();
    REQUIRE(f.context().conceptEditor().conceptId() == indexing);

    f.context().conceptEditor().setTitle("");
    auto* overlay = QmlFixture::child(window.get(), "mapOverlay");
    REQUIRE(QMetaObject::invokeMethod(overlay, "focusConcept", Q_ARG(QString, paging)));
    QmlFixture::settle();
    CHECK(f.context().map().topicId() == databases);
    CHECK(f.context().map().selectedId() == indexing);
    CHECK(f.context().conceptEditor().conceptId() == indexing);
    CHECK(overlay->property("focusId").toString().isEmpty());
}
