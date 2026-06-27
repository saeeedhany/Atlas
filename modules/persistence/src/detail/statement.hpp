#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "atlas/core/result.hpp"
#include "atlas/persistence/persistence_error.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace atlas::persistence::detail {

using atlas::core::Result;

// Runs SQL with no parameters and no result rows (DDL, BEGIN/COMMIT/
// ROLLBACK, PRAGMAs). Kept separate from Statement: most callers in
// this module don't need bind/step at all, and sqlite3_exec is the
// simpler, more direct tool for those cases.
Result<void, PersistenceError> execute(sqlite3* db, std::string_view sql);

// Thin RAII wrapper around sqlite3_stmt*. Internal to atlas-persistence
// — repositories use it, nothing outside this module ever sees it.
class Statement {
public:
    static Result<Statement, PersistenceError> prepare(sqlite3* db, std::string_view sql);

    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // 1-based parameter indices, matching SQLite's own convention.
    void bindText(int index, std::string_view value);
    void bindOptionalText(int index, const std::optional<std::string>& value);
    void bindInt64(int index, int64_t value);

    // true => a row is available (read via columnX); false => done.
    Result<bool, PersistenceError> step();

    std::string columnText(int index) const;
    std::optional<std::string> columnOptionalText(int index) const;
    int64_t columnInt64(int index) const;

private:
    Statement(sqlite3_stmt* stmt, sqlite3* db);

    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;  // not owned; used only for sqlite3_errmsg
};

}  // namespace atlas::persistence::detail
