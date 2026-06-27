#pragma once

#include <optional>
#include <vector>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/result.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;
using atlas::core::Result;

// CRUD only — deliberately. Querying by relationship, traversing
// dependencies, or anything that needs to reason about the graph as a
// whole belongs to atlas-graph, which loads everything via findAll()
// once and builds its own in-memory index. Mixing that into a
// repository is how repositories grow into 2,000-line god classes.
class KnowledgeObjectRepository {
public:
    // `database` must outlive this repository.
    explicit KnowledgeObjectRepository(Database& database);

    // Upserts by id: inserts a new row, or replaces an existing one.
    Result<void, PersistenceError> save(const KnowledgeObject& object);

    Result<std::optional<KnowledgeObject>, PersistenceError> findById(
        const KnowledgeObjectId& id);

    Result<std::vector<KnowledgeObject>, PersistenceError> findAll();

    // No-op (not an error) if the id doesn't exist. Cascades to the
    // object's examples/mini-projects/references and any relationship
    // touching it, via ON DELETE CASCADE.
    Result<void, PersistenceError> remove(const KnowledgeObjectId& id);

private:
    Database* database_;
};

}  // namespace atlas::persistence
