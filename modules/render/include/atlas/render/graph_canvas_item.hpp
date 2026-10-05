#pragma once

#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QTextLayout>
#include <QTimer>
#include <QVariant>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atlas/render/canvas_geometry.hpp"
#include "atlas/render/region_geometry.hpp"
#include "atlas/render/theme.hpp"

class QSGTextNode;

namespace atlas::render {

struct RenderNode {
    QString id;
    double x = 0.0;
    double y = 0.0;
    QString label;
    double recall = -1.0;
    bool ghost = false;
    QString groupKey;
    QString groupLabel;
    bool hinted = false;
    int degree = 0;
};

enum class EdgeMark { None, Hidden, Recalled, Partial, Missed, Confused };

struct RenderEdge {
    QString id{};
    QString sourceId;
    QString targetId;
    bool directed = true;
    bool contrast = false;
    bool ghost = false;
    EdgeMark mark = EdgeMark::None;
};

struct RenderRegion {
    QString key;
    QRectF rect;
    int hue = 0;
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
    Q_PROPERTY(bool animated READ animated WRITE setAnimated NOTIFY animatedChanged)

public:
    static constexpr double kCollapseZoom = 0.35;
    static constexpr double kLabelZoom = 0.55;

    explicit GraphCanvasItem(QQuickItem* parent = nullptr);

    void setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges);
    const std::vector<RenderNode>& nodes() const { return nodes_; }
    const std::vector<RenderEdge>& edges() const { return edges_; }

    void setRegions(std::vector<RenderRegion> regions);
    const std::vector<RenderRegion>& regions() const { return regions_; }

    void setTheme(ThemeMode mode);
    ThemeMode theme() const { return themeMode_; }

    void setHighlight(const QString& selectedId, const std::unordered_set<QString>& neighborIds);
    void clearHighlight();

    double zoom() const { return scale_; }
    void setZoom(double zoom);
    Q_INVOKABLE void zoomBy(double factor);
    bool collapsed() const { return hasGroups_ && scale_ < kCollapseZoom; }
    Q_INVOKABLE QString groupAt(double screenX, double screenY) const;

    bool animated() const { return animated_; }
    void setAnimated(bool animated);
    Q_INVOKABLE void centerOn(const QString& id);
    Q_INVOKABLE void fitToContent();
    Q_INVOKABLE void zoomAt(double factor, double screenX, double screenY);
    Q_INVOKABLE QVariant screenPositionOf(const QString& id) const;
    Q_INVOKABLE QString linkAt(double screenX, double screenY) const;
    Q_INVOKABLE QPointF mapToScreen(double worldX, double worldY) const;
    Q_INVOKABLE QPointF mapToWorld(double screenX, double screenY) const;
    Q_INVOKABLE QString nodeAt(double screenX, double screenY) const;
    Q_INVOKABLE void fitWorldRect(double x, double y, double width, double height);
    Q_INVOKABLE void handleDoubleClick(double screenX, double screenY);

    double shownRecallOf(const QString& id) const;
    const std::unordered_set<QString>& highlightedNeighbors() const { return neighborIds_; }

    const QTextLayout* labelLayoutFor(const QString& id) const;

signals:
    void nodeClicked(QString id);
    void nodeRightClicked(QString id);
    void nodeHovered(QString id);
    void groupClicked(QString key);
    void zoomChanged();
    void linkHovered(QString id);
    void viewChanged();
    void animatedChanged();
    void backgroundDoubleClicked(double worldX, double worldY);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct LabelLayout {
        QString text;
        std::shared_ptr<QTextLayout> layout;
        qreal width = 0.0;
    };

    static LabelLayout makeLabel(const QString& text);
    static void syncLabels(std::unordered_map<QString, LabelLayout>& cache,
                           const std::unordered_map<QString, QString>& wanted);
    int hitTest(double worldX, double worldY) const;
    Vec2 positionOf(const QString& id) const;
    QPointF toScreen(QPointF world) const;
    void tickAnimation();
    void moveCamera(double scale, double offsetX, double offsetY);
    void applyCamera(double scale, double offsetX, double offsetY);
    void viewMoved();
    bool insideCull(QPointF world) const;
    void fitOnce();
    void buildRegions(SceneVertices& out, const Theme& theme) const;
    void buildHighlight(SceneVertices& out, const Theme& theme) const;
    void buildNodes(SceneVertices& out, const Theme& theme) const;
    void buildEdges(SceneVertices& out, const Theme& theme) const;
    void buildGroups(SceneVertices& out, const Theme& theme) const;
    void fillLabels(QSGTextNode* labels, QSGTextNode* ghostLabels, const Theme& theme) const;

    std::vector<RenderNode> nodes_;
    std::vector<RenderEdge> edges_;
    std::vector<RenderRegion> regions_;
    std::unordered_map<QString, LabelLayout> labelLayouts_;
    std::unordered_map<QString, LabelLayout> groupLayouts_;

    std::unordered_map<QString, QPointF> targetPositions_;
    std::unordered_map<QString, QPointF> currentPositions_;
    std::unordered_map<QString, double> shownRecall_;
    std::unordered_map<QString, double> targetRecall_;
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
    bool highlightDirty_ = true;
    bool labelsDirty_ = true;
    bool regionsDirty_ = true;

    bool animated_ = true;
    bool fitted_ = false;
    bool cameraMoving_ = false;
    double targetScale_ = 1.0;
    double targetOffsetX_ = 0.0;
    double targetOffsetY_ = 0.0;
    QRectF cullRect_;
    QString hoveredLinkId_;
};

void registerGraphCanvasQmlType();

}  // namespace atlas::render
