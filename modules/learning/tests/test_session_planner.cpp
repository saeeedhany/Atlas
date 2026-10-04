#include "atlas/learning/session_planner.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

namespace {

struct World {
    GraphEngine graph;
    Fsrs fsrs;
    NetworkRules rules{graph, fsrs};
    SessionPlanner planner{graph, rules, fsrs};
    StateMap states;

    void review(const ItemRef& item, Grade grade, TimePoint at) {
        auto it = states.try_emplace(item, MemoryState{item}).first;
        it->second = fsrs.review(it->second, grade, at);
    }
    void review(const KnowledgeObjectId& id, Grade grade, TimePoint at) {
        review(ItemRef::forConcept(id), grade, at);
    }
    std::vector<KnowledgeObjectId> order(const SessionPlan& plan) const {
        std::vector<KnowledgeObjectId> ids;
        for (const auto& focus : plan.focuses) ids.push_back(focus.conceptId);
        return ids;
    }
};

}  // namespace

TEST_CASE("a first session introduces frontier concepts up to the daily limit") {
    World w;
    auto tree = addConcept(w.graph, "Tree");
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    addLink(w.graph, btree, tree, RelationshipType::DependsOn);

    auto plan = w.planner.plan(w.states, day(0), 0, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{tree, hash});
    CHECK(plan.focuses[0].isNew);
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forConcept(tree)});

    SessionLimits oneNew;
    oneNew.newPerDay = 1;
    CHECK(w.order(w.planner.plan(w.states, day(0), 0, oneNew)) == std::vector<KnowledgeObjectId>{tree});
    CHECK(w.planner.plan(w.states, day(0), 5, SessionLimits{}).focuses.empty());
}

TEST_CASE("due items come first, weakest recall first, then new concepts") {
    World w;
    auto tree = addConcept(w.graph, "Tree");
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    addLink(w.graph, btree, tree, RelationshipType::DependsOn);
    w.review(tree, Grade::Good, day(0));
    w.review(hash, Grade::Hard, day(0));

    auto plan = w.planner.plan(w.states, day(3), 0, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{hash, tree, btree});
    CHECK_FALSE(plan.focuses[0].isNew);
    CHECK(plan.focuses[2].isNew);
}

TEST_CASE("a ready link is grouped under its source concept") {
    World w;
    auto tree = addConcept(w.graph, "Tree");
    auto btree = addConcept(w.graph, "B-Tree");
    auto link = addLink(w.graph, btree, tree, RelationshipType::DependsOn);
    w.review(tree, Grade::Good, day(0));
    w.review(btree, Grade::Good, day(0));

    auto plan = w.planner.plan(w.states, day(0), 2, SessionLimits{});
    REQUIRE(plan.focuses.size() == 1);
    CHECK(plan.focuses[0].conceptId == btree);
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forLink(link)});
}

TEST_CASE("a symmetric link goes to the weaker concept and its contrast partner follows") {
    World w;
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    auto link = addLink(w.graph, hash, btree, RelationshipType::AlternativeTo);
    w.review(btree, Grade::Good, day(0));
    w.review(hash, Grade::Hard, day(0));

    auto plan = w.planner.plan(w.states, day(0.5), 2, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{hash, btree});
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forLink(link)});
    CHECK(plan.focuses[1].items == std::vector<ItemRef>{ItemRef::forConcept(btree)});
}

TEST_CASE("topics interleave so one topic never runs three in a row when another is waiting") {
    World w;
    auto a = TopicId::generate();
    auto b = TopicId::generate();
    auto weakest = addConcept(w.graph, "Weakest", a);
    auto weak = addConcept(w.graph, "Weak", a);
    auto fair = addConcept(w.graph, "Fair", a);
    auto other = addConcept(w.graph, "Other", b);
    w.review(weakest, Grade::Again, day(0));
    w.review(weak, Grade::Hard, day(0));
    w.review(fair, Grade::Good, day(0));
    w.review(other, Grade::Easy, day(0));

    auto plan = w.planner.plan(w.states, day(10), 0, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{weakest, weak, other, fair});

    SessionLimits two;
    two.maxFocus = 2;
    CHECK(w.order(w.planner.plan(w.states, day(10), 0, two)) == std::vector<KnowledgeObjectId>{weakest, weak});
}

TEST_CASE("the estimate uses the median focus time when it is known") {
    World w;
    for (const char* title : {"A", "B", "C"}) addConcept(w.graph, title);
    auto plan = w.planner.plan(w.states, day(0), 0, SessionLimits{});
    REQUIRE(plan.focuses.size() == 3);
    CHECK(plan.estimatedDuration == std::chrono::seconds(120));
    auto measured = w.planner.plan(w.states, day(0), 0, SessionLimits{}, std::chrono::milliseconds(20000));
    CHECK(measured.estimatedDuration == std::chrono::seconds(60));
}

TEST_CASE("states for concepts and links no longer in the graph are ignored") {
    World w;
    w.review(ItemRef::forConcept(KnowledgeObjectId::generate()), Grade::Good, day(0));
    w.review(ItemRef::forLink(RelationshipId::generate()), Grade::Good, day(0));
    auto plan = w.planner.plan(w.states, day(30), 0, SessionLimits{});
    CHECK(plan.focuses.empty());
    CHECK(plan.estimatedDuration == std::chrono::seconds(0));
}
