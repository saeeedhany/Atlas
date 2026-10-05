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
}

TEST_CASE("layout timing at 10,000 nodes / ~30,000 edges, default 50 iterations" * doctest::test_suite("scale")) {
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
    auto positions = ForceDirectedLayout::compute(graph);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "[layout-scale] nodes=" << graph.nodeCount() << " edges=" << graph.edgeCount()
              << " iterations=50 time=" << ms << " ms"
              << " (" << (ms / 50.0) << " ms/iteration)\n";

    CHECK(positions.size() == static_cast<size_t>(kNodeCount));
}
