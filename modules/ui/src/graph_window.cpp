#include "atlas/ui/graph_window.hpp"

#include <QAction>
#include <QCursor>
#include <QMenu>
#include <QMessageBox>
#include <QQuickWidget>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <unordered_set>

#include "atlas/core/enums.hpp"
#include "atlas/core/uuid.hpp"
#include "atlas/render/force_directed_layout.hpp"
#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/ui/roadmap_dialog.hpp"

namespace atlas::ui {

namespace {

using atlas::core::Difficulty;

// Color-codes nodes by intrinsic Difficulty — cheap, immediately
// useful visual mapping that reinforces the domain model rather than
// being arbitrary decoration. A real legend/styling pass is later work.
// The four colors themselves live in atlas::render::Theme (tuned
// separately per light/dark for contrast) — this function just picks
// the right index; it has no color literals of its own anymore.
QColor colorForDifficulty(Difficulty difficulty, const atlas::render::Theme& theme) {
    switch (difficulty) {
        case Difficulty::Beginner: return theme.nodeDifficulty[0];
        case Difficulty::Intermediate: return theme.nodeDifficulty[1];
        case Difficulty::Advanced: return theme.nodeDifficulty[2];
        case Difficulty::Expert: return theme.nodeDifficulty[3];
    }
    return QColor(150, 150, 150);
}

}  // namespace

GraphWindow::GraphWindow(WorkspaceController& controller, QWidget* parent, bool standalone,
                          atlas::core::TopicId topicId)
    // Qt::Window forces a real top-level window despite having a parent.
    // When embedded (standalone=false), we pass no extra flag so Qt
    // keeps this as a regular child widget inside the parent's layout
    // (the splitter in MainWindow). When standalone=true (the default),
    // Qt::Window ensures it opens as a real separate window.
    : QWidget(parent, standalone ? Qt::Window : Qt::Widget),
      controller_(&controller),
      topicId_(topicId) {
    static bool typeRegistered = false;
    if (!typeRegistered) {
        atlas::render::registerGraphCanvasQmlType();
        typeRegistered = true;
    }

    setWindowTitle("Atlas - Graph View");

    quickWidget_ = new QQuickWidget(this);
    quickWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget_->setClearColor(atlas::render::themeFor(themeMode_).background);
    quickWidget_->setSource(
        QUrl::fromLocalFile(QString(ATLAS_RENDER_QML_DIR) + "/GraphView.qml"));
    if (quickWidget_->status() == QQuickWidget::Error) {
        for (const auto& error : quickWidget_->errors()) {
            qWarning() << "GraphWindow: QML load error:" << error.toString();
        }
    }

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(quickWidget_);

    connect(controller_, &WorkspaceController::graphChanged, this, &GraphWindow::refreshGraph);
    // nodeClicked is connected after QML loads, in refreshGraph(), once
    // the canvas item actually exists.

    resize(800, 600);

    // Best-effort: centers over the parent when the window manager
    // honors requested geometry (floating mode). A tiling WM may
    // override this entirely, which is fine — this just avoids
    // multiple utility windows spawning stacked at an identical
    // default position when it doesn't.
    if (parent != nullptr) {
        move(parent->geometry().center() - rect().center());
    }

    refreshGraph();
}

void GraphWindow::onNodeRightClicked(const QString& id) {
    auto parsed = atlas::core::Uuid::parse(id.toStdString());
    if (!parsed.has_value()) return;
    atlas::core::KnowledgeObjectId nodeId(*parsed);

    auto object = controller_->findKnowledgeObject(nodeId);
    if (!object.has_value()) return;

    QMenu menu(this);
    QAction* roadmapAction = menu.addAction(
        QString("Generate roadmap to \"%1\"").arg(QString::fromStdString(object->title())));

    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen != roadmapAction) return;

    auto roadmapResult = controller_->roadmapFor(nodeId);
    if (!roadmapResult.hasValue()) {
        QString message = (roadmapResult.error().code ==
                            WorkspaceController::RoadmapErrorCode::CycleDetected)
                               ? "This concept's dependencies contain a cycle, so no valid "
                                 "learning order exists. Check for a DependsOn loop."
                               : "That concept no longer exists.";
        QMessageBox::warning(this, "Atlas", message);
        return;
    }

