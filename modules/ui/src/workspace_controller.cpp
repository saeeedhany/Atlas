#include "atlas/ui/workspace_controller.hpp"

#include <algorithm>
#include <utility>

namespace atlas::ui {

WorkspaceController::WorkspaceController(atlas::persistence::Database& database, QObject* parent)
    : QObject(parent),
      database_(&database),
      objectRepository_(database),
      relationshipRepository_(database),
      topicRepository_(database) {}

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
    std::string title, TopicId topicId) {
    // Checked up front, same reasoning as the duplicate-edge check in
    // createRelationship: the knowledge_objects.topic_id foreign key
    // would reject this too, but "no such Topic" is a legible error
    // this layer can produce, versus a raw SQL constraint-violation
    // message bubbling up from the repository.
    auto topicResult = topicRepository_.findById(topicId);
    if (!topicResult.hasValue()) {
        return Result<KnowledgeObjectId, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, topicResult.error().detail});
    }
    if (!topicResult.value().has_value()) {
        return Result<KnowledgeObjectId, ControllerFailure>::err(
            {ControllerErrorCode::NotFound, "No Topic with that id"});
    }

    auto created = KnowledgeObject::create(std::move(title));
    if (!created.hasValue()) {
        return Result<KnowledgeObjectId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed, "Title cannot be empty"});
    }
    auto object = std::move(created).value();
    object.assignToTopic(topicId);
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
    if (edits.topicId.has_value()) {
        auto topicResult = topicRepository_.findById(*edits.topicId);
        if (!topicResult.hasValue()) {
            return Result<void, ControllerFailure>::err(
                {ControllerErrorCode::PersistenceFailed, topicResult.error().detail});
        }
        if (!topicResult.value().has_value()) {
            return Result<void, ControllerFailure>::err(
                {ControllerErrorCode::NotFound, "No Topic with that id"});
        }
        updated.assignToTopic(*edits.topicId);
    }
    if (edits.examples.has_value()) updated.setExamples(*edits.examples);
    if (edits.miniProjects.has_value()) updated.setMiniProjects(*edits.miniProjects);
    if (edits.references.has_value()) updated.setReferences(*edits.references);

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

    const KnowledgeObject* sourceObject = graph_.findNode(sourceId);
    const KnowledgeObject* targetObject = graph_.findNode(targetId);
    if (sourceObject == nullptr || targetObject == nullptr) {
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::NotFound, "No KnowledgeObject with that id"});
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

std::vector<KnowledgeObject> WorkspaceController::search(const std::string& query) const {
    if (query.empty()) return allKnowledgeObjects();

    std::vector<KnowledgeObject> results;
    for (const auto& id : graph_.search(query)) {
        const KnowledgeObject* object = graph_.findNode(id);
        if (object != nullptr) results.push_back(*object);
    }
    return results;
}

Result<std::vector<KnowledgeObject>, WorkspaceController::RoadmapFailure>
WorkspaceController::roadmapFor(const KnowledgeObjectId& id) const {
    auto roadmapIds = graph_.learningRoadmapFor(id);
    if (!roadmapIds.hasValue()) {
        auto code = (roadmapIds.error() == atlas::graph::GraphError::UnknownNode)
                        ? RoadmapErrorCode::UnknownNode
                        : RoadmapErrorCode::CycleDetected;
        return Result<std::vector<KnowledgeObject>, RoadmapFailure>::err(RoadmapFailure{code});
    }

    std::vector<KnowledgeObject> roadmap;
    roadmap.reserve(roadmapIds.value().size());
    for (const auto& nodeId : roadmapIds.value()) {
        const KnowledgeObject* object = graph_.findNode(nodeId);
        if (object != nullptr) roadmap.push_back(*object);
    }
    return Result<std::vector<KnowledgeObject>, RoadmapFailure>::ok(std::move(roadmap));
}

