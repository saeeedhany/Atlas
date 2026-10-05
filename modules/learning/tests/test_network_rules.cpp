#include "atlas/learning/network_rules.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

namespace {

struct Fixture {
    GraphEngine graph;
    KnowledgeObjectId tree = addConcept(graph, "Tree");
    KnowledgeObjectId btree = addConcept(graph, "B-Tree");
    KnowledgeObjectId index = addConcept(graph, "Index");
    KnowledgeObjectId hash = addConcept(graph, "Hash Table");
    RelationshipId btreeNeedsTree = addLink(graph, btree, tree, RelationshipType::DependsOn);
    RelationshipId indexNeedsBtree = addLink(graph, index, btree, RelationshipType::DependsOn);
    RelationshipId hashVsBtree = addLink(graph, hash, btree, RelationshipType::AlternativeTo);
    Fsrs fsrs;
    NetworkRules rules{graph, fsrs};
    StateMap states;

    void review(const KnowledgeObjectId& conceptId, Grade grade, TimePoint at) {
        auto item = ItemRef::forConcept(conceptId);
        auto it = states.try_emplace(item, MemoryState{item}).first;
        it->second = fsrs.review(it->second, grade, at);
    }
};

}

TEST_CASE("the frontier starts with concepts that have no prerequisites, highest leverage first") {
    Fixture f;
    CHECK(f.rules.frontier(day(0), f.states) == std::vector<KnowledgeObjectId>{f.tree, f.hash});
    CHECK(f.rules.leverage(f.tree) == 2);
    CHECK(f.rules.leverage(f.hash) == 0);
}

TEST_CASE("a solid prerequisite opens the frontier to the concept that needs it") {
    Fixture f;
    f.review(f.tree, Grade::Good, day(0));
    CHECK(f.rules.isSolid(f.tree, day(3), f.states));
    CHECK(f.rules.frontier(day(3), f.states) == std::vector<KnowledgeObjectId>{f.btree, f.hash});
}

TEST_CASE("a failed or faded prerequisite keeps the frontier closed") {
    Fixture failed;
    failed.review(failed.tree, Grade::Again, day(0));
    CHECK_FALSE(failed.rules.isOnFrontier(failed.btree, day(0), failed.states));

    Fixture faded;
    faded.review(faded.tree, Grade::Good, day(0));
    CHECK_FALSE(faded.rules.isSolid(faded.tree, day(10), faded.states));
    CHECK_FALSE(faded.rules.isOnFrontier(faded.btree, day(10), faded.states));
}

TEST_CASE("concepts in a dependency cycle do not block each other") {
    GraphEngine graph;
    auto a = addConcept(graph, "A");
    auto b = addConcept(graph, "B");
    addLink(graph, a, b, RelationshipType::DependsOn);
    addLink(graph, b, a, RelationshipType::DependsOn);
    NetworkRules rules(graph);
    CHECK(rules.isOnFrontier(a, day(0), {}));
    CHECK(rules.isOnFrontier(b, day(0), {}));
}

TEST_CASE("every concept in a three way dependency cycle is on the frontier") {
    GraphEngine graph;
    auto a = addConcept(graph, "A");
    auto b = addConcept(graph, "B");
    auto c = addConcept(graph, "C");
    addLink(graph, a, b, RelationshipType::DependsOn);
    addLink(graph, b, c, RelationshipType::DependsOn);
    addLink(graph, c, a, RelationshipType::DependsOn);
    NetworkRules rules(graph);
    CHECK(rules.isOnFrontier(a, day(0), {}));
    CHECK(rules.isOnFrontier(b, day(0), {}));
    CHECK(rules.isOnFrontier(c, day(0), {}));
    CHECK(rules.frontier(day(0), {}).size() == 3);
}

TEST_CASE("an unsolid prerequisite outside a cycle blocks only the concept that needs it") {
    GraphEngine graph;
    auto a = addConcept(graph, "A");
    auto b = addConcept(graph, "B");
    auto x = addConcept(graph, "X");
    addLink(graph, a, b, RelationshipType::DependsOn);
    addLink(graph, b, a, RelationshipType::DependsOn);
    addLink(graph, a, x, RelationshipType::DependsOn);
    NetworkRules rules(graph);
    CHECK_FALSE(rules.isOnFrontier(a, day(0), {}));
    CHECK(rules.isOnFrontier(b, day(0), {}));
}

TEST_CASE("the schema boost grows with the recall chance of prerequisites") {
    Fixture f;
    CHECK(f.rules.schemaBoost(ItemRef::forConcept(f.tree), day(0), f.states) == 1.0);
    CHECK(f.rules.schemaBoost(ItemRef::forConcept(f.btree), day(0), f.states) == 1.0);

    f.review(f.tree, Grade::Good, day(0));
    CHECK(f.rules.schemaBoost(ItemRef::forConcept(f.btree), day(0), f.states) == doctest::Approx(1.2));
    CHECK(f.rules.schemaBoost(ItemRef::forLink(f.btreeNeedsTree), day(0), f.states) == 1.0);
    CHECK(f.rules.boostFn()(ItemRef::forConcept(f.btree), day(0), f.states) == doctest::Approx(1.2));
}

TEST_CASE("a link is ready once both of its concepts are introduced") {
    Fixture f;
    f.review(f.tree, Grade::Good, day(0));
    CHECK_FALSE(f.rules.isLinkReady(f.btreeNeedsTree, f.states));
    f.review(f.btree, Grade::Good, day(1));
    CHECK(f.rules.isLinkReady(f.btreeNeedsTree, f.states));
    CHECK_FALSE(f.rules.isLinkReady(RelationshipId::generate(), f.states));
}

TEST_CASE("contrast partners are found from either side") {
    Fixture f;
    CHECK(contrastPartners(f.graph, f.btree) == std::vector<KnowledgeObjectId>{f.hash});
    CHECK(contrastPartners(f.graph, f.hash) == std::vector<KnowledgeObjectId>{f.btree});
    CHECK(contrastPartners(f.graph, f.tree).empty());
}
