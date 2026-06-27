#include "atlas/core/knowledge_object.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("KnowledgeObject::create rejects an empty title") {
    auto result = KnowledgeObject::create("");
    CHECK(!result.hasValue());
    CHECK(result.error() == ValidationError::EmptyTitle);
}

TEST_CASE("KnowledgeObject::create succeeds with a minimal title-only object") {
    auto result = KnowledgeObject::create("Recursion");
    REQUIRE(result.hasValue());
    const auto& object = result.value();
    CHECK(object.title() == "Recursion");
    CHECK(object.definition().empty());
    CHECK(object.difficulty() == Difficulty::Beginner);
    CHECK(object.confidence() == ConfidenceLevel::Unknown);
}

TEST_CASE("KnowledgeObject::renameTo rejects an empty title and leaves the object unchanged") {
    auto result = KnowledgeObject::create("Recursion");
    REQUIRE(result.hasValue());
    auto object = std::move(result).value();

    auto renameResult = object.renameTo("");
    CHECK(!renameResult.hasValue());
    CHECK(object.title() == "Recursion");
}

TEST_CASE("KnowledgeObject::renameTo succeeds and advances updatedAt") {
    auto result = KnowledgeObject::create("Recursion");
    REQUIRE(result.hasValue());
    auto object = std::move(result).value();
    auto before = object.updatedAt();

    auto renameResult = object.renameTo("Tail Recursion");
    CHECK(renameResult.hasValue());
    CHECK(object.title() == "Tail Recursion");
    CHECK(object.updatedAt() >= before);
}

TEST_CASE("KnowledgeObject accumulates examples, mini projects, and references") {
    auto result = KnowledgeObject::create("Binary Search");
    REQUIRE(result.hasValue());
    auto object = std::move(result).value();

    object.addExample({"Find x in a sorted array", std::string{"while (lo <= hi) { ... }"}});
    object.addMiniProject(
        {"Implement iterative binary search", "From scratch, no library calls."});
    object.addReference({"CLRS, Chapter 2", std::nullopt});

    CHECK(object.examples().size() == 1);
    CHECK(object.miniProjects().size() == 1);
    CHECK(object.references().size() == 1);
}

TEST_CASE("Two KnowledgeObjects created with the same title have different ids") {
    auto a = KnowledgeObject::create("Recursion");
    auto b = KnowledgeObject::create("Recursion");
    REQUIRE(a.hasValue());
    REQUIRE(b.hasValue());
    CHECK(!(a.value().id() == b.value().id()));
}

TEST_CASE("KnowledgeObject::reconstruct rejects a record with an empty title") {
    KnowledgeObject::StorageRecord record{
        KnowledgeObjectId::generate(), "", "def", "", "", {}, {}, {}, "",
        Difficulty::Beginner,          ConfidenceLevel::Unknown,
        std::chrono::system_clock::now(), std::chrono::system_clock::now()};
    auto result = KnowledgeObject::reconstruct(std::move(record));
    CHECK(!result.hasValue());
    CHECK(result.error() == ValidationError::EmptyTitle);
}

TEST_CASE("KnowledgeObject::reconstruct preserves the original id and timestamps") {
    auto id = KnowledgeObjectId::generate();
    auto createdAt = std::chrono::system_clock::now() - std::chrono::hours(24);
    auto updatedAt = std::chrono::system_clock::now() - std::chrono::hours(1);

    KnowledgeObject::StorageRecord record{
        id, "Dynamic Programming", "Optimization via subproblem reuse", "", "",
        {Example{"Fibonacci with memoization", std::nullopt}}, {}, {}, "some notes",
        Difficulty::Intermediate, ConfidenceLevel::Familiar, createdAt, updatedAt};

    auto result = KnowledgeObject::reconstruct(std::move(record));
    REQUIRE(result.hasValue());
    const auto& object = result.value();

    // The point of reconstruct() vs create(): identity and history are
    // preserved exactly, not regenerated.
    CHECK(object.id() == id);
    CHECK(object.createdAt() == createdAt);
    CHECK(object.updatedAt() == updatedAt);
    CHECK(object.examples().size() == 1);
    CHECK(object.notes() == "some notes");
}
