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

// A single concept in the knowledge graph. Holds only intrinsic
// content — relationships to other KnowledgeObjects live in the graph
// engine (M2), never as fields here. Storing "Depends On" / "Used By"
// here too would duplicate the same fact in two places with no single
// source of truth (see M0 design notes).
class KnowledgeObject {
public:
    static Result<KnowledgeObject, ValidationError> create(
        std::string title, std::string definition = "", std::string problemSolved = "",
        std::string whyItExists = "", Difficulty difficulty = Difficulty::Beginner,
        ConfidenceLevel confidence = ConfidenceLevel::Unknown);

    // Plain data-transfer struct describing the complete state of a
    // KnowledgeObject as stored. Used only at the persistence boundary
    // — see reconstruct() below.
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

        // Trailing and defaulted deliberately: this field was added
        // after StorageRecord already had many positional-brace-init
        // call sites across every module's tests (see
        // docs/DECISIONS.md's "enums are stored as text" entry for the
        // general spirit — small, deliberate storage-shape decisions
        // documented in one place). A default member initializer lets
        // every existing `StorageRecord{a, b, c, ...}` call keep
        // compiling unchanged; only call sites that need to set a
        // topic actually mention this field. std::optional, not a bare
        // TopicId, because "not yet assigned to a topic" is a real
        // state a KnowledgeObject can be in transiently (see
        // assignToTopic()) even though every object reachable through
        // WorkspaceController ends up with one.
        std::optional<TopicId> topicId = std::nullopt;
    };

    // Rebuilds a KnowledgeObject from a previously-saved StorageRecord
    // (an existing id and historical timestamps, not freshly generated
    // ones). Still validates the title: a corrupted database row should
    // fail loudly here rather than produce an invalid in-memory object.
    // This is deliberately separate from create() — create() expresses
    // "a new concept was authored," reconstruct() expresses "a concept
    // is being loaded back." Conflating them either makes create() take
    // an id/timestamps it shouldn't, or makes reconstruct() go through
    // mutators that stamp updatedAt to "now" on every load — both wrong.
    static Result<KnowledgeObject, ValidationError> reconstruct(StorageRecord record);

    // Title is the only field with a real invariant (must be
    // non-empty), so it's the only mutator that can fail. Everything
    // else is a plain setter — a concept can legitimately have a blank
    // definition while it's still being authored.
    Result<void, ValidationError> renameTo(std::string newTitle);
    void redefineAs(std::string newDefinition);
    void describeProblemAs(std::string problemSolved);
    void explainWhyItExistsAs(std::string whyItExists);
    void addExample(Example example);
    void addMiniProject(MiniProject project);
    void addReference(Reference reference);
    void setNotes(std::string notes);
    void setDifficulty(Difficulty difficulty);
    void setConfidence(ConfidenceLevel confidence);

    // No validation possible to fail here (unlike renameTo) — every
    // TopicId is equally valid as far as this class can tell; whether
    // it refers to a Topic that actually exists is a WorkspaceController/
    // repository-layer concern, the same division of responsibility as
    // Relationship's self-loop check (intrinsic, lives on the class)
    // vs. its duplicate-edge check (needs the whole graph, lives on
    // the controller).
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

}  // namespace atlas::core
