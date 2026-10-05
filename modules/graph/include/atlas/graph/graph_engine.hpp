#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/core/result.hpp"
#include "atlas/core/search.hpp"
#include "atlas/graph/graph_error.hpp"

namespace atlas::graph {

using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;
using atlas::core::Relationship;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;
using atlas::core::Result;
using atlas::core::TopicId;

class GraphEngine {
public:
    GraphEngine() = default;
    GraphEngine(GraphEngine&&) = default;
    GraphEngine& operator=(GraphEngine&&) = default;
    GraphEngine(const GraphEngine&) = delete;
    GraphEngine& operator=(const GraphEngine&) = delete;

    Result<void, GraphError> addNode(KnowledgeObject node);
    Result<void, GraphError> addEdge(Relationship edge);

    Result<void, GraphError> updateNode(KnowledgeObject node);

    bool removeNode(const KnowledgeObjectId& id);
    bool removeEdge(const RelationshipId& id);

    const KnowledgeObject* findNode(const KnowledgeObjectId& id) const;
    const Relationship* findEdge(const RelationshipId& id) const;

    size_t nodeCount() const { return nodeIndexById_.size(); }
    size_t edgeCount() const { return edgeIndexById_.size(); }

    std::vector<KnowledgeObjectId> allNodeIds() const;

    std::vector<RelationshipId> allEdgeIds() const;

    std::vector<KnowledgeObjectId> search(std::string_view query) const;

    bool hasDuplicateEdge(const KnowledgeObjectId& source, const KnowledgeObjectId& target,
                            RelationshipType type) const;

    enum class Direction { Outgoing, Incoming, Both };

    std::vector<KnowledgeObjectId> neighbors(
        const KnowledgeObjectId& id, std::optional<RelationshipType> type = std::nullopt,
        Direction direction = Direction::Outgoing) const;

    std::vector<KnowledgeObjectId> dependsOn(const KnowledgeObjectId& id) const;
    std::vector<KnowledgeObjectId> usedBy(const KnowledgeObjectId& id) const;

    std::vector<KnowledgeObjectId> transitiveDependencies(const KnowledgeObjectId& id) const;

    std::vector<KnowledgeObjectId> transitiveDependents(const KnowledgeObjectId& id) const;

    Result<std::vector<KnowledgeObjectId>, GraphError> topologicalOrder() const;

    Result<std::vector<KnowledgeObjectId>, GraphError> learningRoadmapFor(
        const KnowledgeObjectId& id) const;

    struct ProjectSuggestion {
        KnowledgeObjectId conceptId;
        double readiness;
        int leverage;
    };

    std::vector<ProjectSuggestion> suggestProjects(const TopicId& topicId) const;

private:
    struct NodeSlot {
        std::optional<KnowledgeObject> object;
    };
    struct EdgeSlot {
        std::optional<Relationship> relationship;
    };

    void indexEdge(size_t edgeIndex);
    void unindexEdge(size_t edgeIndex);

    // deque keeps returned pointers valid when nodes or edges are appended.
    std::deque<NodeSlot> nodes_;
    std::unordered_map<KnowledgeObjectId, size_t> nodeIndexById_;

    std::deque<EdgeSlot> edges_;
    std::unordered_map<RelationshipId, size_t> edgeIndexById_;

    std::vector<std::vector<size_t>> outgoingEdgeIndices_;
    std::vector<std::vector<size_t>> incomingEdgeIndices_;
};

}
