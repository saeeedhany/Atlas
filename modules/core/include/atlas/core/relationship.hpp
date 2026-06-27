#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "atlas/core/enums.hpp"
#include "atlas/core/result.hpp"
#include "atlas/core/strong_id.hpp"

namespace atlas::core {

enum class RelationshipValidationError {
    SelfLoop,
};

// A directed, typed edge between two KnowledgeObjects, identified only
// by their IDs. Relationship has no knowledge of the KnowledgeObject
// class at all — full decoupling. Only the graph engine (M2) ever
// holds both and links them.
//
// Self-loop is the only validation rule that belongs here: it's a
// property of this single edge. "Does this edge already exist between
// these two nodes" requires knowing the whole graph and belongs to the
// graph engine instead.
class Relationship {
public:
    static Result<Relationship, RelationshipValidationError> create(
        KnowledgeObjectId sourceId, KnowledgeObjectId targetId, RelationshipType type,
        std::optional<std::string> note = std::nullopt);

    // Plain data-transfer struct for the persistence boundary — same
    // rationale as KnowledgeObject::StorageRecord.
    struct StorageRecord {
        RelationshipId id;
        KnowledgeObjectId sourceId;
        KnowledgeObjectId targetId;
        RelationshipType type;
        std::optional<std::string> note;
        std::chrono::system_clock::time_point createdAt;
    };

    // Rebuilds a Relationship from a previously-saved StorageRecord
    // (existing id and creation time, not freshly generated). Still
    // enforces the self-loop rule: a corrupted row should fail loudly
    // rather than silently produce an invalid edge.
    static Result<Relationship, RelationshipValidationError> reconstruct(StorageRecord record);

    const RelationshipId& id() const { return id_; }
    const KnowledgeObjectId& sourceId() const { return sourceId_; }
    const KnowledgeObjectId& targetId() const { return targetId_; }
    RelationshipType type() const { return type_; }
    const std::optional<std::string>& note() const { return note_; }
    std::chrono::system_clock::time_point createdAt() const { return createdAt_; }

private:
    explicit Relationship(StorageRecord record);

    RelationshipId id_;
    KnowledgeObjectId sourceId_;
    KnowledgeObjectId targetId_;
    RelationshipType type_;
    std::optional<std::string> note_;
    std::chrono::system_clock::time_point createdAt_;
};

}  // namespace atlas::core
