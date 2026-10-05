#include "atlas/learning/network_rules.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace atlas::learning {

using atlas::core::ItemKind;
using atlas::core::Phase;
using atlas::core::RelationshipType;

namespace {

bool byIdText(const KnowledgeObjectId& a, const KnowledgeObjectId& b) {
    return a.toString() < b.toString();
}

}

std::vector<KnowledgeObjectId> contrastPartners(const GraphEngine& graph,
                                                const KnowledgeObjectId& conceptId) {
    std::vector<KnowledgeObjectId> partners;
    for (auto type : {RelationshipType::AlternativeTo, RelationshipType::OppositeOf}) {
        auto found = graph.neighbors(conceptId, type, GraphEngine::Direction::Both);
        partners.insert(partners.end(), found.begin(), found.end());
    }
    std::sort(partners.begin(), partners.end(), byIdText);
    partners.erase(std::unique(partners.begin(), partners.end()), partners.end());
    return partners;
}

NetworkRules::NetworkRules(const GraphEngine& graph, Fsrs fsrs, NetworkConfig config)
    : graph_(&graph), fsrs_(std::move(fsrs)), config_(config) {}

bool NetworkRules::isIntroduced(const ItemRef& item, const StateMap& states) const {
    auto it = states.find(item);
    return it != states.end() && it->second.phase != Phase::New;
}

bool NetworkRules::isSolid(const KnowledgeObjectId& conceptId, TimePoint now,
                           const StateMap& states) const {
    auto it = states.find(ItemRef::forConcept(conceptId));
    if (it == states.end() || it->second.phase != Phase::Review) return false;
    return fsrs_.recallChance(it->second, now) >= config_.solidThreshold;
}

NetworkRules::Components NetworkRules::dependencyComponents() const {
    struct Visit {
        KnowledgeObjectId id;
        std::vector<KnowledgeObjectId> prerequisites;
        size_t next = 0;
    };
    Components components;
    std::unordered_map<KnowledgeObjectId, int> order;
    std::unordered_map<KnowledgeObjectId, int> lowLink;
    std::unordered_set<KnowledgeObjectId> onStack;
    std::vector<KnowledgeObjectId> stack;
    std::vector<Visit> visits;
    int nextOrder = 0;
    int nextComponent = 0;

    auto enter = [&](const KnowledgeObjectId& id) {
        order[id] = lowLink[id] = nextOrder++;
        stack.push_back(id);
        onStack.insert(id);
        visits.push_back(Visit{id, graph_->dependsOn(id)});
    };

    for (const auto& root : graph_->allNodeIds()) {
        if (order.contains(root)) continue;
        enter(root);
        while (!visits.empty()) {
            auto& visit = visits.back();
            if (visit.next < visit.prerequisites.size()) {
                auto prerequisite = visit.prerequisites[visit.next++];
                if (!order.contains(prerequisite)) {
                    enter(prerequisite);
                } else if (onStack.contains(prerequisite)) {
                    lowLink[visit.id] = std::min(lowLink[visit.id], order[prerequisite]);
                }
                continue;
            }
            auto id = visit.id;
            visits.pop_back();
            if (!visits.empty()) {
                auto& parentLow = lowLink[visits.back().id];
                parentLow = std::min(parentLow, lowLink[id]);
            }
            if (lowLink[id] != order[id]) continue;
            while (true) {
                auto member = stack.back();
                stack.pop_back();
                onStack.erase(member);
                components[member] = nextComponent;
                if (member == id) break;
            }
            ++nextComponent;
        }
    }
    return components;
}

bool NetworkRules::isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now,
                                const StateMap& states) const {
    return isOnFrontier(conceptId, now, states, dependencyComponents());
}

bool NetworkRules::isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now, const StateMap& states,
                                const Components& components) const {
    if (isIntroduced(ItemRef::forConcept(conceptId), states)) return false;
    auto own = components.find(conceptId);
    for (const auto& prerequisite : graph_->dependsOn(conceptId)) {
        if (isSolid(prerequisite, now, states)) continue;
        auto other = components.find(prerequisite);
        if (own == components.end() || other == components.end() || own->second != other->second) {
            return false;
        }
    }
    return true;
}

std::vector<KnowledgeObjectId> NetworkRules::frontier(TimePoint now, const StateMap& states) const {
    auto components = dependencyComponents();
    std::vector<std::pair<size_t, KnowledgeObjectId>> ranked;
    for (const auto& id : graph_->allNodeIds()) {
        if (isOnFrontier(id, now, states, components)) ranked.emplace_back(graph_->usedBy(id).size(), id);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return byIdText(a.second, b.second);
    });
    std::vector<KnowledgeObjectId> result;
    for (const auto& entry : ranked) result.push_back(entry.second);
    return result;
}

double NetworkRules::schemaBoost(const ItemRef& item, TimePoint at, const StateMap& states) const {
    if (item.kind != ItemKind::Concept) return 1.0;
    auto prerequisites = graph_->dependsOn(KnowledgeObjectId(item.id));
    if (prerequisites.empty()) return 1.0;
    double total = 0.0;
    for (const auto& prerequisite : prerequisites) {
        auto it = states.find(ItemRef::forConcept(prerequisite));
        if (it != states.end()) total += fsrs_.recallChance(it->second, at);
    }
    return 1.0 + config_.schemaBeta * total / static_cast<double>(prerequisites.size());
}

BoostFn NetworkRules::boostFn() const {
    return [this](const ItemRef& item, TimePoint at, const StateMap& states) {
        return schemaBoost(item, at, states);
    };
}

bool NetworkRules::isLinkReady(const RelationshipId& linkId, const StateMap& states) const {
    const auto* link = graph_->findEdge(linkId);
    return link != nullptr && isIntroduced(ItemRef::forConcept(link->sourceId()), states) &&
           isIntroduced(ItemRef::forConcept(link->targetId()), states);
}

int NetworkRules::leverage(const KnowledgeObjectId& conceptId) const {
    return static_cast<int>(graph_->transitiveDependents(conceptId).size());
}

}
