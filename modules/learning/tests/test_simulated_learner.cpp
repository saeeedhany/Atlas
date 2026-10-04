#include <random>
#include <string>

#include "atlas/learning/memory_ledger.hpp"
#include "atlas/learning/network_rules.hpp"
#include "atlas/learning/session_planner.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

TEST_CASE("a simulated learner keeps retention near the target over 90 days") {
    GraphEngine graph;
    std::vector<std::vector<KnowledgeObjectId>> chains(10);
    for (size_t chain = 0; chain < chains.size(); ++chain) {
        for (int level = 0; level < 4; ++level) {
            auto title = "C" + std::to_string(chain) + "L" + std::to_string(level);
            auto id = addConcept(graph, title.c_str());
            if (level > 0) addLink(graph, id, chains[chain].back(), RelationshipType::DependsOn);
            chains[chain].push_back(id);
        }
    }
    for (size_t chain = 0; chain + 1 < chains.size(); chain += 2) {
        addLink(graph, chains[chain][1], chains[chain + 1][1], RelationshipType::AlternativeTo);
    }

    Fsrs fsrs;
    NetworkRules rules(graph, fsrs);
    SessionPlanner planner(graph, rules, fsrs);
    MemoryLedger ledger(fsrs);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> coin(0.0, 1.0);

    StateMap states;
    int reviewed = 0;
    int recalled = 0;
    int max_daily_focuses = 0;
    int late_period_reviewed = 0;
    for (int d = 0; d < 90; ++d) {
        TimePoint now = day(d + 0.375);
        auto session = planner.plan(states, now, 0, SessionLimits{});
        CHECK(session.focuses.size() <= static_cast<size_t>(SessionLimits{}.maxFocus));
        max_daily_focuses = std::max(max_daily_focuses, static_cast<int>(session.focuses.size()));
        for (const auto& focus : session.focuses) {
            for (const auto& item : focus.items) {
                auto it = states.find(item);
                bool first = it == states.end() || it->second.phase == Phase::New;
                Grade grade = Grade::Good;
                if (!first) {
                    bool remembered = coin(rng) < fsrs.recallChance(it->second, now);
                    grade = remembered ? Grade::Good : Grade::Again;
                    if (d >= 60) {
                        ++reviewed;
                        ++late_period_reviewed;
                        if (remembered) ++recalled;
                    }
                }
                ledger.apply(states, event(item, now, grade), rules.boostFn());
            }
        }
    }

    int introduced = 0;
    for (const auto& id : graph.allNodeIds()) {
        if (rules.isIntroduced(ItemRef::forConcept(id), states)) ++introduced;
    }
    CHECK(introduced == 40);
    REQUIRE(reviewed > 0);
    double retention = static_cast<double>(recalled) / reviewed;
    CHECK(retention >= 0.80);
    CHECK(retention <= 0.98);

    int total_items = graph.nodeCount() + graph.edgeCount();
    double late_period_days = 30.0;
    double avg_daily_reviewed = static_cast<double>(late_period_reviewed) / late_period_days;
    double max_allowed_avg = static_cast<double>(total_items) / 3.0;
    CHECK(avg_daily_reviewed <= max_allowed_avg);
}
