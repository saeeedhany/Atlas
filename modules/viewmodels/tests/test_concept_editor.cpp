#include <QVariantMap>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/concept_editor.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

QVariantMap item(const QString& primary, const QString& secondary) {
    return QVariantMap{{"primary", primary}, {"secondary", secondary}};
}

struct Fixture {
    Database db = openTestDatabase();
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    MemoryController memory{db, workspace, systemClock()};
    PlacementController placements{db, workspace};
    ConceptEditor editor{workspace, memory, placements};
    KnowledgeObjectId tree = workspace.createKnowledgeObject("Tree").value();
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(placements.load().hasValue());
        REQUIRE(memory.load().hasValue());
        QObject::connect(&editor, &ConceptEditor::errorOccurred, [this] { ++errors; });
        editor.setConceptId(idString(tree));
    }
};

}  // namespace

TEST_CASE("loading a concept fills the fields") {
    Fixture f;
    CHECK(f.editor.exists());
    CHECK(f.editor.title() == "Tree");
    CHECK(f.editor.difficulty() == 0);
    CHECK(f.editor.topicId() == idString(uncategorizedTopicId()));
    CHECK(f.editor.recall() < 0.0);
    CHECK_FALSE(f.editor.dirty());
    CHECK(f.editor.difficultyNames().size() == 4);
}

TEST_CASE("edits are saved, including lists with optional parts") {
    Fixture f;
    f.editor.setDefinition("A hierarchy of nodes");
    f.editor.setDifficulty(2);
    f.editor.setExamples({item("File system", "/usr/bin"), item("DOM", "")});
    f.editor.setMiniProjects({item("Build a trie", "Autocomplete")});
    f.editor.setReferences({item("CLRS", ""), item("", "")});
    CHECK(f.editor.dirty());
    REQUIRE(f.editor.save());
    CHECK_FALSE(f.editor.dirty());

    auto saved = f.workspace.findKnowledgeObject(f.tree);
    REQUIRE(saved.has_value());
    CHECK(saved->definition() == "A hierarchy of nodes");
    CHECK(saved->difficulty() == Difficulty::Advanced);
    REQUIRE(saved->examples().size() == 2);
    CHECK(saved->examples()[0].snippet == "/usr/bin");
    CHECK_FALSE(saved->examples()[1].snippet.has_value());
    CHECK(saved->miniProjects()[0].description == "Autocomplete");
    REQUIRE(saved->references().size() == 1);
    CHECK_FALSE(saved->references()[0].url.has_value());
    CHECK(f.editor.references().size() == 1);
    CHECK(f.editor.examples().size() == 2);
}

TEST_CASE("an empty title is refused and reported") {
    Fixture f;
    f.editor.setTitle("");
    CHECK_FALSE(f.editor.save());
    CHECK(f.errors == 1);
    CHECK(f.workspace.findKnowledgeObject(f.tree)->title() == "Tree");
}

TEST_CASE("revert drops unsaved edits") {
    Fixture f;
    f.editor.setNotes("draft");
    f.editor.revert();
    CHECK(f.editor.notes().isEmpty());
    CHECK_FALSE(f.editor.dirty());
}

TEST_CASE("an unsaved draft survives other graph changes") {
    Fixture f;
    f.editor.setNotes("draft");
    f.workspace.createKnowledgeObject("Hash Table");
    CHECK(f.editor.notes() == "draft");
}

TEST_CASE("a concept deleted anywhere clears the editor") {
    Fixture f;
    f.editor.setNotes("draft");
    REQUIRE(f.workspace.removeKnowledgeObject(f.tree).hasValue());
    CHECK_FALSE(f.editor.exists());
    CHECK(f.editor.title().isEmpty());
    CHECK(f.editor.conceptId().isEmpty());
}

TEST_CASE("remove deletes the concept") {
    Fixture f;
    REQUIRE(f.editor.remove());
    CHECK_FALSE(f.workspace.findKnowledgeObject(f.tree).has_value());
    CHECK_FALSE(f.editor.exists());
}

TEST_CASE("pinning writes through to the placement") {
    Fixture f;
    f.editor.setPinned(true);
    CHECK(f.placements.isPinned(f.tree));
    CHECK(f.editor.pinned());
}
