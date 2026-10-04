#include "atlas/learning/memory_ledger.hpp"

#include <algorithm>

namespace atlas::learning {

using atlas::core::Phase;

std::vector<ReviewEvent> inReplayOrder(std::vector<ReviewEvent> events, TimePoint now) {
    for (auto& event : events) event.reviewedAt = std::min(event.reviewedAt, now);
    std::sort(events.begin(), events.end(), [](const ReviewEvent& a, const ReviewEvent& b) {
        if (a.reviewedAt != b.reviewedAt) return a.reviewedAt < b.reviewedAt;
        return a.id.toString() < b.id.toString();
    });
    return events;
}

MemoryLedger::MemoryLedger(Fsrs fsrs) : fsrs_(std::move(fsrs)) {}

void MemoryLedger::apply(StateMap& states, const ReviewEvent& event, const BoostFn& boost) const {
    auto current = states.try_emplace(event.item, MemoryState{event.item}).first->second;
    bool first = current.phase == Phase::New;
    double factor = first && boost ? boost(event.item, event.reviewedAt, states) : 1.0;
    states[event.item] = fsrs_.review(current, event.grade, event.reviewedAt, factor);
}

StateMap MemoryLedger::replay(std::vector<ReviewEvent> events, TimePoint now, const BoostFn& boost,
                              const ForecastFn& forecast) const {
    StateMap states;
    for (const auto& event : inReplayOrder(std::move(events), now)) {
        auto known = states.find(event.item);
        if (forecast && known != states.end() && known->second.phase != Phase::New) {
            forecast(event, fsrs_.recallChance(known->second, event.reviewedAt));
        }
        apply(states, event, boost);
    }
    return states;
}

}  // namespace atlas::learning
