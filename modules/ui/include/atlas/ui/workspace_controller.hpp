#pragma once

#include <QObject>

#include <optional>
#include <string>
#include <vector>

#include "atlas/core/enums.hpp"
#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/core/result.hpp"
#include "atlas/graph/graph_engine.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/relationship_repository.hpp"
#include "atlas/ui/controller_error.hpp"

namespace atlas::ui {

using atlas::core::ConfidenceLevel;
using atlas::core::Difficulty;
using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;
using atlas::core::Relationship;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;
using atlas::core::Result;

// Optional fields only: every field the dialog doesn't touch is left
// nullopt, and updateKnowledgeObject() leaves the corresponding
// property unchanged. (In the current dialog every field is always
// present since it always shows the full form, but the controller API
// doesn't assume that — a future partial-edit UI can rely on this.)
struct KnowledgeObjectEdits {
    std::optional<std::string> title;
    std::optional<std::string> definition;
    std::optional<std::string> problemSolved;
    std::optional<std::string> whyItExists;
    std::optional<std::string> notes;
    std::optional<Difficulty> difficulty;
    std::optional<ConfidenceLevel> confidence;
};

// The bridge between persistence/graph and the Qt UI. Every mutating
// method here follows one rule without exception: write to the
// database first, and only touch the in-memory graph after that write
// has already succeeded. That ordering is what makes the graph's state
// always either consistent with the database or strictly behind it —
// never ahead of it, never diverged — without needing any
// rollback-on-failure logic anywhere in this class.
//
// QObject-based (not pure C++) specifically so widgets can connect to
// its signals and stay in sync without polling. This is the first
// place in the codebase that depends on Qt; atlas-core, -persistence,
// and -graph remain Qt-free below this layer.
class WorkspaceController : public QObject {
    Q_OBJECT

public:
    // `database` must outlive this controller.
    explicit WorkspaceController(atlas::persistence::Database& database,
                                  QObject* parent = nullptr);

    // Populates the in-memory graph from the database. Call exactly
    // once, before any other method — there's currently no guard
    // against calling it twice and double-loading, since nothing in
    // this milestone's UI flow does that.
    Result<void, ControllerFailure> load();

    Result<KnowledgeObjectId, ControllerFailure> createKnowledgeObject(std::string title);
    Result<void, ControllerFailure> updateKnowledgeObject(const KnowledgeObjectId& id,
                                                            KnowledgeObjectEdits edits);
    Result<void, ControllerFailure> removeKnowledgeObject(const KnowledgeObjectId& id);

    // Rejects a self-loop and rejects a duplicate — including, for a
    // symmetric type, the same pair stored in reverse order — by
    // checking the in-memory graph *before* writing anything, via
    // GraphEngine::hasDuplicateEdge(). That ordering means a rejection
    // is never discovered only after the database has already
    // accepted a row the graph wouldn't have.
    Result<RelationshipId, ControllerFailure> createRelationship(
        const KnowledgeObjectId& sourceId, const KnowledgeObjectId& targetId, RelationshipType type,
        std::optional<std::string> note);
    Result<void, ControllerFailure> removeRelationship(const RelationshipId& id);

    // Snapshots, sorted for deterministic display — GraphEngine itself
    // has no opinion on ordering (its allNodeIds()/allEdgeIds() are
    // hash-map ordered), so the controller imposes one here, since
    // this is the layer that knows it's serving a UI list.
    std::vector<KnowledgeObject> allKnowledgeObjects() const;
    std::vector<Relationship> allRelationships() const;
    std::optional<KnowledgeObject> findKnowledgeObject(const KnowledgeObjectId& id) const;

    // Read-only access to the underlying graph, for consumers that
    // need to reason about structure directly (e.g. GraphWindow's
    // layout computation). Every write still goes through this
    // class's own validated methods above — this accessor can't be
    // used to bypass the write-database-first ordering, since
    // GraphEngine itself has no persistence awareness to bypass.
    const atlas::graph::GraphEngine& graph() const { return graph_; }

signals:
    // One signal for every kind of change, not one per
    // operation/entity. The earlier design had
    // knowledgeObjectAdded/Updated/Removed, and adding
    // relationshipAdded/Removed alongside them would have made five —
    // and removeKnowledgeObject's cascade (deleting relationships that
    // touched the removed object) would silently never have fired any
    // relationship-specific signal for those cascaded removals. A
    // single signal that every dependent view treats as "go refresh
    // yourself" can't miss a cascade, because there's nothing
    // fine-grained to forget to wire up.
    void graphChanged();

private:
    atlas::persistence::Database* database_;
    atlas::persistence::KnowledgeObjectRepository objectRepository_;
    atlas::persistence::RelationshipRepository relationshipRepository_;
    atlas::graph::GraphEngine graph_;
};

}  // namespace atlas::ui
