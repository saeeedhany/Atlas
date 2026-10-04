#pragma once

#include <chrono>
#include <string>

#include "atlas/core/result.hpp"
#include "atlas/core/strong_id.hpp"

namespace atlas::core {

enum class TopicValidationError {
    EmptyName,
};

// A named, top-level grouping of KnowledgeObjects - "OS," "Databases,"
// "Distributed Systems." Topics don't nest (that's real future scope,
// not this one) and a KnowledgeObject belongs to exactly one Topic at
// a time (see KnowledgeObject::topicId()). Isolation between topics
// (nothing in one topic's subgraph connects to another's) is enforced
// by WorkspaceController when creating relationships - same reasoning
// as the duplicate-edge check: an app-level rule about how two
// KnowledgeObjects relate, not something intrinsic to either object or
// to Topic itself.
class Topic {
public:
    static Result<Topic, TopicValidationError> create(std::string name,
                                                        std::string description = "");

    // Plain data-transfer struct for the persistence boundary - same
    // rationale as KnowledgeObject::StorageRecord.
    struct StorageRecord {
        TopicId id;
        std::string name;
        std::string description;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point updatedAt;
    };

    // See KnowledgeObject::reconstruct() for why this is a separate
    // factory from create() rather than create() accepting an
    // optional id/timestamps.
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

// The one Topic every pre-existing KnowledgeObject gets backfilled
// into when the topics feature is first migrated onto a database that
// predates it (see persistence migration 2) - a fixed, well-known id
// (the nil UUID) rather than one generated at migration time, so it's
// the same id on every machine's database and can be referenced from
// C++ without a round trip through storage. Not special beyond that:
// it's a normal Topic row, renamable and (once empty) removable like
// any other.
TopicId uncategorizedTopicId();

}  // namespace atlas::core
