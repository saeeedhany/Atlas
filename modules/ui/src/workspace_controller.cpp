#include "atlas/ui/workspace_controller.hpp"

#include <algorithm>
#include <utility>

namespace atlas::ui {

WorkspaceController::WorkspaceController(atlas::persistence::Database& database, QObject* parent)
    : QObject(parent),
      database_(&database),
      objectRepository_(database),
      relationshipRepository_(database) {}

Result<void, ControllerFailure> WorkspaceController::load() {
    auto objectsResult = objectRepository_.findAll();
    if (!objectsResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, objectsResult.error().detail});
    }
    for (auto& object : objectsResult.value()) {
        auto addResult = graph_.addNode(std::move(object));
        if (!addResult.hasValue()) {
            return Result<void, ControllerFailure>::err(
                {ControllerErrorCode::GraphInconsistency,
                 "addNode failed while loading KnowledgeObjects from the database"});
        }
    }

    auto relationshipsResult = relationshipRepository_.findAll();
    if (!relationshipsResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, relationshipsResult.error().detail});
    }
    for (auto& relationship : relationshipsResult.value()) {
        auto addResult = graph_.addEdge(std::move(relationship));
        if (!addResult.hasValue()) {
            return Result<void, ControllerFailure>::err(
                {ControllerErrorCode::GraphInconsistency,
                 "addEdge failed while loading Relationships from the database"});
        }
    }
    return Result<void, ControllerFailure>::ok();
}

Result<KnowledgeObjectId, ControllerFailure> WorkspaceController::createKnowledgeObject(
    std::string title) {
    auto created = KnowledgeObject::create(std::move(title));
    if (!created.hasValue()) {
        return Result<KnowledgeObjectId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed, "Title cannot be empty"});
    }
    auto object = std::move(created).value();
    auto id = object.id();

    auto saveResult = objectRepository_.save(object);
    if (!saveResult.hasValue()) {
        return Result<KnowledgeObjectId, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, saveResult.error().detail});
    }

    auto graphResult = graph_.addNode(std::move(object));
    if (!graphResult.hasValue()) {
        // The database now has this object but the in-memory graph
        // doesn't. Should be unreachable — the id was just freshly
        // generated — but if it ever happens, the database remains the
        // source of truth and a future load() would self-heal. Surface
        // it rather than silently swallowing the inconsistency.
        return Result<KnowledgeObjectId, ControllerFailure>::err(
            {ControllerErrorCode::GraphInconsistency, "addNode failed after a successful save"});
    }

    emit graphChanged();
    return Result<KnowledgeObjectId, ControllerFailure>::ok(id);
}

Result<void, ControllerFailure> WorkspaceController::updateKnowledgeObject(
    const KnowledgeObjectId& id, KnowledgeObjectEdits edits) {
    const KnowledgeObject* current = graph_.findNode(id);
    if (current == nullptr) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::NotFound, "No KnowledgeObject with that id"});
    }

    // Copy-modify-then-replace, never mutate the graph's live object
    // directly: this is what lets every error below return early with
    // the graph and database both still in their original, consistent
    // state.
    KnowledgeObject updated = *current;

    if (edits.title.has_value()) {
        auto renameResult = updated.renameTo(*edits.title);
        if (!renameResult.hasValue()) {
            return Result<void, ControllerFailure>::err(
                {ControllerErrorCode::ValidationFailed, "Title cannot be empty"});
        }
    }
    if (edits.definition.has_value()) updated.redefineAs(*edits.definition);
    if (edits.problemSolved.has_value()) updated.describeProblemAs(*edits.problemSolved);
    if (edits.whyItExists.has_value()) updated.explainWhyItExistsAs(*edits.whyItExists);
    if (edits.notes.has_value()) updated.setNotes(*edits.notes);
    if (edits.difficulty.has_value()) updated.setDifficulty(*edits.difficulty);
    if (edits.confidence.has_value()) updated.setConfidence(*edits.confidence);

    auto saveResult = objectRepository_.save(updated);
    if (!saveResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, saveResult.error().detail});
    }

    auto updateResult = graph_.updateNode(std::move(updated));
    if (!updateResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::GraphInconsistency, "updateNode failed after a successful save"});
    }

    emit graphChanged();
    return Result<void, ControllerFailure>::ok();
}

