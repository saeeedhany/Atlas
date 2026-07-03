#include "atlas/core/topic.hpp"

#include "atlas/core/knowledge_object.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("Topic::create rejects an empty name") {
    auto result = Topic::create("");
    CHECK(!result.hasValue());
    CHECK(result.error() == TopicValidationError::EmptyName);
}

TEST_CASE("Topic::create succeeds with a name-only topic") {
    auto result = Topic::create("Operating Systems");
    REQUIRE(result.hasValue());
    const auto& topic = result.value();
    CHECK(topic.name() == "Operating Systems");
    CHECK(topic.description().empty());
}

TEST_CASE("Two Topics created with the same name have different ids") {
    auto a = Topic::create("Databases");
    auto b = Topic::create("Databases");
    REQUIRE(a.hasValue());
    REQUIRE(b.hasValue());
    CHECK(!(a.value().id() == b.value().id()));
}

TEST_CASE("Topic::renameTo rejects an empty name and leaves the topic unchanged") {
    auto result = Topic::create("Databases");
    REQUIRE(result.hasValue());
    auto topic = std::move(result).value();

    auto renameResult = topic.renameTo("");
    CHECK(!renameResult.hasValue());
    CHECK(topic.name() == "Databases");
}

TEST_CASE("Topic::renameTo succeeds and advances updatedAt") {
    auto result = Topic::create("Databases");
    REQUIRE(result.hasValue());
    auto topic = std::move(result).value();
    auto before = topic.updatedAt();

    CHECK(topic.renameTo("Distributed Databases").hasValue());
    CHECK(topic.name() == "Distributed Databases");
    CHECK(topic.updatedAt() >= before);
}

TEST_CASE("Topic::reconstruct rejects a record with an empty name") {
    Topic::StorageRecord record{TopicId::generate(), "", "", std::chrono::system_clock::now(),
                                 std::chrono::system_clock::now()};
    auto result = Topic::reconstruct(std::move(record));
    CHECK(!result.hasValue());
    CHECK(result.error() == TopicValidationError::EmptyName);
}

TEST_CASE("Topic::reconstruct preserves the original id and timestamps") {
    auto id = TopicId::generate();
    auto createdAt = std::chrono::system_clock::now() - std::chrono::hours(24);
    auto updatedAt = std::chrono::system_clock::now() - std::chrono::hours(1);

    Topic::StorageRecord record{id, "Compilers", "How source becomes machine code", createdAt,
                                 updatedAt};
    auto result = Topic::reconstruct(std::move(record));
    REQUIRE(result.hasValue());
    const auto& topic = result.value();

    CHECK(topic.id() == id);
    CHECK(topic.createdAt() == createdAt);
    CHECK(topic.updatedAt() == updatedAt);
}

TEST_CASE("uncategorizedTopicId is a fixed, stable id across calls") {
    CHECK(uncategorizedTopicId() == uncategorizedTopicId());
}

TEST_CASE("A freshly created KnowledgeObject has no topic assigned") {
    auto result = KnowledgeObject::create("Recursion");
    REQUIRE(result.hasValue());
    CHECK(!result.value().topicId().has_value());
}

TEST_CASE("KnowledgeObject::assignToTopic sets the topic and advances updatedAt") {
    auto result = KnowledgeObject::create("Recursion");
    REQUIRE(result.hasValue());
    auto object = std::move(result).value();
    auto before = object.updatedAt();
    auto topicId = TopicId::generate();

    object.assignToTopic(topicId);
    REQUIRE(object.topicId().has_value());
    CHECK(*object.topicId() == topicId);
    CHECK(object.updatedAt() >= before);
}
