#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/relationship_repository.hpp"
#include "atlas/persistence/topic_repository.hpp"
#include "atlas/ui/workspace_controller.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::ui;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

}  // namespace

TEST_CASE("createKnowledgeObject persists and adds to the graph") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.createKnowledgeObject("Hash Tables");
    REQUIRE(result.hasValue());
    auto id = result.value();

    auto found = controller.findKnowledgeObject(id);
    REQUIRE(found.has_value());
    CHECK(found->title() == "Hash Tables");

    // Independently verify it actually reached the database, not just
    // the in-memory graph.
    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->title() == "Hash Tables");
}

TEST_CASE("createKnowledgeObject rejects an empty title without touching the graph") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.createKnowledgeObject("");
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);
    CHECK(controller.allKnowledgeObjects().empty());
}

TEST_CASE("createKnowledgeObject emits graphChanged exactly once") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    int signalCount = 0;
    QObject::connect(&controller, &WorkspaceController::graphChanged, [&]() { ++signalCount; });

    REQUIRE(controller.createKnowledgeObject("Recursion").hasValue());
    CHECK(signalCount == 1);
}

TEST_CASE("updateKnowledgeObject changes fields and persists them") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto id = controller.createKnowledgeObject("Draft").value();

    KnowledgeObjectEdits edits;
    edits.title = "Tail Call Optimization";
    edits.definition = "Reusing the current stack frame for a tail call";
    edits.difficulty = Difficulty::Advanced;
    edits.confidence = ConfidenceLevel::Learning;

    auto result = controller.updateKnowledgeObject(id, edits);
    REQUIRE(result.hasValue());

    auto updated = controller.findKnowledgeObject(id);
    REQUIRE(updated.has_value());
    CHECK(updated->title() == "Tail Call Optimization");
    CHECK(updated->definition() == "Reusing the current stack frame for a tail call");
    CHECK(updated->difficulty() == Difficulty::Advanced);
    CHECK(updated->confidence() == ConfidenceLevel::Learning);

    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->title() == "Tail Call Optimization");
}

TEST_CASE("updateKnowledgeObject rejects an empty title and leaves the object unchanged") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto id = controller.createKnowledgeObject("Original Title").value();

    KnowledgeObjectEdits edits;
    edits.title = "";
    auto result = controller.updateKnowledgeObject(id, edits);
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);

    auto unchanged = controller.findKnowledgeObject(id);
    REQUIRE(unchanged.has_value());
    CHECK(unchanged->title() == "Original Title");
}

TEST_CASE("updateKnowledgeObject on an unknown id returns NotFound") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.updateKnowledgeObject(KnowledgeObjectId::generate(), {});
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::NotFound);
}

TEST_CASE("removeKnowledgeObject removes from both the graph and the database") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto id = controller.createKnowledgeObject("Temporary").value();

    REQUIRE(controller.removeKnowledgeObject(id).hasValue());

    CHECK(!controller.findKnowledgeObject(id).has_value());

    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    CHECK(!persisted.value().has_value());
}

TEST_CASE("allKnowledgeObjects returns a deterministic, alphabetically-sorted snapshot") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    REQUIRE(controller.createKnowledgeObject("Zebra").hasValue());
    REQUIRE(controller.createKnowledgeObject("Apple").hasValue());
    REQUIRE(controller.createKnowledgeObject("Mango").hasValue());

    auto all = controller.allKnowledgeObjects();
    REQUIRE(all.size() == 3);
    CHECK(all[0].title() == "Apple");
    CHECK(all[1].title() == "Mango");
    CHECK(all[2].title() == "Zebra");
}

TEST_CASE("search ranks results by relevance, not alphabetically") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    REQUIRE(controller.createKnowledgeObject("Zebra Tail Call").hasValue());
    auto unrelated = KnowledgeObjectEdits{};
    auto aphidId = controller.createKnowledgeObject("Aphid").value();
    unrelated.definition = "involves a tail call somewhere in its body";
    REQUIRE(controller.updateKnowledgeObject(aphidId, unrelated).hasValue());

    auto results = controller.search("tail call");
    REQUIRE(results.size() == 2);
    // "Zebra Tail Call" matches in the title (higher weight) even
    // though "Aphid" would sort first alphabetically — this is the
    // whole point of ranking by relevance instead of reusing
    // allKnowledgeObjects()'s alphabetical order.
    CHECK(results.front().title() == "Zebra Tail Call");
}

