#include "atlas/persistence/knowledge_object_repository.hpp"

#include <sqlite3.h>

#include <chrono>
#include <utility>

#include "detail/statement.hpp"

namespace atlas::persistence {

using atlas::core::ConfidenceLevel;
using atlas::core::confidenceLevelFromString;
using atlas::core::Difficulty;
using atlas::core::difficultyFromString;
using atlas::core::Example;
using atlas::core::MiniProject;
using atlas::core::Reference;
using atlas::core::toDisplayString;

namespace {

int64_t toMillis(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromMillis(int64_t millis) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(millis));
}

Result<void, PersistenceError> upsertMainRow(sqlite3* db, const KnowledgeObject& object) {
    auto statementResult = detail::Statement::prepare(db, R"sql(
        INSERT INTO knowledge_objects
            (id, title, definition, problem_solved, why_it_exists, notes,
             difficulty, confidence, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            title = excluded.title,
            definition = excluded.definition,
            problem_solved = excluded.problem_solved,
            why_it_exists = excluded.why_it_exists,
            notes = excluded.notes,
            difficulty = excluded.difficulty,
            confidence = excluded.confidence,
            updated_at = excluded.updated_at;
    )sql");
    if (!statementResult.hasValue()) {
        return Result<void, PersistenceError>::err(std::move(statementResult).error());
    }
    auto statement = std::move(statementResult).value();

    statement.bindText(1, object.id().toString());
    statement.bindText(2, object.title());
    statement.bindText(3, object.definition());
    statement.bindText(4, object.problemSolved());
    statement.bindText(5, object.whyItExists());
    statement.bindText(6, object.notes());
    statement.bindText(7, toDisplayString(object.difficulty()));
    statement.bindText(8, toDisplayString(object.confidence()));
    statement.bindInt64(9, toMillis(object.createdAt()));
    statement.bindInt64(10, toMillis(object.updatedAt()));

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) {
        return Result<void, PersistenceError>::err(stepResult.error());
    }
    return Result<void, PersistenceError>::ok();
}

// Child collections (examples, mini-projects, references) are small —
// a handful of items per object, nothing like the 10k+-node scale the
// graph itself needs to handle — so "delete everything for this
// parent, then re-insert the current list" is simple, always correct,
// and fast enough. No diffing logic to get wrong.
Result<void, PersistenceError> replaceChildRows(sqlite3* db, const KnowledgeObject& object) {
    const std::string id = object.id().toString();

    for (const char* table : {"examples", "mini_projects", "reference_entries"}) {
        auto stmtResult = detail::Statement::prepare(
            db, std::string("DELETE FROM ") + table + " WHERE knowledge_object_id = ?;");
        if (!stmtResult.hasValue()) {
            return Result<void, PersistenceError>::err(std::move(stmtResult).error());
        }
        auto stmt = std::move(stmtResult).value();
        stmt.bindText(1, id);
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    }

    int position = 0;
    for (const Example& example : object.examples()) {
        auto stmtResult = detail::Statement::prepare(db, R"sql(
            INSERT INTO examples (knowledge_object_id, position, description, snippet)
            VALUES (?, ?, ?, ?);
        )sql");
        if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
        auto stmt = std::move(stmtResult).value();
        stmt.bindText(1, id);
        stmt.bindInt64(2, position++);
        stmt.bindText(3, example.description);
        stmt.bindOptionalText(4, example.snippet);
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    }

    position = 0;
    for (const MiniProject& project : object.miniProjects()) {
        auto stmtResult = detail::Statement::prepare(db, R"sql(
            INSERT INTO mini_projects (knowledge_object_id, position, title, description)
            VALUES (?, ?, ?, ?);
        )sql");
        if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
        auto stmt = std::move(stmtResult).value();
        stmt.bindText(1, id);
        stmt.bindInt64(2, position++);
        stmt.bindText(3, project.title);
        stmt.bindText(4, project.description);
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    }

    position = 0;
    for (const Reference& reference : object.references()) {
        auto stmtResult = detail::Statement::prepare(db, R"sql(
            INSERT INTO reference_entries (knowledge_object_id, position, title, url)
            VALUES (?, ?, ?, ?);
        )sql");
        if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
        auto stmt = std::move(stmtResult).value();
        stmt.bindText(1, id);
        stmt.bindInt64(2, position++);
        stmt.bindText(3, reference.title);
        stmt.bindOptionalText(4, reference.url);
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    }

    return Result<void, PersistenceError>::ok();
}

Result<std::vector<Example>, PersistenceError> loadExamples(sqlite3* db, const std::string& id) {
    auto stmtResult = detail::Statement::prepare(
        db, "SELECT description, snippet FROM examples WHERE knowledge_object_id = ? ORDER BY position;");
    if (!stmtResult.hasValue()) return Result<std::vector<Example>, PersistenceError>::err(std::move(stmtResult).error());
    auto stmt = std::move(stmtResult).value();
    stmt.bindText(1, id);

    std::vector<Example> examples;
    while (true) {
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<std::vector<Example>, PersistenceError>::err(stepResult.error());
        if (!stepResult.value()) break;
        examples.push_back(Example{stmt.columnText(0), stmt.columnOptionalText(1)});
    }
    return Result<std::vector<Example>, PersistenceError>::ok(std::move(examples));
}

Result<std::vector<MiniProject>, PersistenceError> loadMiniProjects(sqlite3* db,
                                                                      const std::string& id) {
    auto stmtResult = detail::Statement::prepare(
        db, "SELECT title, description FROM mini_projects WHERE knowledge_object_id = ? ORDER BY position;");
    if (!stmtResult.hasValue()) return Result<std::vector<MiniProject>, PersistenceError>::err(std::move(stmtResult).error());
    auto stmt = std::move(stmtResult).value();
    stmt.bindText(1, id);

    std::vector<MiniProject> projects;
    while (true) {
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<std::vector<MiniProject>, PersistenceError>::err(stepResult.error());
        if (!stepResult.value()) break;
        projects.push_back(MiniProject{stmt.columnText(0), stmt.columnText(1)});
    }
    return Result<std::vector<MiniProject>, PersistenceError>::ok(std::move(projects));
}

Result<std::vector<Reference>, PersistenceError> loadReferences(sqlite3* db,
                                                                   const std::string& id) {
    auto stmtResult = detail::Statement::prepare(
        db, "SELECT title, url FROM reference_entries WHERE knowledge_object_id = ? ORDER BY position;");
    if (!stmtResult.hasValue()) return Result<std::vector<Reference>, PersistenceError>::err(std::move(stmtResult).error());
    auto stmt = std::move(stmtResult).value();
    stmt.bindText(1, id);

    std::vector<Reference> references;
    while (true) {
        auto stepResult = stmt.step();
        if (!stepResult.hasValue()) return Result<std::vector<Reference>, PersistenceError>::err(stepResult.error());
        if (!stepResult.value()) break;
        references.push_back(Reference{stmt.columnText(0), stmt.columnOptionalText(1)});
    }
    return Result<std::vector<Reference>, PersistenceError>::ok(std::move(references));
}

// Builds the full KnowledgeObject for the row the statement currently
// points at (i.e. call this right after a successful step() that
// returned true), including its child collections.
Result<KnowledgeObject, PersistenceError> rowToObject(sqlite3* db, detail::Statement& statement) {
    std::string id = statement.columnText(0);

    auto difficulty = difficultyFromString(statement.columnText(6));
    auto confidence = confidenceLevelFromString(statement.columnText(7));
    if (!difficulty.has_value() || !confidence.has_value()) {
        return Result<KnowledgeObject, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation,
            "knowledge_objects row " + id + " has an unrecognized difficulty/confidence value"});
    }

    auto examplesResult = loadExamples(db, id);
    if (!examplesResult.hasValue()) return Result<KnowledgeObject, PersistenceError>::err(examplesResult.error());
    auto projectsResult = loadMiniProjects(db, id);
    if (!projectsResult.hasValue()) return Result<KnowledgeObject, PersistenceError>::err(projectsResult.error());
    auto referencesResult = loadReferences(db, id);
    if (!referencesResult.hasValue()) return Result<KnowledgeObject, PersistenceError>::err(referencesResult.error());

    auto parsedId = atlas::core::Uuid::parse(id);
    if (!parsedId.has_value()) {
        return Result<KnowledgeObject, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation, "knowledge_objects row has a malformed id: " + id});
    }

    KnowledgeObject::StorageRecord record{
        KnowledgeObjectId(*parsedId),
        statement.columnText(1),
        statement.columnText(2),
        statement.columnText(3),
        statement.columnText(4),
        std::move(examplesResult).value(),
        std::move(projectsResult).value(),
        std::move(referencesResult).value(),
        statement.columnText(5),
        *difficulty,
        *confidence,
        fromMillis(statement.columnInt64(8)),
        fromMillis(statement.columnInt64(9))};

    auto reconstructed = KnowledgeObject::reconstruct(std::move(record));
    if (!reconstructed.hasValue()) {
        return Result<KnowledgeObject, PersistenceError>::err(PersistenceError{
            PersistenceErrorCode::ConstraintViolation,
            "knowledge_objects row " + id + " failed domain validation on load"});
    }
    return Result<KnowledgeObject, PersistenceError>::ok(std::move(reconstructed).value());
}

