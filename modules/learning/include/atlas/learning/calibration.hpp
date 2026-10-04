#pragma once

#include <array>
#include <vector>

#include "atlas/learning/memory_ledger.hpp"

namespace atlas::learning {

using atlas::core::Certainty;

double expectedSuccess(Certainty certainty);

struct LevelStats {
    int attempts = 0;
    int successes = 0;
    double observedRate() const;
};

struct LearnerCalibration {
    std::array<LevelStats, 4> levels{};
    double overconfidence = 0.0;
    int total = 0;
};

LearnerCalibration learnerCalibration(const std::vector<ReviewEvent>& events);

struct Forecast {
    double recallChance;
    bool recalled;
};

struct ForecastBucket {
    int count = 0;
    double meanPredicted = 0.0;
    double observedRate = 0.0;
};

struct ModelCalibration {
    std::array<ForecastBucket, 10> buckets{};
    double logLoss = 0.0;
    int total = 0;
};

ModelCalibration modelCalibration(const std::vector<Forecast>& forecasts);

std::vector<Forecast> collectForecasts(const MemoryLedger& ledger, std::vector<ReviewEvent> events,
                                       TimePoint now, const BoostFn& boost = {});

}  // namespace atlas::learning
