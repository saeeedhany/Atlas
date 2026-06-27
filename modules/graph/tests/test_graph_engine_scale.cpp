// This is a smoke test against catastrophic algorithmic mistakes (an
// accidental O(n^2) or O(n^3) somewhere), not a tuned performance
// benchmark — that's a separate concern for later, with Release builds
// and a real methodology. The time bounds here are deliberately
// generous: this runs in a Debug build with no optimization, and the
// CI/sandbox machine running it is an unknown quantity. The point is
// "does this finish in a reasonable multiple of what it should," not
// "is this fast."
//
// Validates atlas-graph's data structures at the scale the original
// requirements call for (10,000+ nodes). Rendering performance at that
// scale is a separate, later concern (M4) — this only proves the
// in-memory index itself doesn't fall over.

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

}  // namespace

TEST_CASE("GraphEngine handles 10,000 nodes and ~30,000 edges without an algorithmic blowup") {
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

    std::mt19937 rng(42);  // fixed seed: deterministic, reproducible failures
    std::uniform_int_distribution<int> pick(0, kNodeCount - 1);
    int edgesAdded = 0;
    for (int i = 0; i < kEdgeAttempts; ++i) {
        int sourceIdx = pick(rng);
        int targetIdx = pick(rng);
        if (sourceIdx == targetIdx) continue;  // no self-loops
        auto edge = Relationship::create(ids[sourceIdx], ids[targetIdx], RelationshipType::DependsOn);
        if (!edge.hasValue()) continue;
        if (graph.addEdge(std::move(edge).value()).hasValue()) ++edgesAdded;
        // Duplicates are expected and silently skipped — random pairs
        // at this density will collide sometimes.
    }
    auto afterEdges = std::chrono::steady_clock::now();

    auto deps = graph.transitiveDependencies(ids[0]);
    auto afterTraversal = std::chrono::steady_clock::now();

    // The random edges almost certainly contain a cycle at this
    // density, so topologicalOrder() is expected to report one rather
    // than succeed — we're timing it, not asserting success.
    auto topoResult = graph.topologicalOrder();
    auto afterTopoSort = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };

    std::cout << "[scale] nodes=" << graph.nodeCount() << " edges=" << graph.edgeCount() << "\n"
              << "[scale] insert nodes:     " << ms(start, afterNodes) << " ms\n"
              << "[scale] insert edges:     " << ms(afterNodes, afterEdges) << " ms\n"
              << "[scale] transitiveDeps:   " << ms(afterEdges, afterTraversal) << " ms"
              << " (found " << deps.size() << ")\n"
              << "[scale] topologicalOrder: " << ms(afterTraversal, afterTopoSort) << " ms"
              << " (cycle detected: " << (!topoResult.hasValue()) << ")\n";

    CHECK(graph.nodeCount() == static_cast<size_t>(kNodeCount));
    CHECK(edgesAdded > 0);

    // Generous upper bounds — see file header. These exist to catch a
    // real regression (e.g. an accidental O(n^2) loop), not to assert
    // anything about real-world performance.
    CHECK(ms(start, afterEdges) < 5000);
    CHECK(ms(afterEdges, afterTraversal) < 2000);
    CHECK(ms(afterTraversal, afterTopoSort) < 2000);
}
