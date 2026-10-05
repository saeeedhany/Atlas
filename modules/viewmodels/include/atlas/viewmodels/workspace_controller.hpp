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
#include "atlas/viewmodels/controller_error.hpp"

namespace atlas::viewmodels {

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

struct KnowledgeObjectEdits {
    std::optional<std::string> title;
    std::optional<std::string> definition;
    std::optional<std::string> problemSolved;
    std::optional<std::string> whyItExists;
    std::optional<std::string> notes;
    std::optional<Difficulty> difficulty;
    std::optional<ConfidenceLevel> confidence;

    std::optional<TopicId> topicId;

    std::optional<std::vector<atlas::core::Example>> examples;
    std::optional<std::vector<atlas::core::MiniProject>> miniProjects;
    std::optional<std::vector<atlas::core::Reference>> references;
};

class WorkspaceController : public QObject {
    Q_OBJECT

public:
    explicit WorkspaceController(atlas::persistence::Database& database,
                                  QObject* parent = nullptr);

    Result<void, ControllerFailure> load();

    Result<KnowledgeObjectId, ControllerFailure> createKnowledgeObject(
        std::string title, TopicId topicId = atlas::core::uncategorizedTopicId());
    Result<void, ControllerFailure> updateKnowledgeObject(const KnowledgeObjectId& id,
                                                            KnowledgeObjectEdits edits);
    Result<void, ControllerFailure> removeKnowledgeObject(const KnowledgeObjectId& id);

    Result<TopicId, ControllerFailure> createTopic(std::string name, std::string description = "");
    Result<void, ControllerFailure> renameTopic(const TopicId& id, std::string newName);
    Result<void, ControllerFailure> removeTopic(const TopicId& id);
    Result<std::vector<Topic>, ControllerFailure> topics();
    std::vector<Topic> allTopics();
    std::optional<Topic> findTopic(const TopicId& id);

    std::vector<KnowledgeObject> knowledgeObjectsInTopic(const TopicId& topicId) const;

    Result<RelationshipId, ControllerFailure> createRelationship(
        const KnowledgeObjectId& sourceId, const KnowledgeObjectId& targetId, RelationshipType type,
        std::optional<std::string> note);
    Result<void, ControllerFailure> removeRelationship(const RelationshipId& id);
    Result<void, ControllerFailure> setRelationshipNote(const RelationshipId& id,
                                                       std::optional<std::string> note);

    std::vector<KnowledgeObject> allKnowledgeObjects() const;
    std::vector<Relationship> allRelationships() const;
    std::optional<KnowledgeObject> findKnowledgeObject(const KnowledgeObjectId& id) const;

    std::vector<KnowledgeObject> search(const std::string& query) const;

    const atlas::graph::GraphEngine& graph() const { return graph_; }

    enum class RoadmapErrorCode { UnknownNode, CycleDetected };
    struct RoadmapFailure {
        RoadmapErrorCode code;
    };

    Result<std::vector<KnowledgeObject>, RoadmapFailure> roadmapFor(
        const KnowledgeObjectId& id) const;

    struct ProjectSuggestion {
        KnowledgeObject knowledgeObject;
        double readiness;
        int leverage;
    };

    std::vector<ProjectSuggestion> suggestProjects(const atlas::core::TopicId& topicId) const;

signals:
    void graphChanged();

    void topicsChanged();

private:
    atlas::persistence::Database* database_;
    atlas::persistence::KnowledgeObjectRepository objectRepository_;
    atlas::persistence::RelationshipRepository relationshipRepository_;
    atlas::persistence::TopicRepository topicRepository_;
    atlas::graph::GraphEngine graph_;
};

}
