#include "atlas/persistence/topic_repository.hpp"

#include <sqlite3.h>

#include <chrono>
#include <utility>

#include "atlas/core/uuid.hpp"
#include "detail/statement.hpp"

namespace atlas::persistence {

namespace {

int64_t toMillis(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromMillis(int64_t millis) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(millis));
}

constexpr const char* kSelectColumns = "id, name, description, created_at, updated_at";

Result<Topic, PersistenceError> rowToTopic(detail::Statement& statement) {
    std::string id = statement.columnText(0);
    auto parsedId = atlas::core::Uuid::parse(id);
    if (!parsedId.has_value()) {
        return Result<Topic, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation, "topics row has a malformed id: " + id});
    }

    Topic::StorageRecord record{TopicId(*parsedId), statement.columnText(1),
                                 statement.columnText(2), fromMillis(statement.columnInt64(3)),
                                 fromMillis(statement.columnInt64(4))};

    auto reconstructed = Topic::reconstruct(std::move(record));
    if (!reconstructed.hasValue()) {
        return Result<Topic, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation,
            "topics row " + id + " failed domain validation on load"});
    }
    return Result<Topic, PersistenceError>::ok(std::move(reconstructed).value());
}

}  // namespace

TopicRepository::TopicRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> TopicRepository::save(const Topic& topic) {
    sqlite3* db = database_->handle();

    auto stmtResult = detail::Statement::prepare(db, R"sql(
        INSERT INTO topics (id, name, description, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            updated_at = excluded.updated_at;
    )sql");
    if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
    auto statement = std::move(stmtResult).value();

    statement.bindText(1, topic.id().toString());
    statement.bindText(2, topic.name());
    statement.bindText(3, topic.description());
    statement.bindInt64(4, toMillis(topic.createdAt()));
    statement.bindInt64(5, toMillis(topic.updatedAt()));

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    return Result<void, PersistenceError>::ok();
}

Result<std::optional<Topic>, PersistenceError> TopicRepository::findById(const TopicId& id) {
    sqlite3* db = database_->handle();

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM topics WHERE id = ?;";
    auto stmtResult = detail::Statement::prepare(db, sql);
    if (!stmtResult.hasValue()) {
        return Result<std::optional<Topic>, PersistenceError>::err(std::move(stmtResult).error());
    }
    auto statement = std::move(stmtResult).value();
    statement.bindText(1, id.toString());

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) {
        return Result<std::optional<Topic>, PersistenceError>::err(stepResult.error());
    }
    if (!stepResult.value()) {
        return Result<std::optional<Topic>, PersistenceError>::ok(std::nullopt);
    }

    auto topicResult = rowToTopic(statement);
    if (!topicResult.hasValue()) {
        return Result<std::optional<Topic>, PersistenceError>::err(std::move(topicResult).error());
    }
    return Result<std::optional<Topic>, PersistenceError>::ok(std::move(topicResult).value());
}

Result<std::vector<Topic>, PersistenceError> TopicRepository::findAll() {
    sqlite3* db = database_->handle();

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM topics;";
    auto stmtResult = detail::Statement::prepare(db, sql);
    if (!stmtResult.hasValue()) {
        return Result<std::vector<Topic>, PersistenceError>::err(std::move(stmtResult).error());
    }
    auto statement = std::move(stmtResult).value();

    std::vector<Topic> topics;
    while (true) {
        auto stepResult = statement.step();
        if (!stepResult.hasValue()) {
            return Result<std::vector<Topic>, PersistenceError>::err(stepResult.error());
        }
        if (!stepResult.value()) break;

        auto topicResult = rowToTopic(statement);
        if (!topicResult.hasValue()) {
            return Result<std::vector<Topic>, PersistenceError>::err(std::move(topicResult).error());
        }
        topics.push_back(std::move(topicResult).value());
    }
    return Result<std::vector<Topic>, PersistenceError>::ok(std::move(topics));
}

Result<void, PersistenceError> TopicRepository::remove(const TopicId& id) {
    sqlite3* db = database_->handle();
    auto stmtResult = detail::Statement::prepare(db, "DELETE FROM topics WHERE id = ?;");
    if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
    auto statement = std::move(stmtResult).value();
    statement.bindText(1, id.toString());

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    return Result<void, PersistenceError>::ok();
}

}  // namespace atlas::persistence
