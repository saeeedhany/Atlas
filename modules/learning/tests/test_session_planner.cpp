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

TEST_CASE("a symmetric link goes to the weaker concept even when it is the target") {
    World w;
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    auto link = addLink(w.graph, btree, hash, RelationshipType::AlternativeTo);
    w.review(btree, Grade::Good, day(0));
    w.review(hash, Grade::Hard, day(0));

    auto plan = w.planner.plan(w.states, day(0.5), 2, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{hash, btree});
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forLink(link)});
}

TEST_CASE("maxFocus is a hard cap even when a contrast pair straddles it") {
    World w;
    auto a = addConcept(w.graph, "A");
    auto b = addConcept(w.graph, "B");
    addLink(w.graph, a, b, RelationshipType::AlternativeTo);
    w.review(a, Grade::Again, day(0));
    w.review(b, Grade::Easy, day(0));

    SessionLimits one;
    one.maxFocus = 1;
    auto plan = w.planner.plan(w.states, day(1), 2, one);
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{a});

    SessionLimits none;
    none.maxFocus = 0;
    CHECK(w.planner.plan(w.states, day(1), 2, none).focuses.empty());
}

TEST_CASE("interleaving counts the focuses of a contrast pair, not just unit fronts") {
    World w;
    auto ta = TopicId::generate();
    auto tb = TopicId::generate();
    auto a1 = addConcept(w.graph, "A1", ta);
    auto a2 = addConcept(w.graph, "A2", ta);
    auto a3 = addConcept(w.graph, "A3", ta);
    auto b1 = addConcept(w.graph, "B1", tb);
    addLink(w.graph, a1, a2, RelationshipType::AlternativeTo);
    w.review(a1, Grade::Again, day(0));
    w.review(a3, Grade::Hard, day(0));
    w.review(a2, Grade::Good, day(0));
    w.review(b1, Grade::Easy, day(0));

    auto plan = w.planner.plan(w.states, day(10), 2, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{a1, a2, b1, a3});
}

TEST_CASE("due concepts with equal recall are ordered by transitive leverage") {
    for (bool firstLeads : {true, false}) {
        World w;
        auto first = addConcept(w.graph, "First");
        auto second = addConcept(w.graph, "Second");
        auto leader = firstLeads ? first : second;
        auto follower = firstLeads ? second : first;
        auto middle = addConcept(w.graph, "Middle");
        auto top = addConcept(w.graph, "Top");
        auto single = addConcept(w.graph, "Single");
        addLink(w.graph, middle, leader, RelationshipType::DependsOn);
        addLink(w.graph, top, middle, RelationshipType::DependsOn);
        addLink(w.graph, single, follower, RelationshipType::DependsOn);
        w.review(leader, Grade::Good, day(0));
        w.review(follower, Grade::Good, day(0));

        auto plan = w.planner.plan(w.states, day(30), 0, SessionLimits{});
        REQUIRE(plan.focuses.size() >= 2);
        CHECK(plan.focuses[0].conceptId == leader);
        CHECK(plan.focuses[1].conceptId == follower);
    }
}
