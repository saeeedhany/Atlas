#include "detail/statement.hpp"

#include <sqlite3.h>

#include <utility>

namespace atlas::persistence::detail {

namespace {

PersistenceError errorFrom(sqlite3* db, PersistenceErrorCode code) {
    return PersistenceError{code, db ? sqlite3_errmsg(db) : "unknown sqlite error"};
}

}  // namespace

Result<void, PersistenceError> execute(sqlite3* db, std::string_view sql) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, std::string(sql).c_str(), nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        std::string detail = errorMessage ? errorMessage : "unknown sqlite error";
        sqlite3_free(errorMessage);
        return Result<void, PersistenceError>::err(
            PersistenceError{PersistenceErrorCode::QueryFailed, std::move(detail)});
    }
    return Result<void, PersistenceError>::ok();
}

Result<Statement, PersistenceError> Statement::prepare(sqlite3* db, std::string_view sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, std::string(sql).c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Statement, PersistenceError>::err(
            errorFrom(db, PersistenceErrorCode::QueryFailed));
    }
    return Result<Statement, PersistenceError>::ok(Statement(stmt, db));
}

Statement::Statement(sqlite3_stmt* stmt, sqlite3* db) : stmt_(stmt), db_(db) {}

Statement::~Statement() {
    if (stmt_ != nullptr) sqlite3_finalize(stmt_);
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_), db_(other.db_) {
    other.stmt_ = nullptr;
    other.db_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_ != nullptr) sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        db_ = other.db_;
        other.stmt_ = nullptr;
        other.db_ = nullptr;
    }
    return *this;
}

void Statement::bindText(int index, std::string_view value) {
    // SQLITE_TRANSIENT tells SQLite to copy the bytes immediately,
    // since `value` may not outlive this call (e.g. a temporary built
    // from a std::string that's about to be destroyed).
    sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                       SQLITE_TRANSIENT);
}

void Statement::bindOptionalText(int index, const std::optional<std::string>& value) {
    if (value.has_value()) {
        bindText(index, *value);
    } else {
        sqlite3_bind_null(stmt_, index);
    }
}

void Statement::bindInt64(int index, int64_t value) { sqlite3_bind_int64(stmt_, index, value); }

Result<bool, PersistenceError> Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return Result<bool, PersistenceError>::ok(true);
    if (rc == SQLITE_DONE) return Result<bool, PersistenceError>::ok(false);

    auto code = (rc == SQLITE_CONSTRAINT) ? PersistenceErrorCode::ConstraintViolation
                                            : PersistenceErrorCode::QueryFailed;
    return Result<bool, PersistenceError>::err(errorFrom(db_, code));
}

std::string Statement::columnText(int index) const {
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
    return text != nullptr ? std::string(text) : std::string{};
}

std::optional<std::string> Statement::columnOptionalText(int index) const {
    if (sqlite3_column_type(stmt_, index) == SQLITE_NULL) return std::nullopt;
    return columnText(index);
}

int64_t Statement::columnInt64(int index) const { return sqlite3_column_int64(stmt_, index); }

}  // namespace atlas::persistence::detail
