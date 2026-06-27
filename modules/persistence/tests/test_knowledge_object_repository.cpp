#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::core;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

}  // namespace

TEST_CASE("save then findById round-trips a minimal object") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);

    auto created = KnowledgeObject::create("Hash Tables", "A key-value lookup structure");
    REQUIRE(created.hasValue());
    auto object = std::move(created).value();
    auto id = object.id();

    REQUIRE(repo.save(object).hasValue());

    auto found = repo.findById(id);
    REQUIRE(found.hasValue());
    REQUIRE(found.value().has_value());
    CHECK(found.value()->title() == "Hash Tables");
    CHECK(found.value()->definition() == "A key-value lookup structure");
    CHECK(found.value()->id() == id);
}

TEST_CASE("findById returns an empty optional, not an error, for a missing id") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);

    auto result = repo.findById(KnowledgeObjectId::generate());
    REQUIRE(result.hasValue());
    CHECK(!result.value().has_value());
}

TEST_CASE("examples, mini projects, and references round-trip in their original order") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);

    auto created = KnowledgeObject::create("Binary Search");
    REQUIRE(created.hasValue());
    auto object = std::move(created).value();

    object.addExample({"First example", std::nullopt});
    object.addExample({"Second example", std::string{"code here"}});
    object.addMiniProject({"Project A", "desc A"});
    object.addMiniProject({"Project B", "desc B"});
    object.addReference({"Ref A", std::nullopt});
    object.addReference({"Ref B", std::string{"https://example.com"}});

    REQUIRE(repo.save(object).hasValue());

    auto found = repo.findById(object.id());
    REQUIRE(found.hasValue());
    REQUIRE(found.value().has_value());
    const auto& loaded = *found.value();

    REQUIRE(loaded.examples().size() == 2);
    CHECK(loaded.examples()[0].description == "First example");
    CHECK(loaded.examples()[1].description == "Second example");
    REQUIRE(loaded.examples()[1].snippet.has_value());
    CHECK(*loaded.examples()[1].snippet == "code here");

    REQUIRE(loaded.miniProjects().size() == 2);
    CHECK(loaded.miniProjects()[0].title == "Project A");
    CHECK(loaded.miniProjects()[1].title == "Project B");

    REQUIRE(loaded.references().size() == 2);
    CHECK(loaded.references()[0].title == "Ref A");
    CHECK(loaded.references()[1].title == "Ref B");
}

TEST_CASE("findAll returns every saved object") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);

    for (const char* title : {"Recursion", "Iteration", "Memoization"}) {
        auto created = KnowledgeObject::create(title);
        REQUIRE(created.hasValue());
        REQUIRE(repo.save(created.value()).hasValue());
    }

    auto all = repo.findAll();
    REQUIRE(all.hasValue());
    CHECK(all.value().size() == 3);
}

TEST_CASE("saving the same object twice updates the row in place rather than duplicating it") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);

    auto created = KnowledgeObject::create("Tail Call");
    REQUIRE(created.hasValue());
    auto object = std::move(created).value();
    REQUIRE(repo.save(object).hasValue());

    REQUIRE(object.renameTo("Tail Call Optimization").hasValue());
    object.addExample({"An example added after the first save", std::nullopt});
    REQUIRE(repo.save(object).hasValue());

    auto all = repo.findAll();
    REQUIRE(all.hasValue());
    REQUIRE(all.value().size() == 1);  // updated in place, not duplicated
    CHECK(all.value().front().title() == "Tail Call Optimization");
    CHECK(all.value().front().examples().size() == 1);
}

TEST_CASE("remove deletes the object") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);

    auto created = KnowledgeObject::create("Temporary Concept");
    REQUIRE(created.hasValue());
    auto object = std::move(created).value();
    REQUIRE(repo.save(object).hasValue());

    REQUIRE(repo.remove(object.id()).hasValue());

    auto found = repo.findById(object.id());
    REQUIRE(found.hasValue());
    CHECK(!found.value().has_value());
}

TEST_CASE("remove on a nonexistent id is not an error") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository repo(db);
    CHECK(repo.remove(KnowledgeObjectId::generate()).hasValue());
}
