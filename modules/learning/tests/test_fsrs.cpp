#include <cmath>

#include "atlas/learning/fsrs.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using atlas::learning::testing::day;

namespace {
constexpr double kEps = 1e-8;

MemoryState freshState() {
    MemoryState state;
    state.item = ItemRef::forConcept(KnowledgeObjectId::generate());
    return state;
}
}

TEST_CASE("retrievability follows the FSRS-6 power curve") {
    Fsrs fsrs;
    CHECK(fsrs.retrievability(10.0, 10.0) == doctest::Approx(0.9).epsilon(kEps));
    CHECK(fsrs.retrievability(5.0, 10.0) == doctest::Approx(0.9403442889).epsilon(kEps));
    CHECK(fsrs.retrievability(30.0, 10.0) == doctest::Approx(0.8093881036).epsilon(kEps));
    CHECK(fsrs.retrievability(0.0, 10.0) == doctest::Approx(1.0));
}

TEST_CASE("initial stability and difficulty match the reference values") {
    Fsrs fsrs;
    CHECK(fsrs.initialStability(Grade::Again) == doctest::Approx(0.212).epsilon(kEps));
    CHECK(fsrs.initialStability(Grade::Good) == doctest::Approx(2.3065).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Again) == doctest::Approx(6.4133).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Hard) == doctest::Approx(5.1121707056).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Good) == doctest::Approx(2.1181039705).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Easy) == doctest::Approx(1.0).epsilon(kEps));
}

TEST_CASE("difficulty update applies damping and mean reversion") {
    Fsrs fsrs;
    CHECK(fsrs.nextDifficulty(5.0, Grade::Again) == doctest::Approx(8.3417623693).epsilon(kEps));
    CHECK(fsrs.nextDifficulty(5.0, Grade::Good) == doctest::Approx(4.9902283693).epsilon(kEps));
    CHECK(fsrs.nextDifficulty(5.0, Grade::Easy) == doctest::Approx(3.3144613693).epsilon(kEps));
}

TEST_CASE("stability updates match the reference values") {
    Fsrs fsrs;
    CHECK(fsrs.recallStability(5.0, 10.0, 0.9, Grade::Good) == doctest::Approx(32.0267294820).epsilon(kEps));
    CHECK(fsrs.recallStability(5.0, 10.0, 0.9, Grade::Hard) == doctest::Approx(23.2468751105).epsilon(kEps));
    CHECK(fsrs.recallStability(5.0, 10.0, 0.9, Grade::Easy) == doctest::Approx(51.2538616468).epsilon(kEps));
    CHECK(fsrs.recallStability(5.0, 10.0, 0.7, Grade::Good) == doctest::Approx(81.7063939568).epsilon(kEps));
    CHECK(fsrs.forgetStability(5.0, 10.0, 0.9) == doctest::Approx(1.3919869730).epsilon(kEps));
    CHECK(fsrs.shortTermStability(2.3065, Grade::Good) == doctest::Approx(2.3065).epsilon(kEps));
    CHECK(fsrs.shortTermStability(2.3065, Grade::Again) == doctest::Approx(0.7750839829).epsilon(kEps));
}

TEST_CASE("the interval at 90 percent retention equals stability and is capped") {
    Fsrs fsrs;
    CHECK(fsrs.intervalDays(10.0) == doctest::Approx(10.0).epsilon(kEps));
    CHECK(fsrs.intervalDays(1e9) == doctest::Approx(36500.0));
}

TEST_CASE("a full review history follows the reference scenario") {
    Fsrs fsrs;
    MemoryState state = freshState();

    state = fsrs.review(state, Grade::Good, day(0));
    CHECK(state.phase == Phase::Review);
    CHECK(state.stability == doctest::Approx(2.3065).epsilon(kEps));
    CHECK(state.difficulty == doctest::Approx(2.1181039705).epsilon(kEps));
    REQUIRE(state.dueAt.has_value());
    CHECK(elapsedDays(day(0), *state.dueAt) == doctest::Approx(2.3065).epsilon(1e-6));

    state = fsrs.review(state, Grade::Good, day(2));
    CHECK(state.stability == doctest::Approx(10.9643323358).epsilon(kEps));
    CHECK(state.difficulty == doctest::Approx(2.1112142358).epsilon(kEps));

    state = fsrs.review(state, Grade::Again, day(22));
    CHECK(state.phase == Phase::Relearning);
    CHECK(state.lapseCount == 1);
    CHECK(state.reviewCount == 3);
    CHECK(state.stability == doctest::Approx(1.6595898557).epsilon(kEps));
    CHECK(state.difficulty == doctest::Approx(7.3922381323).epsilon(kEps));
}

TEST_CASE("a first review rated again starts the learning phase") {
    Fsrs fsrs;
    auto state = fsrs.review(freshState(),
                             Grade::Again, day(0));
    CHECK(state.phase == Phase::Learning);
    CHECK(state.lapseCount == 0);
}

TEST_CASE("the first review boost multiplies initial stability only") {
    Fsrs fsrs;
    MemoryState fresh = freshState();
    auto boosted = fsrs.review(fresh, Grade::Good, day(0), 1.2);
    CHECK(boosted.stability == doctest::Approx(2.3065 * 1.2).epsilon(kEps));

    auto second = fsrs.review(boosted, Grade::Good, day(3), 5.0);
    auto unboosted = fsrs.review(boosted, Grade::Good, day(3));
    CHECK(second == unboosted);
}

TEST_CASE("a review dated before the previous one counts as zero elapsed days") {
    Fsrs fsrs;
    auto state = fsrs.review(freshState(),
                             Grade::Good, day(5));
    auto earlier = fsrs.review(state, Grade::Good, day(4));
    CHECK(std::isfinite(earlier.stability));
    CHECK(earlier.stability >= state.stability);
    CHECK(elapsedDays(day(5), day(4)) == 0.0);
}

TEST_CASE("an item never reviewed has zero recall chance") {
    Fsrs fsrs;
    MemoryState fresh = freshState();
    CHECK(fsrs.recallChance(fresh, day(0)) == 0.0);
}
