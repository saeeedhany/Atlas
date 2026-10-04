#include "atlas/learning/rebuild_grader.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;
using std::chrono::milliseconds;

namespace {

struct Fixture {
    GraphEngine graph;
    KnowledgeObjectId tree = addConcept(graph, "Tree");
    KnowledgeObjectId btree = addConcept(graph, "B-Tree");
    KnowledgeObjectId index = addConcept(graph, "Index");
    KnowledgeObjectId hash = addConcept(graph, "Hash Table");
    KnowledgeObjectId skiplist = addConcept(graph, "Skip List");
    KnowledgeObjectId array = addConcept(graph, "Array");
    KnowledgeObjectId heap = addConcept(graph, "Heap");
    RelationshipId needsTree = addLink(graph, btree, tree, RelationshipType::DependsOn);
    RelationshipId indexNeeds = addLink(graph, index, btree, RelationshipType::DependsOn);
    RelationshipId versusHash = addLink(graph, btree, hash, RelationshipType::AlternativeTo);
    RelationshipId skipVersusTree = addLink(graph, skiplist, tree, RelationshipType::AlternativeTo);
    RelationshipId heapNeedsArray = addLink(graph, heap, array, RelationshipType::DependsOn);
    RelationshipId treeNeedsArray = addLink(graph, tree, array, RelationshipType::DependsOn);
    RebuildGrader grader{graph};
    std::vector<RelationshipId> hidden{needsTree, indexNeeds, versusHash};

    RebuildAnswer answer(std::vector<RecalledLink> recalled) const {
        RebuildAnswer result{btree};
        result.recalled = std::move(recalled);
        return result;
    }
};

}  // namespace

TEST_CASE("every link recalled correctly without hints is good") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.tree, RelationshipType::DependsOn, true},
                                           {f.index, RelationshipType::DependsOn, false},
                                           {f.hash, RelationshipType::AlternativeTo, false}}),
                                 f.hidden, std::nullopt);
    REQUIRE(graded.size() == 3);
    for (const auto& link : graded) {
        CHECK(link.grade == Grade::Good);
        CHECK(link.hintsUsed == 0);
        CHECK_FALSE(link.wrongTarget.has_value());
    }
}

TEST_CASE("a wrong direction or type is hard, but symmetric links accept either direction") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.tree, RelationshipType::DependsOn, false},
                                           {f.index, RelationshipType::Uses, false},
                                           {f.hash, RelationshipType::AlternativeTo, true}}),
                                 f.hidden, std::nullopt);
    CHECK(graded[0].grade == Grade::Hard);
    CHECK(graded[1].grade == Grade::Hard);
    CHECK(graded[2].grade == Grade::Good);
}

TEST_CASE("a hinted link is hard when recalled and again when missed") {
    Fixture f;
    auto answer = f.answer({{f.tree, RelationshipType::DependsOn, true}});
    answer.hinted = {f.tree, f.index};
    auto graded = f.grader.grade(answer, f.hidden, std::nullopt);
    CHECK(graded[0].grade == Grade::Hard);
    CHECK(graded[0].hintsUsed == 1);
    CHECK(graded[1].grade == Grade::Again);
    CHECK(graded[1].hintsUsed == 1);
    CHECK(graded[2].grade == Grade::Again);
    CHECK(graded[2].hintsUsed == 0);
}

TEST_CASE("easy needs a certain prediction, a fast answer, and enough history") {
    Fixture f;
    auto answer = f.answer({{f.tree, RelationshipType::DependsOn, true}});
    answer.predicted = Certainty::Certain;
    answer.responseTime = milliseconds(5000);
    std::vector<RelationshipId> onlyTree{f.needsTree};

    CHECK(f.grader.grade(answer, onlyTree, milliseconds(10000))[0].grade == Grade::Easy);
    CHECK(f.grader.grade(answer, onlyTree, std::nullopt)[0].grade == Grade::Good);

    answer.responseTime = milliseconds(15000);
    CHECK(f.grader.grade(answer, onlyTree, milliseconds(10000))[0].grade == Grade::Good);

    answer.responseTime = milliseconds(5000);
    answer.predicted = Certainty::FairlySure;
    CHECK(f.grader.grade(answer, onlyTree, milliseconds(10000))[0].grade == Grade::Good);
}

TEST_CASE("a missed link records the contrast partner named instead of it") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.skiplist, RelationshipType::DependsOn, true}}),
                                 f.hidden, std::nullopt);
    CHECK(graded[0].grade == Grade::Again);
    CHECK(graded[0].wrongTarget == f.skiplist);
    CHECK_FALSE(graded[1].wrongTarget.has_value());
    CHECK_FALSE(graded[2].wrongTarget.has_value());
}

TEST_CASE("a missed link records a named concept that shares a neighbor with its target") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.heap, RelationshipType::DependsOn, true}}),
                                 f.hidden, std::nullopt);
    CHECK(graded[0].wrongTarget == f.heap);
    CHECK_FALSE(graded[1].wrongTarget.has_value());
}

TEST_CASE("naming a hidden neighbor twice is not reported as a confusion") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.tree, RelationshipType::DependsOn, true},
                                           {f.tree, RelationshipType::DependsOn, true}}),
                                 f.hidden, std::nullopt);
    for (const auto& link : graded) CHECK_FALSE(link.wrongTarget.has_value());
}

TEST_CASE("two links to the same concept are matched by type first") {
    GraphEngine graph;
    auto a = addConcept(graph, "A");
    auto b = addConcept(graph, "B");
    auto uses = addLink(graph, a, b, RelationshipType::Uses);
    auto needs = addLink(graph, a, b, RelationshipType::DependsOn);
    RebuildGrader grader(graph);
    RebuildAnswer answer{a};
    answer.recalled = {{b, RelationshipType::DependsOn, true}, {b, RelationshipType::Uses, true}};
    auto graded = grader.grade(answer, {uses, needs}, std::nullopt);
    CHECK(graded[0].grade == Grade::Good);
    CHECK(graded[1].grade == Grade::Good);
}

TEST_CASE("the median response needs at least 20 rebuild events") {
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    std::vector<ReviewEvent> events;
    for (int i = 1; i <= 30; ++i) {
        events.push_back(event(item, day(i), Grade::Good, Exercise::Explain, Certainty::Unsure, milliseconds(1)));
    }
    for (int i = 1; i <= 19; ++i) {
        events.push_back(event(item, day(i), Grade::Good, Exercise::Rebuild, Certainty::Unsure, milliseconds(i * 1000)));
    }
    CHECK_FALSE(medianRebuildResponse(events).has_value());

    events.push_back(event(item, day(20), Grade::Good, Exercise::Rebuild, Certainty::Unsure, milliseconds(20000)));
    CHECK(medianRebuildResponse(events) == milliseconds(11000));
}
