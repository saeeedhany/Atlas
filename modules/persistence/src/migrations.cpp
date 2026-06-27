#include "migrations.hpp"

#include <sqlite3.h>

#include <array>
#include <chrono>

#include "detail/statement.hpp"

namespace atlas::persistence::detail {

namespace {

struct Migration {
    int version;
    const char* description;
    const char* sql;
};

// Each entry here is permanent history once shipped: never edit a
// migration that's already been released, only append new ones. The
// schema_migrations table is what lets us tell, for any given .db file
// on a person's disk, exactly which of these has already been applied.
constexpr std::array<Migration, 1> kMigrations{{
    {1, "Initial schema: knowledge objects, relationships, and child content tables", R"sql(
        CREATE TABLE knowledge_objects (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            definition TEXT NOT NULL DEFAULT '',
            problem_solved TEXT NOT NULL DEFAULT '',
            why_it_exists TEXT NOT NULL DEFAULT '',
            notes TEXT NOT NULL DEFAULT '',
            difficulty TEXT NOT NULL,
            confidence TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );

        CREATE TABLE examples (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            knowledge_object_id TEXT NOT NULL REFERENCES knowledge_objects(id) ON DELETE CASCADE,
            position INTEGER NOT NULL,
            description TEXT NOT NULL,
            snippet TEXT
        );
        CREATE INDEX idx_examples_knowledge_object_id ON examples(knowledge_object_id);

        CREATE TABLE mini_projects (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            knowledge_object_id TEXT NOT NULL REFERENCES knowledge_objects(id) ON DELETE CASCADE,
            position INTEGER NOT NULL,
            title TEXT NOT NULL,
            description TEXT NOT NULL
        );
        CREATE INDEX idx_mini_projects_knowledge_object_id ON mini_projects(knowledge_object_id);

        CREATE TABLE reference_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            knowledge_object_id TEXT NOT NULL REFERENCES knowledge_objects(id) ON DELETE CASCADE,
            position INTEGER NOT NULL,
            title TEXT NOT NULL,
            url TEXT
        );
        CREATE INDEX idx_reference_entries_knowledge_object_id ON reference_entries(knowledge_object_id);

        CREATE TABLE relationships (
            id TEXT PRIMARY KEY,
            source_id TEXT NOT NULL REFERENCES knowledge_objects(id) ON DELETE CASCADE,
            target_id TEXT NOT NULL REFERENCES knowledge_objects(id) ON DELETE CASCADE,
            type TEXT NOT NULL,
            note TEXT,
            created_at INTEGER NOT NULL,
            UNIQUE(source_id, target_id, type)
        );
        CREATE INDEX idx_relationships_source_id ON relationships(source_id);
        CREATE INDEX idx_relationships_target_id ON relationships(target_id);
    )sql"},
}};

Result<int, PersistenceError> currentSchemaVersion(sqlite3* db) {
    auto ensureTable = execute(db, R"sql(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at INTEGER NOT NULL
        );
    )sql");
    if (!ensureTable.hasValue()) return Result<int, PersistenceError>::err(ensureTable.error());

    auto statementResult =
        Statement::prepare(db, "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;");
    if (!statementResult.hasValue()) {
        return Result<int, PersistenceError>::err(std::move(statementResult).error());
    }
    auto statement = std::move(statementResult).value();

    auto stepResult = statement.step();
    if (!stepResult.hasValue()) return Result<int, PersistenceError>::err(stepResult.error());

    return Result<int, PersistenceError>::ok(static_cast<int>(statement.columnInt64(0)));
}

Result<void, PersistenceError> applyMigration(sqlite3* db, const Migration& migration) {
    auto begin = execute(db, "BEGIN;");
    if (!begin.hasValue()) return begin;

    auto applySql = execute(db, migration.sql);
    if (!applySql.hasValue()) {
        execute(db, "ROLLBACK;");
        return applySql;
    }

    auto statementResult =
        Statement::prepare(db, "INSERT INTO schema_migrations (version, applied_at) VALUES (?, ?);");
    if (!statementResult.hasValue()) {
        execute(db, "ROLLBACK;");
        return Result<void, PersistenceError>::err(std::move(statementResult).error());
    }
    auto statement = std::move(statementResult).value();
    statement.bindInt64(1, migration.version);
    statement.bindInt64(2, std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count());
    auto stepResult = statement.step();
    if (!stepResult.hasValue()) {
        execute(db, "ROLLBACK;");
        return Result<void, PersistenceError>::err(std::move(stepResult).error());
    }

    return execute(db, "COMMIT;");
}

}  // namespace

Result<void, PersistenceError> runMigrations(sqlite3* db) {
    auto versionResult = currentSchemaVersion(db);
    if (!versionResult.hasValue()) {
        return Result<void, PersistenceError>::err(versionResult.error());
    }
    int currentVersion = versionResult.value();

    for (const auto& migration : kMigrations) {
        if (migration.version <= currentVersion) continue;
        auto applyResult = applyMigration(db, migration);
        if (!applyResult.hasValue()) {
            return Result<void, PersistenceError>::err(PersistenceError{
                PersistenceErrorCode::MigrationFailed,
                std::move(applyResult).error().detail});
        }
    }
    return Result<void, PersistenceError>::ok();
}

}  // namespace atlas::persistence::detail
