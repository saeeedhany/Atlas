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

// In-memory index over KnowledgeObjects and Relationships. Owns no
// storage of its own beyond what's added via addNode/addEdge - this is
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
// live application state - accidentally passing it by value shouldn't
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
    // GraphError::UnknownNode if the id isn't present - this never
    // creates a node, only updates one that's already there.
    //
    // Exists specifically so callers (e.g. atlas-ui's controller) can
    // do "copy current state, mutate the copy, write the copy to the
    // database, and only then call updateNode" - meaning the graph is
    // never touched until the database write has already succeeded. No
    // rollback-on-persistence-failure logic is needed anywhere if every
    // write path follows that order.
    //
    // Note on pointers: a KnowledgeObject* from findNode() for this id
    // keeps pointing at the same object identity after an update (the
    // node's slot itself isn't moved), but the update replaces its
    // content via assignment - so any previously-cached references
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
    // nodes/edges never invalidates it - backed by std::deque internally
    // specifically to make that guarantee hold, unlike std::vector,
    // whose reallocation on growth would invalidate every existing
    // pointer. Don't change the backing container without re-checking this.
    const KnowledgeObject* findNode(const KnowledgeObjectId& id) const;
    const Relationship* findEdge(const RelationshipId& id) const;

    size_t nodeCount() const { return nodeIndexById_.size(); }
    size_t edgeCount() const { return edgeIndexById_.size(); }

    // Every live node id, in unspecified order (backed by a hash map -
    // don't rely on iteration order being stable across calls). Callers
    // needing a stable presentation order (e.g. a UI list) sort this
    // themselves; GraphEngine shouldn't have an opinion about display
    // ordering.
    std::vector<KnowledgeObjectId> allNodeIds() const;

    // Every live edge id, in unspecified order - same caveat as
    // allNodeIds(). Needed once a UI wants to list/manage relationships
    // directly, not just traverse them from a node's perspective.
    std::vector<RelationshipId> allEdgeIds() const;

    // Ranks every live node by atlas::core::matchScore() against
    // `query`, highest score first, with a deterministic tie-break (by
    // id string) so two identical searches never return a different
    // order just because of hash-map iteration order. An empty query
    // returns every live node, unranked - "show everything," not
    // "rank everything as equally relevant," since there's no
    // meaningful ranking to apply against nothing.
    std::vector<KnowledgeObjectId> search(std::string_view query) const;

    // Public (not just an addEdge() implementation detail) specifically
    // so callers can pre-flight-check before writing anything to
    // persistence: "would the graph accept this edge?" This is what
    // lets WorkspaceController reject a duplicate (including a
    // symmetric type's reverse-pair, which the database's ordered
    // UNIQUE constraint alone wouldn't catch) before any database
    // write happens, rather than discovering the rejection only after
    // the database has already accepted it.
    bool hasDuplicateEdge(const KnowledgeObjectId& source, const KnowledgeObjectId& target,
                            RelationshipType type) const;

    enum class Direction { Outgoing, Incoming, Both };

    // Returns the ids of nodes connected to `id`, optionally filtered
    // to a single RelationshipType. For a symmetric type (see
    // isSymmetric()), Direction is irrelevant - the edge was indexed
    // both ways at insertion time, so Outgoing and Incoming both find
    // it from either endpoint.
    std::vector<KnowledgeObjectId> neighbors(
        const KnowledgeObjectId& id, std::optional<RelationshipType> type = std::nullopt,
        Direction direction = Direction::Outgoing) const;

    // Convenience wrappers. usedBy() is deliberately *not* stored
    // anywhere - it's neighbors() filtered to DependsOn, Incoming. See
    // atlas-core's design notes on why "Used By" is a query, not a field.
    std::vector<KnowledgeObjectId> dependsOn(const KnowledgeObjectId& id) const;
    std::vector<KnowledgeObjectId> usedBy(const KnowledgeObjectId& id) const;

    // BFS over outgoing DependsOn edges. Does not include `id` itself.
    std::vector<KnowledgeObjectId> transitiveDependencies(const KnowledgeObjectId& id) const;

    // The mirror of transitiveDependencies: BFS over incoming
    // DependsOn edges (i.e. usedBy()) - every concept that
    // transitively depends on `id`, directly or indirectly. Does not
    // include `id` itself. This is the "how much does mastering this
    // concept unlock elsewhere" query - used as the leverage signal in
    // suggestProjects(), but useful as a general primitive on its own
    // (e.g. "what would break, conceptually, if I forgot this").
    std::vector<KnowledgeObjectId> transitiveDependents(const KnowledgeObjectId& id) const;

    // Kahn's algorithm over the DependsOn subgraph. Foundational
    // primitive for learning-roadmap generation - ordering concepts
    // so every dependency comes before its dependents. Returns
    // GraphError::CycleDetected if DependsOn isn't a DAG.
    Result<std::vector<KnowledgeObjectId>, GraphError> topologicalOrder() const;

    // The actual "what should I learn, in what order" primitive: the
    // full graph's topological order, restricted to just `id` and its
    // transitive dependency closure - not the whole graph. Without
    // this restriction, asking for a roadmap "to learn X" would
    // surface every unrelated concept in the workspace that happens to
    // have no dependencies, which isn't what "roadmap for X" means.
    // The result is ordered so the last entry is always `id` itself
    // (assuming no cycle) and every entry before it is a prerequisite,
    // each one appearing only after all of *its* prerequisites have.
    Result<std::vector<KnowledgeObjectId>, GraphError> learningRoadmapFor(
        const KnowledgeObjectId& id) const;

    // A single ranked suggestion: which concept, and why it's worth
    // doing next. `readiness` is the fraction (0.0-1.0) of the
    // concept's direct DependsOn prerequisites that are already
    // Confident or Mastered (1.0 if it has none). `leverage` is the
    // number of other concepts that transitively depend on this one -
    // a rough proxy for "how much does practicing this unlock
    // elsewhere." Both are exposed, not just the final score, so a UI
    // can explain *why* something was suggested, not just present a
    // ranked list with no justification.
    struct ProjectSuggestion {
        KnowledgeObjectId conceptId;
        double readiness;
        int leverage;
    };

    // Ranks every concept in `topicId` that (a) has at least one
    // MiniProject and (b) isn't already Mastered, by
    // readiness * (1 + leverage), highest first, with a deterministic
    // tie-break (by id string). Concepts with no MiniProjects are
    // never suggested regardless of how well-connected they are -
    // there's nothing to actually go *do*. This is a pure function of
    // graph structure + each object's own fields; no AI involved (that
    // is deliberately a separate, later feature - see docs/DECISIONS.md).
    std::vector<ProjectSuggestion> suggestProjects(const TopicId& topicId) const;

private:
    struct NodeSlot {
        std::optional<KnowledgeObject> object;  // nullopt => tombstoned
    };
    struct EdgeSlot {
        std::optional<Relationship> relationship;  // nullopt => tombstoned
    };

    void indexEdge(size_t edgeIndex);
    void unindexEdge(size_t edgeIndex);

    // std::deque, not std::vector: appending to a deque never
    // invalidates references to its existing elements (only iterators),
    // which is exactly the guarantee findNode()/findEdge() depend on.
    // Still O(1) operator[] for the dense-index lookups everywhere else
    // in this class - see the pointer validity contract above.
    std::deque<NodeSlot> nodes_;
    std::unordered_map<KnowledgeObjectId, size_t> nodeIndexById_;

    std::deque<EdgeSlot> edges_;
    std::unordered_map<RelationshipId, size_t> edgeIndexById_;

    // Parallel to nodes_, indexed by the same dense node index.
    std::vector<std::vector<size_t>> outgoingEdgeIndices_;
    std::vector<std::vector<size_t>> incomingEdgeIndices_;
};

}  // namespace atlas::graph
