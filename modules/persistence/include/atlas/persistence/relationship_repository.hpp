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
    // `database` must outlive this repository.
    explicit RelationshipRepository(Database& database);

    // Upserts by id. A duplicate (source, target, type) triple — even
    // with a different id — is rejected by the database's UNIQUE
    // constraint and surfaces as PersistenceErrorCode::ConstraintViolation.
    Result<void, PersistenceError> save(const Relationship& relationship);

    Result<std::optional<Relationship>, PersistenceError> findById(const RelationshipId& id);

    Result<std::vector<Relationship>, PersistenceError> findAll();

    Result<void, PersistenceError> remove(const RelationshipId& id);

private:
    Database* database_;
};

}  // namespace atlas::persistence
