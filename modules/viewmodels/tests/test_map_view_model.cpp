#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/map_view_model.hpp"
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
    QTemporaryDir dir;
    QSettings store{dir.filePath("settings.ini"), QSettings::IniFormat};
    AppSettings settings{store};
    Palette palette{settings};
    TimePoint now = std::chrono::system_clock::now();
    MemoryController memory{db, workspace, [this] { return now; }};
    PlacementController placements{db, workspace};
    MapViewModel map{workspace, memory, placements, palette};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(placements.load().hasValue());
        REQUIRE(memory.load().hasValue());
        QObject::connect(&map, &MapViewModel::errorOccurred, [this] { ++errors; });
        settle();
    }

    static void settle() { QCoreApplication::processEvents(); }

    TopicId topic(const char* name) { return workspace.createTopic(name).value(); }
    KnowledgeObjectId addConcept(const char* title, TopicId topicId) {
        return workspace.createKnowledgeObject(title, topicId).value();
    }
    const atlas::render::RenderNode* node(const KnowledgeObjectId& id) const {
        for (const auto& node : map.nodes()) {
            if (node.id == idString(id)) return &node;
        }
        return nullptr;
    }
};

}  // namespace

TEST_CASE("ids round trip and reject garbage") {
    auto id = KnowledgeObjectId::generate();
    CHECK(parseId<KnowledgeObjectId>(idString(id)) == id);
    CHECK_FALSE(parseId<KnowledgeObjectId>("not an id").has_value());
}

TEST_CASE("an empty database gives an empty scene without errors") {
    Fixture f;
    CHECK(f.map.nodes().empty());
    CHECK(f.map.conceptCount() == 0);
    CHECK(f.errors == 0);
}

TEST_CASE("a topic scope shows outside neighbors as ghosts") {
    Fixture f;
    auto os = f.topic("Operating Systems");
    auto databases = f.topic("Databases");
    auto paging = f.addConcept("Paging", os);
    auto indexing = f.addConcept("Indexing", databases);
    REQUIRE(f.workspace.createRelationship(indexing, paging, RelationshipType::Uses, std::nullopt).hasValue());
    f.settle();

    CHECK(f.map.nodes().size() == 2);
    CHECK_FALSE(f.node(indexing)->ghost);

    f.map.setTopicId(idString(os));
    CHECK(f.map.conceptCount() == 1);
    REQUIRE(f.node(indexing) != nullptr);
    CHECK(f.node(indexing)->ghost);
    CHECK_FALSE(f.node(paging)->ghost);
    CHECK(f.node(paging)->groupLabel == "Operating Systems");
    REQUIRE(f.map.edges().size() == 1);
    CHECK(f.map.edges()[0].ghost);
    CHECK(f.map.edges()[0].directed);
}

TEST_CASE("links carry their style and nodes carry recall") {
    Fixture f;
    auto topic = uncategorizedTopicId();
    auto btree = f.addConcept("B-Tree", topic);
    auto hash = f.addConcept("Hash Table", topic);
    REQUIRE(f.workspace.createRelationship(btree, hash, RelationshipType::AlternativeTo, std::nullopt).hasValue());
    f.settle();
    REQUIRE(f.map.edges().size() == 1);
    CHECK(f.map.edges()[0].contrast);
    CHECK_FALSE(f.map.edges()[0].directed);
    CHECK(f.node(btree)->recall < 0.0);

    ReviewEvent review;
    review.id = Uuid::generate();
    review.item = ItemRef::forConcept(btree);
    review.sessionId = Uuid::generate();
    review.deviceId = "test";
    review.reviewedAt = f.now;
    review.grade = Grade::Good;
    MemoryState state{review.item};
    state.phase = Phase::Review;
    state.stability = 2.3065;
    state.difficulty = 5.0;
    state.lastReviewedAt = f.now;
    REQUIRE(LearningRepository(f.db).record({review}, {state}).hasValue());
    REQUIRE(f.memory.load().hasValue());
    f.settle();
    CHECK(f.node(btree)->recall == doctest::Approx(1.0));
}

