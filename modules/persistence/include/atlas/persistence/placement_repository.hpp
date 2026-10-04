#pragma once

#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/core/result.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::Placement;

class PlacementRepository {
public:
    explicit PlacementRepository(Database& database);

    Result<void, PersistenceError> saveAll(const std::vector<Placement>& placements);
    Result<std::vector<Placement>, PersistenceError> findAll();

private:
    Database* database_;
};

}  // namespace atlas::persistence
