#include <cmath>

#include "atlas/render/force_directed_layout.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::graph;
using namespace atlas::render;

namespace {

KnowledgeObject makeNode(const char* title) { return KnowledgeObject::create(title).value(); }

double distance(const Point2D& a, const Point2D& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

TEST_CASE("compute on an empty graph returns an empty map") {
    GraphEngine graph;
    auto positions = ForceDirectedLayout::compute(graph);
    CHECK(positions.empty());
}

TEST_CASE("every live node receives a position") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());

    auto positions = ForceDirectedLayout::compute(graph);
    CHECK(positions.size() == 2);
    CHECK(positions.count(aId) == 1);
    CHECK(positions.count(bId) == 1);
}

TEST_CASE("positions are finite — no NaN/inf from a degenerate single-node graph") {
    GraphEngine graph;
    auto a = makeNode("Solo");
    auto aId = a.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());

    auto positions = ForceDirectedLayout::compute(graph);
    REQUIRE(positions.count(aId) == 1);
    CHECK(std::isfinite(positions[aId].x));
    CHECK(std::isfinite(positions[aId].y));
}

TEST_CASE("the same graph and config produce identical positions across calls") {
    // Determinism matters: a layout that shuffles every time you open
    // the app (or every time you add an unrelated node) would make the
    // graph feel unstable to navigate, even if nothing the user cares
    // about changed.
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto aId = a.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());

    auto first = ForceDirectedLayout::compute(graph);
    auto second = ForceDirectedLayout::compute(graph);

    CHECK(first[aId].x == second[aId].x);
    CHECK(first[aId].y == second[aId].y);
}

TEST_CASE("connected nodes end up closer together than an unrelated distant pair, on average") {
    // Not a precise geometric claim (force-directed layout has no
    // single "correct" answer) — just the qualitative property the
    // algorithm exists to provide: a tight cluster connected by edges,
    // and a lone unconnected node, should not end up equidistant.
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto isolated = makeNode("Isolated");
    auto aId = a.id();
    auto bId = b.id();
    auto cId = c.id();
    auto isolatedId = isolated.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.addNode(std::move(isolated)).hasValue());

    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::RelatedTo).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(bId, cId, RelationshipType::RelatedTo).value())
                .hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, cId, RelationshipType::RelatedTo).value())
                .hasValue());

    LayoutConfig config;
    config.iterations = 200;  // let it fully settle for this assertion
    auto positions = ForceDirectedLayout::compute(graph, config);

    double withinCluster = distance(positions[aId], positions[bId]);
    double toIsolated = distance(positions[aId], positions[isolatedId]);
    CHECK(withinCluster < toIsolated);
}

TEST_CASE("layout is reasonably stable when one unrelated node is added") {
    // A weaker, more honest claim than "identical positions": adding
    // an unrelated fourth node shouldn't make the original triangle
    // collapse or fly apart — the existing cluster's *shape* should
    // survive roughly intact even though exact coordinates shift.
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto c = makeNode("C");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addNode(std::move(c)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::RelatedTo).value())
                .hasValue());

    LayoutConfig config;
    config.iterations = 200;
    auto before = ForceDirectedLayout::compute(graph, config);
    double beforeDistance = distance(before[aId], before[bId]);

    auto d = makeNode("D");
    REQUIRE(graph.addNode(std::move(d)).hasValue());
    auto after = ForceDirectedLayout::compute(graph, config);
    double afterDistance = distance(after[aId], after[bId]);

    // Same order of magnitude — not asserting exact equality, since
    // the whole point of recomputing is that it's allowed to change.
    CHECK(afterDistance < beforeDistance * 3.0);
}