TEST_CASE("deleting the selected concept clears the selection") {
    Fixture f;
    auto tree = f.addConcept("Tree", uncategorizedTopicId());
    f.settle();
    int selectionChanges = 0;
    QObject::connect(&f.map, &MapViewModel::selectedIdChanged, [&] { ++selectionChanges; });
    f.map.setSelectedId(idString(tree));
    REQUIRE(f.workspace.removeKnowledgeObject(tree).hasValue());
    f.settle();
    CHECK(f.map.selectedId().isEmpty());
    CHECK(selectionChanges == 2);
}

TEST_CASE("the attached canvas follows the scene and the theme") {
    Fixture f;
    f.addConcept("Tree", uncategorizedTopicId());
    atlas::render::GraphCanvasItem canvas;
    f.map.attach(&canvas);
    CHECK(canvas.nodes().size() == 1);
    f.settings.setDarkTheme(false);
    f.settle();
    CHECK(canvas.theme() == atlas::render::ThemeMode::Light);
    f.addConcept("Hash", uncategorizedTopicId());
    f.settle();
    CHECK(canvas.nodes().size() == 2);
}

TEST_CASE("clicking a collapsed topic opens it and clicking a node selects it") {
    Fixture f;
    auto os = f.topic("OS");
    auto paging = f.addConcept("Paging", os);
    f.settle();
    atlas::render::GraphCanvasItem canvas;
    f.map.attach(&canvas);
    emit canvas.groupClicked(idString(os));
    CHECK(f.map.topicId() == idString(os));
    emit canvas.nodeClicked(idString(paging));
    CHECK(f.map.selectedId() == idString(paging));
}

TEST_CASE("search and new concepts stay inside the topic scope") {
    Fixture f;
    auto os = f.topic("OS");
    f.addConcept("Paging", os);
    f.addConcept("Indexing", uncategorizedTopicId());
    f.map.setTopicId(idString(os));

    auto results = f.map.search("");
    REQUIRE(results.size() == 1);
    CHECK(results[0].toMap().value("title").toString() == "Paging");

    QString created = f.map.createConcept("Segmentation");
    REQUIRE_FALSE(created.isEmpty());
    auto object = f.workspace.findKnowledgeObject(*parseId<KnowledgeObjectId>(created));
    REQUIRE(object.has_value());
    CHECK(object->topicId() == os);
    CHECK(f.map.selectedId() == created);
}

TEST_CASE("bad input is reported, not ignored") {
    Fixture f;
    f.map.setTopicId("garbage");
    CHECK(f.map.topicId().isEmpty());
    CHECK(f.map.createConcept("").isEmpty());
    CHECK(f.errors == 1);
    QQuickItem notACanvas;
    f.map.attach(&notACanvas);
    CHECK(f.errors == 2);
}

TEST_CASE("one new concept gives one scene change and never flies in from the origin") {
    Fixture f;
    f.addConcept("Tree", uncategorizedTopicId());
    f.settle();
    int sceneChanges = 0;
    std::vector<atlas::render::RenderNode> seen;
    QObject::connect(&f.map, &MapViewModel::sceneChanged, [&] {
        ++sceneChanges;
        seen.insert(seen.end(), f.map.nodes().begin(), f.map.nodes().end());
    });

    auto added = f.workspace.createKnowledgeObject("Hash", uncategorizedTopicId()).value();
    f.settle();

    CHECK(sceneChanges == 1);
    auto placed = f.placements.position(added);
    REQUIRE(placed.has_value());
    REQUIRE(f.node(added) != nullptr);
    for (const auto& node : seen) {
        if (node.id != idString(added)) continue;
        CHECK(node.x == placed->x);
        CHECK(node.y == placed->y);
    }
}

TEST_CASE("the topic scope is stored in canonical form") {
    Fixture f;
    auto os = f.topic("OS");
    f.map.setTopicId(idString(os).toUpper());
    CHECK(f.map.topicId() == idString(os));
}

