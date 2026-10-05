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

class Relationship {
public:
    static Result<Relationship, RelationshipValidationError> create(
        KnowledgeObjectId sourceId, KnowledgeObjectId targetId, RelationshipType type,
        std::optional<std::string> note = std::nullopt);

    struct StorageRecord {
        RelationshipId id;
        KnowledgeObjectId sourceId;
        KnowledgeObjectId targetId;
        RelationshipType type;
        std::optional<std::string> note;
        std::chrono::system_clock::time_point createdAt;
    };

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

}
