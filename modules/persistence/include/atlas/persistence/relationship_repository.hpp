#pragma once

#include <optional>
#include <vector>

#include "atlas/core/relationship.hpp"
#include "atlas/core/result.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::Relationship;
using atlas::core::RelationshipId;
using atlas::core::Result;

class RelationshipRepository {
public:
    explicit RelationshipRepository(Database& database);

    Result<void, PersistenceError> save(const Relationship& relationship);
    Result<void, PersistenceError> updateNote(const RelationshipId& id,
                                             const std::optional<std::string>& note);

    Result<std::optional<Relationship>, PersistenceError> findById(const RelationshipId& id);

    Result<std::vector<Relationship>, PersistenceError> findAll();

    Result<void, PersistenceError> remove(const RelationshipId& id);

private:
    Database* database_;
};

}
