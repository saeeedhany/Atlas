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

Result<void, PersistenceError> execute(sqlite3* db, std::string_view sql);

class Statement {
public:
    static Result<Statement, PersistenceError> prepare(sqlite3* db, std::string_view sql);

    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bindText(int index, std::string_view value);
    void bindOptionalText(int index, const std::optional<std::string>& value);
    void bindInt64(int index, int64_t value);
    void bindDouble(int index, double value);
    void bindOptionalInt64(int index, std::optional<int64_t> value);

    Result<bool, PersistenceError> step();

    std::string columnText(int index) const;
    std::optional<std::string> columnOptionalText(int index) const;
    int64_t columnInt64(int index) const;
    double columnDouble(int index) const;
    std::optional<int64_t> columnOptionalInt64(int index) const;

private:
    Statement(sqlite3_stmt* stmt, sqlite3* db);

    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;
};

template <typename Body>
Result<void, PersistenceError> inTransaction(sqlite3* db, Body&& body) {
    auto begin = execute(db, "BEGIN;");
    if (!begin.hasValue()) return begin;
    auto result = body();
    if (!result.hasValue()) {
        execute(db, "ROLLBACK;");
        return result;
    }
    auto commit = execute(db, "COMMIT;");
    if (!commit.hasValue()) execute(db, "ROLLBACK;");
    return commit;
}

}
