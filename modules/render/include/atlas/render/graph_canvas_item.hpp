#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QString>

#include <vector>

namespace atlas::render {

// Plain data the canvas needs to draw — deliberately decoupled from
// GraphEngine/KnowledgeObject. GraphCanvasItem only knows how to draw
// circles-as-quads and lines; it has no idea what a "knowledge object"
// is. Whoever owns the canvas (atlas-ui's GraphWindow) is responsible
// for converting graph + layout data into this shape.
struct RenderNode {
    QString id;
    double x = 0.0;
    double y = 0.0;
    QColor color;
};

struct RenderEdge {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
};

// Hand-built Qt Quick scene graph rendering: all nodes are one batched
// QSGGeometryNode (DrawTriangles), all edges another (DrawLines) — one
// GPU draw call each, regardless of node count. This is the whole
// reason this milestone chose Qt Quick over Qt Widgets' QGraphicsView:
// QGraphicsView does CPU-side per-item work that doesn't batch this way.
//
// Pan/zoom never touches this geometry — they only update a
// QSGTransformNode's matrix, which costs nothing on the CPU side. Only
// setGraphData() (called when the underlying graph structurally
// changes or layout is recomputed) rebuilds the actual vertex buffers.
class GraphCanvasItem : public QQuickItem {
    Q_OBJECT

public:
    explicit GraphCanvasItem(QQuickItem* parent = nullptr);

    void setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    std::vector<RenderNode> nodes_;
    std::vector<RenderEdge> edges_;
    bool dataDirty_ = true;

    // Camera state. Deliberately simple for this milestone: zoom is
    // centered on the origin, not the cursor — a real polish item for
    // later, not pretended-away here.
    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
    double scale_ = 1.0;

    bool dragging_ = false;
    QPointF lastMousePos_;
};

// Manual qmlRegisterType call rather than the QML_ELEMENT macro +
// qt_add_qml_module CMake integration — fewer moving parts, no
// dependency on a specific Qt6 minor version's module-registration
// machinery. Call once, before loading any QML that uses GraphCanvas.
void registerGraphCanvasQmlType();

}  // namespace atlas::render
