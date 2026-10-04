#pragma once

#include <array>

#include "atlas/core/memory.hpp"

namespace atlas::learning {

using atlas::core::Grade;
using atlas::core::MemoryState;
using atlas::core::TimePoint;

struct FsrsParameters {
    std::array<double, 21> w{0.212,  1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194,
                             0.001,  1.8722, 0.1666, 0.796,  1.4835, 0.0614, 0.2629,
                             1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, 0.1542};
    double desiredRetention = 0.9;
    double maximumIntervalDays = 36500.0;
};

double elapsedDays(TimePoint from, TimePoint to);

class Fsrs {
public:
    explicit Fsrs(FsrsParameters parameters = {});

    double retrievability(double elapsed, double stability) const;
    double recallChance(const MemoryState& state, TimePoint now) const;
    double intervalDays(double stability) const;

    double initialStability(Grade grade) const;
    double initialDifficulty(Grade grade) const;
    double nextDifficulty(double difficulty, Grade grade) const;
    double shortTermStability(double stability, Grade grade) const;
    double recallStability(double difficulty, double stability, double retrievability, Grade grade) const;
    double forgetStability(double difficulty, double stability, double retrievability) const;

    MemoryState review(const MemoryState& state, Grade grade, TimePoint at,
                       double firstReviewBoost = 1.0) const;

    const FsrsParameters& parameters() const { return parameters_; }

private:
    double rawInitialDifficulty(Grade grade) const;

    FsrsParameters parameters_;
    double decay_;
    double factor_;
};

}  // namespace atlas::learning