    RoadmapDialog dialog(QString::fromStdString(object->title()), roadmapResult.value(), this);
    dialog.exec();
}

void GraphWindow::setTheme(atlas::render::ThemeMode mode) {
    if (themeMode_ == mode) return;
    themeMode_ = mode;
    quickWidget_->setClearColor(atlas::render::themeFor(themeMode_).background);
    // refreshGraph() both retints node colors (colorForDifficulty
    // depends on themeMode_) and pushes the new theme into the canvas
    // item for dots/edges/selection rings — one call covers both, same
    // "single refresh path" reasoning as WorkspaceController::graphChanged.
    refreshGraph();
}

void GraphWindow::setTopic(atlas::core::TopicId topicId) {
    if (topicId_ == topicId) return;
    topicId_ = topicId;
    selectedNodeId_.clear();  // a selection from the old topic means nothing in the new one
    refreshGraph();
}

void GraphWindow::setSelectedKnowledgeObject(
    std::optional<atlas::core::KnowledgeObjectId> id) {
    if (id.has_value()) {
        selectedNodeId_ = QString::fromStdString(id->toString());
    } else {
        selectedNodeId_.clear();
    }
    updateHighlight();
}

void GraphWindow::onNodeClicked(const QString& id) {
    selectedNodeId_ = id;
    updateHighlight();
}

void GraphWindow::onNodeHovered(const QString& id) {
    if (id.isEmpty()) {
        QToolTip::hideText();
        return;
    }

    auto parsed = atlas::core::Uuid::parse(id.toStdString());
    if (!parsed.has_value()) return;
    auto object = controller_->findKnowledgeObject(atlas::core::KnowledgeObjectId(*parsed));
    if (!object.has_value()) return;

    // Rich text: bold title, then a couple of the fields most useful
    // for "should I click into this" at a glance — difficulty and
    // confidence (the two axes that actually distinguish concepts,
    // per atlas-core's design notes), plus the start of the
    // definition if there is one. Deliberately not the full
    // definition/notes/examples — a tooltip that's a wall of text
    // defeats the point of a quick glance.
    QString text = QString("<b>%1</b>").arg(QString::fromStdString(object->title()).toHtmlEscaped());
    text += QString("<br>%1 &middot; %2")
                .arg(QString::fromStdString(
                         std::string(atlas::core::toDisplayString(object->difficulty()))),
                     QString::fromStdString(
                         std::string(atlas::core::toDisplayString(object->confidence()))));
    if (!object->definition().empty()) {
        QString definition = QString::fromStdString(object->definition());
        constexpr int kMaxLength = 140;
        if (definition.length() > kMaxLength) {
            definition = definition.left(kMaxLength).trimmed() + "...";
        }
        text += QString("<br><i>%1</i>").arg(definition.toHtmlEscaped());
    }

    // QToolTip::showText, not this widget's own setToolTip(): the
    // tooltip's content changes per-node while the mouse never leaves
    // this one QQuickWidget, so a static per-widget tooltip set once
    // wouldn't update as the cursor moves between nodes. showText()
    // lets each hover push fresh content at the current cursor position.
    QToolTip::showText(QCursor::pos(), text, quickWidget_);
}

void GraphWindow::updateHighlight() {
    auto* canvas = canvasItem();
    if (canvas == nullptr) return;

    if (selectedNodeId_.isEmpty()) {
        canvas->clearHighlight();
        return;
    }

    // Parse the clicked id back to a KnowledgeObjectId so we can
    // query GraphEngine for its neighborhood.
    auto parsed = atlas::core::Uuid::parse(selectedNodeId_.toStdString());
    if (!parsed.has_value()) {
        canvas->clearHighlight();
        return;
    }
    atlas::core::KnowledgeObjectId selectedId(*parsed);

    const auto& graph = controller_->graph();

    // Neighborhood = direct DependsOn neighbors in both directions +
    // direct Uses/Related/etc. neighbors. Using Direction::Both gives
    // us everyone immediately connected, regardless of edge direction
    // or type — the most immediately useful view.
    auto directNeighbors = graph.neighbors(selectedId, std::nullopt,
                                             atlas::graph::GraphEngine::Direction::Both);

    std::unordered_set<QString> neighborSet;
    for (const auto& nid : directNeighbors) {
        neighborSet.insert(QString::fromStdString(nid.toString()));
    }

    canvas->setHighlight(selectedNodeId_, neighborSet);
}

