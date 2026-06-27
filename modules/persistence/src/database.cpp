#include "atlas/persistence/database.hpp"

#include <sqlite3.h>

#include <utility>

#include "detail/statement.hpp"
#include "migrations.hpp"

namespace atlas::persistence {

Result<Database, PersistenceError> Database::open(const std::string& path) {
    sqlite3* raw = nullptr;
    int rc = sqlite3_open(path.c_str(), &raw);
    if (rc != SQLITE_OK) {
        std::string detail = raw != nullptr ? sqlite3_errmsg(raw) : "unknown sqlite error";
        if (raw != nullptr) sqlite3_close(raw);
        return Result<Database, PersistenceError>::err(
            PersistenceError{PersistenceErrorCode::ConnectionFailed, std::move(detail)});
    }

    // SQLite enforces foreign keys per-connection and defaults to OFF;
    // we rely on ON DELETE CASCADE for referential integrity, so this
    // must be set before anything else touches the connection.
    auto fkResult = detail::execute(raw, "PRAGMA foreign_keys = ON;");
    if (!fkResult.hasValue()) {
        sqlite3_close(raw);
        return Result<Database, PersistenceError>::err(std::move(fkResult).error());
    }

    // WAL improves concurrent read/write behavior for a desktop app
    // with no real downside for our access pattern.
    auto walResult = detail::execute(raw, "PRAGMA journal_mode = WAL;");
    if (!walResult.hasValue()) {
        sqlite3_close(raw);
        return Result<Database, PersistenceError>::err(std::move(walResult).error());
    }

    auto migrationResult = detail::runMigrations(raw);
    if (!migrationResult.hasValue()) {
        sqlite3_close(raw);
        return Result<Database, PersistenceError>::err(std::move(migrationResult).error());
    }

    return Result<Database, PersistenceError>::ok(Database(raw));
}

Database::Database(sqlite3* handle) : handle_(handle) {}

Database::~Database() {
    if (handle_ != nullptr) sqlite3_close(handle_);
}

Database::Database(Database&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        if (handle_ != nullptr) sqlite3_close(handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

}  // namespace atlas::persistence
