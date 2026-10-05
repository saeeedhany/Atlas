#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include "atlas/graph/graph_engine.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::graph;

namespace {

constexpr int kNodeCount = 10'000;
constexpr int kEdgeAttempts = 30'000;

}

TEST_CASE("GraphEngine handles 10,000 nodes and ~30,000 edges without an algorithmic blowup" * doctest::test_suite("scale")) {
    GraphEngine graph;
    std::vector<KnowledgeObjectId> ids;
    ids.reserve(kNodeCount);

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kNodeCount; ++i) {
        auto node = KnowledgeObject::create("Concept " + std::to_string(i));
        REQUIRE(node.hasValue());
        ids.push_back(node.value().id());
        REQUIRE(graph.addNode(std::move(node).value()).hasValue());
    }
    auto afterNodes = std::chrono::steady_clock::now();

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> pick(0, kNodeCount - 1);
    int edgesAdded = 0;
    for (int i = 0; i < kEdgeAttempts; ++i) {
        int sourceIdx = pick(rng);
        int targetIdx = pick(rng);
        if (sourceIdx == targetIdx) continue;
        auto edge = Relationship::create(ids[sourceIdx], ids[targetIdx], RelationshipType::DependsOn);
        if (!edge.hasValue()) continue;
        if (graph.addEdge(std::move(edge).value()).hasValue()) ++edgesAdded;
    }
    auto afterEdges = std::chrono::steady_clock::now();

    auto deps = graph.transitiveDependencies(ids[0]);
    auto afterTraversal = std::chrono::steady_clock::now();

    auto topoResult = graph.topologicalOrder();
    auto afterTopoSort = std::chrono::steady_clock::now();

    auto searchResults = graph.search("Concept 1");
    auto afterSearch = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };

    std::cout << "[scale] nodes=" << graph.nodeCount() << " edges=" << graph.edgeCount() << "\n"
              << "[scale] insert nodes:     " << ms(start, afterNodes) << " ms\n"
              << "[scale] insert edges:     " << ms(afterNodes, afterEdges) << " ms\n"
              << "[scale] transitiveDeps:   " << ms(afterEdges, afterTraversal) << " ms"
              << " (found " << deps.size() << ")\n"
              << "[scale] topologicalOrder: " << ms(afterTraversal, afterTopoSort) << " ms"
              << " (cycle detected: " << (!topoResult.hasValue()) << ")\n"
              << "[scale] search:           " << ms(afterTopoSort, afterSearch) << " ms"
              << " (found " << searchResults.size() << ")\n";

    CHECK(graph.nodeCount() == static_cast<size_t>(kNodeCount));
    CHECK(edgesAdded > 0);

    CHECK(ms(start, afterEdges) < 5000);
    CHECK(ms(afterEdges, afterTraversal) < 2000);
    CHECK(ms(afterTraversal, afterTopoSort) < 2000);
    CHECK(ms(afterTopoSort, afterSearch) < 1000);
}
