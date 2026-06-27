#include <filesystem>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using atlas::core::KnowledgeObject;

TEST_CASE("Database::open on :memory: succeeds and the schema is ready immediately") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    // The only externally-observable proof the schema exists: a query
    // against it succeeds instead of failing with "no such table."
    KnowledgeObjectRepository repo(db);
    auto allResult = repo.findAll();
    REQUIRE(allResult.hasValue());
    CHECK(allResult.value().empty());
}

TEST_CASE("Database::open fails gracefully when the parent directory doesn't exist") {
    auto dbResult = Database::open("/this/path/does/not/exist/atlas.db");
    CHECK(!dbResult.hasValue());
    CHECK(dbResult.error().code == PersistenceErrorCode::ConnectionFailed);
}

TEST_CASE("Reopening the same file twice is idempotent and preserves data") {
    auto path = (std::filesystem::temp_directory_path() / "atlas_m1_idempotent_test.db").string();
    std::filesystem::remove(path);

    {
        auto dbResult = Database::open(path);
        REQUIRE(dbResult.hasValue());
        auto db = std::move(dbResult).value();
        KnowledgeObjectRepository repo(db);

        auto objectResult = KnowledgeObject::create("Persisted Across Reopen");
        REQUIRE(objectResult.hasValue());
        auto saveResult = repo.save(objectResult.value());
        REQUIRE(saveResult.hasValue());
    }  // Database destructor closes the connection here.

    {
        auto dbResult = Database::open(path);
        REQUIRE(dbResult.hasValue());  // migrations must not fail or re-run destructively
        auto db = std::move(dbResult).value();
        KnowledgeObjectRepository repo(db);

        auto allResult = repo.findAll();
        REQUIRE(allResult.hasValue());
        REQUIRE(allResult.value().size() == 1);
        CHECK(allResult.value().front().title() == "Persisted Across Reopen");
    }

    std::filesystem::remove(path);
}
