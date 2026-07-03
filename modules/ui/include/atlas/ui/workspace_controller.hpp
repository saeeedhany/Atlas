#pragma once

#include <QObject>

#include <optional>
#include <string>
#include <vector>

#include "atlas/core/enums.hpp"
#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/core/result.hpp"
#include "atlas/core/topic.hpp"
#include "atlas/graph/graph_engine.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/relationship_repository.hpp"
#include "atlas/persistence/topic_repository.hpp"
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
using atlas::core::Topic;
using atlas::core::TopicId;

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

    // Reassigns the object to a different Topic. Validated the same
    // way createKnowledgeObject's topicId is (must reference a Topic
    // that actually exists) — see
    // WorkspaceController::updateKnowledgeObject.
    std::optional<TopicId> topicId;
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

    // topicId defaults to the Uncategorized bucket — see
    // GraphWindow's constructor comment for the same "trailing
    // default, existing call sites keep their existing meaning"
    // reasoning. Fails with ValidationFailed (not a raw
    // PersistenceFailed constraint-violation) if topicId doesn't
    // reference a Topic that actually exists — checked up front so
    // the error is legible instead of a bare SQL foreign-key message.
    Result<KnowledgeObjectId, ControllerFailure> createKnowledgeObject(
        std::string title, TopicId topicId = atlas::core::uncategorizedTopicId());
    Result<void, ControllerFailure> updateKnowledgeObject(const KnowledgeObjectId& id,
                                                            KnowledgeObjectEdits edits);
    Result<void, ControllerFailure> removeKnowledgeObject(const KnowledgeObjectId& id);

    // --- Topics ---
    // No in-memory cache the way KnowledgeObjects/Relationships have
    // graph_: Topics don't participate in graph traversal (they don't
    // nest, don't have edges of their own), and at the scale a person
    // actually has topics — dozens, not thousands — querying
    // TopicRepository directly on every call is simpler and can't go
    // stale, with no meaningful performance cost to weigh against that
    // simplicity. See docs/DECISIONS.md's general bias toward "measure
    // before optimizing."
    Result<TopicId, ControllerFailure> createTopic(std::string name, std::string description = "");
    Result<void, ControllerFailure> renameTopic(const TopicId& id, std::string newName);
    Result<void, ControllerFailure> removeTopic(const TopicId& id);
    // Not const: TopicRepository::findAll()/findById() aren't const
    // either (see their header comment) — same convention this
    // controller already follows for every other query that touches a
    // repository, not a new one introduced here.
    std::vector<Topic> allTopics();  // sorted by name
    std::optional<Topic> findTopic(const TopicId& id);

    // Sorted the same way allKnowledgeObjects() is — see that method's
    // comment for why.
    std::vector<KnowledgeObject> knowledgeObjectsInTopic(const TopicId& topicId) const;

    // Rejects a self-loop, rejects a duplicate — including, for a
    // symmetric type, the same pair stored in reverse order — and
    // rejects connecting two KnowledgeObjects in different Topics.
    // The topic check is here, not on Relationship or KnowledgeObject:
    // it's an app-level rule about how two objects may relate (same
    // division as the duplicate-edge check), not something intrinsic
    // to either class. All three checks run against the in-memory
    // graph *before* writing anything, so a rejection is never
    // discovered only after the database has already accepted a row.
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

    // Unlike allKnowledgeObjects(), NOT sorted alphabetically when a
    // query is given — ranked by relevance via GraphEngine::search(),
    // since for a search result that ranking is the actual point. An
    // empty query delegates to allKnowledgeObjects() instead of
    // GraphEngine::search()'s own "unranked" empty-query behavior, so
    // clearing the search box gives back the same familiar
    // alphabetical order the plain list view uses.
    std::vector<KnowledgeObject> search(const std::string& query) const;

    // Read-only access to the underlying graph, for consumers that
    // need to reason about structure directly (e.g. GraphWindow's
    // layout computation). Every write still goes through this
    // class's own validated methods above — this accessor can't be
    // used to bypass the write-database-first ordering, since
    // GraphEngine itself has no persistence awareness to bypass.
    const atlas::graph::GraphEngine& graph() const { return graph_; }

    enum class RoadmapErrorCode { UnknownNode, CycleDetected };
    struct RoadmapFailure {
        RoadmapErrorCode code;
    };

    // Resolves GraphEngine::learningRoadmapFor() ids back to full
    // KnowledgeObjects, in dependency order — same resolve-and-wrap
    // pattern as allKnowledgeObjects()/search(), just translating
    // GraphError into a UI-facing error shape instead of reusing
    // GraphError directly (atlas-ui shouldn't need to know about
    // atlas-graph's error enum to handle this).
    Result<std::vector<KnowledgeObject>, RoadmapFailure> roadmapFor(
        const KnowledgeObjectId& id) const;

    // The UI-facing counterpart of atlas::graph::GraphEngine::ProjectSuggestion
    // — same readiness/leverage fields, but with the concept already
    // resolved to a full KnowledgeObject (so a dialog can show its
    // title and MiniProjects directly, the same resolve-and-wrap
    // pattern as roadmapFor()/search() above).
    struct ProjectSuggestion {
        KnowledgeObject knowledgeObject;
        double readiness;
        int leverage;
    };

    // No Result wrapper, unlike roadmapFor(): an empty or unmatched
    // topic is a valid, non-error outcome here (there's simply nothing
    // to suggest yet), not a failure state the way an unknown node id
    // or a dependency cycle is for a roadmap.
    std::vector<ProjectSuggestion> suggestProjects(const atlas::core::TopicId& topicId) const;

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

    // Separate from graphChanged() deliberately: the two have
    // genuinely different observers with different refresh costs.
    // TopicSelectorWindow cares about this and not graphChanged();
    // GraphWindow/KnowledgeObjectPanel care about graphChanged() and
    // not this. Collapsing them the way the three original
    // KnowledgeObject signals were collapsed into one graphChanged()
    // would mean every topic rename triggers a full graph relayout in
    // whichever topic happens to be open — wasted work for an
    // unrelated observer, not a case of "something might be missed."
    void topicsChanged();

private:
    atlas::persistence::Database* database_;
    atlas::persistence::KnowledgeObjectRepository objectRepository_;
    atlas::persistence::RelationshipRepository relationshipRepository_;
    atlas::persistence::TopicRepository topicRepository_;
    atlas::graph::GraphEngine graph_;
};

}  // namespace atlas::ui
