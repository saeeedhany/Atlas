#include "atlas/core/topic.hpp"

#include <utility>

#include "atlas/core/uuid.hpp"

namespace atlas::core {

Result<Topic, TopicValidationError> Topic::create(std::string name, std::string description) {
    if (name.empty()) {
        return Result<Topic, TopicValidationError>::err(TopicValidationError::EmptyName);
    }
    auto now = std::chrono::system_clock::now();
    StorageRecord record{TopicId::generate(), std::move(name), std::move(description),
                          /*createdAt=*/now, /*updatedAt=*/now};
    return Result<Topic, TopicValidationError>::ok(Topic(std::move(record)));
}

Result<Topic, TopicValidationError> Topic::reconstruct(StorageRecord record) {
    if (record.name.empty()) {
        return Result<Topic, TopicValidationError>::err(TopicValidationError::EmptyName);
    }
    return Result<Topic, TopicValidationError>::ok(Topic(std::move(record)));
}

Topic::Topic(StorageRecord record)
    : id_(record.id),
      name_(std::move(record.name)),
      description_(std::move(record.description)),
      createdAt_(record.createdAt),
      updatedAt_(record.updatedAt) {}

void Topic::touch() { updatedAt_ = std::chrono::system_clock::now(); }

Result<void, TopicValidationError> Topic::renameTo(std::string newName) {
    if (newName.empty()) {
        return Result<void, TopicValidationError>::err(TopicValidationError::EmptyName);
    }
    name_ = std::move(newName);
    touch();
    return Result<void, TopicValidationError>::ok();
}

void Topic::redescribeAs(std::string description) {
    description_ = std::move(description);
    touch();
}

TopicId uncategorizedTopicId() {
    // Parse of a fixed, valid literal - never fails in practice, but
    // routed through the same fallible Uuid::parse() every other UUID
    // in this codebase goes through rather than a separate hand-rolled
    // "all zero bytes" constructor.
    return TopicId(*Uuid::parse("00000000-0000-0000-0000-000000000000"));
}

}  // namespace atlas::core
