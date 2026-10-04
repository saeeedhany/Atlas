#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/learning/fsrs.hpp"

namespace atlas::learning {

using atlas::core::ItemRef;
using atlas::core::ReviewEvent;

using StateMap = std::unordered_map<ItemRef, MemoryState>;
using BoostFn = std::function<double(const ItemRef&, TimePoint, const StateMap&)>;
using ForecastFn = std::function<void(const ReviewEvent&, double recallChance)>;

inline constexpr int kReplayVersion = 1;

std::vector<ReviewEvent> inReplayOrder(std::vector<ReviewEvent> events, TimePoint now);

class MemoryLedger {
public:
    explicit MemoryLedger(Fsrs fsrs = Fsrs{});

    void apply(StateMap& states, const ReviewEvent& event, const BoostFn& boost = {}) const;

    StateMap replay(std::vector<ReviewEvent> events, TimePoint now, const BoostFn& boost = {},
                    const ForecastFn& forecast = {}) const;

private:
    Fsrs fsrs_;
};

}  // namespace atlas::learning
