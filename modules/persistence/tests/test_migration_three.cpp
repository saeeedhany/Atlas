#include <sqlite3.h>

#include <filesystem>
#include <string>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

void removeDatabaseFiles(const std::string& path) {
    std::error_code ignored;
    for (const char* suffix : {"", "-wal", "-shm"}) std::filesystem::remove(path + suffix, ignored);
}

int tableCount(const std::string& path, const char* table) {
    sqlite3* raw = nullptr;
    REQUIRE(sqlite3_open(path.c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM sqlite_master WHERE name = ?;", -1, &statement, nullptr);
    sqlite3_bind_text(statement, 1, table, -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    sqlite3_close(raw);
    return count;
}

}  // namespace

TEST_CASE("migration 3 upgrades a version 2 database and keeps its data") {
    auto path = (std::filesystem::temp_directory_path() / ("atlas_v2_" + Uuid::generate().toString() + ".db")).string();
    {
        auto db = Database::open(path).value();
        KnowledgeObjectRepository objects(db);
        REQUIRE(objects.save(KnowledgeObject::create("Paging").value()).hasValue());
    }
    {
        sqlite3* raw = nullptr;
        REQUIRE(sqlite3_open(path.c_str(), &raw) == SQLITE_OK);
        REQUIRE(sqlite3_exec(raw,
                             "DROP TRIGGER trg_knowledge_objects_forget_learning;"
                             "DROP TRIGGER trg_relationships_forget_learning;"
                             "DROP TABLE review_events; DROP TABLE memory_states;"
                             "DROP TABLE memory_meta; DROP TABLE node_placements;"
                             "DELETE FROM schema_migrations WHERE version = 3;",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(raw);
    }
    REQUIRE(tableCount(path, "review_events") == 0);

    {
        auto reopened = Database::open(path);
        REQUIRE(reopened.hasValue());
        auto db = std::move(reopened).value();
        KnowledgeObjectRepository objects(db);
        CHECK(objects.findAll().value().size() == 1);
    }
    for (const char* table : {"review_events", "memory_states", "memory_meta", "node_placements",
                              "trg_knowledge_objects_forget_learning", "trg_relationships_forget_learning"}) {
        CHECK(tableCount(path, table) == 1);
    }
    removeDatabaseFiles(path);
}
