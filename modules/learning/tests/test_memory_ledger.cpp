#include <algorithm>

#include "atlas/learning/memory_ledger.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

TEST_CASE("apply gives the same state as a direct review") {
    MemoryLedger ledger;
    Fsrs fsrs;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());

    StateMap states;
    ledger.apply(states, event(item, day(0), Grade::Good));
    CHECK(states.at(item) == fsrs.review(MemoryState{item}, Grade::Good, day(0)));
}

TEST_CASE("replay does not depend on the order events arrive in") {
    MemoryLedger ledger;
    auto a = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto b = ItemRef::forLink(RelationshipId::generate());
    std::vector<ReviewEvent> events{event(a, day(0), Grade::Good), event(b, day(0.5), Grade::Hard),
                                    event(a, day(3), Grade::Again), event(b, day(4), Grade::Good),
                                    event(a, day(4), Grade::Good)};
    auto forward = ledger.replay(events, day(10));
    std::reverse(events.begin(), events.end());
    auto backward = ledger.replay(events, day(10));
    CHECK(forward.at(a) == backward.at(a));
    CHECK(forward.at(b) == backward.at(b));
    CHECK(forward.at(a).reviewCount == 3);
}

TEST_CASE("replay clamps review times in the future to now") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto states = ledger.replay({event(item, day(10), Grade::Good)}, day(5));
    CHECK(states.at(item).lastReviewedAt == day(5));
}

TEST_CASE("the boost applies to the first review only") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    BoostFn doubleIt = [](const ItemRef&, TimePoint, const StateMap&) { return 2.0; };

    StateMap states;
    ledger.apply(states, event(item, day(0), Grade::Good), doubleIt);
    CHECK(states.at(item).stability == doctest::Approx(2.3065 * 2.0));

    auto before = states.at(item);
    ledger.apply(states, event(item, day(5), Grade::Good), doubleIt);
    CHECK(states.at(item) == Fsrs{}.review(before, Grade::Good, day(5)));
}

TEST_CASE("a forecast is reported for every review except the first") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    std::vector<double> forecasts;
    ledger.replay({event(item, day(0), Grade::Good), event(item, day(2.3065), Grade::Again),
                   event(item, day(5), Grade::Good)},
                  day(10), {}, [&](const ReviewEvent&, double chance) { forecasts.push_back(chance); });
    REQUIRE(forecasts.size() == 2);
    CHECK(forecasts[0] == doctest::Approx(0.9).epsilon(1e-6));
}