TEST_CASE("search with an empty query behaves like allKnowledgeObjects()") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    REQUIRE(controller.createKnowledgeObject("Zebra").hasValue());
    REQUIRE(controller.createKnowledgeObject("Apple").hasValue());

    auto results = controller.search("");
    REQUIRE(results.size() == 2);
    CHECK(results[0].title() == "Apple");  // alphabetical, same as allKnowledgeObjects()
    CHECK(results[1].title() == "Zebra");
}

TEST_CASE("search excludes objects that don't match") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    REQUIRE(controller.createKnowledgeObject("Recursion").hasValue());
    REQUIRE(controller.createKnowledgeObject("Linked List").hasValue());

    auto results = controller.search("recursion");
    REQUIRE(results.size() == 1);
    CHECK(results.front().title() == "Recursion");
}

TEST_CASE("load() populates the graph from data already in the database") {
    auto db = openTestDatabase();
    // load() actually reads from storage rather than only reflecting
    // whatever the controller itself wrote during this process.
    KnowledgeObjectRepository repo(db);
    auto seeded = KnowledgeObject::create("Seeded From Disk");
    REQUIRE(seeded.hasValue());
    REQUIRE(repo.save(seeded.value()).hasValue());

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto all = controller.allKnowledgeObjects();
    REQUIRE(all.size() == 1);
    CHECK(all.front().title() == "Seeded From Disk");
}

TEST_CASE("createRelationship persists and adds to the graph") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto a = controller.createKnowledgeObject("Stack").value();
    auto b = controller.createKnowledgeObject("Recursion").value();

    auto result = controller.createRelationship(a, b, RelationshipType::Uses,
                                                  std::string{"a note"});
    REQUIRE(result.hasValue());

    auto all = controller.allRelationships();
    REQUIRE(all.size() == 1);
    CHECK(all.front().sourceId() == a);
    CHECK(all.front().targetId() == b);
    CHECK(all.front().type() == RelationshipType::Uses);

    RelationshipRepository repo(db);
    auto persisted = repo.findById(result.value());
    REQUIRE(persisted.hasValue());
    CHECK(persisted.value().has_value());
}

TEST_CASE("createRelationship rejects a self-loop") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto a = controller.createKnowledgeObject("A").value();

    auto result = controller.createRelationship(a, a, RelationshipType::DependsOn, std::nullopt);
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);
}

TEST_CASE("createRelationship rejects a duplicate before writing to the database") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();

    REQUIRE(controller.createRelationship(a, b, RelationshipType::RelatedTo, std::nullopt)
                .hasValue());

    // Reverse pair, symmetric type — same fact, must be rejected even
    // though a naive ordered UNIQUE constraint wouldn't catch it.
    auto result = controller.createRelationship(b, a, RelationshipType::RelatedTo, std::nullopt);
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);

    RelationshipRepository repo(db);
    auto all = repo.findAll();
    REQUIRE(all.hasValue());
    CHECK(all.value().size() == 1);  // the rejected attempt never reached the database
}

TEST_CASE("removeRelationship removes from both the graph and the database") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();
    auto relId =
        controller.createRelationship(a, b, RelationshipType::Causes, std::nullopt).value();

    REQUIRE(controller.removeRelationship(relId).hasValue());

    CHECK(controller.allRelationships().empty());

    RelationshipRepository repo(db);
    auto persisted = repo.findById(relId);
    REQUIRE(persisted.hasValue());
    CHECK(!persisted.value().has_value());
}

TEST_CASE("roadmapFor rejects an unknown id") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.roadmapFor(KnowledgeObjectId::generate());
    CHECK(!result.hasValue());
    CHECK(result.error().code == WorkspaceController::RoadmapErrorCode::UnknownNode);
}

TEST_CASE("roadmapFor resolves a dependency chain to full objects in order") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto a = controller.createKnowledgeObject("Dynamic Programming").value();
    auto b = controller.createKnowledgeObject("Recursion").value();
    auto c = controller.createKnowledgeObject("Functions").value();
    REQUIRE(controller.createRelationship(a, b, RelationshipType::DependsOn, std::nullopt)
                .hasValue());
    REQUIRE(controller.createRelationship(b, c, RelationshipType::DependsOn, std::nullopt)
                .hasValue());

    auto result = controller.roadmapFor(a);
    REQUIRE(result.hasValue());
    const auto& roadmap = result.value();
    REQUIRE(roadmap.size() == 3);
    CHECK(roadmap[0].title() == "Functions");
    CHECK(roadmap[1].title() == "Recursion");
    CHECK(roadmap[2].title() == "Dynamic Programming");
}

