#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/core/result.hpp"
#include "atlas/graph/graph_error.hpp"

namespace atlas::graph {

using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;
using atlas::core::Relationship;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;
using atlas::core::Result;

// In-memory index over KnowledgeObjects and Relationships. Owns no
// storage of its own beyond what's added via addNode/addEdge — this is
// a pure data structure, not a database. atlas-app is responsible for
// populating it (typically once at startup, from
// KnowledgeObjectRepository::findAll() / RelationshipRepository::findAll())
// and for persisting any changes back through the repositories; this
// class has no idea SQLite exists.
//
// All CRUD-shaped operations here mirror the persistence layer's
// invariants (foreign-key validity, edge uniqueness) deliberately:
// this is the layer that actually understands graph semantics like
// isSymmetric(), so it's the right place to enforce them, even though
// the database also enforces a looser version as a safety net.
//
// Move-only, not copyable: a deep copy of a 10k+ node graph is
// expensive and there should only ever be one of these representing
// live application state — accidentally passing it by value shouldn't
// silently clone the whole thing.
class GraphEngine {
public:
    GraphEngine() = default;
    GraphEngine(GraphEngine&&) = default;
    GraphEngine& operator=(GraphEngine&&) = default;
    GraphEngine(const GraphEngine&) = delete;
    GraphEngine& operator=(const GraphEngine&) = delete;

    Result<void, GraphError> addNode(KnowledgeObject node);
    Result<void, GraphError> addEdge(Relationship edge);

    // Replaces the content of an existing node in place. Returns
    // GraphError::UnknownNode if the id isn't present — this never
    // creates a node, only updates one that's already there.
    //
    // Exists specifically so callers (e.g. atlas-ui's controller) can
    // do "copy current state, mutate the copy, write the copy to the
    // database, and only then call updateNode" — meaning the graph is
    // never touched until the database write has already succeeded. No
    // rollback-on-persistence-failure logic is needed anywhere if every
    // write path follows that order.
    //
    // Note on pointers: a KnowledgeObject* from findNode() for this id
    // keeps pointing at the same object identity after an update (the
    // node's slot itself isn't moved), but the update replaces its
    // content via assignment — so any previously-cached references
    // into its internals (e.g. a `const std::string&` from an earlier
    // .title() call) are stale afterward, even though the outer pointer
    // itself isn't dangling. Don't hold internal references across an
    // updateNode call for the same id.
    Result<void, GraphError> updateNode(KnowledgeObject node);

    // Cascades: removing a node also removes every edge touching it,
    // mirroring the database's ON DELETE CASCADE. Returns false (not
    // an error) if the id isn't present.
    bool removeNode(const KnowledgeObjectId& id);
    bool removeEdge(const RelationshipId& id);

    // Pointer validity contract: a pointer returned here remains valid
    // until the specific node/edge it points to is removed (directly,
    // or via cascade from removing its node). Adding or removing *other*
    // nodes/edges never invalidates it — backed by std::deque internally
    // specifically to make that guarantee hold, unlike std::vector,
    // whose reallocation on growth would invalidate every existing
    // pointer. Don't change the backing container without re-checking this.
    const KnowledgeObject* findNode(const KnowledgeObjectId& id) const;
    const Relationship* findEdge(const RelationshipId& id) const;

    size_t nodeCount() const { return nodeIndexById_.size(); }
    size_t edgeCount() const { return edgeIndexById_.size(); }

    // Every live node id, in unspecified order (backed by a hash map —
    // don't rely on iteration order being stable across calls). Callers
    // needing a stable presentation order (e.g. a UI list) sort this
    // themselves; GraphEngine shouldn't have an opinion about display
    // ordering.
    std::vector<KnowledgeObjectId> allNodeIds() const;

    enum class Direction { Outgoing, Incoming, Both };

    // Returns the ids of nodes connected to `id`, optionally filtered
    // to a single RelationshipType. For a symmetric type (see
    // isSymmetric()), Direction is irrelevant — the edge was indexed
    // both ways at insertion time, so Outgoing and Incoming both find
    // it from either endpoint.
    std::vector<KnowledgeObjectId> neighbors(
        const KnowledgeObjectId& id, std::optional<RelationshipType> type = std::nullopt,
        Direction direction = Direction::Outgoing) const;

    // Convenience wrappers. usedBy() is deliberately *not* stored
    // anywhere — it's neighbors() filtered to DependsOn, Incoming. See
    // atlas-core's design notes on why "Used By" is a query, not a field.
    std::vector<KnowledgeObjectId> dependsOn(const KnowledgeObjectId& id) const;
    std::vector<KnowledgeObjectId> usedBy(const KnowledgeObjectId& id) const;

    // BFS over outgoing DependsOn edges. Does not include `id` itself.
    std::vector<KnowledgeObjectId> transitiveDependencies(const KnowledgeObjectId& id) const;

    // Kahn's algorithm over the DependsOn subgraph. Foundational
    // primitive for learning-roadmap generation (M6) — ordering
    // concepts so every dependency comes before its dependents.
    // Returns GraphError::CycleDetected if DependsOn isn't a DAG.
    Result<std::vector<KnowledgeObjectId>, GraphError> topologicalOrder() const;

private:
    struct NodeSlot {
        std::optional<KnowledgeObject> object;  // nullopt => tombstoned
    };
    struct EdgeSlot {
        std::optional<Relationship> relationship;  // nullopt => tombstoned
    };

    bool hasDuplicateEdge(const KnowledgeObjectId& source, const KnowledgeObjectId& target,
                            RelationshipType type) const;
    void indexEdge(size_t edgeIndex);
    void unindexEdge(size_t edgeIndex);

    // std::deque, not std::vector: appending to a deque never
    // invalidates references to its existing elements (only iterators),
    // which is exactly the guarantee findNode()/findEdge() depend on.
    // Still O(1) operator[] for the dense-index lookups everywhere else
    // in this class — see the pointer validity contract above.
    std::deque<NodeSlot> nodes_;
    std::unordered_map<KnowledgeObjectId, size_t> nodeIndexById_;

    std::deque<EdgeSlot> edges_;
    std::unordered_map<RelationshipId, size_t> edgeIndexById_;

    // Parallel to nodes_, indexed by the same dense node index.
    std::vector<std::vector<size_t>> outgoingEdgeIndices_;
    std::vector<std::vector<size_t>> incomingEdgeIndices_;
};

}  // namespace atlas::graph
