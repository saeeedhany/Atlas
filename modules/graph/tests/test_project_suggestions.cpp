#include "atlas/graph/graph_engine.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::graph;

namespace {

KnowledgeObject makeNode(const char* title, TopicId topic, ConfidenceLevel confidence,
                          bool withMiniProject) {
    auto result = KnowledgeObject::create(title);
    REQUIRE(result.hasValue());
    auto object = std::move(result).value();
    object.assignToTopic(topic);
    object.setConfidence(confidence);
    if (withMiniProject) {
        object.addMiniProject(MiniProject{"Build it", "A hands-on project for this concept"});
    }
    return object;
}

}  // namespace

TEST_CASE("transitiveDependents is the mirror of transitiveDependencies") {
    GraphEngine graph;
    // A depends on B, B depends on C — so C's dependents (transitively)
    // are B and A; A has no dependents.
    auto a = KnowledgeObject::create("A").value();
    auto b = KnowledgeObject::create("B").value();
    auto c = KnowledgeObject::create("C").value();
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, cId, RelationshipType::DependsOn).value())
                .hasValue());

    auto cDependents = graph.transitiveDependents(cId);
    CHECK(cDependents.size() == 2);
    CHECK(std::find(cDependents.begin(), cDependents.end(), aId) != cDependents.end());
    CHECK(std::find(cDependents.begin(), cDependents.end(), bId) != cDependents.end());

    CHECK(graph.transitiveDependents(aId).empty());
}

TEST_CASE("transitiveDependents on an unknown id returns empty, not a crash") {
    GraphEngine graph;
    CHECK(graph.transitiveDependents(KnowledgeObjectId::generate()).empty());
}

TEST_CASE("suggestProjects excludes concepts with no MiniProjects") {
    GraphEngine graph;
    auto topic = TopicId::generate();
    auto withProject = makeNode("Has Project", topic, ConfidenceLevel::Learning, true);
    auto withoutProject = makeNode("No Project", topic, ConfidenceLevel::Learning, false);
    auto withId = withProject.id();
    REQUIRE(graph.addNode(std::move(withProject)).hasValue());
    REQUIRE(graph.addNode(std::move(withoutProject)).hasValue());

    auto suggestions = graph.suggestProjects(topic);
    REQUIRE(suggestions.size() == 1);
    CHECK(suggestions.front().conceptId == withId);
}

TEST_CASE("suggestProjects excludes concepts already at Mastered confidence") {
    GraphEngine graph;
    auto topic = TopicId::generate();
    auto mastered = makeNode("Already Mastered", topic, ConfidenceLevel::Mastered, true);
    auto learning = makeNode("Still Learning", topic, ConfidenceLevel::Learning, true);
    auto learningId = learning.id();
    REQUIRE(graph.addNode(std::move(mastered)).hasValue());
    REQUIRE(graph.addNode(std::move(learning)).hasValue());

    auto suggestions = graph.suggestProjects(topic);
    REQUIRE(suggestions.size() == 1);
    CHECK(suggestions.front().conceptId == learningId);
}

TEST_CASE("suggestProjects only includes concepts from the requested topic") {
    GraphEngine graph;
    auto topicA = TopicId::generate();
    auto topicB = TopicId::generate();
    auto inA = makeNode("In Topic A", topicA, ConfidenceLevel::Learning, true);
    auto inB = makeNode("In Topic B", topicB, ConfidenceLevel::Learning, true);
    auto inAId = inA.id();
    REQUIRE(graph.addNode(std::move(inA)).hasValue());
    REQUIRE(graph.addNode(std::move(inB)).hasValue());

    auto suggestions = graph.suggestProjects(topicA);
    REQUIRE(suggestions.size() == 1);
    CHECK(suggestions.front().conceptId == inAId);
}