constexpr const char* kSelectColumns =
    "id, title, definition, problem_solved, why_it_exists, notes, difficulty, confidence, "
    "created_at, updated_at";

}  // namespace

KnowledgeObjectRepository::KnowledgeObjectRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> KnowledgeObjectRepository::save(const KnowledgeObject& object) {
    sqlite3* db = database_->handle();

    auto begin = detail::execute(db, "BEGIN;");
    if (!begin.hasValue()) return begin;

    auto upsertResult = upsertMainRow(db, object);
    if (!upsertResult.hasValue()) {
        detail::execute(db, "ROLLBACK;");
        return upsertResult;
    }

    auto childResult = replaceChildRows(db, object);
    if (!childResult.hasValue()) {
        detail::execute(db, "ROLLBACK;");
        return childResult;
    }

    return detail::execute(db, "COMMIT;");
}

Result<std::optional<KnowledgeObject>, PersistenceError> KnowledgeObjectRepository::findById(
    const KnowledgeObjectId& id) {
    sqlite3* db = database_->handle();

    std::string sql = std::string("SELECT ") + kSelectColumns +
                       " FROM knowledge_objects WHERE id = ?;";
    auto stmtResult = detail::Statement::prepare(db, sql);
    if (!stmtResult.hasValue()) {
        return Result<std::optional<KnowledgeObject>, PersistenceError>::err(
            std::move(stmtResult).error());
    }
    auto statement = std::move(stmtResult).value();
    statement.bindText(1, id.toString());

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) {
        return Result<std::optional<KnowledgeObject>, PersistenceError>::err(stepResult.error());
    }
    if (!stepResult.value()) {
        return Result<std::optional<KnowledgeObject>, PersistenceError>::ok(std::nullopt);
    }

    auto objectResult = rowToObject(db, statement);
    if (!objectResult.hasValue()) {
        return Result<std::optional<KnowledgeObject>, PersistenceError>::err(
            std::move(objectResult).error());
    }
    return Result<std::optional<KnowledgeObject>, PersistenceError>::ok(
        std::move(objectResult).value());
}