TEST_CASE("a selection outside the new scope is cleared") {
    Fixture f;
    auto os = f.topic("OS");
    auto databases = f.topic("Databases");
    auto paging = f.addConcept("Paging", os);
    f.addConcept("Indexing", databases);
    f.settle();
    f.map.setSelectedId(idString(paging));
    REQUIRE(f.map.selectedId() == idString(paging));

    f.map.setTopicId(idString(databases));
    CHECK(f.map.selectedId().isEmpty());

    f.map.setSelectedId(idString(paging));
    CHECK(f.map.selectedId().isEmpty());
    f.map.setSelectedId("garbage");
    CHECK(f.map.selectedId().isEmpty());
}

TEST_CASE("a known concept can be selected before the pending refresh shows it") {
    Fixture f;
    auto os = f.topic("OS");
    auto databases = f.topic("Databases");
    f.settle();

    auto tree = f.addConcept("Tree", uncategorizedTopicId());
    REQUIRE(f.node(tree) == nullptr);
    f.map.setSelectedId(idString(tree));
    CHECK(f.map.selectedId() == idString(tree));
    f.settle();
    CHECK(f.node(tree) != nullptr);
    CHECK(f.map.selectedId() == idString(tree));

    f.map.setTopicId(idString(databases));
    auto paging = f.addConcept("Paging", os);
    f.map.setSelectedId(idString(paging));
    CHECK(f.map.selectedId() == idString(paging));
    f.settle();
    CHECK(f.node(paging) == nullptr);
    CHECK(f.map.selectedId().isEmpty());
}

TEST_CASE("deleting the scoped topic resets the scope") {
    Fixture f;
    auto os = f.topic("OS");
    f.map.setTopicId(idString(os));
    int scopeChanges = 0;
    QObject::connect(&f.map, &MapViewModel::topicIdChanged, [&] { ++scopeChanges; });

    REQUIRE(f.workspace.removeTopic(os).hasValue());
    CHECK(f.map.topicId().isEmpty());
    CHECK(scopeChanges == 1);
    CHECK_FALSE(f.map.createConcept("Recursion").isEmpty());
    CHECK(f.errors == 0);
}

TEST_CASE("attaching a new canvas disconnects the old one") {
    Fixture f;
    auto tree = f.addConcept("Tree", uncategorizedTopicId());
    f.settle();
    atlas::render::GraphCanvasItem oldCanvas;
    atlas::render::GraphCanvasItem newCanvas;
    f.map.attach(&oldCanvas);
    f.map.attach(&newCanvas);

    emit oldCanvas.nodeClicked(idString(tree));
    CHECK(f.map.selectedId().isEmpty());
    emit newCanvas.nodeClicked(idString(tree));
    CHECK(f.map.selectedId() == idString(tree));
}

TEST_CASE("edges carry relationship ids and info lookups describe concepts and links") {
    Fixture f;
    auto tree = f.addConcept("Tree", uncategorizedTopicId());
    auto btree = f.addConcept("B-Tree", uncategorizedTopicId());
    auto link = f.workspace.createRelationship(btree, tree, RelationshipType::DependsOn, std::string("needs a tree")).value();
    QCoreApplication::processEvents();
    REQUIRE(f.map.edges().size() == 1);
    CHECK(f.map.edges()[0].id == idString(link));

    auto concept_ = f.map.conceptInfo(idString(tree));
    CHECK(concept_.value("title").toString() == "Tree");
    CHECK(concept_.value("recall").toDouble() < 0.0);
    CHECK(concept_.value("topic").toString() == "Uncategorized");

    auto info = f.map.linkInfo(idString(link));
    CHECK(info.value("sourceId").toString() == idString(btree));
    CHECK(info.value("source").toString() == "B-Tree");
    CHECK(info.value("target").toString() == "Tree");
    CHECK(info.value("typeName").toString() == "depends on");
    CHECK(info.value("note").toString() == "needs a tree");
    CHECK(f.map.linkInfo("garbage").isEmpty());
}

TEST_CASE("a single topic view never offers topic collapse") {
    Fixture f;
    auto os = f.topic("OS");
    f.addConcept("Paging", os);
    QCoreApplication::processEvents();
    CHECK_FALSE(f.map.nodes()[0].groupKey.isEmpty());
    f.map.setTopicId(idString(os));
    CHECK(f.map.nodes()[0].groupKey.isEmpty());
}
