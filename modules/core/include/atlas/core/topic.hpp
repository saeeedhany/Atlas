#pragma once

#include <chrono>
#include <string>

#include "atlas/core/result.hpp"
#include "atlas/core/strong_id.hpp"

namespace atlas::core {

enum class TopicValidationError {
    EmptyName,
};

class Topic {
public:
    static Result<Topic, TopicValidationError> create(std::string name,
                                                        std::string description = "");

    struct StorageRecord {
        TopicId id;
        std::string name;
        std::string description;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point updatedAt;
    };

    static Result<Topic, TopicValidationError> reconstruct(StorageRecord record);

    Result<void, TopicValidationError> renameTo(std::string newName);
    void redescribeAs(std::string description);

    const TopicId& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    std::chrono::system_clock::time_point createdAt() const { return createdAt_; }
    std::chrono::system_clock::time_point updatedAt() const { return updatedAt_; }

private:
    explicit Topic(StorageRecord record);
    void touch();

    TopicId id_;
    std::string name_;
    std::string description_;
    std::chrono::system_clock::time_point createdAt_;
    std::chrono::system_clock::time_point updatedAt_;
};

TopicId uncategorizedTopicId();

}
