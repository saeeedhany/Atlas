#pragma once

#include "atlas/core/result.hpp"
#include "atlas/persistence/persistence_error.hpp"

struct sqlite3;

namespace atlas::persistence::detail {

using atlas::core::Result;

Result<void, PersistenceError> runMigrations(sqlite3* db);

}
