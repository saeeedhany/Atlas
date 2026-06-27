#pragma once

#include <string>

#include "atlas/core/result.hpp"
#include "atlas/persistence/persistence_error.hpp"

// Forward-declared, not included: callers of atlas-persistence never
// need the real sqlite3 type, only Database/Repository classes do, and
// those only internally (see handle()). This keeps SQLite's headers
// out of every translation unit that merely uses this module.
struct sqlite3;

namespace atlas::persistence {

using atlas::core::Result;

// Owns a single SQLite connection for the lifetime of the object: opens
// the database file, turns on foreign-key enforcement (off by default
// in SQLite, and we rely on it for cascade deletes), enables WAL mode,
// and runs any pending schema migrations — all before the constructor
// returns successfully. A Database that exists is a Database that's
// ready to use.
class Database {
public:
    static Result<Database, PersistenceError> open(const std::string& path);

    ~Database();
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

private:
    // Repositories need the raw handle to prepare statements; nothing
    // outside atlas-persistence does, so it stays private + friended
    // rather than becoming part of the public surface.
    friend class KnowledgeObjectRepository;
    friend class RelationshipRepository;

    explicit Database(sqlite3* handle);
    sqlite3* handle() const { return handle_; }

    sqlite3* handle_ = nullptr;
};

}  // namespace atlas::persistence
