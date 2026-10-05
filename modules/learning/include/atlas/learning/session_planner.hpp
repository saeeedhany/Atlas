#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "atlas/learning/network_rules.hpp"

namespace atlas::learning {

using atlas::core::Relationship;
using atlas::core::TopicId;

struct SessionLimits {
    int newPerDay = 5;
    int maxFocus = 12;
    std::chrono::seconds defaultFocusTime{40};
};

struct FocusPlan {
    KnowledgeObjectId conceptId;
    std::vector<ItemRef> items;
    bool isNew = false;
};

struct SessionPlan {
    std::vector<FocusPlan> focuses;
    std::chrono::seconds estimatedDuration{0};
};

class SessionPlanner {
public:
    SessionPlanner(const GraphEngine& graph, const NetworkRules& rules, Fsrs fsrs = Fsrs{});

    SessionPlan plan(const StateMap& states, TimePoint now, int introducedToday,
                     const SessionLimits& limits,
                     std::optional<std::chrono::milliseconds> medianFocusTime = std::nullopt) const;

private:
    using FocusUnit = std::vector<FocusPlan>;

    double conceptRecall(const KnowledgeObjectId& conceptId, const StateMap& states, TimePoint now) const;
    KnowledgeObjectId linkOwner(const Relationship& link, const StateMap& states, TimePoint now) const;
    std::optional<TopicId> topicOf(const KnowledgeObjectId& conceptId) const;
    std::vector<FocusUnit> interleaveByTopic(std::vector<FocusUnit> units, size_t focusLimit) const;

    const GraphEngine* graph_;
    const NetworkRules* rules_;
    Fsrs fsrs_;
};

}
