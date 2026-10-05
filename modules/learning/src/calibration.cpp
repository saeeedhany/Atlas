#include "atlas/learning/calibration.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::learning {

namespace {
constexpr double kProbabilityFloor = 1e-6;
}

double expectedSuccess(Certainty certainty) {
    switch (certainty) {
        case Certainty::Guess: return 0.25;
        case Certainty::Unsure: return 0.50;
        case Certainty::FairlySure: return 0.75;
        case Certainty::Certain: return 0.95;
    }
    return 0.50;
}

double LevelStats::observedRate() const {
    return attempts == 0 ? 0.0 : static_cast<double>(successes) / attempts;
}

LearnerCalibration learnerCalibration(const std::vector<ReviewEvent>& events) {
    LearnerCalibration result;
    double expected = 0.0;
    int successes = 0;
    for (const auto& event : events) {
        auto& level = result.levels[static_cast<size_t>(event.predicted) - 1];
        bool success = event.grade != Grade::Again;
        ++level.attempts;
        if (success) {
            ++level.successes;
            ++successes;
        }
        expected += expectedSuccess(event.predicted);
        ++result.total;
    }
    if (result.total > 0) result.overconfidence = (expected - successes) / result.total;
    return result;
}

ModelCalibration modelCalibration(const std::vector<Forecast>& forecasts) {
    ModelCalibration result;
    double loss = 0.0;
    for (const auto& forecast : forecasts) {
        auto index = std::min<size_t>(static_cast<size_t>(forecast.recallChance * 10.0), 9);
        auto& bucket = result.buckets[index];
        ++bucket.count;
        bucket.meanPredicted += forecast.recallChance;
        bucket.observedRate += forecast.recalled ? 1.0 : 0.0;
        double p = std::clamp(forecast.recallChance, kProbabilityFloor, 1.0 - kProbabilityFloor);
        loss -= forecast.recalled ? std::log(p) : std::log(1.0 - p);
        ++result.total;
    }
    for (auto& bucket : result.buckets) {
        if (bucket.count == 0) continue;
        bucket.meanPredicted /= bucket.count;
        bucket.observedRate /= bucket.count;
    }
    if (result.total > 0) result.logLoss = loss / result.total;
    return result;
}

std::vector<Forecast> collectForecasts(const MemoryLedger& ledger, std::vector<ReviewEvent> events,
                                       TimePoint now, const BoostFn& boost) {
    std::vector<Forecast> forecasts;
    ledger.replay(std::move(events), now, boost, [&](const ReviewEvent& event, double chance) {
        forecasts.push_back(Forecast{chance, event.grade != Grade::Again});
    });
    return forecasts;
}

}
