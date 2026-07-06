#include "atlas/render/graph_canvas_item.hpp"

#include <QHoverEvent>
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
constexpr float kNodeRadius       = 8.0f;
constexpr float kNodeBorderWidth  = 2.2f;
constexpr float kSelectRingRadius = 15.0f;  // also the hit-test radius — see hitTest()
constexpr float kNeighborRingRadius = 13.0f;
constexpr float kHoverRingRadius   = 12.0f;
constexpr int   kCircleSegments   = 16;  // triangle-fan resolution; see appendCircleFan
constexpr double kMinScale        = 0.05;
constexpr double kMaxScale         = 10.0;
constexpr double kDragThreshold    = 4.0;   // pixels — below this, treat as a click

// Animation: exponential ease toward target each tick, not a fixed-
// duration tween. Simpler to reason about (no need to track "when did
// this node start moving" per node, since a node can get a new target
// mid-flight if the graph changes again before settling) and reads as
// a natural "glide, slowing as it arrives" motion without needing an
// easing-curve library. kEaseFactor closer to 1.0 = snappier/faster;
// closer to 0.0 = slower/floatier. 0.2 lands close to what Obsidian's
// own settle animation feels like without being sluggish.
constexpr double kEaseFactor = 0.2;
constexpr double kAnimationEpsilonSq = 0.01;  // squared world-units; below this, snap and stop
constexpr int kAnimationIntervalMs = 16;      // ~60fps

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
    while (spacing < kMinDotSpacingPx) spacing *= 2.0f;
    while (spacing > kMaxDotSpacingPx) spacing *= 0.5f;
    return spacing;
}

// Appends one filled circle (a triangle fan, `segments` triangles) to
// a ColoredPoint2D vertex buffer at `vertexData[startIndex...]`,
// returning the next free index. Used for node fills, node borders,
// and all three ring types (selected/neighbor/hover) — every circular
// shape this canvas draws goes through this one function, so "make
// circles smoother" or "add anti-aliasing later" is a one-place change.
int appendCircleFan(QSGGeometry::ColoredPoint2D* vertexData, int startIndex, float cx, float cy,
                     float radius, QColor color) {
    constexpr float kTwoPi = 6.283185307179586f;
    uchar r = static_cast<uchar>(color.red());
    uchar g = static_cast<uchar>(color.green());
    uchar b = static_cast<uchar>(color.blue());
    uchar a = static_cast<uchar>(color.alpha());

    int vi = startIndex;
    for (int i = 0; i < kCircleSegments; ++i) {
        float theta0 = kTwoPi * static_cast<float>(i) / static_cast<float>(kCircleSegments);
        float theta1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);
        float x0 = cx + radius * std::cos(theta0);
        float y0 = cy + radius * std::sin(theta0);
        float x1 = cx + radius * std::cos(theta1);
        float y1 = cy + radius * std::sin(theta1);
        vertexData[vi + 0].set(cx, cy, r, g, b, a);
        vertexData[vi + 1].set(x0, y0, r, g, b, a);
        vertexData[vi + 2].set(x1, y1, r, g, b, a);
        vi += 3;
    }
    return vi;
}

// Vertex count contributed by one appendCircleFan call — every caller
// that pre-sizes a buffer needs this, so it's named rather than
// repeating `kCircleSegments * 3` at each call site.
constexpr int kVerticesPerCircle = kCircleSegments * 3;

}  // namespace

GraphCanvasItem::GraphCanvasItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);

    animationTimer_ = new QTimer(this);
    animationTimer_->setInterval(kAnimationIntervalMs);
    connect(animationTimer_, &QTimer::timeout, this, &GraphCanvasItem::tickAnimation);
}