TEST_CASE("roadmapFor reports a cycle as a CycleDetected failure, not a crash") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();
    REQUIRE(controller.createRelationship(a, b, RelationshipType::DependsOn, std::nullopt)
                .hasValue());
    REQUIRE(controller.createRelationship(b, a, RelationshipType::DependsOn, std::nullopt)
                .hasValue());

    auto result = controller.roadmapFor(a);
    CHECK(!result.hasValue());
    CHECK(result.error().code == WorkspaceController::RoadmapErrorCode::CycleDetected);
}

TEST_CASE("removing a KnowledgeObject cascades and the relationship list reflects it") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();
    REQUIRE(controller.createRelationship(a, b, RelationshipType::DependsOn, std::nullopt)
                .hasValue());

    REQUIRE(controller.removeKnowledgeObject(a).hasValue());

    // This is exactly the cascade scenario that motivated collapsing
    // to one graphChanged signal — removeKnowledgeObject never emits
    // anything relationship-specific, so a view that only refreshed on
    // a (now-removed) relationshipRemoved signal would have missed it.
    CHECK(controller.allRelationships().empty());
}

TEST_CASE("A freshly created KnowledgeObject with no topic argument lands in Uncategorized") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto id = controller.createKnowledgeObject("Undecided").value();
    auto found = controller.findKnowledgeObject(id);
    REQUIRE(found.has_value());
    REQUIRE(found->topicId().has_value());
    CHECK(*found->topicId() == uncategorizedTopicId());
}

TEST_CASE("createTopic persists and allTopics includes it alongside the seeded Uncategorized topic") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.createTopic("Operating Systems", "Kernels and schedulers");
    REQUIRE(result.hasValue());

    auto topics = controller.allTopics();
    CHECK(topics.size() == 2);  // Uncategorized + Operating Systems

    bool found = false;
    for (const auto& topic : topics) {
        if (topic.id() == result.value()) {
            found = true;
            CHECK(topic.name() == "Operating Systems");
        }
    }
    CHECK(found);
}

TEST_CASE("createTopic rejects an empty name") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.createTopic("");
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);
}

TEST_CASE("createKnowledgeObject with an explicit topic assigns it, and rejects an unknown topic") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto topicId = controller.createTopic("Databases").value();
    auto objectId = controller.createKnowledgeObject("Indexing", topicId).value();

    auto found = controller.findKnowledgeObject(objectId);
    REQUIRE(found.has_value());
    REQUIRE(found->topicId().has_value());
    CHECK(*found->topicId() == topicId);

    auto badResult = controller.createKnowledgeObject("Orphaned", TopicId::generate());
    CHECK(!badResult.hasValue());
    CHECK(badResult.error().code == ControllerErrorCode::NotFound);
}

TEST_CASE("knowledgeObjectsInTopic only returns members of that topic") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    controller.createKnowledgeObject("Paging", os);
    controller.createKnowledgeObject("Scheduling", os);
    controller.createKnowledgeObject("Indexing", databases);

    auto osMembers = controller.knowledgeObjectsInTopic(os);
    CHECK(osMembers.size() == 2);
    for (const auto& object : osMembers) {
        REQUIRE(object.topicId().has_value());
        CHECK(*object.topicId() == os);
    }

    auto dbMembers = controller.knowledgeObjectsInTopic(databases);
    CHECK(dbMembers.size() == 1);
    CHECK(dbMembers.front().title() == "Indexing");
}

TEST_CASE("createRelationship rejects connecting KnowledgeObjects in different topics") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    auto paging = controller.createKnowledgeObject("Paging", os).value();
    auto indexing = controller.createKnowledgeObject("Indexing", databases).value();

    auto result =
        controller.createRelationship(paging, indexing, RelationshipType::RelatedTo, std::nullopt);
    CHECK(!result.hasValue());
    CHECK(result.error().code == ControllerErrorCode::ValidationFailed);
    CHECK(controller.allRelationships().empty());
}

TEST_CASE("createRelationship allows connecting two KnowledgeObjects in the same topic") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto paging = controller.createKnowledgeObject("Paging", os).value();
    auto virtualMemory = controller.createKnowledgeObject("Virtual Memory", os).value();

    auto result = controller.createRelationship(virtualMemory, paging, RelationshipType::DependsOn,
                                                  std::nullopt);
    CHECK(result.hasValue());
}

TEST_CASE("removeTopic refuses to remove Uncategorized") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto result = controller.removeTopic(uncategorizedTopicId());
    CHECK(!result.hasValue());
}

TEST_CASE("removeTopic refuses to remove a topic that still has members") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto topicId = controller.createTopic("Operating Systems").value();
    controller.createKnowledgeObject("Paging", topicId);

    auto result = controller.removeTopic(topicId);
    CHECK(!result.hasValue());
    CHECK(controller.allTopics().size() == 2);  // still there — nothing was removed
}

