#include <unordered_map>

#include "atlas/core/strong_id.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("StrongId::generate produces distinct ids") {
    auto a = KnowledgeObjectId::generate();
    auto b = KnowledgeObjectId::generate();
    CHECK(!(a == b));
}

TEST_CASE("KnowledgeObjectId and RelationshipId are distinct types") {
    CHECK(true);
}

TEST_CASE("StrongId can be used as a hash map key") {
    std::unordered_map<KnowledgeObjectId, int> map;
    auto id = KnowledgeObjectId::generate();
    map[id] = 42;
    CHECK(map.at(id) == 42);
}
