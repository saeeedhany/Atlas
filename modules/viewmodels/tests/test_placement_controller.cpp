#include "atlas/persistence/database.hpp"
#include "atlas/persistence/placement_repository.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
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

struct Fixture {
    Database db = openTestDatabase();
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    KnowledgeObjectId a = addConcept("A");
    KnowledgeObjectId b = addConcept("B");
    KnowledgeObjectId c = addConcept("C");

    KnowledgeObjectId addConcept(const char* title) {
        REQUIRE(loaded);
        return workspace.createKnowledgeObject(title).value();
    }
};

bool samePoint(const atlas::render::Point2D& left, const atlas::render::Point2D& right) {
    return left.x == right.x && left.y == right.y;
}

}  // namespace

TEST_CASE("load places every concept and saves the positions") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    for (const auto& id : {f.a, f.b, f.c}) CHECK(placements.position(id).has_value());
    PlacementRepository repository(f.db);
    CHECK(repository.findAll().value().size() == 3);
}

TEST_CASE("adding a concept never moves saved concepts") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    auto before = *placements.position(f.a);
    auto added = f.workspace.createKnowledgeObject("D").value();
    REQUIRE(f.workspace.createRelationship(added, f.a, RelationshipType::DependsOn, std::nullopt).hasValue());
    CHECK(samePoint(*placements.position(f.a), before));
    CHECK(placements.position(added).has_value());
}

TEST_CASE("removing a concept forgets its position") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(f.workspace.removeKnowledgeObject(f.c).hasValue());
    CHECK_FALSE(placements.position(f.c).has_value());
}

TEST_CASE("positions survive a restart") {
    Fixture f;
    atlas::render::Point2D saved;
    {
        PlacementController first(f.db, f.workspace);
        REQUIRE(first.load().hasValue());
        saved = *first.position(f.b);
    }
    PlacementController second(f.db, f.workspace);
    REQUIRE(second.load().hasValue());
    CHECK(samePoint(*second.position(f.b), saved));
}

TEST_CASE("tidy keeps pinned concepts exactly where they are") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(f.workspace.createRelationship(f.a, f.b, RelationshipType::DependsOn, std::nullopt).hasValue());
    REQUIRE(placements.setPinned(f.a, true).hasValue());
    auto pinnedBefore = *placements.position(f.a);
    REQUIRE(placements.tidy().hasValue());
    CHECK(placements.isPinned(f.a));
    CHECK(samePoint(*placements.position(f.a), pinnedBefore));
}

TEST_CASE("placementsChanged fires when new concepts are placed") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    int fired = 0;
    QObject::connect(&placements, &PlacementController::placementsChanged, [&] { ++fired; });
    REQUIRE(placements.load().hasValue());
    f.workspace.createKnowledgeObject("E");
    CHECK(fired == 2);
}