Result<std::vector<KnowledgeObject>, PersistenceError> KnowledgeObjectRepository::findAll() {
    sqlite3* db = database_->handle();

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM knowledge_objects;";
    auto stmtResult = detail::Statement::prepare(db, sql);
    if (!stmtResult.hasValue()) {
        return Result<std::vector<KnowledgeObject>, PersistenceError>::err(
            std::move(stmtResult).error());
    }
    auto statement = std::move(stmtResult).value();

    std::vector<KnowledgeObject> objects;
    while (true) {
        auto stepResult = statement.step();
        if (!stepResult.hasValue()) {
            return Result<std::vector<KnowledgeObject>, PersistenceError>::err(stepResult.error());
        }
        if (!stepResult.value()) break;

        auto objectResult = rowToObject(db, statement);
        if (!objectResult.hasValue()) {
            return Result<std::vector<KnowledgeObject>, PersistenceError>::err(
                std::move(objectResult).error());
        }
        objects.push_back(std::move(objectResult).value());
    }
    return Result<std::vector<KnowledgeObject>, PersistenceError>::ok(std::move(objects));
}

Result<void, PersistenceError> KnowledgeObjectRepository::remove(const KnowledgeObjectId& id) {
    sqlite3* db = database_->handle();
    auto stmtResult = detail::Statement::prepare(db, "DELETE FROM knowledge_objects WHERE id = ?;");
    if (!stmtResult.hasValue()) return Result<void, PersistenceError>::err(std::move(stmtResult).error());
    auto statement = std::move(stmtResult).value();
    statement.bindText(1, id.toString());

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) return Result<void, PersistenceError>::err(stepResult.error());
    return Result<void, PersistenceError>::ok();
}

}  // namespace atlas::persistence
