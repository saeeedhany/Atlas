#pragma once

#include <optional>
#include <vector>

#include "atlas/core/result.hpp"
#include "atlas/core/topic.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::Result;
using atlas::core::Topic;
using atlas::core::TopicId;

// CRUD only — same reasoning as KnowledgeObjectRepository. There's no
// graph traversal concern here at all (Topics don't nest), so unlike
// KnowledgeObjectRepository this class is about as simple as a
// repository gets: no child rows, no domain-specific queries.
class TopicRepository {
public:
    // `database` must outlive this repository.
    explicit TopicRepository(Database& database);

    // Upserts by id: inserts a new row, or replaces an existing one.
    Result<void, PersistenceError> save(const Topic& topic);

    Result<std::optional<Topic>, PersistenceError> findById(const TopicId& id);

    Result<std::vector<Topic>, PersistenceError> findAll();

    // No-op (not an error) if the id doesn't exist. Does NOT cascade
    // to the KnowledgeObjects that referenced this topic — the
    // knowledge_objects.topic_id foreign key has no ON DELETE clause
    // (see migration 2), so removing a topic that still has members
    // fails at the database level with a foreign-key-constraint error
    // rather than silently orphaning or cascading away real content.
    // WorkspaceController is expected to check membership and prompt
    // the person before calling this, not treat it as always-safe.
    Result<void, PersistenceError> remove(const TopicId& id);

private:
    Database* database_;
};

}  // namespace atlas::persistence
