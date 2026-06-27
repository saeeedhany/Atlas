#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/relationship_repository.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::core;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

KnowledgeObjectId saveStubObject(KnowledgeObjectRepository& repo, const char* title) {
    auto created = KnowledgeObject::create(title);
    REQUIRE(created.hasValue());
    auto object = std::move(created).value();
    auto id = object.id();
    REQUIRE(repo.save(object).hasValue());
    return id;
}

}  // namespace

TEST_CASE("save then findById round-trips a relationship") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);

    auto a = saveStubObject(objects, "Stack");
    auto b = saveStubObject(objects, "Recursion");

    auto created = Relationship::create(a, b, RelationshipType::Uses, std::string{"a note"});
    REQUIRE(created.hasValue());
    auto relationship = std::move(created).value();
    REQUIRE(relationships.save(relationship).hasValue());

    auto found = relationships.findById(relationship.id());
    REQUIRE(found.hasValue());
    REQUIRE(found.value().has_value());
    CHECK(found.value()->sourceId() == a);
    CHECK(found.value()->targetId() == b);
    CHECK(found.value()->type() == RelationshipType::Uses);
    REQUIRE(found.value()->note().has_value());
    CHECK(*found.value()->note() == "a note");
}

TEST_CASE("findAll returns every saved relationship") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);

    auto a = saveStubObject(objects, "A");
    auto b = saveStubObject(objects, "B");
    auto c = saveStubObject(objects, "C");

    REQUIRE(relationships.save(Relationship::create(a, b, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(relationships.save(Relationship::create(b, c, RelationshipType::DependsOn).value())
                .hasValue());

    auto all = relationships.findAll();
    REQUIRE(all.hasValue());
    CHECK(all.value().size() == 2);
}

TEST_CASE("remove deletes a relationship") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);

    auto a = saveStubObject(objects, "A");
    auto b = saveStubObject(objects, "B");
    auto relationship = Relationship::create(a, b, RelationshipType::Causes).value();
    REQUIRE(relationships.save(relationship).hasValue());

    REQUIRE(relationships.remove(relationship.id()).hasValue());

    auto found = relationships.findById(relationship.id());
    REQUIRE(found.hasValue());
    CHECK(!found.value().has_value());
}

TEST_CASE("a duplicate (source, target, type) edge with a different id is rejected") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);

    auto a = saveStubObject(objects, "A");
    auto b = saveStubObject(objects, "B");

    auto first = Relationship::create(a, b, RelationshipType::DependsOn).value();
    REQUIRE(relationships.save(first).hasValue());

    auto second = Relationship::create(a, b, RelationshipType::DependsOn).value();
    auto saveResult = relationships.save(second);
    CHECK(!saveResult.hasValue());
    CHECK(saveResult.error().code == PersistenceErrorCode::ConstraintViolation);
}

TEST_CASE("removing a KnowledgeObject cascades and removes relationships touching it") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);

    auto a = saveStubObject(objects, "A");
    auto b = saveStubObject(objects, "B");
    auto relationship = Relationship::create(a, b, RelationshipType::RelatedTo).value();
    REQUIRE(relationships.save(relationship).hasValue());

    REQUIRE(objects.remove(a).hasValue());

    auto found = relationships.findById(relationship.id());
    REQUIRE(found.hasValue());
    CHECK(!found.value().has_value());  // gone via ON DELETE CASCADE, not orphaned
}
