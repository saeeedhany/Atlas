#include "atlas/core/enums.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("Difficulty round-trips through toDisplayString/difficultyFromString") {
    for (auto value : {Difficulty::Beginner, Difficulty::Intermediate, Difficulty::Advanced,
                        Difficulty::Expert}) {
        auto text = toDisplayString(value);
        auto parsed = difficultyFromString(text);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == value);
    }
}

TEST_CASE("ConfidenceLevel round-trips through toDisplayString/confidenceLevelFromString") {
    for (auto value : {ConfidenceLevel::Unknown, ConfidenceLevel::Learning,
                        ConfidenceLevel::Familiar, ConfidenceLevel::Confident,
                        ConfidenceLevel::Mastered}) {
        auto text = toDisplayString(value);
        auto parsed = confidenceLevelFromString(text);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == value);
    }
}

TEST_CASE("RelationshipType round-trips through toDisplayString/relationshipTypeFromString") {
    for (auto value :
         {RelationshipType::DependsOn, RelationshipType::Uses, RelationshipType::Implements,
          RelationshipType::Solves, RelationshipType::Contains, RelationshipType::PartOf,
          RelationshipType::RelatedTo, RelationshipType::AlternativeTo,
          RelationshipType::OppositeOf, RelationshipType::Causes}) {
        auto text = toDisplayString(value);
        auto parsed = relationshipTypeFromString(text);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == value);
    }
}

TEST_CASE("fromString rejects unrecognized text") {
    CHECK(!difficultyFromString("Nonsense").has_value());
    CHECK(!confidenceLevelFromString("Nonsense").has_value());
    CHECK(!relationshipTypeFromString("Nonsense").has_value());
}
