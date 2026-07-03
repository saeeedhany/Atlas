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
#include <cmath>
#include <utility>

namespace atlas::render {

namespace {
constexpr float kNodeHalfSize  = 6.0f;
constexpr float kSelectHalf    = 9.0f;   // selection ring drawn behind the node
constexpr double kMinScale     = 0.05;
constexpr double kMaxScale     = 10.0;
constexpr double kDragThreshold = 4.0;   // pixels — below this, treat as a click

// Dot grid, in screen pixels. Base spacing is a world-space value that
// gets scaled by the current zoom (so dots feel attached to content,
// the same illusion pan/zoom of nodes already gives), but clamped to a
// pixel range so it never becomes too dense to render cheaply (zoomed
// out) or too sparse to read as a grid (zoomed in) — the standard
// "halve/double until in range" LOD trick, applied to a value instead
// of to actual geometry detail.
constexpr float kBaseDotSpacingWorld = 40.0f;
constexpr float kMinDotSpacingPx     = 18.0f;
constexpr float kMaxDotSpacingPx     = 72.0f;
constexpr float kDotRadius           = 1.3f;

float effectiveDotSpacingPx(double scale) {
    float spacing = static_cast<float>(kBaseDotSpacingWorld * scale);
    // scale is clamped to [kMinScale, kMaxScale] elsewhere, so this
    // loop is bounded to a handful of iterations either direction —
    // not an unbounded while.
    while (spacing < kMinDotSpacingPx) spacing *= 2.0f;
    while (spacing > kMaxDotSpacingPx) spacing *= 0.5f;
    return spacing;
}
}  // namespace

GraphCanvasItem::GraphCanvasItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void GraphCanvasItem::setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges) {
    nodes_ = std::move(nodes);
    edges_ = std::move(edges);
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::setHighlight(const QString& selectedId,
                                    const std::unordered_set<QString>& neighborIds) {
    selectedId_  = selectedId;
    neighborIds_ = neighborIds;
    highlightDirty_ = true;
    dataDirty_ = true;  // forces a full vertex-buffer rebuild
    update();
}

void GraphCanvasItem::clearHighlight() {
    selectedId_.clear();
    neighborIds_.clear();
    highlightDirty_ = true;
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::setTheme(ThemeMode mode) {
    if (themeMode_ == mode) return;
    themeMode_ = mode;
    // Both dirty: dataDirty_ retints edges/selection rings (built in
    // the same pass as node geometry), backgroundDirty_ rebuilds the
    // dot grid at the new dot color.
    dataDirty_ = true;
    backgroundDirty_ = true;
    update();
}

int GraphCanvasItem::hitTest(double worldX, double worldY) const {
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        // Test against the larger selection ring so small quads are
        // easier to click.
        double dx = worldX - nodes_[i].x;
        double dy = worldY - nodes_[i].y;
        if (std::abs(dx) <= kSelectHalf && std::abs(dy) <= kSelectHalf) return i;
    }
    return -1;
}

QSGNode* GraphCanvasItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    // Top-level node is a plain container: the dot grid lives directly
    // under it in screen space (never transformed by pan/zoom — see
    // effectiveDotSpacingPx), while everything else lives under a
    // QSGTransformNode child exactly as before.
    auto* container = static_cast<QSGNode*>(oldNode);
    QSGGeometryNode* backgroundNode = nullptr;
    QSGTransformNode* root          = nullptr;
    QSGGeometryNode* selectionNode  = nullptr;
    QSGGeometryNode* edgesNode      = nullptr;
    QSGGeometryNode* nodesNode      = nullptr;

    if (container == nullptr) {
        container = new QSGNode();

        backgroundNode = new QSGGeometryNode();
        auto* bgGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        bgGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        backgroundNode->setGeometry(bgGeom);
        backgroundNode->setFlag(QSGNode::OwnsGeometry);
        auto* bgMat = new QSGVertexColorMaterial();
        backgroundNode->setMaterial(bgMat);
        backgroundNode->setFlag(QSGNode::OwnsMaterial);
        container->appendChildNode(backgroundNode);

        root = new QSGTransformNode();
        container->appendChildNode(root);

        // Layer order within root: selection rings -> edges -> nodes
        selectionNode = new QSGGeometryNode();
        auto* selGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        selGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        selectionNode->setGeometry(selGeom);
        selectionNode->setFlag(QSGNode::OwnsGeometry);
        auto* selMat = new QSGVertexColorMaterial();
        selectionNode->setMaterial(selMat);
        selectionNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(selectionNode);

        edgesNode = new QSGGeometryNode();
        auto* edgeGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        edgeGeom->setDrawingMode(QSGGeometry::DrawLines);
        edgesNode->setGeometry(edgeGeom);
        edgesNode->setFlag(QSGNode::OwnsGeometry);
        auto* edgeMat = new QSGFlatColorMaterial();
        edgeMat->setColor(themeFor(themeMode_).edge);
        edgesNode->setMaterial(edgeMat);
        edgesNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(edgesNode);

        nodesNode = new QSGGeometryNode();
        auto* nodeGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        nodeGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        nodesNode->setGeometry(nodeGeom);
        nodesNode->setFlag(QSGNode::OwnsGeometry);
        auto* nodeMat = new QSGVertexColorMaterial();
        nodesNode->setMaterial(nodeMat);
        nodesNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(nodesNode);
    } else {
        backgroundNode = static_cast<QSGGeometryNode*>(container->firstChild());
        root           = static_cast<QSGTransformNode*>(backgroundNode->nextSibling());
        selectionNode  = static_cast<QSGGeometryNode*>(root->firstChild());
        edgesNode      = static_cast<QSGGeometryNode*>(selectionNode->nextSibling());
        nodesNode      = static_cast<QSGGeometryNode*>(edgesNode->nextSibling());
    }

    QMatrix4x4 matrix;
    matrix.translate(static_cast<float>(offsetX_), static_cast<float>(offsetY_));
    matrix.scale(static_cast<float>(scale_));
    root->setMatrix(matrix);

    const Theme& theme = themeFor(themeMode_);

    if (backgroundDirty_) {
        float spacing = effectiveDotSpacingPx(scale_);
        float width   = static_cast<float>(this->width());
        float height  = static_cast<float>(this->height());

        // Phase-shift the grid by the current pan offset (mod spacing)
        // so dots appear to belong to world space while the actual
        // vertex count only ever depends on viewport size / spacing —
        // never on graph size or how far the user has panned. Scale
        // enters only through effectiveDotSpacingPx above, not here:
        // the world-space pan offset itself is already in the same
        // (unscaled-by-zoom) screen-pixel units mouseMoveEvent applies
        // it in.
        float phaseX = std::fmod(static_cast<float>(offsetX_), spacing);
        if (phaseX < 0) phaseX += spacing;
        float phaseY = std::fmod(static_cast<float>(offsetY_), spacing);
        if (phaseY < 0) phaseY += spacing;

        int colCount = (width > 0 && spacing > 0)
                           ? static_cast<int>(width / spacing) + 2
                           : 0;
        int rowCount = (height > 0 && spacing > 0)
                           ? static_cast<int>(height / spacing) + 2
                           : 0;

        auto* bgGeom = backgroundNode->geometry();
        bgGeom->allocate(colCount * rowCount * 6);
        if (colCount > 0 && rowCount > 0) {
            auto* bv = bgGeom->vertexDataAsColoredPoint2D();
            uchar r = static_cast<uchar>(theme.dot.red());
            uchar g = static_cast<uchar>(theme.dot.green());
            uchar b = static_cast<uchar>(theme.dot.blue());
            uchar a = static_cast<uchar>(theme.dot.alpha());
            int vi = 0;
            for (int col = -1; col < colCount - 1; ++col) {
                for (int row = -1; row < rowCount - 1; ++row) {
                    float cx = phaseX + static_cast<float>(col) * spacing;
                    float cy = phaseY + static_cast<float>(row) * spacing;
                    bv[vi+0].set(cx-kDotRadius, cy-kDotRadius, r,g,b,a);
                    bv[vi+1].set(cx+kDotRadius, cy-kDotRadius, r,g,b,a);
                    bv[vi+2].set(cx+kDotRadius, cy+kDotRadius, r,g,b,a);
                    bv[vi+3].set(cx-kDotRadius, cy-kDotRadius, r,g,b,a);
                    bv[vi+4].set(cx+kDotRadius, cy+kDotRadius, r,g,b,a);
                    bv[vi+5].set(cx-kDotRadius, cy+kDotRadius, r,g,b,a);
                    vi += 6;
                }
            }
        }
        backgroundNode->markDirty(QSGNode::DirtyGeometry);
        backgroundDirty_ = false;
    }

    if (dataDirty_) {
        bool hasHighlight = !selectedId_.isEmpty();

        // --- Selection rings ---
        // Allocate one ring quad per highlighted node (selected + neighbors).
        int ringCount = 0;
        if (hasHighlight) {
            for (const auto& n : nodes_) {
                if (n.id == selectedId_ || neighborIds_.count(n.id)) ++ringCount;
            }
        }
        auto* selGeom = selectionNode->geometry();
        selGeom->allocate(ringCount * 6);
        if (ringCount > 0) {
            auto* v = selGeom->vertexDataAsColoredPoint2D();
            int vi = 0;
            for (const auto& n : nodes_) {
                bool isSelected  = (n.id == selectedId_);
                bool isNeighbor  = neighborIds_.count(n.id) > 0;
                if (!isSelected && !isNeighbor) continue;
                float cx = static_cast<float>(n.x);
                float cy = static_cast<float>(n.y);
                const QColor& ringColor = isSelected ? theme.selectedRing : theme.neighborRing;
                uchar r = static_cast<uchar>(ringColor.red());
                uchar g = static_cast<uchar>(ringColor.green());
                uchar b = static_cast<uchar>(ringColor.blue());
                uchar a = static_cast<uchar>(ringColor.alpha());
                v[vi+0].set(cx-kSelectHalf, cy-kSelectHalf, r,g,b,a);
                v[vi+1].set(cx+kSelectHalf, cy-kSelectHalf, r,g,b,a);
                v[vi+2].set(cx+kSelectHalf, cy+kSelectHalf, r,g,b,a);
                v[vi+3].set(cx-kSelectHalf, cy-kSelectHalf, r,g,b,a);
                v[vi+4].set(cx+kSelectHalf, cy+kSelectHalf, r,g,b,a);
                v[vi+5].set(cx-kSelectHalf, cy+kSelectHalf, r,g,b,a);
                vi += 6;
            }
        }
        selectionNode->markDirty(QSGNode::DirtyGeometry);

        // --- Edges ---
        // Dim edges that aren't connected to the selected node when
        // highlight is active.
        auto* edgeGeom = edgesNode->geometry();
        edgeGeom->allocate(static_cast<int>(edges_.size()) * 2);
        auto* ev = edgeGeom->vertexDataAsPoint2D();
        for (size_t i = 0; i < edges_.size(); ++i) {
            ev[i*2+0].set(static_cast<float>(edges_[i].x1), static_cast<float>(edges_[i].y1));
            ev[i*2+1].set(static_cast<float>(edges_[i].x2), static_cast<float>(edges_[i].y2));
        }
        // Edge material is flat-color; we can't dim individual edges
        // without switching to per-vertex color on edges too — that's
        // a later polish item. For now just dim the whole edge layer
        // when a highlight is active.
        auto* edgeMat = static_cast<QSGFlatColorMaterial*>(edgesNode->material());
        edgeMat->setColor(hasHighlight ? theme.edgeDimmed : theme.edge);
        edgesNode->markDirty(QSGNode::DirtyGeometry);
        edgesNode->markDirty(QSGNode::DirtyMaterial);

        // --- Nodes ---
        auto* nodeGeom = nodesNode->geometry();
        nodeGeom->allocate(static_cast<int>(nodes_.size()) * 6);
        auto* nv = nodeGeom->vertexDataAsColoredPoint2D();
        for (size_t i = 0; i < nodes_.size(); ++i) {
            float cx = static_cast<float>(nodes_[i].x);
            float cy = static_cast<float>(nodes_[i].y);
            QColor c = nodes_[i].color;

            if (hasHighlight) {
                bool isSelected = (nodes_[i].id == selectedId_);
                bool isNeighbor = neighborIds_.count(nodes_[i].id) > 0;
                if (!isSelected && !isNeighbor) {
                    // Dim non-neighborhood nodes to 25% alpha.
                    c = QColor(c.red(), c.green(), c.blue(), 60);
                }
            }
            auto r = static_cast<uchar>(c.red());
            auto g = static_cast<uchar>(c.green());
            auto b = static_cast<uchar>(c.blue());
            auto a = static_cast<uchar>(c.alpha());
            size_t base = i * 6;
            nv[base+0].set(cx-kNodeHalfSize, cy-kNodeHalfSize, r,g,b,a);
            nv[base+1].set(cx+kNodeHalfSize, cy-kNodeHalfSize, r,g,b,a);
            nv[base+2].set(cx+kNodeHalfSize, cy+kNodeHalfSize, r,g,b,a);
            nv[base+3].set(cx-kNodeHalfSize, cy-kNodeHalfSize, r,g,b,a);
            nv[base+4].set(cx+kNodeHalfSize, cy+kNodeHalfSize, r,g,b,a);
            nv[base+5].set(cx-kNodeHalfSize, cy+kNodeHalfSize, r,g,b,a);
        }
        nodesNode->markDirty(QSGNode::DirtyGeometry);

        dataDirty_ = false;
        highlightDirty_ = false;
    }

    return container;
}

void GraphCanvasItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragMoved_ = false;
        lastMousePos_ = event->position();
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        // Right-click never pans, so it's a direct hit-test on press
        // rather than going through the drag/release state machine
        // left-click uses.
        double worldX = (event->position().x() - offsetX_) / scale_;
        double worldY = (event->position().y() - offsetY_) / scale_;
        int hit = hitTest(worldX, worldY);
        if (hit >= 0) emit nodeRightClicked(nodes_[hit].id);
        event->accept();
    }
}

void GraphCanvasItem::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        QPointF delta = event->position() - lastMousePos_;
        if (!dragMoved_ &&
            (std::abs(delta.x()) > kDragThreshold || std::abs(delta.y()) > kDragThreshold)) {
            dragMoved_ = true;
        }
        offsetX_ += delta.x();
        offsetY_ += delta.y();
        lastMousePos_ = event->position();
        backgroundDirty_ = true;
        update();
        event->accept();
    }
}

void GraphCanvasItem::mouseReleaseEvent(QMouseEvent* event) {
    if (!dragMoved_) {
        // This was a click, not a drag — hit-test in world space.
        double worldX = (event->position().x() - offsetX_) / scale_;
        double worldY = (event->position().y() - offsetY_) / scale_;
        int hit = hitTest(worldX, worldY);
        if (hit >= 0) {
            emit nodeClicked(nodes_[hit].id);
        } else {
            // Clicking empty space clears the selection.
            if (!selectedId_.isEmpty()) {
                clearHighlight();
                emit nodeClicked(QString{});
            }
        }
    }
    dragging_ = false;
    dragMoved_ = false;
    event->accept();
}

void GraphCanvasItem::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.1 : (1.0 / 1.1);
    scale_ = std::clamp(scale_ * factor, kMinScale, kMaxScale);
    backgroundDirty_ = true;
    update();
    event->accept();
}

void GraphCanvasItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    // The dot grid covers exactly the viewport (see updatePaintNode),
    // so a resize needs a rebuild the same as a pan/zoom does.
    backgroundDirty_ = true;
    update();
}

void registerGraphCanvasQmlType() {
    qmlRegisterType<GraphCanvasItem>("Atlas.Render", 1, 0, "GraphCanvas");
}

}  // namespace atlas::render
