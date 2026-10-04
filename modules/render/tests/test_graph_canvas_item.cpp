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
    canvas.setGraphData({makeNode("a", 0, 0, "t1")}, {});
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

TEST_CASE("without group keys the canvas never collapses") {
    GraphCanvasItem canvas;
    canvas.setGraphData({makeNode("a", 100, 100, ""), makeNode("b", 100, 100, "")}, {});
    canvas.setZoom(0.2);
    CHECK_FALSE(canvas.collapsed());
    CHECK(canvas.groupAt(22, 20).isEmpty());
}

TEST_CASE("label layouts are reused across scene updates and rebuilt only when their text changes") {
    GraphCanvasItem canvas;
    canvas.setGraphData({makeNode("a", 0, 0, "t1"), makeNode("b", 10, 0, "t1")}, {});
    const QTextLayout* first = canvas.labelLayoutFor("a");
    const QTextLayout* second = canvas.labelLayoutFor("b");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    canvas.setGraphData({makeNode("a", 5, 5, "t1"), makeNode("b", 10, 0, "t1")}, {});
    CHECK(canvas.labelLayoutFor("a") == first);
    CHECK(canvas.labelLayoutFor("b") == second);

    auto renamed = makeNode("b", 10, 0, "t1");
    renamed.label = "Renamed";
    canvas.setGraphData({makeNode("a", 5, 5, "t1"), renamed}, {});
    CHECK(canvas.labelLayoutFor("a") == first);
    REQUIRE(canvas.labelLayoutFor("b") != nullptr);
    CHECK(canvas.labelLayoutFor("b") != second);
    CHECK(canvas.labelLayoutFor("b")->text() == "Renamed");

    canvas.setGraphData({makeNode("a", 5, 5, "t1")}, {});
    CHECK(canvas.labelLayoutFor("b") == nullptr);
}

TEST_CASE("centerOn puts the node in the middle of the view") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", 0, 0, ""), makeNode("b", 500, 300, "")}, {});
    canvas.centerOn("b");
    QPointF screen = canvas.screenPositionOf("b").toPointF();
    CHECK(screen.x() == doctest::Approx(400.0));
    CHECK(screen.y() == doctest::Approx(300.0));
    CHECK_FALSE(canvas.screenPositionOf("missing").isValid());
}

TEST_CASE("without animation a moved node jumps straight to its new position") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", 0, 0, ""), makeNode("b", 100, 0, "")}, {});
    QPointF before = canvas.screenPositionOf("b").toPointF();
    canvas.setGraphData({makeNode("a", 0, 0, ""), makeNode("b", 300, 50, "")}, {});
    QPointF after = canvas.screenPositionOf("b").toPointF();
    CHECK(after.x() - before.x() == doctest::Approx(200.0 * canvas.zoom()));
    CHECK(after.y() - before.y() == doctest::Approx(50.0 * canvas.zoom()));
}

TEST_CASE("fitToContent shows every node") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", -1000, -500, ""), makeNode("b", 1000, 500, "")}, {});
    canvas.fitToContent();
    for (const char* id : {"a", "b"}) {
        QPointF screen = canvas.screenPositionOf(id).toPointF();
        CHECK(screen.x() >= 0.0);
        CHECK(screen.x() <= 800.0);
        CHECK(screen.y() >= 0.0);
        CHECK(screen.y() <= 600.0);
    }
}

TEST_CASE("zoomAt keeps the point under the cursor fixed") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", 100, 100, "")}, {});
    QPointF before = canvas.screenPositionOf("a").toPointF();
    canvas.zoomAt(2.0, before.x(), before.y());
    QPointF after = canvas.screenPositionOf("a").toPointF();
    CHECK(after.x() == doctest::Approx(before.x()));
    CHECK(after.y() == doctest::Approx(before.y()));
    CHECK(canvas.zoom() > 1.0);
}

TEST_CASE("linkAt finds the link under the cursor") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    RenderEdge edge;
    edge.id = "link";
    edge.sourceId = "a";
    edge.targetId = "b";
    canvas.setGraphData({makeNode("a", 0, 0, ""), makeNode("b", 400, 0, "")}, {edge});
    canvas.centerOn("a");
    QPointF a = canvas.screenPositionOf("a").toPointF();
    QPointF b = canvas.screenPositionOf("b").toPointF();
    QPointF middle = (a + b) / 2.0;
    CHECK(canvas.linkAt(middle.x(), middle.y() + 3) == "link");
    CHECK(canvas.linkAt(middle.x(), middle.y() + 40).isEmpty());
}

TEST_CASE("resizing the canvas reports a view change so culled geometry is rebuilt") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(200, 150));
    int changes = 0;
    QObject::connect(&canvas, &GraphCanvasItem::viewChanged, [&changes] { ++changes; });
    canvas.setSize(QSizeF(1600, 1200));
    CHECK(changes > 0);
}

TEST_CASE("hidden links are neither drawn targets nor hoverable") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    RenderEdge edge;
    edge.id = "link";
    edge.sourceId = "a";
    edge.targetId = "b";
    edge.mark = EdgeMark::Hidden;
    canvas.setGraphData({makeNode("a", 0, 0, ""), makeNode("b", 400, 0, "")}, {edge});
    canvas.centerOn("a");
    QPointF middle = (canvas.screenPositionOf("a").toPointF() + canvas.screenPositionOf("b").toPointF()) / 2.0;
    CHECK(canvas.linkAt(middle.x(), middle.y()).isEmpty());
    CHECK(canvas.edges()[0].mark == EdgeMark::Hidden);
}

TEST_CASE("memory rings ease to new values unless motion is reduced") {
    GraphCanvasItem canvas;
    canvas.setGraphData({makeNode("a", 0, 0, "", 0.2)}, {});
    CHECK(canvas.shownRecallOf("a") == doctest::Approx(0.2));
    canvas.setGraphData({makeNode("a", 0, 0, "", 0.9)}, {});
    CHECK(canvas.shownRecallOf("a") == doctest::Approx(0.2));

    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", 0, 0, "", 0.5)}, {});
    CHECK(canvas.shownRecallOf("a") == doctest::Approx(0.5));
    CHECK(canvas.shownRecallOf("missing") == -1.0);
}
