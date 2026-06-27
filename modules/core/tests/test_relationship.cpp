#include "atlas/core/relationship.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("Relationship::create rejects a self-loop") {
    auto id = KnowledgeObjectId::generate();
    auto result = Relationship::create(id, id, RelationshipType::DependsOn);
    CHECK(!result.hasValue());
    CHECK(result.error() == RelationshipValidationError::SelfLoop);
}

TEST_CASE("Relationship::create succeeds between two distinct objects") {
    auto a = KnowledgeObjectId::generate();
    auto b = KnowledgeObjectId::generate();
    auto result = Relationship::create(a, b, RelationshipType::DependsOn);
    REQUIRE(result.hasValue());
    const auto& rel = result.value();
    CHECK(rel.sourceId() == a);
    CHECK(rel.targetId() == b);
    CHECK(rel.type() == RelationshipType::DependsOn);
    CHECK(!rel.note().has_value());
}

TEST_CASE("isSymmetric correctly classifies relationship types") {
    CHECK(isSymmetric(RelationshipType::RelatedTo));
    CHECK(isSymmetric(RelationshipType::AlternativeTo));
    CHECK(isSymmetric(RelationshipType::OppositeOf));
    CHECK(!isSymmetric(RelationshipType::DependsOn));
    CHECK(!isSymmetric(RelationshipType::Uses));
    CHECK(!isSymmetric(RelationshipType::Causes));
}

TEST_CASE("Relationship::reconstruct rejects a self-loop even when loaded from storage") {
    auto id = KnowledgeObjectId::generate();
    Relationship::StorageRecord record{RelationshipId::generate(), id, id,
                                        RelationshipType::DependsOn, std::nullopt,
                                        std::chrono::system_clock::now()};
    auto result = Relationship::reconstruct(std::move(record));
    CHECK(!result.hasValue());
    CHECK(result.error() == RelationshipValidationError::SelfLoop);
}

TEST_CASE("Relationship::reconstruct preserves the original id and createdAt") {
    auto relId = RelationshipId::generate();
    auto a = KnowledgeObjectId::generate();
    auto b = KnowledgeObjectId::generate();
    auto createdAt = std::chrono::system_clock::now() - std::chrono::days(3);

    Relationship::StorageRecord record{relId, a, b, RelationshipType::Uses,
                                        std::string{"loaded from disk"}, createdAt};
    auto result = Relationship::reconstruct(std::move(record));
    REQUIRE(result.hasValue());
    const auto& rel = result.value();
    CHECK(rel.id() == relId);
    CHECK(rel.createdAt() == createdAt);
    REQUIRE(rel.note().has_value());
    CHECK(*rel.note() == "loaded from disk");
}
