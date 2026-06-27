#include "atlas/persistence/relationship_repository.hpp"

#include <sqlite3.h>

#include <chrono>
#include <utility>

#include "detail/statement.hpp"

namespace atlas::persistence {

using atlas::core::KnowledgeObjectId;
using atlas::core::relationshipTypeFromString;
using atlas::core::toDisplayString;
using atlas::core::Uuid;

namespace {

int64_t toMillis(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromMillis(int64_t millis) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(millis));
}

constexpr const char* kSelectColumns = "id, source_id, target_id, type, note, created_at";

Result<Relationship, PersistenceError> rowToRelationship(detail::Statement& statement) {
    std::string id = statement.columnText(0);
    std::string sourceId = statement.columnText(1);
    std::string targetId = statement.columnText(2);

    auto type = relationshipTypeFromString(statement.columnText(3));
    if (!type.has_value()) {
        return Result<Relationship, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation,
            "relationships row " + id + " has an unrecognized type value"});
    }

    auto parsedId = Uuid::parse(id);
    auto parsedSource = Uuid::parse(sourceId);
    auto parsedTarget = Uuid::parse(targetId);
    if (!parsedId.has_value() || !parsedSource.has_value() || !parsedTarget.has_value()) {
        return Result<Relationship, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation,
            "relationships row " + id + " has a malformed id/source_id/target_id"});
    }

    Relationship::StorageRecord record{RelationshipId(*parsedId),
                                        KnowledgeObjectId(*parsedSource),
                                        KnowledgeObjectId(*parsedTarget),
                                        *type,
                                        statement.columnOptionalText(4),
                                        fromMillis(statement.columnInt64(5))};

    auto reconstructed = Relationship::reconstruct(std::move(record));
    if (!reconstructed.hasValue()) {
        return Result<Relationship, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation,
            "relationships row " + id + " failed domain validation on load"});
    }
    return Result<Relationship, PersistenceError>::ok(std::move(reconstructed).value());
}

}  // namespace

RelationshipRepository::RelationshipRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> RelationshipRepository::save(const Relationship& relationship) {
    sqlite3* db = database_->handle();

    auto stmtResult = detail::Statement::prepare(db, R"sql(
        INSERT INTO relationships (id, source_id, target_id, type, note, created_at)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            source_id = excluded.source_id,
            target_id = excluded.target_id,
            type = excluded.type,
            note = excluded.note;
    )sql");
    if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
    auto statement = std::move(stmtResult).value();

    statement.bindText(1, relationship.id().toString());
    statement.bindText(2, relationship.sourceId().toString());
    statement.bindText(3, relationship.targetId().toString());
    statement.bindText(4, toDisplayString(relationship.type()));
    statement.bindOptionalText(5, relationship.note());
    statement.bindInt64(6, toMillis(relationship.createdAt()));

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    return Result<void, PersistenceError>::ok();
}

Result<std::optional<Relationship>, PersistenceError> RelationshipRepository::findById(
    const RelationshipId& id) {
    sqlite3* db = database_->handle();
    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM relationships WHERE id = ?;";
    auto stmtResult = detail::Statement::prepare(db, sql);
    if (!stmtResult.hasValue()) {
        return Result<std::optional<Relationship>, PersistenceError>::err(std::move(stmtResult).error());
    }
    auto statement = std::move(stmtResult).value();
    statement.bindText(1, id.toString());

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) {
        return Result<std::optional<Relationship>, PersistenceError>::err(stepResult.error());
    }
    if (!stepResult.value()) {
        return Result<std::optional<Relationship>, PersistenceError>::ok(std::nullopt);
    }

    auto relResult = rowToRelationship(statement);
    if (!relResult.hasValue()) {
        return Result<std::optional<Relationship>, PersistenceError>::err(std::move(relResult).error());
    }
    return Result<std::optional<Relationship>, PersistenceError>::ok(std::move(relResult).value());
}

Result<std::vector<Relationship>, PersistenceError> RelationshipRepository::findAll() {
    sqlite3* db = database_->handle();
    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM relationships;";
    auto stmtResult = detail::Statement::prepare(db, sql);
    if (!stmtResult.hasValue()) {
        return Result<std::vector<Relationship>, PersistenceError>::err(std::move(stmtResult).error());
    }
    auto statement = std::move(stmtResult).value();

    std::vector<Relationship> relationships;
    while (true) {
        auto stepResult = statement.step();
        if (!stepResult.hasValue()) {
            return Result<std::vector<Relationship>, PersistenceError>::err(stepResult.error());
        }
        if (!stepResult.value()) break;

        auto relResult = rowToRelationship(statement);
        if (!relResult.hasValue()) {
            return Result<std::vector<Relationship>, PersistenceError>::err(std::move(relResult).error());
        }
        relationships.push_back(std::move(relResult).value());
    }
    return Result<std::vector<Relationship>, PersistenceError>::ok(std::move(relationships));
}

Result<void, PersistenceError> RelationshipRepository::remove(const RelationshipId& id) {
    sqlite3* db = database_->handle();
    auto stmtResult = detail::Statement::prepare(db, "DELETE FROM relationships WHERE id = ?;");
    if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
    auto statement = std::move(stmtResult).value();
    statement.bindText(1, id.toString());

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    return Result<void, PersistenceError>::ok();
}

}  // namespace atlas::persistence
