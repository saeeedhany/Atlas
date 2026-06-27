#include "atlas/core/relationship.hpp"

#include <utility>

namespace atlas::core {

Result<Relationship, RelationshipValidationError> Relationship::create(
    KnowledgeObjectId sourceId, KnowledgeObjectId targetId, RelationshipType type,
    std::optional<std::string> note) {
    if (sourceId == targetId) {
        return Result<Relationship, RelationshipValidationError>::err(
            RelationshipValidationError::SelfLoop);
    }
    StorageRecord record{RelationshipId::generate(), sourceId, targetId, type, std::move(note),
                          std::chrono::system_clock::now()};
    return Result<Relationship, RelationshipValidationError>::ok(Relationship(std::move(record)));
}

Result<Relationship, RelationshipValidationError> Relationship::reconstruct(
    StorageRecord record) {
    if (record.sourceId == record.targetId) {
        return Result<Relationship, RelationshipValidationError>::err(
            RelationshipValidationError::SelfLoop);
    }
    return Result<Relationship, RelationshipValidationError>::ok(Relationship(std::move(record)));
}

Relationship::Relationship(StorageRecord record)
    : id_(record.id),
      sourceId_(record.sourceId),
      targetId_(record.targetId),
      type_(record.type),
      note_(std::move(record.note)),
      createdAt_(record.createdAt) {}

}  // namespace atlas::core