std::vector<WorkspaceController::ProjectSuggestion> WorkspaceController::suggestProjects(
    const atlas::core::TopicId& topicId) const {
    std::vector<ProjectSuggestion> suggestions;
    for (const auto& raw : graph_.suggestProjects(topicId)) {
        const KnowledgeObject* object = graph_.findNode(raw.conceptId);
        if (object == nullptr) continue;  // shouldn't happen: id just came from this same graph
        suggestions.push_back(ProjectSuggestion{*object, raw.readiness, raw.leverage});
    }
    return suggestions;
}

Result<TopicId, ControllerFailure> WorkspaceController::createTopic(std::string name,
                                                                      std::string description) {
    auto created = Topic::create(std::move(name), std::move(description));
    if (!created.hasValue()) {
        return Result<TopicId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed, "Name cannot be empty"});
    }
    auto topic = std::move(created).value();
    auto id = topic.id();

    auto saveResult = topicRepository_.save(topic);
    if (!saveResult.hasValue()) {
        return Result<TopicId, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, saveResult.error().detail});
    }

    emit topicsChanged();
    return Result<TopicId, ControllerFailure>::ok(id);
}

Result<void, ControllerFailure> WorkspaceController::renameTopic(const TopicId& id,
                                                                    std::string newName) {
    auto found = topicRepository_.findById(id);
    if (!found.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, found.error().detail});
    }
    if (!found.value().has_value()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::NotFound, "No Topic with that id"});
    }
    auto topic = std::move(found).value().value();

    auto renameResult = topic.renameTo(std::move(newName));
    if (!renameResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed, "Name cannot be empty"});
    }

    auto saveResult = topicRepository_.save(topic);
    if (!saveResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, saveResult.error().detail});
    }

    emit topicsChanged();
    return Result<void, ControllerFailure>::ok();
}

Result<void, ControllerFailure> WorkspaceController::removeTopic(const TopicId& id) {
    if (id == atlas::core::uncategorizedTopicId()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed,
             "The Uncategorized topic can't be removed — it's where migrated and "
             "not-yet-sorted concepts live"});
    }

    // Checked up front rather than just letting the database's foreign
    // key (knowledge_objects.topic_id has no ON DELETE clause — see
    // migration 2) reject the delete: same "legible error instead of a
    // raw constraint-violation message" reasoning as
    // createKnowledgeObject's topic check.
    auto members = knowledgeObjectsInTopic(id);
    if (!members.empty()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed,
             "This topic still has " + std::to_string(members.size()) +
                 " concept(s) in it — move or delete them first"});
    }

    auto removeResult = topicRepository_.remove(id);
    if (!removeResult.hasValue()) {
        return Result<void, ControllerFailure>::err(
            {ControllerErrorCode::PersistenceFailed, removeResult.error().detail});
    }

    emit topicsChanged();
    return Result<void, ControllerFailure>::ok();
}

std::vector<Topic> WorkspaceController::allTopics() {
    auto result = topicRepository_.findAll();
    std::vector<Topic> topics = result.hasValue() ? std::move(result).value() : std::vector<Topic>{};
    std::sort(topics.begin(), topics.end(),
              [](const Topic& a, const Topic& b) { return a.name() < b.name(); });
    return topics;
}

std::optional<Topic> WorkspaceController::findTopic(const TopicId& id) {
    auto result = topicRepository_.findById(id);
    if (!result.hasValue()) return std::nullopt;
    return result.value();
}

std::vector<KnowledgeObject> WorkspaceController::knowledgeObjectsInTopic(
    const TopicId& topicId) const {
    std::vector<KnowledgeObject> objects;
    for (const auto& id : graph_.allNodeIds()) {
        const KnowledgeObject* object = graph_.findNode(id);
        if (object != nullptr && object->topicId().has_value() && *object->topicId() == topicId) {
            objects.push_back(*object);
        }
    }
    std::sort(objects.begin(), objects.end(), [](const KnowledgeObject& a, const KnowledgeObject& b) {
        return a.title() < b.title();
    });
    return objects;
}

}  // namespace atlas::ui
