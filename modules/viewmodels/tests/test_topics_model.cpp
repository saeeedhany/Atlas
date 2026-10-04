#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/topics_model.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

struct Fixture {
    Database db = openTestDatabase();
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    TopicsModel topics{workspace};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        QObject::connect(&topics, &TopicsModel::errorOccurred, [this] { ++errors; });
    }

    QVariant at(int row, TopicsModel::Role role) const { return topics.data(topics.index(row), role); }
};

}  // namespace

TEST_CASE("the seeded Uncategorized topic is listed and marked") {
    Fixture f;
    REQUIRE(f.topics.count() == 1);
    CHECK(f.at(0, TopicsModel::NameRole).toString() == "Uncategorized");
    CHECK(f.at(0, TopicsModel::IsUncategorizedRole).toBool());
    CHECK(f.topics.roleNames().value(TopicsModel::ConceptCountRole) == "conceptCount");
}

TEST_CASE("create, rename, and remove keep the list sorted and in sync") {
    Fixture f;
    QString databases = f.topics.create("Databases");
    REQUIRE_FALSE(databases.isEmpty());
    CHECK(f.topics.count() == 2);
    CHECK(f.at(0, TopicsModel::NameRole).toString() == "Databases");

    CHECK(f.topics.rename(databases, "Storage"));
    CHECK(f.topics.nameOf(databases) == "Storage");

    CHECK(f.topics.remove(databases));
    CHECK(f.topics.count() == 1);
    CHECK(f.errors == 0);
}

TEST_CASE("concept counts follow the graph") {
    Fixture f;
    QString os = f.topics.create("OS");
    f.workspace.createKnowledgeObject("Paging", *parseId<TopicId>(os));
    CHECK(f.at(0, TopicsModel::ConceptCountRole).toInt() == 1);
}

TEST_CASE("refused operations are reported") {
    Fixture f;
    CHECK_FALSE(f.topics.remove(idString(uncategorizedTopicId())));
    CHECK(f.topics.create("").isEmpty());
    CHECK_FALSE(f.topics.rename("garbage", "Name"));
    QString os = f.topics.create("OS");
    f.workspace.createKnowledgeObject("Paging", *parseId<TopicId>(os));
    CHECK_FALSE(f.topics.remove(os));
    CHECK(f.errors == 4);
}
