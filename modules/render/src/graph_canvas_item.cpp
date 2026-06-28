#include "atlas/render/graph_canvas_item.hpp"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <QSGVertexColorMaterial>
#include <QWheelEvent>
#include <QtQml/qqml.h>

#include <algorithm>
#include <utility>

namespace atlas::render {

namespace {
constexpr float kNodeHalfSize = 6.0f;
constexpr double kMinScale = 0.05;
constexpr double kMaxScale = 10.0;
}  // namespace

GraphCanvasItem::GraphCanvasItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

void GraphCanvasItem::setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges) {
    nodes_ = std::move(nodes);
    edges_ = std::move(edges);
    dataDirty_ = true;
    update();
}

QSGNode* GraphCanvasItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* root = static_cast<QSGTransformNode*>(oldNode);
    QSGGeometryNode* edgesNode = nullptr;
    QSGGeometryNode* nodesNode = nullptr;

    if (root == nullptr) {
        root = new QSGTransformNode();

        edgesNode = new QSGGeometryNode();
        auto* edgeGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        edgeGeometry->setDrawingMode(QSGGeometry::DrawLines);
        edgesNode->setGeometry(edgeGeometry);
        edgesNode->setFlag(QSGNode::OwnsGeometry);
        auto* edgeMaterial = new QSGFlatColorMaterial();
        edgeMaterial->setColor(QColor(170, 170, 170));
        edgesNode->setMaterial(edgeMaterial);
        edgesNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(edgesNode);  // drawn first, so nodes render on top

        nodesNode = new QSGGeometryNode();
        auto* nodeGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        nodeGeometry->setDrawingMode(QSGGeometry::DrawTriangles);
        nodesNode->setGeometry(nodeGeometry);
        nodesNode->setFlag(QSGNode::OwnsGeometry);
        auto* nodeMaterial = new QSGVertexColorMaterial();
        nodesNode->setMaterial(nodeMaterial);
        nodesNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(nodesNode);
    } else {
        edgesNode = static_cast<QSGGeometryNode*>(root->firstChild());
        nodesNode = static_cast<QSGGeometryNode*>(edgesNode->nextSibling());
    }

    // Pan/zoom: only the transform matrix changes. No geometry rebuild
    // — this is the cheap path, taken every frame during a drag.
    QMatrix4x4 matrix;
    matrix.translate(static_cast<float>(offsetX_), static_cast<float>(offsetY_));
    matrix.scale(static_cast<float>(scale_));
    root->setMatrix(matrix);

    // Structural data change: rebuild both vertex buffers. Expensive
    // relative to a transform update, but only happens when
    // setGraphData() was called (graph edited, or layout recomputed) —
    // never during a plain pan/zoom.
    if (dataDirty_) {
        QSGGeometry* edgeGeometry = edgesNode->geometry();
        edgeGeometry->allocate(static_cast<int>(edges_.size()) * 2);
        QSGGeometry::Point2D* edgeVertices = edgeGeometry->vertexDataAsPoint2D();
        for (size_t i = 0; i < edges_.size(); ++i) {
            edgeVertices[i * 2].set(static_cast<float>(edges_[i].x1),
                                     static_cast<float>(edges_[i].y1));
            edgeVertices[i * 2 + 1].set(static_cast<float>(edges_[i].x2),
                                          static_cast<float>(edges_[i].y2));
        }
        edgesNode->markDirty(QSGNode::DirtyGeometry);

        QSGGeometry* nodeGeometry = nodesNode->geometry();
        nodeGeometry->allocate(static_cast<int>(nodes_.size()) * 6);
        QSGGeometry::ColoredPoint2D* nodeVertices = nodeGeometry->vertexDataAsColoredPoint2D();
        for (size_t i = 0; i < nodes_.size(); ++i) {
            float cx = static_cast<float>(nodes_[i].x);
            float cy = static_cast<float>(nodes_[i].y);
            const QColor& color = nodes_[i].color;
            auto r = static_cast<uchar>(color.red());
            auto g = static_cast<uchar>(color.green());
            auto b = static_cast<uchar>(color.blue());
            auto a = static_cast<uchar>(color.alpha());

            size_t base = i * 6;
            // Two triangles forming a square "node" — quads, not
            // actual circles, for this milestone's spike; a real
            // circle/icon shader is a visual-polish item for later.
            nodeVertices[base + 0].set(cx - kNodeHalfSize, cy - kNodeHalfSize, r, g, b, a);
            nodeVertices[base + 1].set(cx + kNodeHalfSize, cy - kNodeHalfSize, r, g, b, a);
            nodeVertices[base + 2].set(cx + kNodeHalfSize, cy + kNodeHalfSize, r, g, b, a);
            nodeVertices[base + 3].set(cx - kNodeHalfSize, cy - kNodeHalfSize, r, g, b, a);
            nodeVertices[base + 4].set(cx + kNodeHalfSize, cy + kNodeHalfSize, r, g, b, a);
            nodeVertices[base + 5].set(cx - kNodeHalfSize, cy + kNodeHalfSize, r, g, b, a);
        }
        nodesNode->markDirty(QSGNode::DirtyGeometry);

        dataDirty_ = false;
    }

    return root;
}

void GraphCanvasItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        lastMousePos_ = event->position();
        event->accept();
    }
}

void GraphCanvasItem::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        QPointF delta = event->position() - lastMousePos_;
        offsetX_ += delta.x();
        offsetY_ += delta.y();
        lastMousePos_ = event->position();
        update();  // schedules updatePaintNode; geometry untouched, dataDirty_ stays false
        event->accept();
    }
}

void GraphCanvasItem::mouseReleaseEvent(QMouseEvent* event) {
    dragging_ = false;
    event->accept();
}

void GraphCanvasItem::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.1 : (1.0 / 1.1);
    scale_ = std::clamp(scale_ * factor, kMinScale, kMaxScale);
    update();
    event->accept();
}

void registerGraphCanvasQmlType() {
    qmlRegisterType<GraphCanvasItem>("Atlas.Render", 1, 0, "GraphCanvas");
}

}  // namespace atlas::render
