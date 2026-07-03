#include "atlas/persistence/topic_repository.hpp"

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

TEST_CASE("Migration 2 seeds a fixed Uncategorized topic on a fresh database") {
    auto db = openTestDatabase();
    TopicRepository repo(db);

    auto found = repo.findById(uncategorizedTopicId());
    REQUIRE(found.hasValue());
    REQUIRE(found.value().has_value());
    CHECK(found.value()->name() == "Uncategorized");
}

TEST_CASE("save then findById round-trips a Topic") {
    auto db = openTestDatabase();
    TopicRepository repo(db);

    auto created = Topic::create("Operating Systems", "Kernels, schedulers, memory management");
    REQUIRE(created.hasValue());
    auto topic = std::move(created).value();
    auto id = topic.id();

    REQUIRE(repo.save(topic).hasValue());

    auto found = repo.findById(id);
    REQUIRE(found.hasValue());
    REQUIRE(found.value().has_value());
    CHECK(found.value()->name() == "Operating Systems");
    CHECK(found.value()->description() == "Kernels, schedulers, memory management");
}

TEST_CASE("findById returns an empty optional, not an error, for a missing id") {
    auto db = openTestDatabase();
    TopicRepository repo(db);

    auto found = repo.findById(TopicId::generate());
    REQUIRE(found.hasValue());
    CHECK(!found.value().has_value());
}

TEST_CASE("findAll includes the seeded Uncategorized topic plus any created ones") {
    auto db = openTestDatabase();
    TopicRepository repo(db);

    auto created = Topic::create("Databases");
    REQUIRE(created.hasValue());
    REQUIRE(repo.save(created.value()).hasValue());

    auto all = repo.findAll();
    REQUIRE(all.hasValue());
    CHECK(all.value().size() == 2);  // Uncategorized + Databases
}

TEST_CASE("save on an existing id updates in place rather than duplicating") {
    auto db = openTestDatabase();
    TopicRepository repo(db);

    auto created = Topic::create("Compilers");
    REQUIRE(created.hasValue());
    auto topic = std::move(created).value();
    REQUIRE(repo.save(topic).hasValue());

    REQUIRE(topic.renameTo("Compilers and Interpreters").hasValue());
    REQUIRE(repo.save(topic).hasValue());

    auto found = repo.findById(topic.id());
    REQUIRE(found.hasValue());
    REQUIRE(found.value().has_value());
    CHECK(found.value()->name() == "Compilers and Interpreters");

    auto all = repo.findAll();
    REQUIRE(all.hasValue());
    CHECK(all.value().size() == 2);  // Uncategorized + this one topic, not two
}

TEST_CASE("remove deletes an empty topic") {
    auto db = openTestDatabase();
    TopicRepository repo(db);

    auto created = Topic::create("Temporary Topic");
    REQUIRE(created.hasValue());
    auto topic = std::move(created).value();
    REQUIRE(repo.save(topic).hasValue());

    REQUIRE(repo.remove(topic.id()).hasValue());

    auto found = repo.findById(topic.id());
    REQUIRE(found.hasValue());
    CHECK(!found.value().has_value());
}

TEST_CASE("remove on a nonexistent id is not an error") {
    auto db = openTestDatabase();
    TopicRepository repo(db);
    CHECK(repo.remove(TopicId::generate()).hasValue());
}

TEST_CASE("remove fails at the database level when a KnowledgeObject still references the topic") {
    // No ON DELETE clause on knowledge_objects.topic_id (see migration
    // 2) means SQLite's default foreign-key behavior blocks this
    // delete rather than orphaning or cascading — see
    // TopicRepository::remove()'s doc comment for why that's
    // deliberate. The application layer (WorkspaceController) is
    // expected to check membership and prompt first; this test proves
    // the database backstops that even if it forgets to.
    auto db = openTestDatabase();
    TopicRepository topics(db);
    KnowledgeObjectRepository objects(db);

    auto createdTopic = Topic::create("Occupied Topic");
    REQUIRE(createdTopic.hasValue());
    auto topic = std::move(createdTopic).value();
    REQUIRE(topics.save(topic).hasValue());

    auto createdObject = KnowledgeObject::create("Lives In Occupied Topic");
    REQUIRE(createdObject.hasValue());
    auto object = std::move(createdObject).value();
    object.assignToTopic(topic.id());
    REQUIRE(objects.save(object).hasValue());

    auto removeResult = topics.remove(topic.id());
    CHECK(!removeResult.hasValue());
}
