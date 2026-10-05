#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/topics_model.hpp"
#include "doctest.h"
#include "raw_sql.hpp"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase(const std::string& path) {
    auto result = Database::open(path);
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

struct Fixture {
    QTemporaryDir dir;
    std::string path = dir.filePath("atlas.db").toStdString();
    Database db = openTestDatabase(path);
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    TimePoint now = std::chrono::system_clock::now();
    MemoryController memory{db, workspace, [this] { return now; }};
    TopicsModel topics{workspace, memory};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(memory.load().hasValue());
        QObject::connect(&topics, &TopicsModel::errorOccurred, [this] { ++errors; });
    }

    QVariant at(int row, TopicsModel::Role role) const { return topics.data(topics.index(row), role); }

    KnowledgeObjectId addConcept(const char* title, const QString& topic, bool withProject) {
        auto id = workspace.createKnowledgeObject(title, *parseId<TopicId>(topic)).value();
        if (withProject) {
            KnowledgeObjectEdits edits;
            edits.miniProjects = std::vector<MiniProject>{{"Build it", ""}};
            REQUIRE(workspace.updateKnowledgeObject(id, edits).hasValue());
        }
        return id;
    }

    void learn(const KnowledgeObjectId& id) {
        ReviewEvent event;
        event.id = Uuid::generate();
        event.item = ItemRef::forConcept(id);
        event.sessionId = Uuid::generate();
        event.deviceId = "test";
        event.reviewedAt = now - std::chrono::hours(1);
        event.grade = Grade::Good;
        REQUIRE(memory.record({event}).hasValue());
    }

    void needs(const KnowledgeObjectId& dependent, const KnowledgeObjectId& prerequisite) {
        REQUIRE(workspace.createRelationship(dependent, prerequisite, RelationshipType::DependsOn, std::nullopt)
                    .hasValue());
    }
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
    QString databases = f.topics.createTopic("Databases");
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
    QString os = f.topics.createTopic("OS");
    f.workspace.createKnowledgeObject("Paging", *parseId<TopicId>(os));
    CHECK(f.at(0, TopicsModel::ConceptCountRole).toInt() == 1);
}

TEST_CASE("refused operations are reported") {
    Fixture f;
    CHECK_FALSE(f.topics.remove(idString(uncategorizedTopicId())));
    CHECK(f.topics.createTopic("").isEmpty());
    CHECK_FALSE(f.topics.rename("garbage", "Name"));
    QString os = f.topics.createTopic("OS");
    f.workspace.createKnowledgeObject("Paging", *parseId<TopicId>(os));
    CHECK_FALSE(f.topics.remove(os));
    CHECK(f.errors == 4);
}

TEST_CASE("a topic loading failure is reported and keeps the previous rows") {
    Fixture f;
    f.topics.createTopic("OS");
    REQUIRE(f.topics.count() == 2);
    REQUIRE(executeRawSql(f.path, "DROP TABLE topics;"));

    f.topics.refresh();
    CHECK(f.errors == 1);
    CHECK(f.topics.count() == 2);
    CHECK(f.at(0, TopicsModel::NameRole).toString() == "OS");
}

TEST_CASE("entries list every topic in row order for QML pickers") {
    Fixture f;
    QString databases = f.topics.createTopic("Databases");
    auto entries = f.topics.entries();
    REQUIRE(entries.size() == f.topics.count());
    for (int row = 0; row < f.topics.count(); ++row) {
        auto entry = entries[row].toMap();
        CHECK(entry.value("topicId") == f.at(row, TopicsModel::IdRole));
        CHECK(entry.value("name") == f.at(row, TopicsModel::NameRole));
        CHECK(entry.value("uncategorized").toBool() == (entry.value("topicId").toString() != databases));
    }
}

TEST_CASE("project ideas come from concepts with mini projects") {
    Fixture f;
    QString topic = f.topics.createTopic("Databases");
    auto id = f.workspace.createKnowledgeObject("Index", *parseId<TopicId>(topic)).value();
    KnowledgeObjectEdits edits;
    edits.miniProjects = std::vector<MiniProject>{{"Build a B-Tree", "Insert and search"}};
    REQUIRE(f.workspace.updateKnowledgeObject(id, edits).hasValue());

    auto ideas = f.topics.suggestProjects(topic);
    REQUIRE(ideas.size() == 1);
    auto idea = ideas[0].toMap();
    CHECK(idea.value("title").toString() == "Index");
    CHECK(idea.value("projects").toList()[0].toMap().value("title").toString() == "Build a B-Tree");
    CHECK(f.topics.suggestProjects("garbage").isEmpty());
}

TEST_CASE("project idea readiness is the share of solid prerequisites") {
    Fixture f;
    QString topic = f.topics.createTopic("Databases");
    auto index = f.addConcept("Index", topic, true);
    auto tree = f.addConcept("Tree", topic, false);
    auto hash = f.addConcept("Hash", topic, false);
    f.needs(index, tree);
    f.needs(index, hash);
    f.learn(tree);

    auto ideas = f.topics.suggestProjects(topic);
    REQUIRE(ideas.size() == 1);
    CHECK(ideas[0].toMap().value("readiness").toDouble() == doctest::Approx(0.5));
    CHECK(ideas[0].toMap().value("leverage").toInt() == 0);
}

TEST_CASE("project ideas skip solid concepts and rank by readiness and leverage") {
    Fixture f;
    QString topic = f.topics.createTopic("Databases");
    auto known = f.addConcept("Known", topic, true);
    auto base = f.addConcept("Base", topic, true);
    auto blocked = f.addConcept("Blocked", topic, true);
    auto missing = f.addConcept("Missing", topic, false);
    f.needs(blocked, missing);
    f.needs(blocked, base);
    f.learn(known);

    auto ideas = f.topics.suggestProjects(topic);
    REQUIRE(ideas.size() == 2);
    CHECK(ideas[0].toMap().value("id").toString() == idString(base));
    CHECK(ideas[0].toMap().value("readiness").toDouble() == doctest::Approx(1.0));
    CHECK(ideas[0].toMap().value("leverage").toInt() == 1);
    CHECK(ideas[1].toMap().value("id").toString() == idString(blocked));
    CHECK(ideas[1].toMap().value("readiness").toDouble() == doctest::Approx(0.0));
    for (const auto& idea : ideas) CHECK(idea.toMap().value("id").toString() != idString(known));
}
