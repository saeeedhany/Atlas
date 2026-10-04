#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/concept_links_model.hpp"
#include "atlas/viewmodels/ids.hpp"
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
    MemoryController memory{db, workspace, systemClock()};
    ConceptLinksModel links{workspace, memory};
    KnowledgeObjectId btree = workspace.createKnowledgeObject("B-Tree").value();
    KnowledgeObjectId tree = workspace.createKnowledgeObject("Tree").value();
    TopicId databases = workspace.createTopic("Databases").value();
    KnowledgeObjectId index = workspace.createKnowledgeObject("Index", databases).value();
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(memory.load().hasValue());
        QObject::connect(&links, &ConceptLinksModel::errorOccurred, [this] { ++errors; });
        links.setConceptId(idString(btree));
    }

    QVariant at(int row, ConceptLinksModel::Role role) const { return links.data(links.index(row), role); }
};

}  // namespace

TEST_CASE("type names are readable and in enum order") {
    Fixture f;
    REQUIRE(f.links.typeNames().size() == 10);
    CHECK(f.links.typeNames()[0] == "depends on");
    CHECK(f.links.typeNames()[8] == "opposite of");
}

TEST_CASE("adding outgoing and incoming links lists both, sorted by the other title") {
    Fixture f;
    REQUIRE(f.links.add(idString(f.tree), 0, true, "needs a tree"));
    REQUIRE(f.links.add(idString(f.index), 1, false, ""));
    REQUIRE(f.links.count() == 2);
    CHECK(f.at(0, ConceptLinksModel::OtherTitleRole).toString() == "Index");
    CHECK_FALSE(f.at(0, ConceptLinksModel::OutgoingRole).toBool());
    CHECK(f.at(1, ConceptLinksModel::OtherTitleRole).toString() == "Tree");
    CHECK(f.at(1, ConceptLinksModel::OutgoingRole).toBool());
    CHECK(f.at(1, ConceptLinksModel::TypeNameRole).toString() == "depends on");
    CHECK(f.at(1, ConceptLinksModel::NoteRole).toString() == "needs a tree");
    CHECK(f.at(1, ConceptLinksModel::RecallRole).toDouble() < 0.0);
}

TEST_CASE("removing a link updates the list") {
    Fixture f;
    REQUIRE(f.links.add(idString(f.tree), 0, true, ""));
    QString linkId = f.at(0, ConceptLinksModel::LinkIdRole).toString();
    REQUIRE(f.links.remove(linkId));
    CHECK(f.links.count() == 0);
}

TEST_CASE("duplicates, bad types, and unknown ids are reported") {
    Fixture f;
    REQUIRE(f.links.add(idString(f.tree), 0, true, ""));
    CHECK_FALSE(f.links.add(idString(f.tree), 0, true, ""));
    CHECK_FALSE(f.links.add(idString(f.tree), 42, true, ""));
    CHECK_FALSE(f.links.add("garbage", 0, true, ""));
    CHECK_FALSE(f.links.remove("garbage"));
    CHECK(f.errors == 4);
}

TEST_CASE("candidates come from every topic but never the concept itself") {
    Fixture f;
    auto all = f.links.candidates("");
    CHECK(all.size() == 2);
    for (const auto& entry : all) CHECK(entry.toMap().value("id").toString() != idString(f.btree));
    auto index = f.links.candidates("Ind");
    REQUIRE(index.size() == 1);
    CHECK(index[0].toMap().value("topic").toString() == "Databases");
}
