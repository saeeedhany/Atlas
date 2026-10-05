#include <QCoreApplication>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/notes_model.hpp"
#include "doctest.h"
#include "raw_sql.hpp"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase(const std::string& path) {
    auto result = Database::open(path);
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

struct Fixture {
    QTemporaryDir dir;
    std::string path = dir.filePath("atlas.db").toStdString();
    Database db = openTestDatabase(path);
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    NotesModel notes{db, workspace, systemClock()};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(notes.load().hasValue());
        QObject::connect(&notes, &NotesModel::errorOccurred, [this] { ++errors; });
    }
};

}

TEST_CASE("notes are created, edited, moved, resized, and recolored") {
    Fixture f;
    QString id = f.notes.createNote(10, 20, "Why half?");
    REQUIRE_FALSE(id.isEmpty());
    CHECK(f.notes.count() == 1);
    REQUIRE(f.notes.setBody(id, "Why half full?"));
    REQUIRE(f.notes.move(id, 300, 400));
    REQUIRE(f.notes.resize(id, 40, 300));
    REQUIRE(f.notes.recolor(id, "olive"));
    CHECK_FALSE(f.notes.recolor(id, "neon"));
    auto note = f.notes.note(id);
    CHECK(note.value("body").toString() == "Why half full?");
    CHECK(note.value("x").toDouble() == doctest::Approx(300.0));
    CHECK(note.value("width").toDouble() == doctest::Approx(NotesModel::kMinSize));
    CHECK(note.value("color").toString() == "olive");

    NotesModel reloaded(f.db, f.workspace, systemClock());
    REQUIRE(reloaded.load().hasValue());
    CHECK(reloaded.note(id).value("body").toString() == "Why half full?");
}

TEST_CASE("links follow the life of their targets") {
    Fixture f;
    auto concept_ = f.workspace.createKnowledgeObject("Paging").value();
    auto topic = f.workspace.createTopic("OS").value();
    QString id = f.notes.createNote(0, 0);
    REQUIRE(f.notes.link(id, "concept", idString(concept_)));
    REQUIRE(f.notes.link(id, "topic", idString(topic)));
    CHECK_FALSE(f.notes.link(id, "concept", "garbage"));
    CHECK(f.notes.note(id).value("links").toList().size() == 2);

    REQUIRE(f.workspace.removeKnowledgeObject(concept_).hasValue());
    QCoreApplication::processEvents();
    auto links = f.notes.note(id).value("links").toList();
    REQUIRE(links.size() == 1);
    CHECK(links[0].toMap().value("kind").toString() == "topic");

    REQUIRE(f.notes.unlink(id, "topic", idString(topic)));
    CHECK(f.notes.note(id).value("links").toList().isEmpty());
    REQUIRE(f.notes.remove(id));
    CHECK(f.notes.count() == 0);
}

TEST_CASE("a failed write leaves the board unchanged") {
    Fixture f;
    QString id = f.notes.createNote(5, 5, "Keep");
    REQUIRE(executeRawSql(f.path, "DROP TABLE note_links; DROP TABLE board_notes;"));
    CHECK_FALSE(f.notes.move(id, 900, 900));
    CHECK(f.errors == 1);
    CHECK(f.notes.note(id).value("x").toDouble() == doctest::Approx(5.0));
}

TEST_CASE("removing a linked target refreshes links without resetting the board") {
    Fixture f;
    auto concept_ = f.workspace.createKnowledgeObject("Paging").value();
    QString id = f.notes.createNote(0, 0, "Typing");
    REQUIRE(f.notes.link(id, "concept", idString(concept_)));

    int resets = 0;
    int linkUpdates = 0;
    QObject::connect(&f.notes, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&f.notes, &QAbstractItemModel::dataChanged,
                     [&](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                         if (roles.contains(NotesModel::LinksRole)) ++linkUpdates;
                     });
    REQUIRE(f.workspace.removeKnowledgeObject(concept_).hasValue());
    QCoreApplication::processEvents();
    CHECK(resets == 0);
    CHECK(linkUpdates >= 1);
    CHECK(f.notes.note(id).value("links").toList().isEmpty());
    CHECK(f.notes.count() == 1);
}
