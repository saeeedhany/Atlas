#include "atlas/learning/calibration.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

TEST_CASE("learner calibration compares predictions with results") {
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    std::vector<ReviewEvent> events{
        event(item, day(0), Grade::Good, Exercise::Rebuild, Certainty::Certain),
        event(item, day(1), Grade::Again, Exercise::Rebuild, Certainty::Certain),
        event(item, day(2), Grade::Hard, Exercise::Explain, Certainty::Certain),
        event(item, day(3), Grade::Again, Exercise::Explain, Certainty::Certain),
    };
    auto calibration = learnerCalibration(events);
    CHECK(calibration.total == 4);
    CHECK(calibration.levels[3].attempts == 4);
    CHECK(calibration.levels[3].successes == 2);
    CHECK(calibration.levels[3].observedRate() == doctest::Approx(0.5));
    CHECK(calibration.overconfidence == doctest::Approx(0.45));
}

TEST_CASE("calibration of no events is all zero") {
    auto learner = learnerCalibration({});
    CHECK(learner.total == 0);
    CHECK(learner.overconfidence == 0.0);
    CHECK(learner.levels[0].observedRate() == 0.0);

    auto model = modelCalibration({});
    CHECK(model.total == 0);
    CHECK(model.logLoss == 0.0);
}

TEST_CASE("model calibration buckets forecasts and computes log loss") {
    auto model = modelCalibration({{0.95, true}, {0.95, false}, {0.15, false}});
    CHECK(model.total == 3);
    CHECK(model.buckets[9].count == 2);
    CHECK(model.buckets[9].meanPredicted == doctest::Approx(0.95));
    CHECK(model.buckets[9].observedRate == doctest::Approx(0.5));
    CHECK(model.buckets[1].count == 1);
    CHECK(model.logLoss == doctest::Approx(1.069848).epsilon(1e-5));
}

TEST_CASE("forecasts come from replaying the log") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto forecasts = collectForecasts(
        ledger, {event(item, day(0), Grade::Good), event(item, day(2.3065), Grade::Again)}, day(5));
    REQUIRE(forecasts.size() == 1);
    CHECK(forecasts[0].recallChance == doctest::Approx(0.9).epsilon(1e-6));
    CHECK_FALSE(forecasts[0].recalled);
}
