#pragma once

#include <QWidget>

#include <optional>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/topic.hpp"
#include "atlas/render/theme.hpp"
#include "atlas/ui/workspace_controller.hpp"

class QQuickWidget;

namespace atlas::render {
class GraphCanvasItem;
}  // namespace atlas::render

namespace atlas::ui {

// Hosts the Qt Quick graph canvas inside the Qt Widgets shell via
// QQuickWidget. Usable two ways:
//  - standalone = true (default): a real top-level window (Qt::Window),
//    used by atlas_app's earlier "Graph View" button and still by
//    tests that construct one in isolation.
//  - standalone = false: embedded as a plain child widget, used by
//    MainWindow's unified layout (list panel + canvas side by side in
//    a splitter). The canvas rendering/highlighting logic is identical
//    either way — only the top-level-window-ness differs.
class GraphWindow : public QWidget {
    Q_OBJECT

public:
    // topicId defaults to the Uncategorized bucket, trailing and
    // defaulted for the same reason KnowledgeObject::StorageRecord's
    // topicId is: this constructor had call sites (mostly tests)
    // before topics existed, and none of them need to change to keep
    // meaning what they already meant — "a graph view with no topic
    // scoping specified" now just means "scoped to Uncategorized,"
    // which for a pre-topics workspace is every object there is.
    explicit GraphWindow(WorkspaceController& controller, QWidget* parent = nullptr,
                          bool standalone = true,
                          atlas::core::TopicId topicId = atlas::core::uncategorizedTopicId());

    // The canvas item lives in the QML scene graph rooted at the
    // QQuickWidget's rootObject(), not in this QWidget's own child
    // hierarchy — QWidget::findChild() from GraphWindow itself will
    // never find it. This is the correct way to reach it.
    atlas::render::GraphCanvasItem* canvasItem() const;

public slots:
    // Drives the highlight from an external selection source (e.g.
    // KnowledgeObjectPanel's list selection), distinct from
    // onNodeClicked (driven by clicking the canvas itself). One-
    // directional: calling this never causes GraphWindow to emit
    // anything back toward whoever called it — there is no signal for
    // "I was told to select this." Avoids any list<->canvas selection
    // feedback loop by construction, not by convention.
    void setSelectedKnowledgeObject(std::optional<atlas::core::KnowledgeObjectId> id);

    // Applies to the canvas (background/dots/edges/selection, and
    // retinted node colors via colorForDifficulty) and to the
    // QQuickWidget's own clear color, which the canvas item has no
    // access to set itself. Safe to call before the QML has loaded —
    // the mode is remembered and applied as soon as refreshGraph()
    // next finds a real canvas item.
    void setTheme(atlas::render::ThemeMode mode);

    // Re-scopes this GraphWindow to a different topic in place, rather
    // than MainWindow constructing a fresh one per topic switch — lets
    // the topic-switch transition (see MainWindow) crossfade one
    // stable widget instead of tearing down and rebuilding the QML
    // canvas each time. Clears selection/highlight (a selected node in
    // the old topic has no meaning in the new one) and re-lays-out
    // from scratch — see refreshGraph()'s topic-filtered subgraph
    // construction for why a full relayout, not just a re-filter, is
    // correct here.
    void setTopic(atlas::core::TopicId topicId);

private slots:
    void refreshGraph();
    void onNodeClicked(const QString& id);
    void onNodeRightClicked(const QString& id);

private:
    void updateHighlight();

    WorkspaceController* controller_;
    QQuickWidget* quickWidget_;
    // The currently selected node's UUID string, or empty if none.
    // Stored here (not in canvasItem()) because refreshGraph() needs
    // to re-apply the highlight after rebuilding all render data.
    QString selectedNodeId_;

    atlas::render::ThemeMode themeMode_ = atlas::render::ThemeMode::Dark;
    atlas::core::TopicId topicId_;
};

}  // namespace atlas::ui
