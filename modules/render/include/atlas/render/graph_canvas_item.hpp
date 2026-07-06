#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QTimer>

#include <unordered_map>
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
    // Node ids, not raw coordinates — GraphCanvasItem resolves these
    // to each node's current (possibly still-animating) position at
    // render time, every frame. Storing coordinates directly, as this
    // struct did before, meant an edge's endpoints were frozen at
    // whatever the layout said when setGraphData() was called; once
    // nodes started easing toward a new layout instead of snapping
    // to it (see the animation state below), an edge built from stale
    // coordinates would visibly detach from the node it's supposed to
    // follow mid-animation.
    QString sourceId;
    QString targetId;
};

class GraphCanvasItem : public QQuickItem {
    Q_OBJECT

public:
    explicit GraphCanvasItem(QQuickItem* parent = nullptr);

    // Structural update: nodes/edges added, removed, or re-laid-out.
    // Existing nodes don't jump to their new position — they ease
    // toward it over a short animation (see tickAnimation()), the same
    // "watch the graph settle" feel as Obsidian's graph view, without
    // this canvas adopting continuous per-frame physics simulation
    // (still explicitly out of scope at the node counts this app
    // targets — see docs/DECISIONS.md). Brand-new nodes (an id not
    // seen in the previous call) appear immediately at their target
    // position; only *repositioning* of already-known nodes animates.
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

    // Fired on every hover-move, not just on entering/leaving a node:
    // `id` is empty when hovering empty space, and non-empty (possibly
    // the same id repeated on every move within one node) while over a
    // node. This dumb, stateless "here's what's under the cursor right
    // now" signal keeps GraphCanvasItem ignorant of what a tooltip even
    // is — GraphWindow decides what "hovering this id" means and shows
    // whatever info makes sense; this class only reports geometry hits.
    void nodeHovered(QString id);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    // Returns the index into nodes_ of the node at world-space point,
    // or -1 if nothing was hit.
    int hitTest(double worldX, double worldY) const;

    // Advances every node's displayed position one step closer to its
    // target (exponential ease: current += (target - current) * factor),
    // marks geometry dirty, and stops the timer once every node is
    // within kAnimationEpsilon of its target — so this costs nothing
    // once the graph has settled, not a permanently-running per-frame
    // tick.
    void tickAnimation();

    std::vector<RenderNode> nodes_;
    std::vector<RenderEdge> edges_;
    bool dataDirty_ = true;

    // targetPositions_ is rebuilt from `nodes_` on every setGraphData()
    // call — it's just an id -> position index for O(1) lookup instead
    // of a linear scan over nodes_ per edge per frame. currentPositions_
    // is the actual animated, currently-displayed position for each
    // node; rendering and hit-testing both read from this, never from
    // nodes_[i].x/y directly, so a click during an in-flight animation
    // lands where the node visually is, not where it's headed.
    std::unordered_map<QString, QPointF> targetPositions_;
    std::unordered_map<QString, QPointF> currentPositions_;
    QTimer* animationTimer_;

    // Highlight state. Empty selectedId_ means no selection active.
    QString selectedId_;
    std::unordered_set<QString> neighborIds_;
    bool highlightDirty_ = false;

    // Hover state — deliberately separate from selectedId_/neighborIds_:
    // hovering and selecting are different user intents (glancing at
    // something vs. committing to look at its neighborhood), so they
    // get independent visual treatment (see Theme::hoverRing) and
    // independent dirty tracking. Empty means "nothing hovered."
    QString hoveredId_;

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
