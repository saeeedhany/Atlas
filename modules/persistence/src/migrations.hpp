#pragma once

#include "atlas/core/result.hpp"
#include "atlas/persistence/persistence_error.hpp"

struct sqlite3;

namespace atlas::persistence::detail {

using atlas::core::Result;

// Applies every migration with version > the highest version recorded
// in schema_migrations, each inside its own transaction. Internal to
// atlas-persistence — Database::open() is the only caller.
Result<void, PersistenceError> runMigrations(sqlite3* db);

}  // namespace atlas::persistence::detail