TEST_CASE("removeTopic succeeds once the topic is empty") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto topicId = controller.createTopic("Temporary").value();
    REQUIRE(controller.removeTopic(topicId).hasValue());
    CHECK(controller.allTopics().size() == 1);  // just Uncategorized left
}

TEST_CASE("updateKnowledgeObject can move an object to a different topic") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    auto id = controller.createKnowledgeObject("Misfiled Concept", os).value();

    KnowledgeObjectEdits edits;
    edits.topicId = databases;
    REQUIRE(controller.updateKnowledgeObject(id, edits).hasValue());

    auto found = controller.findKnowledgeObject(id);
    REQUIRE(found.has_value());
    REQUIRE(found->topicId().has_value());
    CHECK(*found->topicId() == databases);
}

TEST_CASE("updateKnowledgeObject replaces examples/miniProjects/references and persists them") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto id = controller.createKnowledgeObject("Recursion").value();

    KnowledgeObjectEdits edits;
    edits.examples = std::vector<Example>{Example{"Factorial", std::nullopt}};
    edits.miniProjects =
        std::vector<MiniProject>{MiniProject{"Implement it", "From scratch, no libraries"}};
    edits.references = std::vector<Reference>{Reference{"CLRS", std::nullopt}};
    REQUIRE(controller.updateKnowledgeObject(id, edits).hasValue());

    auto found = controller.findKnowledgeObject(id);
    REQUIRE(found.has_value());
    REQUIRE(found->examples().size() == 1);
    REQUIRE(found->miniProjects().size() == 1);
    REQUIRE(found->references().size() == 1);
    CHECK(found->examples().front().description == "Factorial");

    // Independently verify it actually reached the database, not just
    // the in-memory graph — same pattern as every other persistence
    // check in this file.
    KnowledgeObjectRepository repo(db);
    auto persisted = repo.findById(id);
    REQUIRE(persisted.hasValue());
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->miniProjects().size() == 1);
}

TEST_CASE("updateKnowledgeObject with no list edits leaves existing lists untouched") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto id = controller.createKnowledgeObject("Recursion").value();
    KnowledgeObjectEdits firstEdit;
    firstEdit.examples = std::vector<Example>{Example{"Factorial", std::nullopt}};
    REQUIRE(controller.updateKnowledgeObject(id, firstEdit).hasValue());

    // A second, unrelated edit that doesn't touch examples at all —
    // nullopt (the KnowledgeObjectEdits default), not an empty vector.
    KnowledgeObjectEdits secondEdit;
    secondEdit.notes = "unrelated change";
    REQUIRE(controller.updateKnowledgeObject(id, secondEdit).hasValue());

    auto found = controller.findKnowledgeObject(id);
    REQUIRE(found.has_value());
    REQUIRE(found->examples().size() == 1);  // still there — nullopt means "don't touch"
    CHECK(found->notes() == "unrelated change");
}

TEST_CASE("suggestProjects resolves ids to full KnowledgeObjects with readiness/leverage") {
    auto db = openTestDatabase();

    // WorkspaceController has no API yet to add a MiniProject to an
    // object, so seed the database directly — same pattern as the
    // "load() populates the graph from data already in the database"
    // test above.
    auto topicResult = Topic::create("Algorithms");
    REQUIRE(topicResult.hasValue());
    auto topic = std::move(topicResult).value();
    auto topicId = topic.id();
    TopicRepository topicRepo(db);
    REQUIRE(topicRepo.save(topic).hasValue());

    auto objectResult = KnowledgeObject::create("Dynamic Programming");
    REQUIRE(objectResult.hasValue());
    auto object = std::move(objectResult).value();
    object.assignToTopic(topicId);
    object.addMiniProject(MiniProject{"Memoized Fibonacci", "Implement it two ways"});
    auto objectId = object.id();
    KnowledgeObjectRepository objectRepo(db);
    REQUIRE(objectRepo.save(object).hasValue());

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto suggestions = controller.suggestProjects(topicId);
    REQUIRE(suggestions.size() == 1);
    CHECK(suggestions.front().knowledgeObject.id() == objectId);
    CHECK(suggestions.front().knowledgeObject.title() == "Dynamic Programming");
    CHECK(suggestions.front().readiness == doctest::Approx(1.0));  // no prerequisites
    CHECK(suggestions.front().leverage == 0);
}

TEST_CASE("suggestProjects on a topic with nothing suggestable returns an empty list, not an error") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto topicId = controller.createTopic("Empty Topic").value();
    auto suggestions = controller.suggestProjects(topicId);
    CHECK(suggestions.empty());
}
