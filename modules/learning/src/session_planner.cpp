#include "atlas/learning/session_planner.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace atlas::learning {

using atlas::core::Phase;

namespace {

struct DueGroup {
    KnowledgeObjectId conceptId;
    std::vector<ItemRef> items{};
    double lowestRecall = 1.0;
    int leverage = 0;
};

bool isDue(const MemoryState& state, TimePoint now) {
    return state.phase != Phase::New && state.dueAt.has_value() && *state.dueAt <= now;
}

}  // namespace

SessionPlanner::SessionPlanner(const GraphEngine& graph, const NetworkRules& rules, Fsrs fsrs)
    : graph_(&graph), rules_(&rules), fsrs_(std::move(fsrs)) {}

double SessionPlanner::conceptRecall(const KnowledgeObjectId& conceptId, const StateMap& states,
                                     TimePoint now) const {
    auto it = states.find(ItemRef::forConcept(conceptId));
    return it == states.end() ? 0.0 : fsrs_.recallChance(it->second, now);
}

KnowledgeObjectId SessionPlanner::linkOwner(const Relationship& link, const StateMap& states,
                                            TimePoint now) const {
    if (!atlas::core::isSymmetric(link.type())) return link.sourceId();
    bool targetWeaker = conceptRecall(link.targetId(), states, now) < conceptRecall(link.sourceId(), states, now);
    return targetWeaker ? link.targetId() : link.sourceId();
}

std::optional<TopicId> SessionPlanner::topicOf(const KnowledgeObjectId& conceptId) const {
    const auto* object = graph_->findNode(conceptId);
    if (object == nullptr) return std::nullopt;
    return object->topicId();
}

std::vector<SessionPlanner::FocusUnit> SessionPlanner::interleaveByTopic(std::vector<FocusUnit> units) const {
    std::vector<FocusUnit> result;
    while (!units.empty()) {
        size_t pick = 0;
        if (result.size() >= 2) {
            auto last = topicOf(result.back().front().conceptId);
            if (last == topicOf(result[result.size() - 2].front().conceptId)) {
                for (size_t i = 0; i < units.size(); ++i) {
                    if (topicOf(units[i].front().conceptId) != last) {
                        pick = i;
                        break;
                    }
                }
            }
        }
        result.push_back(std::move(units[pick]));
        units.erase(units.begin() + static_cast<long>(pick));
    }
    return result;
}

SessionPlan SessionPlanner::plan(const StateMap& states, TimePoint now, int introducedToday,
                                 const SessionLimits& limits,
                                 std::optional<std::chrono::milliseconds> medianFocusTime) const {
    std::unordered_map<KnowledgeObjectId, DueGroup> groups;
    auto addDue = [&](const KnowledgeObjectId& owner, const ItemRef& item, double recall) {
        auto& group = groups.try_emplace(owner, DueGroup{owner}).first->second;
        group.items.push_back(item);
        group.lowestRecall = std::min(group.lowestRecall, recall);
    };

    for (const auto& conceptId : graph_->allNodeIds()) {
        auto it = states.find(ItemRef::forConcept(conceptId));
        if (it != states.end() && isDue(it->second, now)) {
            addDue(conceptId, it->first, fsrs_.recallChance(it->second, now));
        }
    }
    for (const auto& linkId : graph_->allEdgeIds()) {
        if (!rules_->isLinkReady(linkId, states)) continue;
        auto item = ItemRef::forLink(linkId);
        auto it = states.find(item);
        bool fresh = it == states.end() || it->second.phase == Phase::New;
        if (!fresh && !isDue(it->second, now)) continue;
        double recall = fresh ? 0.0 : fsrs_.recallChance(it->second, now);
        addDue(linkOwner(*graph_->findEdge(linkId), states, now), item, recall);
    }

    std::vector<DueGroup*> ordered;
    for (auto& [conceptId, group] : groups) {
        group.leverage = rules_->leverage(conceptId);
        ordered.push_back(&group);
    }
    std::sort(ordered.begin(), ordered.end(), [](const DueGroup* a, const DueGroup* b) {
        if (a->lowestRecall != b->lowestRecall) return a->lowestRecall < b->lowestRecall;
        if (a->leverage != b->leverage) return a->leverage > b->leverage;
        return a->conceptId.toString() < b->conceptId.toString();
    });

    std::vector<FocusUnit> units;
    std::unordered_set<KnowledgeObjectId> placed;
    for (const DueGroup* group : ordered) {
        if (placed.contains(group->conceptId)) continue;
        FocusUnit unit{FocusPlan{group->conceptId, group->items, false}};
        placed.insert(group->conceptId);
        for (const auto& partner : contrastPartners(*graph_, group->conceptId)) {
            if (placed.contains(partner) || !rules_->isIntroduced(ItemRef::forConcept(partner), states)) continue;
            auto found = groups.find(partner);
            auto items = found != groups.end() ? found->second.items
                                               : std::vector<ItemRef>{ItemRef::forConcept(partner)};
            unit.push_back(FocusPlan{partner, std::move(items), false});
            placed.insert(partner);
        }
        units.push_back(std::move(unit));
    }

    int newSlots = std::max(0, limits.newPerDay - introducedToday);
    for (const auto& conceptId : rules_->frontier(now, states)) {
        if (newSlots == 0) break;
        if (placed.contains(conceptId)) continue;
        units.push_back(FocusUnit{FocusPlan{conceptId, {ItemRef::forConcept(conceptId)}, true}});
        placed.insert(conceptId);
        --newSlots;
    }

    SessionPlan plan;
    for (auto& unit : interleaveByTopic(std::move(units))) {
        bool full = plan.focuses.size() + unit.size() > static_cast<size_t>(limits.maxFocus);
        if (!plan.focuses.empty() && full) break;
        for (auto& focus : unit) plan.focuses.push_back(std::move(focus));
    }
    auto perFocus = medianFocusTime ? std::chrono::duration_cast<std::chrono::seconds>(*medianFocusTime)
                                    : limits.defaultFocusTime;
    plan.estimatedDuration = perFocus * static_cast<long>(plan.focuses.size());
    return plan;
}

}  // namespace atlas::learning