atlas::render::GraphCanvasItem* GraphWindow::canvasItem() const {
    if (quickWidget_->rootObject() == nullptr) return nullptr;
    return quickWidget_->rootObject()->findChild<atlas::render::GraphCanvasItem*>("graphCanvas");
}

void GraphWindow::refreshGraph() {
    // A temporary, topic-scoped GraphEngine, built fresh each refresh —
    // not a filter applied after computing layout on the whole
    // workspace. Two things fall out of that for free: (1) force-
    // directed physics never lets an off-screen, different-topic node
    // pull this topic's layout around, and (2) since
    // WorkspaceController::createRelationship rejects any cross-topic
    // edge, every relationship touching a member of this topic is
    // guaranteed to have both endpoints inside it too — so "keep every
    // relationship whose source is a member" below never has to also
    // check the target. GraphEngine itself stays completely topic-
    // agnostic (see docs/DECISIONS.md's reasoning for why graph
    // structure and display/scoping concerns are kept apart) — this
    // construction lives here, in the UI layer, not in atlas-graph.
    atlas::graph::GraphEngine topicGraph;
    for (auto& object : controller_->knowledgeObjectsInTopic(topicId_)) {
        topicGraph.addNode(std::move(object));
    }
    for (auto& relationship : controller_->allRelationships()) {
        if (topicGraph.findNode(relationship.sourceId()) != nullptr) {
            topicGraph.addEdge(std::move(relationship));
        }
    }

    auto positions = atlas::render::ForceDirectedLayout::compute(topicGraph);
    const auto& theme = atlas::render::themeFor(themeMode_);

    std::vector<atlas::render::RenderNode> renderNodes;
    for (const auto& id : topicGraph.allNodeIds()) {
        const auto* object = topicGraph.findNode(id);
        auto posIt = positions.find(id);
        if (object == nullptr || posIt == positions.end()) continue;

        atlas::render::RenderNode node;
        node.id = QString::fromStdString(id.toString());
        node.x = posIt->second.x;
        node.y = posIt->second.y;
        node.color = colorForDifficulty(object->difficulty(), theme);
        renderNodes.push_back(node);
    }

    std::vector<atlas::render::RenderEdge> renderEdges;
    for (const auto& id : topicGraph.allNodeIds()) {
        if (positions.find(id) == positions.end()) continue;
        for (const auto& neighborId : topicGraph.neighbors(
                 id, std::nullopt, atlas::graph::GraphEngine::Direction::Outgoing)) {
            if (positions.find(neighborId) == positions.end()) continue;
            renderEdges.push_back(atlas::render::RenderEdge{
                QString::fromStdString(id.toString()),
                QString::fromStdString(neighborId.toString())});
        }
    }

    auto* canvas = canvasItem();
    if (canvas != nullptr) {
        // Wire up once — connect is idempotent across repeated
        // refreshGraph() calls because Qt deduplicates identical
        // signal/slot connections on the same pair of objects.
        connect(canvas, &atlas::render::GraphCanvasItem::nodeClicked,
                this, &GraphWindow::onNodeClicked, Qt::UniqueConnection);
        connect(canvas, &atlas::render::GraphCanvasItem::nodeRightClicked,
                this, &GraphWindow::onNodeRightClicked, Qt::UniqueConnection);
        connect(canvas, &atlas::render::GraphCanvasItem::nodeHovered,
                this, &GraphWindow::onNodeHovered, Qt::UniqueConnection);
        canvas->setTheme(themeMode_);
        canvas->setGraphData(std::move(renderNodes), std::move(renderEdges));
        // Re-apply the current selection after the data rebuild so
        // the highlight survives a graph structural change.
        updateHighlight();
    }
}

}  // namespace atlas::ui
