#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>
#include <QTemporaryDir>

#include <memory>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/app_context.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::viewmodels;

TEST_CASE("the app context loads, and QML sees every singleton") {
    auto opened = Database::open(":memory:");
    REQUIRE(opened.hasValue());
    auto db = std::move(opened).value();
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);

    AppContext context(db, store, systemClock());
    REQUIRE(context.load().hasValue());
    context.provideSingletons();

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport Atlas.ViewModels\n"
                      "QtObject {\n"
                      "  property int topics: Topics.count\n"
                      "  property int concepts: MapView.conceptCount\n"
                      "  property bool empty: Today.empty\n"
                      "  property bool open: Concept.exists\n"
                      "  property int links: ConceptLinks.count\n"
                      "  property int perDay: AppSettings.newPerDay\n"
                      "  property color accent: Palette.accent\n"
                      "}",
                      QUrl());
    std::unique_ptr<QObject> object(component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(object != nullptr);
    CHECK(object->property("topics").toInt() == 1);
    CHECK(object->property("concepts").toInt() == 0);
    CHECK(object->property("empty").toBool());
    CHECK_FALSE(object->property("open").toBool());
    CHECK(object->property("links").toInt() == 0);
    CHECK(object->property("perDay").toInt() == 5);
    CHECK(object->property("accent").value<QColor>() == context.palette().accent());
}

TEST_CASE("view models react to changes made through the context") {
    auto opened = Database::open(":memory:");
    REQUIRE(opened.hasValue());
    auto db = std::move(opened).value();
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppContext context(db, store, systemClock());
    REQUIRE(context.load().hasValue());

    QString id = context.map().createConcept("Recursion");
    REQUIRE_FALSE(id.isEmpty());
    context.conceptEditor().setConceptId(id);
    CHECK(context.conceptEditor().exists());
    CHECK(context.today().newCount() == 1);
    CHECK(context.map().nodes().size() == 1);
    CHECK(context.placements().position(context.workspace().allKnowledgeObjects().front().id()).has_value());
}
