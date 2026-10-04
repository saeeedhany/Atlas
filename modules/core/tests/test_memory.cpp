#include <unordered_set>

#include "atlas/core/memory.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("item kinds, exercises and phases round-trip through storage strings") {
    for (auto kind : {ItemKind::Concept, ItemKind::Link}) {
        CHECK(itemKindFromString(toStorageString(kind)) == kind);
    }
    for (auto exercise : {Exercise::Rebuild, Exercise::Explain}) {
        CHECK(exerciseFromString(toStorageString(exercise)) == exercise);
    }
    for (auto phase : {Phase::New, Phase::Learning, Phase::Review, Phase::Relearning}) {
        CHECK(phaseFromString(toStorageString(phase)) == phase);
    }
    CHECK_FALSE(itemKindFromString("node").has_value());
    CHECK_FALSE(phaseFromString("").has_value());
}

TEST_CASE("grades and certainty accept only 1 to 4") {
    CHECK(gradeFromInt(1) == Grade::Again);
    CHECK(gradeFromInt(4) == Grade::Easy);
    CHECK_FALSE(gradeFromInt(0).has_value());
    CHECK_FALSE(gradeFromInt(5).has_value());
    CHECK(certaintyFromInt(4) == Certainty::Certain);
    CHECK_FALSE(certaintyFromInt(0).has_value());
}

TEST_CASE("an item ref is identified by kind and id together") {
    auto id = Uuid::generate();
    ItemRef asConcept{ItemKind::Concept, id};
    ItemRef asLink{ItemKind::Link, id};
    CHECK(asConcept != asLink);

    std::unordered_set<ItemRef> set{asConcept, asLink, asConcept};
    CHECK(set.size() == 2);

    auto conceptId = KnowledgeObjectId::generate();
    CHECK(ItemRef::forConcept(conceptId) == ItemRef{ItemKind::Concept, conceptId.value()});
}
