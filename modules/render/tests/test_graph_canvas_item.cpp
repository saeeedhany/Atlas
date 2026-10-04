#include "atlas/render/graph_canvas_item.hpp"
#include "doctest.h"

using namespace atlas::render;

namespace {

RenderNode makeNode(const char* id, double x, double y, const char* group, double recall = -1.0, bool ghost = false) {
    RenderNode node;
    node.id = id;
    node.x = x;
    node.y = y;
    node.label = id;
    node.recall = recall;
    node.ghost = ghost;
    node.groupKey = group;
    node.groupLabel = QStringLiteral("Topic %1").arg(group);
    return node;
}

}  // namespace

TEST_CASE("long labels are shortened with an ellipsis") {
    CHECK(elideLabel("Tree") == "Tree");
    QString shortened = elideLabel(QString(40, QChar('x')));
    CHECK(shortened.size() == 28);
    CHECK(shortened.endsWith(QChar(0x2026)));
}

TEST_CASE("groups summarize members, skip ghosts, and average learned recall only") {
    std::vector<RenderNode> nodes{makeNode("a", 0, 0, "t1", 0.9), makeNode("b", 10, 0, "t1"),
                                  makeNode("c", 100, 100, "t2", 0.4), makeNode("g", 500, 500, "t1", 1.0, true)};
    std::unordered_map<QString, QPointF> positions{{"a", {0, 0}}, {"b", {10, 0}}, {"c", {100, 100}}, {"g", {500, 500}}};
    auto groups = summarizeGroups(nodes, positions);
    REQUIRE(groups.size() == 2);
    CHECK(groups[0].key == "t1");
    CHECK(groups[0].label == "Topic t1");
    CHECK(groups[0].memberCount == 2);
    CHECK(groups[0].center == QPointF(5, 0));
    CHECK(groups[0].meanRecall == doctest::Approx(0.9));
    CHECK(groups[1].meanRecall == doctest::Approx(0.4));
}

TEST_CASE("a group with no learned members has no recall and falls back to node coordinates") {
    auto groups = summarizeGroups({makeNode("a", 3, 4, "t1")}, {});
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].meanRecall < 0.0);
    CHECK(groups[0].center == QPointF(3, 4));
}

TEST_CASE("zoom is clamped and collapses below the threshold") {
    GraphCanvasItem canvas;
    int changes = 0;
    QObject::connect(&canvas, &GraphCanvasItem::zoomChanged, [&] { ++changes; });
    CHECK_FALSE(canvas.collapsed());
    canvas.setZoom(0.2);
    CHECK(canvas.collapsed());
    canvas.setZoom(100.0);
    CHECK(canvas.zoom() == doctest::Approx(10.0));
    canvas.zoomBy(0.5);
    CHECK(canvas.zoom() == doctest::Approx(5.0));
    CHECK(changes == 3);
}

TEST_CASE("graph data keeps memory, ghost, and link style fields") {
    GraphCanvasItem canvas;
    RenderEdge edge;
    edge.sourceId = "a";
    edge.targetId = "b";
    edge.directed = false;
    edge.contrast = true;
    edge.ghost = true;
    canvas.setGraphData({makeNode("a", 0, 0, "t1", 0.7), makeNode("b", 1, 1, "t1", -1.0, true)}, {edge});
    REQUIRE(canvas.nodes().size() == 2);
    CHECK(canvas.nodes()[0].recall == doctest::Approx(0.7));
    CHECK(canvas.nodes()[1].ghost);
    CHECK(canvas.edges()[0].contrast);
    CHECK_FALSE(canvas.edges()[0].directed);
}

TEST_CASE("groupAt finds the collapsed topic under the cursor") {
    GraphCanvasItem canvas;
    canvas.setGraphData({makeNode("a", 100, 100, "t1"), makeNode("b", 100, 100, "t1")}, {});
    CHECK(canvas.groupAt(20, 20).isEmpty());
    canvas.setZoom(0.2);
    CHECK(canvas.groupAt(22, 20) == "t1");
    CHECK(canvas.groupAt(200, 200).isEmpty());
}
