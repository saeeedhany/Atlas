#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/graph/graph_engine.hpp"

namespace atlas::learning {

using atlas::core::Certainty;
using atlas::core::Grade;
using atlas::core::KnowledgeObjectId;
using atlas::core::Relationship;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;
using atlas::core::ReviewEvent;
using atlas::graph::GraphEngine;

struct RecalledLink {
    KnowledgeObjectId other;
    RelationshipType type;
    bool focusIsSource = true;
};

struct RebuildAnswer {
    KnowledgeObjectId focus;
    Certainty predicted = Certainty::Unsure;
    std::vector<RecalledLink> recalled{};
    std::vector<KnowledgeObjectId> hinted{};
    std::chrono::milliseconds responseTime{0};
};

struct GradedLink {
    RelationshipId link;
    Grade grade;
    int hintsUsed = 0;
    std::optional<KnowledgeObjectId> wrongTarget;
};

std::optional<std::chrono::milliseconds> medianRebuildResponse(const std::vector<ReviewEvent>& events);

class RebuildGrader {
public:
    explicit RebuildGrader(const GraphEngine& graph);

    std::vector<GradedLink> grade(const RebuildAnswer& answer, const std::vector<RelationshipId>& hidden,
                                  std::optional<std::chrono::milliseconds> medianResponse) const;

private:
    void attachWrongTarget(std::vector<GradedLink>& graded, const std::vector<KnowledgeObjectId>& targets,
                           const KnowledgeObjectId& wrong, const KnowledgeObjectId& focus) const;

    const GraphEngine* graph_;
};

}
