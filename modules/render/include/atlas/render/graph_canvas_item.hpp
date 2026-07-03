#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QString>

#include <unordered_set>
#include <vector>

#include "atlas/render/theme.hpp"

namespace atlas::render {

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

class GraphCanvasItem : public QQuickItem {
    Q_OBJECT

public:
    explicit GraphCanvasItem(QQuickItem* parent = nullptr);

    void setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges);

    // Switches the canvas's own palette (background, dots, edges,
    // selection rings). Does NOT retint already-set node colors —
    // those were baked into RenderNode::color by whoever called
    // setGraphData (see GraphWindow::colorForDifficulty), since node
    // color is a semantic mapping this class has no opinion on. Callers
    // that want nodes retinted too should re-call setGraphData after
    // switching. Safe to call before the item has painted anything.
    void setTheme(ThemeMode mode);
    ThemeMode theme() const { return themeMode_; }

    // Highlight a specific node and its neighborhood. `selectedId` is
    // rendered with an accent ring (drawn as a slightly larger quad
    // underneath in white). Everything in `neighborIds` renders at
    // full brightness; everything else is dimmed to make the
    // neighborhood stand out. Call clearHighlight() to return to the
    // flat "all nodes equal" rendering.
    void setHighlight(const QString& selectedId, const std::unordered_set<QString>& neighborIds);
    void clearHighlight();

signals:
    // Emitted when the user clicks a node. `id` is the
    // KnowledgeObjectId UUID string (same value stored in RenderNode).
    // Emitting a string rather than a KnowledgeObjectId directly keeps
    // atlas-render free of a dependency on atlas-core.
    void nodeClicked(QString id);

    // Right-click on a node — distinct from nodeClicked (left-click)
    // since they trigger different actions (select-and-highlight vs.
    // a context menu). Not emitted for a right-click on empty space.
    void nodeRightClicked(QString id);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    // Returns the index into nodes_ of the node at world-space point,
    // or -1 if nothing was hit.
    int hitTest(double worldX, double worldY) const;

    std::vector<RenderNode> nodes_;
    std::vector<RenderEdge> edges_;
    bool dataDirty_ = true;

    // Highlight state. Empty selectedId_ means no selection active.
    QString selectedId_;
    std::unordered_set<QString> neighborIds_;
    bool highlightDirty_ = false;

    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
    double scale_ = 1.0;

    bool dragging_ = false;
    bool dragMoved_ = false;  // distinguishes a click from a drag-release
    QPointF lastMousePos_;

    // The dotted background is drawn in screen space, not world space
    // (see graph_canvas_item.cpp for why): its vertex count is bounded
    // by viewport pixel area / dot spacing, not by graph size or zoom
    // level, so it stays cheap to rebuild on every pan/zoom frame —
    // unlike the node/edge geometry, which only rebuilds on a real
    // structural change (see dataDirty_).
    ThemeMode themeMode_ = ThemeMode::Dark;
    bool backgroundDirty_ = true;
};

void registerGraphCanvasQmlType();

}  // namespace atlas::render
