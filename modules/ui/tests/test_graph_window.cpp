#include "atlas/persistence/database.hpp"
#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/ui/graph_window.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::ui;

TEST_CASE("GraphWindow constructs, loads the QML canvas, and finds it by objectName") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    GraphWindow window(controller);
    // If QML loading or type registration had failed, refreshGraph()
    // (called from the constructor) would have found no canvas and
    // silently no-op'd rather than crashed — this checks the happier
    // path actually happened, not just "didn't crash."
    auto* canvas = window.canvasItem();
    CHECK(canvas != nullptr);
}

TEST_CASE("creating and removing KnowledgeObjects through the controller doesn't crash the graph view") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    GraphWindow window(controller);

    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();
    REQUIRE(controller.removeKnowledgeObject(a).hasValue());

    // No CHECK beyond reaching this line without crashing/asserting —
    // this exercises refreshGraph() -> ForceDirectedLayout::compute()
    // -> GraphCanvasItem::setGraphData() end to end, including the
    // Qt Quick scene graph node rebuild path, under real (if synthetic
    // and tiny) graph mutations.
    (void)b;
    CHECK(true);
}
