#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"
#include "doctest.h"
#include "qml_fixture.hpp"

using namespace atlas::core;
using namespace atlas::viewmodels;

namespace {

struct PanelFixture {
    QmlFixture f;
    QString a = f.context().map().createConcept("Alpha");
    QString b = f.context().map().createConcept("Beta");
    std::unique_ptr<QObject> window = f.create("Main");
    QObject* panel = nullptr;

    PanelFixture() {
        QmlFixture::settle();
        panel = QmlFixture::child(window.get(), "conceptPanel");
    }

    void select(const QString& id) {
        f.context().map().setSelectedId(id);
        QmlFixture::settle();
    }
};

}

TEST_CASE("selecting a concept opens the panel on it") {
    PanelFixture p;
    REQUIRE(p.panel);
    CHECK_FALSE(p.panel->property("shown").toBool());
    p.select(p.a);
    CHECK(p.panel->property("shown").toBool());
    CHECK(p.f.context().conceptEditor().conceptId() == p.a);
    CHECK(p.f.context().links().conceptId() == p.a);
    CHECK(QmlFixture::child(p.window.get(), "titleField")->property("text").toString() == "Alpha");
}

TEST_CASE("switching concepts saves the unsaved draft") {
    PanelFixture p;
    p.select(p.a);
    p.f.context().conceptEditor().setDefinition("a draft");
    REQUIRE(p.f.context().conceptEditor().dirty());
    p.select(p.b);
    CHECK(p.f.context().conceptEditor().conceptId() == p.b);
    p.select(p.a);
    CHECK(p.f.context().conceptEditor().definition() == "a draft");
    CHECK_FALSE(p.f.context().conceptEditor().dirty());
}

TEST_CASE("closing the panel saves the draft") {
    PanelFixture p;
    p.select(p.a);
    p.f.context().conceptEditor().setTitle("Alpha prime");
    REQUIRE(QMetaObject::invokeMethod(p.panel, "close"));
    QmlFixture::settle();
    CHECK(p.f.context().map().selectedId().isEmpty());
    CHECK_FALSE(p.panel->property("shown").toBool());
    p.select(p.a);
    CHECK(p.f.context().conceptEditor().title() == "Alpha prime");
}

TEST_CASE("a draft that cannot be saved keeps its concept open") {
    PanelFixture p;
    p.select(p.a);
    p.f.context().conceptEditor().setTitle("");
    p.select(p.b);
    CHECK(p.f.context().conceptEditor().conceptId() == p.a);
    CHECK(p.f.context().map().selectedId() == p.a);
    CHECK(p.f.context().conceptEditor().dirty());
    CHECK(QmlFixture::child(p.window.get(), "toast")->property("shown").toBool());
}

TEST_CASE("closing the window saves the draft") {
    PanelFixture p;
    p.select(p.a);
    p.f.context().conceptEditor().setNotes("for later");
    REQUIRE(QMetaObject::invokeMethod(p.window.get(), "close"));
    CHECK_FALSE(p.f.context().conceptEditor().dirty());
}

TEST_CASE("a draft that cannot be saved keeps the window open") {
    PanelFixture p;
    p.select(p.a);
    p.f.context().conceptEditor().setTitle("");
    REQUIRE(QMetaObject::invokeMethod(p.window.get(), "close"));
    QmlFixture::settle();
    CHECK(p.window->property("visible").toBool());
    CHECK(p.f.context().conceptEditor().dirty());
    CHECK(QmlFixture::child(p.window.get(), "toast")->property("shown").toBool());
}

TEST_CASE("the pin switch always shows the stored pin state") {
    PanelFixture p;
    auto* pin = QmlFixture::child(p.window.get(), "pinSwitch");
    REQUIRE(QMetaObject::invokeMethod(pin, "toggle"));
    REQUIRE(QMetaObject::invokeMethod(pin, "toggled"));
    QmlFixture::settle();
    CHECK_FALSE(pin->property("checked").toBool());

    p.select(p.a);
    REQUIRE(QMetaObject::invokeMethod(pin, "toggle"));
    REQUIRE(QMetaObject::invokeMethod(pin, "toggled"));
    QmlFixture::settle();
    CHECK(p.f.context().conceptEditor().pinned());
    CHECK(pin->property("checked").toBool());

    p.select(p.b);
    CHECK_FALSE(pin->property("checked").toBool());
}

TEST_CASE("confirming delete removes the concept and closes the panel") {
    PanelFixture p;
    p.select(p.a);
    REQUIRE(QMetaObject::invokeMethod(p.panel, "confirmDelete"));
    QmlFixture::settle();
    CHECK(p.f.context().map().conceptCount() == 1);
    CHECK_FALSE(p.panel->property("shown").toBool());
}

TEST_CASE("the panel shows what to learn first") {
    PanelFixture p;
    REQUIRE(p.f.context()
                .workspace()
                .createRelationship(*parseId<KnowledgeObjectId>(p.a), *parseId<KnowledgeObjectId>(p.b),
                                    RelationshipType::DependsOn, std::nullopt)
                .hasValue());
    QmlFixture::settle();
    p.select(p.a);
    auto path = p.panel->property("path").toList();
    REQUIRE(path.size() == 1);
    CHECK(path[0].toMap().value("title").toString() == "Beta");
}

TEST_CASE("the path follows graph changes and reports a loop once per refresh") {
    PanelFixture p;
    int loops = 0;
    QObject::connect(&p.f.context().conceptEditor(), &ConceptEditor::errorOccurred, [&] { ++loops; });
    auto alpha = *parseId<KnowledgeObjectId>(p.a);
    auto beta = *parseId<KnowledgeObjectId>(p.b);
    p.select(p.a);
    REQUIRE(p.panel->property("path").toList().isEmpty());

    REQUIRE(p.f.context().workspace().createRelationship(alpha, beta, RelationshipType::DependsOn, std::nullopt).hasValue());
    QmlFixture::settle();
    CHECK(p.panel->property("path").toList().size() == 1);

    REQUIRE(p.f.context().workspace().createRelationship(beta, alpha, RelationshipType::DependsOn, std::nullopt).hasValue());
    QmlFixture::settle();
    CHECK(p.panel->property("path").toList().isEmpty());
    CHECK(loops == 1);

    p.f.context().map().refresh();
    QmlFixture::settle();
    CHECK(loops == 1);
}
