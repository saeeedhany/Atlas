#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "atlas/core/content_types.hpp"
#include "atlas/core/enums.hpp"
#include "atlas/core/result.hpp"
#include "atlas/core/strong_id.hpp"

namespace atlas::core {

enum class ValidationError {
    EmptyTitle,
};

class KnowledgeObject {
public:
    static Result<KnowledgeObject, ValidationError> create(
        std::string title, std::string definition = "", std::string problemSolved = "",
        std::string whyItExists = "", Difficulty difficulty = Difficulty::Beginner,
        ConfidenceLevel confidence = ConfidenceLevel::Unknown);

    struct StorageRecord {
        KnowledgeObjectId id;
        std::string title;
        std::string definition;
        std::string problemSolved;
        std::string whyItExists;
        std::vector<Example> examples;
        std::vector<MiniProject> miniProjects;
        std::vector<Reference> references;
        std::string notes;
        Difficulty difficulty;
        ConfidenceLevel confidence;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point updatedAt;

        std::optional<TopicId> topicId = std::nullopt;
    };

    static Result<KnowledgeObject, ValidationError> reconstruct(StorageRecord record);

    Result<void, ValidationError> renameTo(std::string newTitle);
    void redefineAs(std::string newDefinition);
    void describeProblemAs(std::string problemSolved);
    void explainWhyItExistsAs(std::string whyItExists);
    void addExample(Example example);
    void addMiniProject(MiniProject project);
    void addReference(Reference reference);

    void setExamples(std::vector<Example> examples);
    void setMiniProjects(std::vector<MiniProject> projects);
    void setReferences(std::vector<Reference> references);
    void setNotes(std::string notes);
    void setDifficulty(Difficulty difficulty);
    void setConfidence(ConfidenceLevel confidence);

    void assignToTopic(TopicId topicId);

    const KnowledgeObjectId& id() const { return id_; }
    const std::string& title() const { return title_; }
    const std::string& definition() const { return definition_; }
    const std::string& problemSolved() const { return problemSolved_; }
    const std::string& whyItExists() const { return whyItExists_; }
    const std::vector<Example>& examples() const { return examples_; }
    const std::vector<MiniProject>& miniProjects() const { return miniProjects_; }
    const std::vector<Reference>& references() const { return references_; }
    const std::string& notes() const { return notes_; }
    Difficulty difficulty() const { return difficulty_; }
    ConfidenceLevel confidence() const { return confidence_; }
    std::chrono::system_clock::time_point createdAt() const { return createdAt_; }
    std::chrono::system_clock::time_point updatedAt() const { return updatedAt_; }
    const std::optional<TopicId>& topicId() const { return topicId_; }

private:
    explicit KnowledgeObject(StorageRecord record);

    void touch();

    KnowledgeObjectId id_;
    std::string title_;
    std::string definition_;
    std::string problemSolved_;
    std::string whyItExists_;
    std::vector<Example> examples_;
    std::vector<MiniProject> miniProjects_;
    std::vector<Reference> references_;
    std::string notes_;
    Difficulty difficulty_;
    ConfidenceLevel confidence_;
    std::chrono::system_clock::time_point createdAt_;
    std::chrono::system_clock::time_point updatedAt_;
    std::optional<TopicId> topicId_;
};

}
