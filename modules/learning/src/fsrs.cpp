#include "atlas/learning/fsrs.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::learning {

using atlas::core::Phase;

namespace {

constexpr double kMinStability = 0.001;
constexpr double kMinDifficulty = 1.0;
constexpr double kMaxDifficulty = 10.0;

double clampStability(double stability) { return std::max(stability, kMinStability); }
double clampDifficulty(double difficulty) { return std::clamp(difficulty, kMinDifficulty, kMaxDifficulty); }
int rating(Grade grade) { return static_cast<int>(grade); }

std::chrono::system_clock::duration toDuration(double days) {
    return std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::duration<double, std::ratio<86400>>(days));
}

}  // namespace

double elapsedDays(TimePoint from, TimePoint to) {
    std::chrono::duration<double, std::ratio<86400>> days = to - from;
    return std::max(days.count(), 0.0);
}

Fsrs::Fsrs(FsrsParameters parameters)
    : parameters_(parameters),
      decay_(-parameters.w[20]),
      factor_(std::pow(0.9, 1.0 / decay_) - 1.0) {}

double Fsrs::retrievability(double elapsed, double stability) const {
    return std::pow(1.0 + factor_ * elapsed / stability, decay_);
}

double Fsrs::recallChance(const MemoryState& state, TimePoint now) const {
    if (state.phase == Phase::New || !state.lastReviewedAt) return 0.0;
    if (!std::isfinite(state.stability) || state.stability <= 0.0) return 0.0;
    return retrievability(elapsedDays(*state.lastReviewedAt, now), state.stability);
}

double Fsrs::intervalDays(double stability) const {
    double raw = stability / factor_ * (std::pow(parameters_.desiredRetention, 1.0 / decay_) - 1.0);
    return std::min(raw, parameters_.maximumIntervalDays);
}

double Fsrs::initialStability(Grade grade) const {
    return clampStability(parameters_.w[rating(grade) - 1]);
}

double Fsrs::rawInitialDifficulty(Grade grade) const {
    const auto& w = parameters_.w;
    return w[4] - std::exp(w[5] * (rating(grade) - 1)) + 1.0;
}

double Fsrs::initialDifficulty(Grade grade) const {
    return clampDifficulty(rawInitialDifficulty(grade));
}

double Fsrs::nextDifficulty(double difficulty, Grade grade) const {
    const auto& w = parameters_.w;
    double delta = -w[6] * (rating(grade) - 3);
    double damped = difficulty + (10.0 - difficulty) * delta / 9.0;
    return clampDifficulty(w[7] * rawInitialDifficulty(Grade::Easy) + (1.0 - w[7]) * damped);
}

double Fsrs::shortTermStability(double stability, Grade grade) const {
    const auto& w = parameters_.w;
    double increase = std::exp(w[17] * (rating(grade) - 3 + w[18])) * std::pow(stability, -w[19]);
    if (grade != Grade::Again) increase = std::max(increase, 1.0);
    return clampStability(stability * increase);
}

double Fsrs::recallStability(double difficulty, double stability, double retrievability,
                             Grade grade) const {
    const auto& w = parameters_.w;
    double hardPenalty = grade == Grade::Hard ? w[15] : 1.0;
    double easyBonus = grade == Grade::Easy ? w[16] : 1.0;
    double growth = std::exp(w[8]) * (11.0 - difficulty) * std::pow(stability, -w[9]) *
                    (std::exp((1.0 - retrievability) * w[10]) - 1.0) * hardPenalty * easyBonus;
    return clampStability(stability * (1.0 + growth));
}

double Fsrs::forgetStability(double difficulty, double stability, double retrievability) const {
    const auto& w = parameters_.w;
    double longTerm = w[11] * std::pow(difficulty, -w[12]) * (std::pow(stability + 1.0, w[13]) - 1.0) *
                      std::exp((1.0 - retrievability) * w[14]);
    double shortTerm = stability / std::exp(w[17] * w[18]);
    return clampStability(std::min(longTerm, shortTerm));
}

MemoryState Fsrs::review(const MemoryState& state, Grade grade, TimePoint at,
                         double firstReviewBoost) const {
    MemoryState next = state;
    if (state.phase == Phase::New || !state.lastReviewedAt) {
        next.stability = clampStability(initialStability(grade) * firstReviewBoost);
        next.difficulty = initialDifficulty(grade);
        next.phase = grade == Grade::Again ? Phase::Learning : Phase::Review;
    } else {
        double elapsed = elapsedDays(*state.lastReviewedAt, at);
        double recall = retrievability(elapsed, state.stability);
        if (elapsed < 1.0) {
            next.stability = shortTermStability(state.stability, grade);
        } else if (grade == Grade::Again) {
            next.stability = forgetStability(state.difficulty, state.stability, recall);
        } else {
            next.stability = recallStability(state.difficulty, state.stability, recall, grade);
        }
        next.difficulty = nextDifficulty(state.difficulty, grade);
        if (grade == Grade::Again && state.phase == Phase::Review) {
            next.phase = Phase::Relearning;
            ++next.lapseCount;
        } else if (grade != Grade::Again) {
            next.phase = Phase::Review;
        }
    }
    ++next.reviewCount;
    next.lastReviewedAt = at;
    next.dueAt = at + toDuration(intervalDays(next.stability));
    return next;
}

}  // namespace atlas::learning
