#include <algorithm>

#include "atlas/graph/graph_engine.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::graph;

namespace {

KnowledgeObject makeNode(const char* title) { return KnowledgeObject::create(title).value(); }

}  // namespace

TEST_CASE("addNode rejects a duplicate id") {
    GraphEngine graph;
    auto node = makeNode("Recursion");
    auto id = node.id();
    // Re-derive an object with the same id via reconstruct, since
    // create() always generates a fresh one.
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
    // B RelatedTo A is the same fact as A RelatedTo B for a symmetric type.
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
    // B DependsOn A is a different fact from A DependsOn B.
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
    auto edge2 = Relationship::create(aId, cId, RelationshipType::RelatedTo).value();  // symmetric
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
    // b and c survive; only their edges to a are gone.
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
    // Regression test: the original implementation stored nodes in a
    // std::vector, whose reallocation on growth invalidates pointers
    // to existing elements. This pattern — hold a pointer, then insert
    // more nodes — is exactly what a UI naturally does (e.g. a cached
    // "selected node" pointer while the user keeps adding concepts),
    // so it isn't a contrived case. Backing storage is std::deque now,
    // specifically because appending to a deque never invalidates
    // references to elements already in it.
    GraphEngine graph;
    auto first = makeNode("First");
    auto firstId = first.id();
    REQUIRE(graph.addNode(std::move(first)).hasValue());

    const KnowledgeObject* ptr = graph.findNode(firstId);
    REQUIRE(ptr != nullptr);

    // Enough insertions to force several reallocations were this still
    // backed by a vector.
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

TEST_CASE("hasDuplicateEdge lets a caller pre-flight-check before writing anything") {
    // This is what WorkspaceController relies on: check before any
    // database write, rather than discovering a graph-level rejection
    // only after persistence has already accepted it.
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
    // Symmetric type: the reverse pair is the same fact.
    CHECK(graph.hasDuplicateEdge(bId, aId, RelationshipType::RelatedTo));
    // Different type between the same pair is not a duplicate.
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

    // Diamond: A depends on B and C; both B and C depend on D.
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, cId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, dId, RelationshipType::DependsOn).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(cId, dId, RelationshipType::DependsOn).value())
                .hasValue());

    auto deps = graph.transitiveDependencies(aId);
    CHECK(deps.size() == 3);  // B, C, D — D reached via both paths but counted once
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

    // A depends on B, B depends on C. Valid order: C, B, A (in some form).
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
