// Determines a real engineering question empirically, the same way
// M2's scale test did: is naive O(n^2) repulsion fast enough at
// 10,000 nodes for a one-time layout computation (not continuous
// simulation — see M4 design notes), or does this milestone need
// Barnes-Hut / grid-bucketing before it can be called done?
//
// No hardcoded pass/fail threshold here on purpose. This test reports
// the real numbers; the decision about whether they're acceptable
// belongs in the response that reads them, not buried in an assertion.

#include <chrono>
#include <iostream>
#include <random>

#include "atlas/render/force_directed_layout.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::graph;
using namespace atlas::render;

namespace {
constexpr int kNodeCount = 10'000;
constexpr int kEdgeAttempts = 30'000;
}  // namespace

TEST_CASE("layout timing at 10,000 nodes / ~30,000 edges, default 50 iterations") {
    GraphEngine graph;
    std::vector<KnowledgeObjectId> ids;
    ids.reserve(kNodeCount);
    for (int i = 0; i < kNodeCount; ++i) {
        auto node = KnowledgeObject::create("Concept " + std::to_string(i));
        REQUIRE(node.hasValue());
        ids.push_back(node.value().id());
        REQUIRE(graph.addNode(std::move(node).value()).hasValue());
    }

    std::mt19937 rng(7);
    std::uniform_int_distribution<int> pick(0, kNodeCount - 1);
    for (int i = 0; i < kEdgeAttempts; ++i) {
        int s = pick(rng);
        int t = pick(rng);
        if (s == t) continue;
        auto edge = Relationship::create(ids[s], ids[t], RelationshipType::DependsOn);
        if (edge.hasValue()) graph.addEdge(std::move(edge).value());
    }

    auto start = std::chrono::steady_clock::now();
    auto positions = ForceDirectedLayout::compute(graph);  // default config: 50 iterations
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "[layout-scale] nodes=" << graph.nodeCount() << " edges=" << graph.edgeCount()
              << " iterations=50 time=" << ms << " ms"
              << " (" << (ms / 50.0) << " ms/iteration)\n";

    CHECK(positions.size() == static_cast<size_t>(kNodeCount));
}