void GraphCanvasItem::setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges) {
    nodes_ = std::move(nodes);
    edges_ = std::move(edges);

    std::unordered_map<QString, QPointF> newTargets;
    newTargets.reserve(nodes_.size());
    for (const auto& n : nodes_) {
        newTargets.emplace(n.id, QPointF(n.x, n.y));
    }

    // Existing nodes keep their current (possibly still mid-flight)
    // displayed position and ease toward the new target from there.
    // Brand-new ids appear immediately at their target — no prior
    // position exists to animate from, and guessing one (e.g. the
    // graph's centroid) was considered out of scope for this pass; a
    // node popping in immediately still reads fine next to others
    // gliding into place.
    for (const auto& [id, target] : newTargets) {
        if (!currentPositions_.contains(id)) {
            currentPositions_[id] = target;
        }
    }
    // Drop positions for nodes that no longer exist, so this map
    // doesn't grow unboundedly across many topic switches/edits over
    // a long-running session.
    for (auto it = currentPositions_.begin(); it != currentPositions_.end();) {
        if (!newTargets.contains(it->first)) {
            it = currentPositions_.erase(it);
        } else {
            ++it;
        }
    }
    targetPositions_ = std::move(newTargets);

    dataDirty_ = true;
    update();
    if (!animationTimer_->isActive()) animationTimer_->start();
}

void GraphCanvasItem::tickAnimation() {
    bool stillMoving = false;
    for (auto& [id, current] : currentPositions_) {
        auto targetIt = targetPositions_.find(id);
        if (targetIt == targetPositions_.end()) continue;  // shouldn't happen; defensive
        const QPointF& target = targetIt->second;

        QPointF delta = target - current;
        double distSq = delta.x() * delta.x() + delta.y() * delta.y();
        if (distSq <= kAnimationEpsilonSq) {
            current = target;  // snap the last little bit rather than asymptotically never arrive
            continue;
        }
        stillMoving = true;
        current += delta * kEaseFactor;
    }

    dataDirty_ = true;
    update();

    if (!stillMoving) animationTimer_->stop();
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
    dataDirty_ = true;
    backgroundDirty_ = true;
    update();
}

int GraphCanvasItem::hitTest(double worldX, double worldY) const {
    // Circular hit test now that nodes render as circles, not squares
    // — matching the visual shape means a click just outside the
    // rendered circle (but inside its old bounding-box corners)
    // correctly misses, instead of feeling like it hit "nothing" when
    // it visually looked like it should have. Reads currentPositions_,
    // not the target x/y baked into `nodes_`, so a click during an
    // in-flight animation is tested against where the node visually is.
    float radiusSq = kSelectRingRadius * kSelectRingRadius;
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        auto it = currentPositions_.find(nodes_[i].id);
        if (it == currentPositions_.end()) continue;
        double dx = worldX - it->second.x();
        double dy = worldY - it->second.y();
        if (dx * dx + dy * dy <= radiusSq) return i;
    }
    return -1;
}

