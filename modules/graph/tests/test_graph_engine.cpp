#include <algorithm>

#include "atlas/graph/graph_engine.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::graph;

namespace {

KnowledgeObject makeNode(const char* title) { return KnowledgeObject::create(title).value(); }

}

TEST_CASE("addNode rejects a duplicate id") {
    GraphEngine graph;
    auto node = makeNode("Recursion");
    auto id = node.id();
    KnowledgeObject::StorageRecord record{id, "Recursion", "", "", "", {}, {}, {}, "",
                                            Difficulty::Beginner, ConfidenceLevel::Unknown,
                                            node.createdAt(), node.updatedAt()};
    REQUIRE(graph.addNode(std::move(node)).hasValue());
    auto duplicate = KnowledgeObject::reconstruct(std::move(record));
    REQUIRE(duplicate.hasValue());
    auto result = graph.addNode(std::move(duplicate).value());
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::DuplicateNode);
}

TEST_CASE("addEdge rejects an edge referencing an unknown node") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto aId = a.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());

    auto edge = Relationship::create(aId, KnowledgeObjectId::generate(), RelationshipType::Uses);
    REQUIRE(edge.hasValue());
    auto result = graph.addEdge(std::move(edge).value());
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::UnknownNode);
}

TEST_CASE("addEdge rejects a duplicate (source, target, type) triple") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    auto result = graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value());
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::DuplicateEdge);
}

TEST_CASE("addEdge rejects a symmetric edge stored in the reverse order as a duplicate") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::RelatedTo).value())
                .hasValue());
    auto result =
        graph.addEdge(Relationship::create(bId, aId, RelationshipType::RelatedTo).value());
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::DuplicateEdge);
}

TEST_CASE("a directional type in reverse order is NOT a duplicate") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    auto result =
        graph.addEdge(Relationship::create(bId, aId, RelationshipType::DependsOn).value());
    CHECK(result.hasValue());
}

TEST_CASE("neighbors finds a symmetric edge from either endpoint regardless of direction") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::AlternativeTo).value())
                .hasValue());

    auto fromA = graph.neighbors(aId, RelationshipType::AlternativeTo, GraphEngine::Direction::Outgoing);
    auto fromB = graph.neighbors(bId, RelationshipType::AlternativeTo, GraphEngine::Direction::Incoming);
    REQUIRE(fromA.size() == 1);
    REQUIRE(fromB.size() == 1);
    CHECK(fromA.front() == bId);
    CHECK(fromB.front() == aId);
}

TEST_CASE("neighbors with Direction::Both does not double-count a symmetric edge") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::OppositeOf).value())
                .hasValue());

    auto result = graph.neighbors(aId, std::nullopt, GraphEngine::Direction::Both);
    REQUIRE(result.size() == 1);
    CHECK(result.front() == bId);
}

TEST_CASE("dependsOn and usedBy are inverse views of the same DependsOn edges") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());

    auto aDeps = graph.dependsOn(aId);
    REQUIRE(aDeps.size() == 1);
    CHECK(aDeps.front() == bId);

    auto bUsedBy = graph.usedBy(bId);
    REQUIRE(bUsedBy.size() == 1);
    CHECK(bUsedBy.front() == aId);
}

TEST_CASE("removeNode cascades to every edge touching it, including symmetric ones") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());

    auto edge1 = Relationship::create(aId, bId, RelationshipType::DependsOn).value();
    auto edge2 = Relationship::create(aId, cId, RelationshipType::RelatedTo).value();
    auto edge1Id = edge1.id();
    auto edge2Id = edge2.id();
    REQUIRE(graph.addEdge(std::move(edge1)).hasValue());
    REQUIRE(graph.addEdge(std::move(edge2)).hasValue());
    REQUIRE(graph.edgeCount() == 2);

    CHECK(graph.removeNode(aId));

    CHECK(graph.findNode(aId) == nullptr);
    CHECK(graph.findEdge(edge1Id) == nullptr);
    CHECK(graph.findEdge(edge2Id) == nullptr);
    CHECK(graph.edgeCount() == 0);
    CHECK(graph.findNode(bId) != nullptr);
    CHECK(graph.findNode(cId) != nullptr);
    CHECK(graph.neighbors(bId).empty());
    CHECK(graph.neighbors(cId).empty());
}

TEST_CASE("removeNode on an unknown id returns false") {
    GraphEngine graph;
    CHECK(!graph.removeNode(KnowledgeObjectId::generate()));
}

