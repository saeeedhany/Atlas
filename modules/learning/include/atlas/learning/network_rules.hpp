#pragma once

#include <unordered_map>
#include <vector>

#include "atlas/graph/graph_engine.hpp"
#include "atlas/learning/fsrs.hpp"
#include "atlas/learning/memory_ledger.hpp"

namespace atlas::learning {

using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipId;
using atlas::graph::GraphEngine;

struct NetworkConfig {
    double solidThreshold = 0.80;
    double schemaBeta = 0.2;
};

std::vector<KnowledgeObjectId> contrastPartners(const GraphEngine& graph,
                                                const KnowledgeObjectId& conceptId);

class NetworkRules {
public:
    NetworkRules(const GraphEngine& graph, Fsrs fsrs = Fsrs{}, NetworkConfig config = {});

    bool isIntroduced(const ItemRef& item, const StateMap& states) const;
    bool isSolid(const KnowledgeObjectId& conceptId, TimePoint now, const StateMap& states) const;
    bool isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now, const StateMap& states) const;
    std::vector<KnowledgeObjectId> frontier(TimePoint now, const StateMap& states) const;

    double schemaBoost(const ItemRef& item, TimePoint at, const StateMap& states) const;
    BoostFn boostFn() const;

    bool isLinkReady(const RelationshipId& linkId, const StateMap& states) const;
    int leverage(const KnowledgeObjectId& conceptId) const;

private:
    using Components = std::unordered_map<KnowledgeObjectId, int>;

    Components dependencyComponents() const;
    bool isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now, const StateMap& states,
                      const Components& components) const;

    const GraphEngine* graph_;
    Fsrs fsrs_;
    NetworkConfig config_;
};

}
