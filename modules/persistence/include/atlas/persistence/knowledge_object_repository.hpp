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

class KnowledgeObjectRepository {
public:
    explicit KnowledgeObjectRepository(Database& database);

    Result<void, PersistenceError> save(const KnowledgeObject& object);

    Result<std::optional<KnowledgeObject>, PersistenceError> findById(
        const KnowledgeObjectId& id);

    Result<std::vector<KnowledgeObject>, PersistenceError> findAll();

    Result<void, PersistenceError> remove(const KnowledgeObjectId& id);

private:
    Database* database_;
};

}