TEST_CASE("a pointer from findNode survives many subsequent insertions") {
    GraphEngine graph;
    auto first = makeNode("First");
    auto firstId = first.id();
    REQUIRE(graph.addNode(std::move(first)).hasValue());

    const KnowledgeObject* ptr = graph.findNode(firstId);
    REQUIRE(ptr != nullptr);

    for (int i = 0; i < 500; ++i) {
        REQUIRE(graph.addNode(makeNode("Filler")).hasValue());
    }

    CHECK(ptr->id() == firstId);
    CHECK(ptr->title() == "First");
}

TEST_CASE("updateNode replaces content in place and rejects an unknown id") {
    GraphEngine graph;
    auto node = makeNode("Draft Title");
    auto id = node.id();
    REQUIRE(graph.addNode(std::move(node)).hasValue());

    auto current = *graph.findNode(id);
    REQUIRE(current.renameTo("Final Title").hasValue());
    REQUIRE(graph.updateNode(current).hasValue());

    const auto* updated = graph.findNode(id);
    REQUIRE(updated != nullptr);
    CHECK(updated->title() == "Final Title");

    auto unknown = makeNode("Nobody Added This");
    auto result = graph.updateNode(std::move(unknown));
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::UnknownNode);
}

TEST_CASE("allNodeIds returns exactly the set of live nodes") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.removeNode(bId));

    auto ids = graph.allNodeIds();
    CHECK(ids.size() == 2);
    CHECK(std::find(ids.begin(), ids.end(), aId) != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), cId) != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), bId) == ids.end());
}

TEST_CASE("allEdgeIds returns exactly the set of live edges") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());

    auto edge1 = Relationship::create(aId, bId, RelationshipType::DependsOn).value();
    auto edge2 = Relationship::create(bId, cId, RelationshipType::Uses).value();
    auto edge1Id = edge1.id();
    auto edge2Id = edge2.id();
    REQUIRE(graph.addEdge(std::move(edge1)).hasValue());
    REQUIRE(graph.addEdge(std::move(edge2)).hasValue());
    REQUIRE(graph.removeEdge(edge1Id));

    auto ids = graph.allEdgeIds();
    CHECK(ids.size() == 1);
    CHECK(ids.front() == edge2Id);
}

TEST_CASE("search with an empty query returns every live node, unranked") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    auto results = graph.search("");
    CHECK(results.size() == 2);
}

TEST_CASE("search ranks a title match above a definition-only match") {
    GraphEngine graph;
    auto titleMatch = KnowledgeObject::create("Tail Call", "unrelated text").value();
    auto definitionMatch =
        KnowledgeObject::create("Unrelated Title", "involves a tail call somewhere").value();
    auto titleMatchId = titleMatch.id();
    auto definitionMatchId = definitionMatch.id();
    REQUIRE(graph.addNode(std::move(titleMatch)).hasValue());
    REQUIRE(graph.addNode(std::move(definitionMatch)).hasValue());

    auto results = graph.search("tail call");
    REQUIRE(results.size() == 2);
    CHECK(results.front() == titleMatchId);
    CHECK(results.back() == definitionMatchId);
}

TEST_CASE("search excludes nodes that don't match and never includes removed nodes") {
    GraphEngine graph;
    auto match = KnowledgeObject::create("Recursion").value();
    auto noMatch = KnowledgeObject::create("Unrelated").value();
    auto removed = KnowledgeObject::create("Recursion Removed").value();
    auto matchId = match.id();
    auto removedId = removed.id();
    REQUIRE(graph.addNode(std::move(match)).hasValue());
    REQUIRE(graph.addNode(std::move(noMatch)).hasValue());
    REQUIRE(graph.addNode(std::move(removed)).hasValue());
    REQUIRE(graph.removeNode(removedId));

    auto results = graph.search("recursion");
    CHECK(results.size() == 1);
    CHECK(results.front() == matchId);
}

TEST_CASE("hasDuplicateEdge lets a caller pre-flight-check before writing anything") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    CHECK(!graph.hasDuplicateEdge(aId, bId, RelationshipType::RelatedTo));

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::RelatedTo).value())
                .hasValue());

    CHECK(graph.hasDuplicateEdge(aId, bId, RelationshipType::RelatedTo));
    CHECK(graph.hasDuplicateEdge(bId, aId, RelationshipType::RelatedTo));
    CHECK(!graph.hasDuplicateEdge(aId, bId, RelationshipType::DependsOn));
}

TEST_CASE("transitiveDependencies follows a chain and a diamond without duplicates") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto d = makeNode("D");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    auto dId = d.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.addNode(std::move(d)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, cId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, dId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(cId, dId, RelationshipType::DependsOn).value())
                .hasValue());

    auto deps = graph.transitiveDependencies(aId);
    CHECK(deps.size() == 3);
}

