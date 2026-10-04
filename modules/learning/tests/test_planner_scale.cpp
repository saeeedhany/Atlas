#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include "atlas/learning/session_planner.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

namespace {

constexpr int kConceptCount = 10'000;
constexpr int kLinkAttempts = 30'000;
constexpr int kCyclePairs = 150;

double secondsToPlan(const SessionPlanner& planner, const StateMap& states, TimePoint now) {
    auto start = std::chrono::steady_clock::now();
    auto plan = planner.plan(states, now, 0, SessionLimits{});
    std::chrono::duration<double> taken = std::chrono::steady_clock::now() - start;
    CHECK_FALSE(plan.focuses.empty());
    return taken.count();
}

}  // namespace

TEST_CASE("plan stays fast at 10,000 concepts and ~30,000 links" * doctest::test_suite("scale")) {
    GraphEngine graph;
    std::vector<KnowledgeObjectId> ids;
    ids.reserve(kConceptCount);
    for (int i = 0; i < kConceptCount; ++i) {
        auto object = KnowledgeObject::create("Concept " + std::to_string(i)).value();
        ids.push_back(object.id());
        REQUIRE(graph.addNode(std::move(object)).hasValue());
    }

    std::mt19937 rng(42);
    for (int i = 0; i < kLinkAttempts; ++i) {
        int source = std::uniform_int_distribution<int>(1, kConceptCount - 1)(rng);
        int target = std::uniform_int_distribution<int>(0, source - 1)(rng);
        auto link = Relationship::create(ids[source], ids[target], RelationshipType::DependsOn);
        if (link.hasValue()) (void)graph.addEdge(std::move(link).value());
    }
    for (int pair = 0; pair < kCyclePairs; ++pair) {
        int i = pair * 2;
        for (auto [from, to] : {std::pair{i, i + 1}, std::pair{i + 1, i}}) {
            auto link = Relationship::create(ids[from], ids[to], RelationshipType::DependsOn);
            if (link.hasValue()) (void)graph.addEdge(std::move(link).value());
        }
    }

    Fsrs fsrs;
    NetworkRules rules(graph, fsrs);
    SessionPlanner planner(graph, rules, fsrs);

    StateMap empty;
    double fresh = secondsToPlan(planner, empty, day(0));

    StateMap halfReviewed;
    for (int i = 0; i < kConceptCount; i += 2) {
        auto item = ItemRef::forConcept(ids[i]);
        halfReviewed.emplace(item, fsrs.review(MemoryState{item}, Grade::Good, day(0)));
    }
    double reviewed = secondsToPlan(planner, halfReviewed, day(30));

    std::cout << "plan at 10k concepts: empty " << fresh << " s, half reviewed " << reviewed << " s\n";
    CHECK(fresh < 2.0);
    CHECK(reviewed < 2.0);
}
