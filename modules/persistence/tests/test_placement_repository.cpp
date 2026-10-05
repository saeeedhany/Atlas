#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/placement_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

KnowledgeObjectId saveConcept(KnowledgeObjectRepository& objects, const char* title) {
    auto object = KnowledgeObject::create(title).value();
    REQUIRE(objects.save(object).hasValue());
    return object.id();
}

}

TEST_CASE("placements round-trip and saving again updates them") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    PlacementRepository placements(db);
    auto a = saveConcept(objects, "A");

    REQUIRE(placements.saveAll({Placement{a, 10.5, -3.25, false}}).hasValue());
    REQUIRE(placements.saveAll({Placement{a, 11.0, 4.0, true}}).hasValue());

    auto all = placements.findAll().value();
    REQUIRE(all.size() == 1);
    CHECK(all[0].conceptId == a);
    CHECK(all[0].x == 11.0);
    CHECK(all[0].y == 4.0);
    CHECK(all[0].pinned);
}

TEST_CASE("a placement for an unknown concept fails the whole batch") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    PlacementRepository placements(db);
    auto a = saveConcept(objects, "A");

    auto result = placements.saveAll({Placement{a, 1.0, 1.0, false}, Placement{KnowledgeObjectId::generate(), 0.0, 0.0, false}});
    CHECK_FALSE(result.hasValue());
    CHECK(placements.findAll().value().empty());
}

TEST_CASE("deleting a concept deletes its placement") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    PlacementRepository placements(db);
    auto a = saveConcept(objects, "A");
    REQUIRE(placements.saveAll({Placement{a, 1.0, 2.0, false}}).hasValue());
    REQUIRE(objects.remove(a).hasValue());
    CHECK(placements.findAll().value().empty());
}
