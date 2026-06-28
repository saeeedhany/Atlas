#include "atlas/ui/graph_window.hpp"

#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "atlas/render/force_directed_layout.hpp"
#include "atlas/render/graph_canvas_item.hpp"

namespace atlas::ui {

namespace {

using atlas::core::Difficulty;

// Color-codes nodes by intrinsic Difficulty — cheap, immediately
// useful visual mapping that reinforces the domain model rather than
// being arbitrary decoration. A real legend/styling pass is later work.
QColor colorForDifficulty(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Beginner: return QColor(100, 200, 120);
        case Difficulty::Intermediate: return QColor(100, 160, 220);
        case Difficulty::Advanced: return QColor(230, 170, 60);
        case Difficulty::Expert: return QColor(220, 90, 90);
    }
    return QColor(150, 150, 150);
}

}  // namespace

GraphWindow::GraphWindow(WorkspaceController& controller, QWidget* parent)
    : QWidget(parent), controller_(&controller) {
    static bool typeRegistered = false;
    if (!typeRegistered) {
        atlas::render::registerGraphCanvasQmlType();
        typeRegistered = true;
    }

    setWindowTitle("Atlas - Graph View");

    quickWidget_ = new QQuickWidget(this);
    quickWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
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

    resize(800, 600);
    refreshGraph();
}

atlas::render::GraphCanvasItem* GraphWindow::canvasItem() const {
    if (quickWidget_->rootObject() == nullptr) return nullptr;
    return quickWidget_->rootObject()->findChild<atlas::render::GraphCanvasItem*>("graphCanvas");
}

void GraphWindow::refreshGraph() {
    const auto& graph = controller_->graph();
    auto positions = atlas::render::ForceDirectedLayout::compute(graph);

    std::vector<atlas::render::RenderNode> renderNodes;
    for (const auto& id : graph.allNodeIds()) {
        const auto* object = graph.findNode(id);
        auto posIt = positions.find(id);
        if (object == nullptr || posIt == positions.end()) continue;

        atlas::render::RenderNode node;
        node.id = QString::fromStdString(id.toString());
        node.x = posIt->second.x;
        node.y = posIt->second.y;
        node.color = colorForDifficulty(object->difficulty());
        renderNodes.push_back(node);
    }

    std::vector<atlas::render::RenderEdge> renderEdges;
    for (const auto& id : graph.allNodeIds()) {
        auto sourcePos = positions.find(id);
        if (sourcePos == positions.end()) continue;
        for (const auto& neighborId :
             graph.neighbors(id, std::nullopt, atlas::graph::GraphEngine::Direction::Outgoing)) {
            auto targetPos = positions.find(neighborId);
            if (targetPos == positions.end()) continue;
            renderEdges.push_back(atlas::render::RenderEdge{
                sourcePos->second.x, sourcePos->second.y, targetPos->second.x,
                targetPos->second.y});
        }
    }

    auto* canvas = canvasItem();
    if (canvas != nullptr) {
        canvas->setGraphData(std::move(renderNodes), std::move(renderEdges));
    }
}

}  // namespace atlas::ui