QSGNode* GraphCanvasItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    // Layer order, bottom to top: dot background (screen space,
    // outside the transform node) -> [highlight/hover rings -> edges
    // -> node borders -> node fills] (all under one transform node, so
    // pan/zoom moves them together).
    auto* container = static_cast<QSGNode*>(oldNode);
    QSGGeometryNode* backgroundNode = nullptr;
    QSGTransformNode* root          = nullptr;
    QSGGeometryNode* ringsNode      = nullptr;
    QSGGeometryNode* edgesNode      = nullptr;
    QSGGeometryNode* nodeBordersNode = nullptr;
    QSGGeometryNode* nodeFillsNode   = nullptr;

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

        ringsNode = new QSGGeometryNode();
        auto* ringGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        ringGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        ringsNode->setGeometry(ringGeom);
        ringsNode->setFlag(QSGNode::OwnsGeometry);
        auto* ringMat = new QSGVertexColorMaterial();
        ringsNode->setMaterial(ringMat);
        ringsNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(ringsNode);

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

        nodeBordersNode = new QSGGeometryNode();
        auto* borderGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        borderGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        nodeBordersNode->setGeometry(borderGeom);
        nodeBordersNode->setFlag(QSGNode::OwnsGeometry);
        auto* borderMat = new QSGVertexColorMaterial();
        nodeBordersNode->setMaterial(borderMat);
        nodeBordersNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(nodeBordersNode);

        nodeFillsNode = new QSGGeometryNode();
        auto* fillGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        fillGeom->setDrawingMode(QSGGeometry::DrawTriangles);
        nodeFillsNode->setGeometry(fillGeom);
        nodeFillsNode->setFlag(QSGNode::OwnsGeometry);
        auto* fillMat = new QSGVertexColorMaterial();
        nodeFillsNode->setMaterial(fillMat);
        nodeFillsNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(nodeFillsNode);
    } else {
        backgroundNode   = static_cast<QSGGeometryNode*>(container->firstChild());
        root             = static_cast<QSGTransformNode*>(backgroundNode->nextSibling());
        ringsNode        = static_cast<QSGGeometryNode*>(root->firstChild());
        edgesNode        = static_cast<QSGGeometryNode*>(ringsNode->nextSibling());
        nodeBordersNode  = static_cast<QSGGeometryNode*>(edgesNode->nextSibling());
        nodeFillsNode    = static_cast<QSGGeometryNode*>(nodeBordersNode->nextSibling());
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

        auto positionOf = [this](const QString& id) -> QPointF {
            auto it = currentPositions_.find(id);
            return it != currentPositions_.end() ? it->second : QPointF(0.0, 0.0);
        };

        // --- Rings: selected / neighbor / hover ---
        // Priority per node, at most one ring each: selected outranks
        // neighbor outranks hover — a node that's both selected and
        // hovered just shows the (stronger, stickier) selection ring,
        // rather than stacking rings that would visually compete.
        int ringedCount = 0;
        for (const auto& n : nodes_) {
            bool isSelected = (n.id == selectedId_);
            bool isNeighbor = hasHighlight && neighborIds_.count(n.id) > 0;
            bool isHovered  = (!isSelected && n.id == hoveredId_);
            if (isSelected || isNeighbor || isHovered) ++ringedCount;
        }
        auto* ringGeom = ringsNode->geometry();
        ringGeom->allocate(ringedCount * kVerticesPerCircle);
        if (ringedCount > 0) {
            auto* rv = ringGeom->vertexDataAsColoredPoint2D();
            int vi = 0;
            for (const auto& n : nodes_) {
                bool isSelected = (n.id == selectedId_);
                bool isNeighbor = hasHighlight && neighborIds_.count(n.id) > 0;
                bool isHovered  = (!isSelected && n.id == hoveredId_);
                if (!isSelected && !isNeighbor && !isHovered) continue;

                float radius = isSelected ? kSelectRingRadius
                               : isNeighbor ? kNeighborRingRadius
                                              : kHoverRingRadius;
                QColor color = isSelected ? theme.selectedRing
                                : isNeighbor ? theme.neighborRing
                                              : theme.hoverRing;
                QPointF p = positionOf(n.id);
                vi = appendCircleFan(rv, vi, static_cast<float>(p.x()), static_cast<float>(p.y()),
                                       radius, color);
            }
        }
        ringsNode->markDirty(QSGNode::DirtyGeometry);

        // --- Edges ---
        // Resolved through currentPositions_ every rebuild, not stored
        // as coordinates on RenderEdge — see RenderEdge's own doc
        // comment for why: this is what makes an edge visually follow
        // its nodes while they're still easing into place.
        auto* edgeGeom = edgesNode->geometry();
        edgeGeom->allocate(static_cast<int>(edges_.size()) * 2);
        auto* ev = edgeGeom->vertexDataAsPoint2D();
        for (size_t i = 0; i < edges_.size(); ++i) {
            QPointF p1 = positionOf(edges_[i].sourceId);
            QPointF p2 = positionOf(edges_[i].targetId);
            ev[i*2+0].set(static_cast<float>(p1.x()), static_cast<float>(p1.y()));
            ev[i*2+1].set(static_cast<float>(p2.x()), static_cast<float>(p2.y()));
        }
        // Edge material is flat-color; we can't dim individual edges
        // without switching to per-vertex color on edges too — that's
        // a later polish item. For now just dim the whole edge layer
        // when a highlight is active.
        auto* edgeMat = static_cast<QSGFlatColorMaterial*>(edgesNode->material());
        edgeMat->setColor(hasHighlight ? theme.edgeDimmed : theme.edge);
        edgesNode->markDirty(QSGNode::DirtyGeometry);
        edgesNode->markDirty(QSGNode::DirtyMaterial);

        // --- Node borders (every node, always — see Theme::nodeBorder) ---
        auto* borderGeom = nodeBordersNode->geometry();
        borderGeom->allocate(static_cast<int>(nodes_.size()) * kVerticesPerCircle);
        {
            auto* bv = borderGeom->vertexDataAsColoredPoint2D();
            int vi = 0;
            for (const auto& n : nodes_) {
                QColor borderColor = theme.nodeBorder;
                if (hasHighlight) {
                    bool isSelected = (n.id == selectedId_);
                    bool isNeighbor = neighborIds_.count(n.id) > 0;
                    if (!isSelected && !isNeighbor) borderColor.setAlpha(60);
                }
                QPointF p = positionOf(n.id);
                vi = appendCircleFan(bv, vi, static_cast<float>(p.x()), static_cast<float>(p.y()),
                                       kNodeRadius + kNodeBorderWidth, borderColor);
            }
        }
        nodeBordersNode->markDirty(QSGNode::DirtyGeometry);

        // --- Node fills ---
        auto* fillGeom = nodeFillsNode->geometry();
        fillGeom->allocate(static_cast<int>(nodes_.size()) * kVerticesPerCircle);
        {
            auto* fv = fillGeom->vertexDataAsColoredPoint2D();
            int vi = 0;
            for (const auto& n : nodes_) {
                QColor c = n.color;
                if (hasHighlight) {
                    bool isSelected = (n.id == selectedId_);
                    bool isNeighbor = neighborIds_.count(n.id) > 0;
                    if (!isSelected && !isNeighbor) c.setAlpha(60);
                }
                QPointF p = positionOf(n.id);
                vi = appendCircleFan(fv, vi, static_cast<float>(p.x()), static_cast<float>(p.y()),
                                       kNodeRadius, c);
            }
        }
        nodeFillsNode->markDirty(QSGNode::DirtyGeometry);

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
        double worldX = (event->position().x() - offsetX_) / scale_;
        double worldY = (event->position().y() - offsetY_) / scale_;
        int hit = hitTest(worldX, worldY);
        if (hit >= 0) {
            emit nodeClicked(nodes_[hit].id);
        } else {
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

void GraphCanvasItem::hoverMoveEvent(QHoverEvent* event) {
    double worldX = (event->position().x() - offsetX_) / scale_;
    double worldY = (event->position().y() - offsetY_) / scale_;
    int hit = hitTest(worldX, worldY);
    QString newHoveredId = hit >= 0 ? nodes_[hit].id : QString{};

    if (newHoveredId != hoveredId_) {
        hoveredId_ = newHoveredId;
        dataDirty_ = true;  // ring layer needs rebuilding to show/move the hover ring
        update();
        emit nodeHovered(hoveredId_);
    }
    event->accept();
}

void GraphCanvasItem::hoverLeaveEvent(QHoverEvent* event) {
    if (!hoveredId_.isEmpty()) {
        hoveredId_.clear();
        dataDirty_ = true;
        update();
        emit nodeHovered(QString{});
    }
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
    backgroundDirty_ = true;
    update();
}

void registerGraphCanvasQmlType() {
    qmlRegisterType<GraphCanvasItem>("Atlas.Render", 1, 0, "GraphCanvas");
}

}  // namespace atlas::render
