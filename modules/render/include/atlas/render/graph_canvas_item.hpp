#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QTextLayout>
#include <QTimer>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atlas/render/canvas_geometry.hpp"
#include "atlas/render/theme.hpp"

class QSGTextNode;

namespace atlas::render {

struct RenderNode {
    QString id;
    double x = 0.0;
    double y = 0.0;
    QColor color;
    QString label;
    double recall = -1.0;
    bool ghost = false;
    QString groupKey;
    QString groupLabel;
};

struct RenderEdge {
    QString sourceId;
    QString targetId;
    bool directed = true;
    bool contrast = false;
    bool ghost = false;
};

struct RenderGroup {
    QString key;
    QString label;
    QPointF center;
    double meanRecall = -1.0;
    int memberCount = 0;
};

QString elideLabel(const QString& text, int maxChars = 28);
std::vector<RenderGroup> summarizeGroups(const std::vector<RenderNode>& nodes,
                                         const std::unordered_map<QString, QPointF>& positions);

struct SceneVertices;

class GraphCanvasItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(bool collapsed READ collapsed NOTIFY zoomChanged)

public:
    static constexpr double kCollapseZoom = 0.35;
    static constexpr double kLabelZoom = 0.55;

    explicit GraphCanvasItem(QQuickItem* parent = nullptr);

    void setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges);
    const std::vector<RenderNode>& nodes() const { return nodes_; }
    const std::vector<RenderEdge>& edges() const { return edges_; }

    void setTheme(ThemeMode mode);
    ThemeMode theme() const { return themeMode_; }

    void setHighlight(const QString& selectedId, const std::unordered_set<QString>& neighborIds);
    void clearHighlight();

    double zoom() const { return scale_; }
    void setZoom(double zoom);
    Q_INVOKABLE void zoomBy(double factor);
    bool collapsed() const { return hasGroups_ && scale_ < kCollapseZoom; }
    Q_INVOKABLE QString groupAt(double screenX, double screenY) const;

signals:
    void nodeClicked(QString id);
    void nodeRightClicked(QString id);
    void nodeHovered(QString id);
    void groupClicked(QString key);
    void zoomChanged();

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
    struct LabelLayout {
        std::shared_ptr<QTextLayout> layout;
        qreal width = 0.0;
    };

    static LabelLayout makeLabel(const QString& text);
    int hitTest(double worldX, double worldY) const;
    Vec2 positionOf(const QString& id) const;
    QPointF toScreen(QPointF world) const;
    void tickAnimation();
    void buildNodes(SceneVertices& out, const Theme& theme) const;
    void buildEdges(SceneVertices& out, const Theme& theme) const;
    void buildGroups(SceneVertices& out, const Theme& theme) const;
    void fillLabels(QSGTextNode* labels, QSGTextNode* ghostLabels, const Theme& theme) const;

    std::vector<RenderNode> nodes_;
    std::vector<RenderEdge> edges_;
    std::unordered_map<QString, LabelLayout> labelLayouts_;
    std::unordered_map<QString, LabelLayout> groupLayouts_;

    std::unordered_map<QString, QPointF> targetPositions_;
    std::unordered_map<QString, QPointF> currentPositions_;
    QTimer* animationTimer_;

    QString selectedId_;
    std::unordered_set<QString> neighborIds_;
    QString hoveredId_;

    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
    double scale_ = 1.0;

    bool dragging_ = false;
    bool dragMoved_ = false;
    QPointF lastMousePos_;

    ThemeMode themeMode_ = ThemeMode::Dark;
    bool backgroundDirty_ = true;
    bool hasGroups_ = false;
    bool dataDirty_ = true;
    bool labelsDirty_ = true;
};

void registerGraphCanvasQmlType();

}  // namespace atlas::render
