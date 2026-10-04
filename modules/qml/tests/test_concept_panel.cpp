#include "doctest.h"
#include "qml_fixture.hpp"

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

}  // namespace

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

TEST_CASE("confirming delete removes the concept and closes the panel") {
    PanelFixture p;
    p.select(p.a);
    REQUIRE(QMetaObject::invokeMethod(p.panel, "confirmDelete"));
    QmlFixture::settle();
    CHECK(p.f.context().map().conceptCount() == 1);
    CHECK_FALSE(p.panel->property("shown").toBool());
}