TEST_CASE("topologicalOrder places every dependency before its dependents") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
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

    auto orderResult = graph.topologicalOrder();
    REQUIRE(orderResult.hasValue());
    const auto& order = orderResult.value();
    REQUIRE(order.size() == 3);

    auto position = [&](const KnowledgeObjectId& id) {
        return std::distance(order.begin(), std::find(order.begin(), order.end(), id));
    };
    CHECK(position(cId) < position(bId));
    CHECK(position(bId) < position(aId));
}

TEST_CASE("topologicalOrder detects a cycle") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, aId, RelationshipType::DependsOn).value())
                .hasValue());

    auto orderResult = graph.topologicalOrder();
    CHECK(!orderResult.hasValue());
    CHECK(orderResult.error() == GraphError::CycleDetected);
}

TEST_CASE("learningRoadmapFor rejects an unknown id") {
    GraphEngine graph;
    auto result = graph.learningRoadmapFor(KnowledgeObjectId::generate());
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::UnknownNode);
}

TEST_CASE("learningRoadmapFor a node with no dependencies is just that node") {
    GraphEngine graph;
    auto a = makeNode("Standalone");
    auto aId = a.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());

    auto result = graph.learningRoadmapFor(aId);
    REQUIRE(result.hasValue());
    REQUIRE(result.value().size() == 1);
    CHECK(result.value().front() == aId);
}

TEST_CASE("learningRoadmapFor orders prerequisites before the target, and excludes unrelated nodes") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto unrelated = makeNode("Unrelated");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    auto unrelatedId = unrelated.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.addNode(std::move(unrelated)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, cId, RelationshipType::DependsOn).value())
                .hasValue());

    auto result = graph.learningRoadmapFor(aId);
    REQUIRE(result.hasValue());
    const auto& roadmap = result.value();

    REQUIRE(roadmap.size() == 3);
    CHECK(std::find(roadmap.begin(), roadmap.end(), unrelatedId) == roadmap.end());

    auto position = [&](const KnowledgeObjectId& id) {
        return std::distance(roadmap.begin(), std::find(roadmap.begin(), roadmap.end(), id));
    };
    CHECK(position(cId) < position(bId));
    CHECK(position(bId) < position(aId));
    CHECK(roadmap.back() == aId);
}

TEST_CASE("learningRoadmapFor a diamond dependency includes the shared prerequisite once") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto d = makeNode("D");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    auto dId = d.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.addNode(std::move(d)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, cId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, dId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(cId, dId, RelationshipType::DependsOn).value())
                .hasValue());

    auto result = graph.learningRoadmapFor(aId);
    REQUIRE(result.hasValue());
    CHECK(result.value().size() == 4);
}

TEST_CASE("learningRoadmapFor ignores a cycle outside the target's dependency chain") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto x = makeNode("X");
    auto y = makeNode("Y");
    auto aId = a.id();
    auto bId = b.id();
    auto xId = x.id();
    auto yId = y.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(x)).hasValue());
    REQUIRE(graph.addNode(std::move(y)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(xId, yId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(yId, xId, RelationshipType::DependsOn).value())
                .hasValue());

    auto result = graph.learningRoadmapFor(aId);
    REQUIRE(result.hasValue());
    CHECK(result.value() == std::vector<KnowledgeObjectId>{bId, aId});
}

TEST_CASE("learningRoadmapFor breaks ties between independent prerequisites by id") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto aId = a.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    std::vector<KnowledgeObjectId> prerequisites;
    for (int i = 0; i < 6; ++i) {
        auto node = makeNode("P");
        prerequisites.push_back(node.id());
        REQUIRE(graph.addNode(std::move(node)).hasValue());
        REQUIRE(graph.addEdge(Relationship::create(aId, prerequisites.back(), RelationshipType::DependsOn).value())
                    .hasValue());
    }
    std::sort(prerequisites.begin(), prerequisites.end(),
              [](const auto& left, const auto& right) { return left.toString() < right.toString(); });
    prerequisites.push_back(aId);

    auto result = graph.learningRoadmapFor(aId);
    REQUIRE(result.hasValue());
    CHECK(result.value() == prerequisites);
}

TEST_CASE("learningRoadmapFor reports a cycle if the target's dependency chain has one") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, aId, RelationshipType::DependsOn).value())
                .hasValue());

    auto result = graph.learningRoadmapFor(aId);
    CHECK(!result.hasValue());
    CHECK(result.error() == GraphError::CycleDetected);
}
