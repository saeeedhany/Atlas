#include <cmath>
#include <string>

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

double distance(const atlas::render::Point2D& point, double x, double y) {
    return std::hypot(point.x - x, point.y - y);
}

void saveAt(Database& db, const KnowledgeObjectId& id, double x, double y) {
    REQUIRE(PlacementRepository(db).saveAll({Placement{id, x, y, false}}).hasValue());
}

}

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

TEST_CASE("a new concept starts near the center of its topic") {
    Fixture f;
    auto os = f.workspace.createTopic("OS").value();
    auto paging = f.workspace.createKnowledgeObject("Paging", os).value();
    auto threads = f.workspace.createKnowledgeObject("Threads", os).value();
    saveAt(f.db, paging, 950.0, 1000.0);
    saveAt(f.db, threads, 1050.0, 1000.0);
    saveAt(f.db, f.a, -1000.0, -1000.0);
    saveAt(f.db, f.b, -1000.0, -900.0);
    saveAt(f.db, f.c, -900.0, -1000.0);
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());

    auto added = f.workspace.createKnowledgeObject("Scheduling", os).value();
    auto placed = placements.position(added);
    REQUIRE(placed.has_value());
    CHECK(distance(*placed, 1000.0, 1000.0) < 150.0);
}

TEST_CASE("a new concept with no topic peers starts near the center of everything") {
    Fixture f;
    saveAt(f.db, f.a, 0.0, 0.0);
    saveAt(f.db, f.b, 1000.0, 0.0);
    saveAt(f.db, f.c, 500.0, 900.0);
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());

    auto empty = f.workspace.createTopic("Empty").value();
    auto added = f.workspace.createKnowledgeObject("Lonely", empty).value();
    auto placed = placements.position(added);
    REQUIRE(placed.has_value());
    CHECK(distance(*placed, 500.0, 300.0) < 150.0);
}

TEST_CASE("graph changes before load write no placements") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    f.workspace.createKnowledgeObject("Early");
    CHECK(PlacementRepository(f.db).findAll().value().empty());
}

TEST_CASE("tidy moves concepts that are not pinned") {
    Fixture f;
    saveAt(f.db, f.a, 0.0, 0.0);
    saveAt(f.db, f.b, 1.0, 0.0);
    saveAt(f.db, f.c, 0.0, 1.0);
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(placements.tidy().hasValue());
    bool moved = !samePoint(*placements.position(f.a), {0.0, 0.0}) ||
                 !samePoint(*placements.position(f.b), {1.0, 0.0}) ||
                 !samePoint(*placements.position(f.c), {0.0, 1.0});
    CHECK(moved);
}

TEST_CASE("a pinned concept stays pinned after a restart") {
    Fixture f;
    {
        PlacementController first(f.db, f.workspace);
        REQUIRE(first.load().hasValue());
        REQUIRE(first.setPinned(f.a, true).hasValue());
    }
    PlacementController second(f.db, f.workspace);
    REQUIRE(second.load().hasValue());
    CHECK(second.isPinned(f.a));
    CHECK_FALSE(second.isPinned(f.b));
}

TEST_CASE("tidy does nothing before load") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.tidy().hasValue());
    PlacementRepository repository(f.db);
    CHECK(repository.findAll().value().empty());
}

TEST_CASE("starting offsets never put a new concept on top of its seed point") {
    Fixture f;
    for (const auto& object : f.workspace.allKnowledgeObjects()) {
        auto offset = startingOffsetFor(object.id());
        double length = std::hypot(offset.x, offset.y);
        CHECK(length >= 0.25 * PlacementController::kMaxStartingOffset - 1e-9);
        CHECK(length <= PlacementController::kMaxStartingOffset + 1e-9);
    }
    for (int i = 0; i < 50; ++i) {
        auto offset = startingOffsetFor(f.addConcept(("N" + std::to_string(i)).c_str()));
        CHECK(std::hypot(offset.x, offset.y) >= 0.25 * PlacementController::kMaxStartingOffset - 1e-9);
    }
}

TEST_CASE("moving concepts shifts them, keeps pins, and persists") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(placements.setPinned(f.a, true).hasValue());
    auto before = *placements.position(f.a);
    int changes = 0;
    QObject::connect(&placements, &PlacementController::placementsChanged, [&] { ++changes; });
    REQUIRE(placements.moveBy({f.a, f.b}, 30.0, -20.0).hasValue());
    CHECK(changes == 1);
    auto after = *placements.position(f.a);
    CHECK(after.x == doctest::Approx(before.x + 30.0));
    CHECK(after.y == doctest::Approx(before.y - 20.0));
    CHECK(placements.isPinned(f.a));

    PlacementController reloaded(f.db, f.workspace);
    REQUIRE(reloaded.load().hasValue());
    CHECK(reloaded.position(f.a)->x == doctest::Approx(after.x));
}