Result<void, ControllerFailure> WorkspaceController::removeKnowledgeObject(
    const KnowledgeObjectId& id) {
    auto removeResult = objectRepository_.remove(id);
    if (!removeResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, removeResult.error().detail});
    }

    graph_.removeNode(id);  // database is the source of truth; already removed there
    emit graphChanged();
    return Result<void, ControllerFailure>::ok();
}

Result<RelationshipId, ControllerFailure> WorkspaceController::createRelationship(
    const KnowledgeObjectId& sourceId, const KnowledgeObjectId& targetId, RelationshipType type,
    std::optional<std::string> note) {
    if (sourceId == targetId) {
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed, "A concept cannot have a relationship to itself"});
    }
    // Checked against the graph *before* writing anything: the database's
    // UNIQUE(source_id, target_id, type) constraint doesn't catch a
    // symmetric type's reverse-pair duplicate, but GraphEngine does.
    // Checking here means that case is rejected before any database
    // write, not discovered as an inconsistency after one.
    if (graph_.hasDuplicateEdge(sourceId, targetId, type)) {
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed,
             "This relationship (or its symmetric equivalent) already exists"});
    }

    auto created = Relationship::create(sourceId, targetId, type, std::move(note));
    if (!created.hasValue()) {
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed, "A concept cannot have a relationship to itself"});
    }
    auto relationship = std::move(created).value();
    auto id = relationship.id();

    auto saveResult = relationshipRepository_.save(relationship);
    if (!saveResult.hasValue()) {
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, saveResult.error().detail});
    }

    auto graphResult = graph_.addEdge(std::move(relationship));
    if (!graphResult.hasValue()) {
        // Should be unreachable now that hasDuplicateEdge is checked
        // up front — same reasoning as createKnowledgeObject's
        // equivalent comment.
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::GraphInconsistency, "addEdge failed after a successful save"});
    }

    emit graphChanged();
    return Result<RelationshipId, ControllerFailure>::ok(id);
}

Result<void, ControllerFailure> WorkspaceController::removeRelationship(
    const RelationshipId& id) {
    auto removeResult = relationshipRepository_.remove(id);
    if (!removeResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, removeResult.error().detail});
    }

    graph_.removeEdge(id);  // database is the source of truth; already removed there
    emit graphChanged();
    return Result<void, ControllerFailure>::ok();
}

std::vector<KnowledgeObject> WorkspaceController::allKnowledgeObjects() const {
    std::vector<KnowledgeObject> objects;
    for (const auto& id : graph_.allNodeIds()) {
        const KnowledgeObject* object = graph_.findNode(id);
        if (object != nullptr) objects.push_back(*object);
    }
    std::sort(objects.begin(), objects.end(), [](const KnowledgeObject& a, const KnowledgeObject& b) {
        return a.title() < b.title();
    });
    return objects;
}

std::vector<Relationship> WorkspaceController::allRelationships() const {
    std::vector<Relationship> relationships;
    for (const auto& id : graph_.allEdgeIds()) {
        const Relationship* relationship = graph_.findEdge(id);
        if (relationship != nullptr) relationships.push_back(*relationship);
    }
    // Chronological: relationships have no natural display name to
    // sort by, but creation order is still deterministic and stable.
    std::sort(relationships.begin(), relationships.end(),
              [](const Relationship& a, const Relationship& b) {
                  return a.createdAt() < b.createdAt();
              });
    return relationships;
}

std::optional<KnowledgeObject> WorkspaceController::findKnowledgeObject(
    const KnowledgeObjectId& id) const {
    const KnowledgeObject* object = graph_.findNode(id);
    if (object == nullptr) return std::nullopt;
    return *object;
}

}  // namespace atlas::ui
