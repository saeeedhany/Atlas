#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/ui/workspace_controller.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::ui;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

}  // namespace

TEST_CASE("createKnowledgeObject persists and adds to the graph") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.createKnowledgeObject("Hash Tables");
    REQUIRE(result.hasValue());
    auto id = result.value();

    auto found = controller.findKnowledgeObject(id);
    REQUIRE(found.has_value());
    CHECK(found->title() == "Hash Tables");

    // Independently verify it actually reached the database, not just
    // the in-memory graph.
    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->title() == "Hash Tables");
}

TEST_CASE("createKnowledgeObject rejects an empty title without touching the graph") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.createKnowledgeObject("");
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);
    CHECK(controller.allKnowledgeObjects().empty());
}

TEST_CASE("createKnowledgeObject emits knowledgeObjectAdded exactly once") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    int signalCount = 0;
    QObject::connect(&controller, &WorkspaceController::knowledgeObjectAdded,
                      [&](KnowledgeObjectId) { ++signalCount; });

    REQUIRE(controller.createKnowledgeObject("Recursion").hasValue());
    CHECK(signalCount == 1);
}

TEST_CASE("updateKnowledgeObject changes fields and persists them") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto id = controller.createKnowledgeObject("Draft").value();

    KnowledgeObjectEdits edits;
    edits.title = "Tail Call Optimization";
    edits.definition = "Reusing the current stack frame for a tail call";
    edits.difficulty = Difficulty::Advanced;
    edits.confidence = ConfidenceLevel::Learning;

    auto result = controller.updateKnowledgeObject(id, edits);
    REQUIRE(result.hasValue());

    auto updated = controller.findKnowledgeObject(id);
    REQUIRE(updated.has_value());
    CHECK(updated->title() == "Tail Call Optimization");
    CHECK(updated->definition() == "Reusing the current stack frame for a tail call");
    CHECK(updated->difficulty() == Difficulty::Advanced);
    CHECK(updated->confidence() == ConfidenceLevel::Learning);

    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->title() == "Tail Call Optimization");
}

TEST_CASE("updateKnowledgeObject rejects an empty title and leaves the object unchanged") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto id = controller.createKnowledgeObject("Original Title").value();

    KnowledgeObjectEdits edits;
    edits.title = "";
    auto result = controller.updateKnowledgeObject(id, edits);
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);

    auto unchanged = controller.findKnowledgeObject(id);
    REQUIRE(unchanged.has_value());
    CHECK(unchanged->title() == "Original Title");
}

TEST_CASE("updateKnowledgeObject on an unknown id returns NotFound") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.updateKnowledgeObject(KnowledgeObjectId::generate(), {});
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::NotFound);
}

TEST_CASE("removeKnowledgeObject removes from both the graph and the database") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto id = controller.createKnowledgeObject("Temporary").value();

    REQUIRE(controller.removeKnowledgeObject(id).hasValue());

    CHECK(!controller.findKnowledgeObject(id).has_value());

    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    CHECK(!persisted.value().has_value());
}

TEST_CASE("allKnowledgeObjects returns a deterministic, alphabetically-sorted snapshot") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    REQUIRE(controller.createKnowledgeObject("Zebra").hasValue());
    REQUIRE(controller.createKnowledgeObject("Apple").hasValue());
    REQUIRE(controller.createKnowledgeObject("Mango").hasValue());

    auto all = controller.allKnowledgeObjects();
    REQUIRE(all.size() == 3);
    CHECK(all[0].title() == "Apple");
    CHECK(all[1].title() == "Mango");
    CHECK(all[2].title() == "Zebra");
}

TEST_CASE("load() populates the graph from data already in the database") {
    auto db = openTestDatabase();

    // Seed the database directly, bypassing the controller, to prove
    // load() actually reads from storage rather than only reflecting
    // whatever the controller itself wrote during this process.
    KnowledgeObjectRepository repo(db);
    auto seeded = KnowledgeObject::create("Seeded From Disk");
    REQUIRE(seeded.hasValue());
    REQUIRE(repo.save(seeded.value()).hasValue());

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto all = controller.allKnowledgeObjects();
    REQUIRE(all.size() == 1);
    CHECK(all.front().title() == "Seeded From Disk");
}
