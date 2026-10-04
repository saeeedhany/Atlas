#include "atlas/learning/network_rules.hpp"

#include <algorithm>
#include <utility>

namespace atlas::learning {

using atlas::core::ItemKind;
using atlas::core::Phase;
using atlas::core::RelationshipType;

namespace {

bool byIdText(const KnowledgeObjectId& a, const KnowledgeObjectId& b) {
    return a.toString() < b.toString();
}

bool contains(const std::vector<KnowledgeObjectId>& ids, const KnowledgeObjectId& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}  // namespace

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

bool NetworkRules::isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now,
                                const StateMap& states) const {
    if (isIntroduced(ItemRef::forConcept(conceptId), states)) return false;
    for (const auto& prerequisite : graph_->dependsOn(conceptId)) {
        if (isSolid(prerequisite, now, states)) continue;
        if (!contains(graph_->transitiveDependencies(prerequisite), conceptId)) return false;
    }
    return true;
}

std::vector<KnowledgeObjectId> NetworkRules::frontier(TimePoint now, const StateMap& states) const {
    std::vector<std::pair<int, KnowledgeObjectId>> ranked;
    for (const auto& id : graph_->allNodeIds()) {
        if (isOnFrontier(id, now, states)) ranked.emplace_back(leverage(id), id);
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

}  // namespace atlas::learning