TEST_CASE("suggestProjects computes readiness from prerequisite confidence") {
    GraphEngine graph;
    auto topic = TopicId::generate();
    auto target = makeNode("Target", topic, ConfidenceLevel::Learning, true);
    auto readyPrereq = makeNode("Ready Prereq", topic, ConfidenceLevel::Confident, false);
    auto unreadyPrereq = makeNode("Unready Prereq", topic, ConfidenceLevel::Unknown, false);
    auto targetId = target.id();
    auto readyId = readyPrereq.id();
    auto unreadyId = unreadyPrereq.id();
    REQUIRE(graph.addNode(std::move(target)).hasValue());
    REQUIRE(graph.addNode(std::move(readyPrereq)).hasValue());
    REQUIRE(graph.addNode(std::move(unreadyPrereq)).hasValue());
    REQUIRE(
        graph.addEdge(Relationship::create(targetId, readyId, RelationshipType::DependsOn).value())
            .hasValue());
    REQUIRE(graph
                .addEdge(Relationship::create(targetId, unreadyId, RelationshipType::DependsOn)
                             .value())
                .hasValue());

    auto suggestions = graph.suggestProjects(topic);
    REQUIRE(suggestions.size() == 1);
    CHECK(suggestions.front().readiness == doctest::Approx(0.5));  // 1 of 2 prereqs ready
}

TEST_CASE("suggestProjects gives a concept with no prerequisites full readiness") {
    GraphEngine graph;
    auto topic = TopicId::generate();
    auto standalone = makeNode("Standalone", topic, ConfidenceLevel::Learning, true);
    auto id = standalone.id();
    REQUIRE(graph.addNode(std::move(standalone)).hasValue());

    auto suggestions = graph.suggestProjects(topic);
    REQUIRE(suggestions.size() == 1);
    CHECK(suggestions.front().conceptId == id);
    CHECK(suggestions.front().readiness == doctest::Approx(1.0));
}

TEST_CASE("suggestProjects ranks higher leverage above lower leverage at equal readiness") {
    GraphEngine graph;
    auto topic = TopicId::generate();
    // highLeverage has two things depending on it; lowLeverage has none.
    // Both have no prerequisites of their own (full readiness), so
    // leverage is the only thing that should differentiate their rank.
    auto highLeverage = makeNode("High Leverage", topic, ConfidenceLevel::Learning, true);
    auto lowLeverage = makeNode("Low Leverage", topic, ConfidenceLevel::Learning, true);
    auto dependent1 = makeNode("Dependent 1", topic, ConfidenceLevel::Learning, false);
    auto dependent2 = makeNode("Dependent 2", topic, ConfidenceLevel::Learning, false);
    auto highId = highLeverage.id();
    auto lowId = lowLeverage.id();
    auto dep1Id = dependent1.id();
    auto dep2Id = dependent2.id();
    REQUIRE(graph.addNode(std::move(highLeverage)).hasValue());
    REQUIRE(graph.addNode(std::move(lowLeverage)).hasValue());
    REQUIRE(graph.addNode(std::move(dependent1)).hasValue());
    REQUIRE(graph.addNode(std::move(dependent2)).hasValue());
    REQUIRE(
        graph.addEdge(Relationship::create(dep1Id, highId, RelationshipType::DependsOn).value())
            .hasValue());
    REQUIRE(
        graph.addEdge(Relationship::create(dep2Id, highId, RelationshipType::DependsOn).value())
            .hasValue());

    auto suggestions = graph.suggestProjects(topic);
    // highLeverage and lowLeverage both have MiniProjects; dependent1/2
    // don't, so only those two should be ranked.
    REQUIRE(suggestions.size() == 2);
    CHECK(suggestions.front().conceptId == highId);
    CHECK(suggestions.back().conceptId == lowId);
}

TEST_CASE("suggestProjects on a topic with no matching concepts returns empty") {
    GraphEngine graph;
    auto suggestions = graph.suggestProjects(TopicId::generate());
    CHECK(suggestions.empty());
}
