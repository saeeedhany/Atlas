#pragma once

#include <QObject>

#include <optional>
#include <string>
#include <vector>

#include "atlas/core/enums.hpp"
#include "atlas/core/knowledge_object.hpp"
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

    // Snapshot, sorted alphabetically by title — GraphEngine itself
    // has no opinion on display order (its allNodeIds() is hash-map
    // ordered), so the controller imposes one here, since this is the
    // layer that knows it's serving a UI list.
    std::vector<KnowledgeObject> allKnowledgeObjects() const;
    std::optional<KnowledgeObject> findKnowledgeObject(const KnowledgeObjectId& id) const;

signals:
    void knowledgeObjectAdded(atlas::core::KnowledgeObjectId id);
    void knowledgeObjectUpdated(atlas::core::KnowledgeObjectId id);
    void knowledgeObjectRemoved(atlas::core::KnowledgeObjectId id);

private:
    atlas::persistence::Database* database_;
    atlas::persistence::KnowledgeObjectRepository objectRepository_;
    atlas::persistence::RelationshipRepository relationshipRepository_;
    atlas::graph::GraphEngine graph_;
};

}  // namespace atlas::ui
