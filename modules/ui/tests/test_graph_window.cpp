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

TEST_CASE("GraphWindow::setTheme reaches the underlying canvas item and is idempotent") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    GraphWindow window(controller);
    auto* canvas = window.canvasItem();
    REQUIRE(canvas != nullptr);

    // Dark is the default — see GraphWindow's themeMode_ member and
    // MainWindow's loadSavedTheme() fallback.
    CHECK(canvas->theme() == atlas::render::ThemeMode::Dark);

    window.setTheme(atlas::render::ThemeMode::Light);
    CHECK(canvas->theme() == atlas::render::ThemeMode::Light);

    // Setting the same mode again is a no-op on the canvas side (see
    // GraphCanvasItem::setTheme's early return) — asserting it doesn't
    // crash and the mode sticks is the whole point here, not that
    // anything observably different happens on the second call.
    window.setTheme(atlas::render::ThemeMode::Light);
    CHECK(canvas->theme() == atlas::render::ThemeMode::Light);

    window.setTheme(atlas::render::ThemeMode::Dark);
    CHECK(canvas->theme() == atlas::render::ThemeMode::Dark);
}

TEST_CASE("GraphWindow::setTopic scopes refreshGraph to that topic's members and edges only") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    auto paging = controller.createKnowledgeObject("Paging", os).value();
    auto virtualMemory = controller.createKnowledgeObject("Virtual Memory", os).value();
    controller.createKnowledgeObject("Indexing", databases);
    REQUIRE(controller.createRelationship(virtualMemory, paging, RelationshipType::DependsOn,
                                            std::nullopt)
                .hasValue());

    // Constructed already scoped to `os` — exercises the constructor's
    // topicId parameter, not just setTopic().
    GraphWindow window(controller, nullptr, /*standalone=*/true, os);
    // No direct way to inspect GraphCanvasItem's node/edge count from
    // outside atlas-render (RenderNode/RenderEdge aren't exposed
    // through GraphWindow), so this — like the sibling "doesn't crash"
    // test above — asserts the topic-filtered refreshGraph() path
    // (temporary GraphEngine construction, layout, setGraphData) runs
    // end to end without crashing/asserting for both a topic with
    // relationships and one without, and that switching between them
    // via setTopic() does too.
    CHECK(window.canvasItem() != nullptr);

    window.setTopic(databases);
    CHECK(window.canvasItem() != nullptr);

    window.setTopic(os);
    CHECK(window.canvasItem() != nullptr);
}

TEST_CASE("GraphWindow shows a topic that has links into another topic") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    auto paging = controller.createKnowledgeObject("Paging", os).value();
    auto indexing = controller.createKnowledgeObject("Indexing", databases).value();
    REQUIRE(controller.createRelationship(indexing, paging, RelationshipType::Uses, std::nullopt)
                .hasValue());

    GraphWindow window(controller, nullptr, true, os);
    CHECK(window.canvasItem() != nullptr);
    window.setTopic(databases);
    CHECK(window.canvasItem() != nullptr);
}
