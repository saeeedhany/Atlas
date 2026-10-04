#include "doctest.h"
#include "qml_fixture.hpp"

#include <QCoreApplication>

TEST_CASE("the links editor finds a target and adds a link") {
    QmlFixture f;
    QString a = f.context().map().createConcept("Alpha");
    QString b = f.context().map().createConcept("Beta");
    f.context().links().setConceptId(a);
    auto editor = f.create("LinksEditor");

    bool added = true;
    REQUIRE(QMetaObject::invokeMethod(editor.get(), "addLink", Q_RETURN_ARG(bool, added), Q_ARG(int, 0),
                                      Q_ARG(bool, true), Q_ARG(QString, QString())));
    CHECK_FALSE(added);

    REQUIRE(QMetaObject::invokeMethod(editor.get(), "searchCandidates", Q_ARG(QString, "bet")));
    REQUIRE(editor->property("candidates").toList().size() == 1);

    REQUIRE(QMetaObject::invokeMethod(editor.get(), "chooseTarget", Q_ARG(QString, b), Q_ARG(QString, "Beta")));
    CHECK(editor->property("targetId").toString() == b);
    CHECK(editor->property("candidates").toList().isEmpty());

    REQUIRE(QMetaObject::invokeMethod(editor.get(), "addLink", Q_RETURN_ARG(bool, added), Q_ARG(int, 0),
                                      Q_ARG(bool, true), Q_ARG(QString, " shared idea ")));
    CHECK(added);
    CHECK(f.context().links().count() == 1);
    QCoreApplication::processEvents();
    CHECK(editor->property("targetId").toString().isEmpty());
}

TEST_CASE("cancel drops the chosen target") {
    QmlFixture f;
    QString a = f.context().map().createConcept("Alpha");
    QString b = f.context().map().createConcept("Beta");
    f.context().links().setConceptId(a);
    auto editor = f.create("LinksEditor");
    REQUIRE(QMetaObject::invokeMethod(editor.get(), "chooseTarget", Q_ARG(QString, b), Q_ARG(QString, "Beta")));
    QmlFixture::child(editor.get(), "linkTypeBox")->setProperty("currentIndex", 2);
    QmlFixture::child(editor.get(), "linkDirectionSwitch")->setProperty("checked", false);
    REQUIRE(QMetaObject::invokeMethod(editor.get(), "cancel"));
    CHECK(editor->property("targetId").toString().isEmpty());
    CHECK(QmlFixture::child(editor.get(), "linkTypeBox")->property("currentIndex").toInt() == 0);
    CHECK(QmlFixture::child(editor.get(), "linkDirectionSwitch")->property("checked").toBool());
    CHECK(f.context().links().count() == 0);
}

TEST_CASE("switching concepts clears the link form") {
    QmlFixture f;
    QString a = f.context().map().createConcept("Alpha");
    QString b = f.context().map().createConcept("Beta");
    QString c = f.context().map().createConcept("Gamma");
    f.context().links().setConceptId(a);
    auto editor = f.create("LinksEditor");

    REQUIRE(QMetaObject::invokeMethod(editor.get(), "chooseTarget", Q_ARG(QString, c), Q_ARG(QString, "Gamma")));
    QmlFixture::child(editor.get(), "linkTypeBox")->setProperty("currentIndex", 2);
    QmlFixture::child(editor.get(), "linkDirectionSwitch")->setProperty("checked", false);
    QmlFixture::child(editor.get(), "linkNoteField")->setProperty("text", "why");
    f.context().links().setConceptId(b);
    CHECK(editor->property("targetId").toString().isEmpty());
    CHECK(editor->property("targetTitle").toString().isEmpty());
    CHECK(QmlFixture::child(editor.get(), "linkTypeBox")->property("currentIndex").toInt() == 0);
    CHECK(QmlFixture::child(editor.get(), "linkDirectionSwitch")->property("checked").toBool());
    CHECK(QmlFixture::child(editor.get(), "linkNoteField")->property("text").toString().isEmpty());

    QmlFixture::child(editor.get(), "linkFindField")->setProperty("text", "gam");
    REQUIRE(QMetaObject::invokeMethod(editor.get(), "searchCandidates", Q_ARG(QString, "gam")));
    REQUIRE_FALSE(editor->property("candidates").toList().isEmpty());
    f.context().links().setConceptId(a);
    CHECK(editor->property("candidates").toList().isEmpty());
    CHECK(QmlFixture::child(editor.get(), "linkFindField")->property("text").toString().isEmpty());
}
