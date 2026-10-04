#include "atlas/render/graph_canvas_item.hpp"

#include "atlas/render/canvas_view.hpp"

#include <QFont>
#include <QHoverEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGTextNode>
#include <QSGTransformNode>
#include <QSGVertexColorMaterial>
#include <QTextLine>
#include <QWheelEvent>
#include <QtQml/qqml.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <utility>

namespace atlas::render {

struct ColoredVertex {
    float x;
    float y;
    QColor color;
};

struct SceneVertices {
    std::vector<ColoredVertex> highlight;
    std::vector<ColoredVertex> edges;
    std::vector<ColoredVertex> arrows;
    std::vector<ColoredVertex> borders;
    std::vector<ColoredVertex> fills;
    std::vector<ColoredVertex> memory;
};

namespace {

constexpr float kNodeRadius = 8.0f;
constexpr float kNodeBorderWidth = 2.2f;
constexpr float kHitRadius = 15.0f;
constexpr float kSelectRingRadius = 17.5f;
constexpr float kSelectRingThickness = 2.0f;
constexpr float kNeighborRingRadius = 16.5f;
constexpr float kHoverRingRadius = 16.5f;
constexpr float kHighlightThickness = 1.5f;
constexpr float kMemoryRingRadius = 12.5f;
constexpr float kMemoryRingThickness = 2.0f;
constexpr int kNewRingDashes = 12;
constexpr float kArrowLength = 7.0f;
constexpr float kArrowHalfWidth = 3.5f;
constexpr float kDashLength = 4.0f;
constexpr float kDashGap = 3.0f;
constexpr int kCircleSegments = 16;
constexpr double kMinScale = 0.05;
constexpr double kMaxScale = 10.0;
constexpr double kDragThreshold = 4.0;
constexpr double kWheelStep = 1.1;
constexpr float kGroupRadiusPx = 20.0f;
constexpr float kLabelGapPx = 6.0f;
constexpr double kCullMarginPx = 60.0;
constexpr int kGhostAlphaPercent = 35;
constexpr int kDimmedAlpha = 60;
constexpr int kDimmedRingAlphaPercent = 40;
constexpr double kEaseFactor = 0.2;
constexpr double kFitMarginPx = 80.0;
constexpr double kFitMaxScale = 1.6;
constexpr double kLinkHoverPx = 6.0;
constexpr double kCameraEpsilon = 0.25;
constexpr double kAnimationEpsilonSq = 0.01;
constexpr int kAnimationIntervalMs = 16;
constexpr float kBaseDotSpacingWorld = 40.0f;
constexpr float kMinDotSpacingPx = 18.0f;
constexpr float kMaxDotSpacingPx = 72.0f;
constexpr float kDotRadius = 1.3f;
constexpr int kLabelPixelSize = 11;

enum Layer { HighlightLayer, EdgeLayer, ArrowLayer, BorderLayer, FillLayer, MemoryLayer, LayerCount };

float effectiveDotSpacingPx(double scale) {
    float spacing = static_cast<float>(kBaseDotSpacingWorld * scale);
    while (spacing < kMinDotSpacingPx) spacing *= 2.0f;
    while (spacing > kMaxDotSpacingPx) spacing *= 0.5f;
    return spacing;
}

QColor faded(QColor color, int percent) {
    color.setAlpha(color.alpha() * percent / 100);
    return color;
}

void appendTriangles(std::vector<ColoredVertex>& out, const std::vector<Vec2>& points, const QColor& color) {
    for (const auto& point : points) out.push_back({point.x, point.y, color});
}

void appendDisc(std::vector<ColoredVertex>& out, Vec2 center, float radius, const QColor& color) {
    constexpr float kTwoPi = 6.283185307179586f;
    for (int i = 0; i < kCircleSegments; ++i) {
        float from = kTwoPi * static_cast<float>(i) / static_cast<float>(kCircleSegments);
        float to = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);
        out.push_back({center.x, center.y, color});
        out.push_back({center.x + radius * std::cos(from), center.y + radius * std::sin(from), color});
        out.push_back({center.x + radius * std::cos(to), center.y + radius * std::sin(to), color});
    }
}

void appendMemoryRing(std::vector<ColoredVertex>& out, Vec2 center, float radius, float thickness, double recall,
                      const Theme& theme, int alphaPercent) {
    if (ringBand(recall) == RingBand::New) {
        appendTriangles(out, dashedRing(center, radius, thickness, kNewRingDashes), faded(theme.ringNew, alphaPercent));
        return;
    }
    appendTriangles(out, ringArc(center, radius, thickness, 1.0), faded(theme.ringTrack, alphaPercent));
    appendTriangles(out, ringArc(center, radius, thickness, recall), faded(ringColor(theme, recall), alphaPercent));
}

std::vector<ColoredVertex> backgroundDots(float width, float height, double offsetX, double offsetY, double scale,
                                          const QColor& color) {
    std::vector<ColoredVertex> out;
    if (width <= 0.0f || height <= 0.0f) return out;
    float spacing = effectiveDotSpacingPx(scale);
    float phaseX = std::fmod(static_cast<float>(offsetX), spacing);
    if (phaseX < 0.0f) phaseX += spacing;
    float phaseY = std::fmod(static_cast<float>(offsetY), spacing);
    if (phaseY < 0.0f) phaseY += spacing;
    int columns = static_cast<int>(width / spacing) + 2;
    int rows = static_cast<int>(height / spacing) + 2;
    for (int column = -1; column < columns - 1; ++column) {
        for (int row = -1; row < rows - 1; ++row) {
            float cx = phaseX + static_cast<float>(column) * spacing;
            float cy = phaseY + static_cast<float>(row) * spacing;
            out.insert(out.end(), {{cx - kDotRadius, cy - kDotRadius, color}, {cx + kDotRadius, cy - kDotRadius, color},
                                   {cx + kDotRadius, cy + kDotRadius, color}, {cx - kDotRadius, cy - kDotRadius, color},
                                   {cx + kDotRadius, cy + kDotRadius, color}, {cx - kDotRadius, cy + kDotRadius, color}});
        }
    }
    return out;
}

QSGGeometryNode* makeColoredNode(QSGGeometry::DrawingMode mode) {
    auto* node = new QSGGeometryNode();
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
    geometry->setDrawingMode(mode);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(new QSGVertexColorMaterial());
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void upload(QSGNode* node, const std::vector<ColoredVertex>& vertices) {
    auto* geometryNode = static_cast<QSGGeometryNode*>(node);
    auto* geometry = geometryNode->geometry();
    geometry->allocate(static_cast<int>(vertices.size()));
    auto* data = geometry->vertexDataAsColoredPoint2D();
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& vertex = vertices[i];
        int alpha = vertex.color.alpha();
        auto premultiplied = [alpha](int channel) { return static_cast<uchar>((channel * alpha + 127) / 255); };
        data[i].set(vertex.x, vertex.y, premultiplied(vertex.color.red()), premultiplied(vertex.color.green()),
                    premultiplied(vertex.color.blue()), static_cast<uchar>(alpha));
    }
    geometryNode->markDirty(QSGNode::DirtyGeometry);
}

QFont labelFont() {
    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(kLabelPixelSize);
    return font;
}

}  // namespace

QString elideLabel(const QString& text, int maxChars) {
    if (text.size() <= maxChars) return text;
    return text.left(maxChars - 1) + QChar(0x2026);
}

std::vector<RenderGroup> summarizeGroups(const std::vector<RenderNode>& nodes,
                                         const std::unordered_map<QString, QPointF>& positions) {
    struct Accumulator {
        QString label;
        double sumX = 0.0;
        double sumY = 0.0;
        int count = 0;
        double recallSum = 0.0;
        int learned = 0;
    };
    std::map<QString, Accumulator> byKey;
    for (const auto& node : nodes) {
        if (node.ghost || node.groupKey.isEmpty()) continue;
        auto& group = byKey[node.groupKey];
        group.label = node.groupLabel;
        auto found = positions.find(node.id);
        QPointF point = found != positions.end() ? found->second : QPointF(node.x, node.y);
        group.sumX += point.x();
        group.sumY += point.y();
        ++group.count;
        if (node.recall >= 0.0) {
            group.recallSum += node.recall;
            ++group.learned;
        }
    }
    std::vector<RenderGroup> groups;
    for (const auto& [key, group] : byKey) {
        groups.push_back(RenderGroup{key, group.label, QPointF(group.sumX / group.count, group.sumY / group.count),
                                     group.learned > 0 ? group.recallSum / group.learned : -1.0, group.count});
    }
    return groups;
}

GraphCanvasItem::GraphCanvasItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    animationTimer_ = new QTimer(this);
    animationTimer_->setInterval(kAnimationIntervalMs);
    connect(animationTimer_, &QTimer::timeout, this, &GraphCanvasItem::tickAnimation);
}

GraphCanvasItem::LabelLayout GraphCanvasItem::makeLabel(const QString& text) {
    LabelLayout label;
    label.text = text;
    label.layout = std::make_shared<QTextLayout>(text, labelFont());
    label.layout->setCacheEnabled(true);
    label.layout->beginLayout();
    QTextLine line = label.layout->createLine();
    if (line.isValid()) {
        line.setLineWidth(10000.0);
        label.width = line.naturalTextWidth();
    }
    label.layout->endLayout();
    return label;
}

void GraphCanvasItem::syncLabels(std::unordered_map<QString, LabelLayout>& cache,
                                 const std::unordered_map<QString, QString>& wanted) {
    for (const auto& [key, text] : wanted) {
        auto found = cache.find(key);
        if (found == cache.end() || found->second.text != text) cache.insert_or_assign(key, makeLabel(text));
    }
    for (auto it = cache.begin(); it != cache.end();) {
        it = wanted.contains(it->first) ? std::next(it) : cache.erase(it);
    }
}

const QTextLayout* GraphCanvasItem::labelLayoutFor(const QString& id) const {
    auto found = labelLayouts_.find(id);
    return found == labelLayouts_.end() ? nullptr : found->second.layout.get();
}

void GraphCanvasItem::setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges) {
    nodes_ = std::move(nodes);
    edges_ = std::move(edges);

    std::unordered_map<QString, QPointF> targets;
    targets.reserve(nodes_.size());
    for (const auto& node : nodes_) targets.emplace(node.id, QPointF(node.x, node.y));
    for (const auto& [id, target] : targets) {
        if (!currentPositions_.contains(id)) currentPositions_[id] = target;
    }
    for (auto it = currentPositions_.begin(); it != currentPositions_.end();) {
        it = targets.contains(it->first) ? std::next(it) : currentPositions_.erase(it);
    }
    targetPositions_ = std::move(targets);

    std::unordered_map<QString, QString> nodeLabels;
    for (const auto& node : nodes_) {
        if (!node.label.isEmpty()) nodeLabels.emplace(node.id, elideLabel(node.label));
    }
    syncLabels(labelLayouts_, nodeLabels);
    std::unordered_map<QString, QString> groupLabels;
    for (const auto& group : summarizeGroups(nodes_, targetPositions_)) {
        groupLabels.emplace(group.key, QStringLiteral("%1 (%2)").arg(elideLabel(group.label)).arg(group.memberCount));
    }
    syncLabels(groupLayouts_, groupLabels);

    bool hadGroups = hasGroups_;
    hasGroups_ = std::any_of(nodes_.begin(), nodes_.end(), [](const RenderNode& node) { return !node.groupKey.isEmpty(); });
    dataDirty_ = true;
    labelsDirty_ = true;
    update();
    if (hadGroups != hasGroups_ && scale_ < kCollapseZoom) emit zoomChanged();
    if (!animationTimer_->isActive()) animationTimer_->start();
    fitOnce();
}

void GraphCanvasItem::fitOnce() {
    if (fitted_ || nodes_.empty() || width() <= 0.0 || height() <= 0.0) return;
    fitted_ = true;
    bool wasAnimated = animated_;
    animated_ = false;
    fitToContent();
    animated_ = wasAnimated;
}

void GraphCanvasItem::tickAnimation() {
    bool stillMoving = false;
    bool nodesChanged = false;
    for (auto& [id, current] : currentPositions_) {
        auto target = targetPositions_.find(id);
        if (target == targetPositions_.end()) continue;
        QPointF delta = target->second - current;
        if (delta.x() * delta.x() + delta.y() * delta.y() <= kAnimationEpsilonSq) {
            if (current != target->second) nodesChanged = true;
            current = target->second;
            continue;
        }
        stillMoving = true;
        nodesChanged = true;
        current += delta * kEaseFactor;
    }
    if (cameraMoving_) {
        double nextScale = scale_ + (targetScale_ - scale_) * kEaseFactor;
        double nextX = offsetX_ + (targetOffsetX_ - offsetX_) * kEaseFactor;
        double nextY = offsetY_ + (targetOffsetY_ - offsetY_) * kEaseFactor;
        bool arrived = std::abs(targetOffsetX_ - nextX) < kCameraEpsilon && std::abs(targetOffsetY_ - nextY) < kCameraEpsilon &&
                       std::abs(targetScale_ - nextScale) < 1e-3;
        if (arrived) {
            nextScale = targetScale_;
            nextX = targetOffsetX_;
            nextY = targetOffsetY_;
            cameraMoving_ = false;
        }
        applyCamera(nextScale, nextX, nextY);
        stillMoving = stillMoving || cameraMoving_;
    }
    if (nodesChanged) dataDirty_ = true;
    update();
    if (!stillMoving) animationTimer_->stop();
}

void GraphCanvasItem::setHighlight(const QString& selectedId, const std::unordered_set<QString>& neighborIds) {
    if (selectedId == selectedId_ && neighborIds == neighborIds_) return;
    selectedId_ = selectedId;
    neighborIds_ = neighborIds;
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::clearHighlight() {
    if (selectedId_.isEmpty() && neighborIds_.empty()) return;
    selectedId_.clear();
    neighborIds_.clear();
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::setTheme(ThemeMode mode) {
    if (themeMode_ == mode) return;
    themeMode_ = mode;
    backgroundDirty_ = true;
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::setAnimated(bool animated) {
    if (animated == animated_) return;
    animated_ = animated;
    emit animatedChanged();
}

bool GraphCanvasItem::insideCull(QPointF world) const { return cullRect_.contains(world); }

void GraphCanvasItem::viewMoved() {
    backgroundDirty_ = true;
    labelsDirty_ = true;
    QRectF viewport = viewportInWorld(offsetX_, offsetY_, scale_, width(), height());
    if (!cullRect_.contains(viewport)) dataDirty_ = true;
    update();
    emit viewChanged();
}

void GraphCanvasItem::applyCamera(double scale, double offsetX, double offsetY) {
    bool zoomed = scale != scale_;
    bool wasCollapsed = collapsed();
    scale_ = scale;
    offsetX_ = offsetX;
    offsetY_ = offsetY;
    if (collapsed() != wasCollapsed) dataDirty_ = true;
    viewMoved();
    if (zoomed) emit zoomChanged();
}

void GraphCanvasItem::moveCamera(double scale, double offsetX, double offsetY) {
    scale = std::clamp(scale, kMinScale, kMaxScale);
    if (!animated_) {
        cameraMoving_ = false;
        applyCamera(scale, offsetX, offsetY);
        return;
    }
    targetScale_ = scale;
    targetOffsetX_ = offsetX;
    targetOffsetY_ = offsetY;
    cameraMoving_ = true;
    if (!animationTimer_->isActive()) animationTimer_->start();
}

void GraphCanvasItem::centerOn(const QString& id) {
    auto target = targetPositions_.find(id);
    if (target == targetPositions_.end()) return;
    double scale = cameraMoving_ ? targetScale_ : scale_;
    moveCamera(scale, width() / 2.0 - target->second.x() * scale, height() / 2.0 - target->second.y() * scale);
}

void GraphCanvasItem::fitToContent() {
    if (width() <= 0.0 || height() <= 0.0) return;
    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    bool any = false;
    for (const auto& node : nodes_) {
        if (node.ghost && nodes_.size() > 1) continue;
        auto position = targetPositions_.find(node.id);
        if (position == targetPositions_.end()) continue;
        double x = position->second.x();
        double y = position->second.y();
        minX = any ? std::min(minX, x) : x;
        maxX = any ? std::max(maxX, x) : x;
        minY = any ? std::min(minY, y) : y;
        maxY = any ? std::max(maxY, y) : y;
        any = true;
    }
    if (!any) return;
    double spanX = std::max(maxX - minX, 1.0);
    double spanY = std::max(maxY - minY, 1.0);
    double scale = std::min((width() - 2.0 * kFitMarginPx) / spanX, (height() - 2.0 * kFitMarginPx) / spanY);
    scale = std::clamp(scale, kMinScale, kFitMaxScale);
    QPointF center((minX + maxX) / 2.0, (minY + maxY) / 2.0);
    moveCamera(scale, width() / 2.0 - center.x() * scale, height() / 2.0 - center.y() * scale);
}

void GraphCanvasItem::zoomAt(double factor, double screenX, double screenY) {
    double scale = std::clamp(scale_ * factor, kMinScale, kMaxScale);
    double worldX = (screenX - offsetX_) / scale_;
    double worldY = (screenY - offsetY_) / scale_;
    cameraMoving_ = false;
    applyCamera(scale, screenX - worldX * scale, screenY - worldY * scale);
}

QVariant GraphCanvasItem::screenPositionOf(const QString& id) const {
    auto position = currentPositions_.find(id);
    if (position == currentPositions_.end()) return {};
    return toScreen(position->second);
}

QString GraphCanvasItem::linkAt(double screenX, double screenY) const {
    if (collapsed()) return {};
    QPointF point(screenX, screenY);
    for (const auto& edge : edges_) {
        auto from = currentPositions_.find(edge.sourceId);
        auto to = currentPositions_.find(edge.targetId);
        if (from == currentPositions_.end() || to == currentPositions_.end()) continue;
        if (distanceToSegment(point, toScreen(from->second), toScreen(to->second)) <= kLinkHoverPx) return edge.id;
    }
    return {};
}

void GraphCanvasItem::setZoom(double zoom) {
    double clamped = std::clamp(zoom, kMinScale, kMaxScale);
    if (clamped == scale_) return;
    zoomAt(clamped / scale_, width() / 2.0, height() / 2.0);
}

void GraphCanvasItem::zoomBy(double factor) { zoomAt(factor, width() / 2.0, height() / 2.0); }

Vec2 GraphCanvasItem::positionOf(const QString& id) const {
    auto found = currentPositions_.find(id);
    if (found == currentPositions_.end()) return {};
    return Vec2{static_cast<float>(found->second.x()), static_cast<float>(found->second.y())};
}

QPointF GraphCanvasItem::toScreen(QPointF world) const {
    return QPointF(world.x() * scale_ + offsetX_, world.y() * scale_ + offsetY_);
}

QString GraphCanvasItem::groupAt(double screenX, double screenY) const {
    if (!collapsed()) return {};
    for (const auto& group : summarizeGroups(nodes_, currentPositions_)) {
        QPointF center = toScreen(group.center);
        double dx = screenX - center.x();
        double dy = screenY - center.y();
        if (dx * dx + dy * dy <= kGroupRadiusPx * kGroupRadiusPx) return group.key;
    }
    return {};
}

int GraphCanvasItem::hitTest(double worldX, double worldY) const {
    double radiusSq = kHitRadius * kHitRadius;
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        auto found = currentPositions_.find(nodes_[static_cast<size_t>(i)].id);
        if (found == currentPositions_.end()) continue;
        double dx = worldX - found->second.x();
        double dy = worldY - found->second.y();
        if (dx * dx + dy * dy <= radiusSq) return i;
    }
    return -1;
}

void GraphCanvasItem::buildHighlight(SceneVertices& out, const Theme& theme) const {
    bool hasHighlight = !selectedId_.isEmpty();
    for (const auto& node : nodes_) {
        Vec2 center = positionOf(node.id);
        bool isSelected = node.id == selectedId_;
        bool isNeighbor = hasHighlight && neighborIds_.contains(node.id);
        bool isHovered = !isSelected && node.id == hoveredId_;
        if (isSelected) appendTriangles(out.highlight, ringArc(center, kSelectRingRadius, kSelectRingThickness, 1.0), theme.selectedRing);
        else if (isNeighbor) appendTriangles(out.highlight, ringArc(center, kNeighborRingRadius, kHighlightThickness, 1.0), theme.neighborRing);
        else if (isHovered) appendTriangles(out.highlight, ringArc(center, kHoverRingRadius, kHighlightThickness, 1.0), theme.hoverRing);
    }
}

void GraphCanvasItem::buildNodes(SceneVertices& out, const Theme& theme) const {
    bool hasHighlight = !selectedId_.isEmpty();
    for (const auto& node : nodes_) {
        Vec2 center = positionOf(node.id);
        if (!insideCull(QPointF(center.x, center.y))) continue;
        bool isSelected = node.id == selectedId_;
        bool isNeighbor = hasHighlight && neighborIds_.contains(node.id);
        int alpha = node.ghost ? kGhostAlphaPercent : 100;
        bool dimmed = hasHighlight && !isSelected && !isNeighbor;
        QColor border = faded(theme.nodeBorder, alpha);
        QColor fill = faded(node.color.isValid() ? node.color : theme.nodeFill, alpha);
        if (dimmed) {
            border.setAlpha(std::min(border.alpha(), kDimmedAlpha));
            fill.setAlpha(std::min(fill.alpha(), kDimmedAlpha));
        }
        appendDisc(out.borders, center, kNodeRadius + kNodeBorderWidth, border);
        appendDisc(out.fills, center, kNodeRadius, fill);
        appendMemoryRing(out.memory, center, kMemoryRingRadius, kMemoryRingThickness, node.recall, theme,
                         dimmed ? std::min(alpha, kDimmedRingAlphaPercent) : alpha);
    }
}

void GraphCanvasItem::buildEdges(SceneVertices& out, const Theme& theme) const {
    bool hasHighlight = !selectedId_.isEmpty();
    float trim = kMemoryRingRadius + kMemoryRingThickness / 2 + 2.0f;
    for (const auto& edge : edges_) {
        Vec2 from = positionOf(edge.sourceId);
        Vec2 to = positionOf(edge.targetId);
        if (!segmentMayCross(cullRect_, QPointF(from.x, from.y), QPointF(to.x, to.y))) continue;
        bool touchesSelection = edge.sourceId == selectedId_ || edge.targetId == selectedId_;
        QColor color = hasHighlight && !touchesSelection ? theme.edgeDimmed : theme.edge;
        if (edge.ghost) color = faded(color, kGhostAlphaPercent);

        Vec2 start = trimmedEnd(to, from, trim);
        Vec2 end = trimmedEnd(from, to, edge.directed ? trim + kArrowLength : trim);
        std::vector<LineSegment> segments =
            edge.contrast ? dashedLine(start, end, kDashLength, kDashGap) : std::vector<LineSegment>{{start, end}};
        for (const auto& segment : segments) {
            out.edges.push_back({segment.from.x, segment.from.y, color});
            out.edges.push_back({segment.to.x, segment.to.y, color});
        }
        if (edge.directed) {
            for (const auto& point : arrowHead(from, to, trim, kArrowLength, kArrowHalfWidth)) {
                out.arrows.push_back({point.x, point.y, color});
            }
        }
    }
}

void GraphCanvasItem::buildGroups(SceneVertices& out, const Theme& theme) const {
    float pixel = 1.0f / static_cast<float>(scale_);
    float radius = kGroupRadiusPx * pixel;
    for (const auto& group : summarizeGroups(nodes_, currentPositions_)) {
        Vec2 center{static_cast<float>(group.center.x()), static_cast<float>(group.center.y())};
        appendDisc(out.borders, center, radius + kNodeBorderWidth * pixel, theme.nodeBorder);
        appendDisc(out.fills, center, radius, theme.nodeFill);
        appendMemoryRing(out.memory, center, radius + 4.0f * pixel, 3.0f * pixel, group.meanRecall, theme, 100);
    }
}

void GraphCanvasItem::fillLabels(QSGTextNode* labels, QSGTextNode* ghostLabels, const Theme& theme) const {
    labels->clear();
    ghostLabels->clear();
    labels->setColor(theme.label);
    ghostLabels->setColor(faded(theme.label, kGhostAlphaPercent));

    auto visible = [this](QPointF screen) {
        return screen.x() > -kCullMarginPx && screen.x() < width() + kCullMarginPx && screen.y() > -kCullMarginPx &&
               screen.y() < height() + kCullMarginPx;
    };

    if (collapsed()) {
        for (const auto& group : summarizeGroups(nodes_, currentPositions_)) {
            auto label = groupLayouts_.find(group.key);
            QPointF screen = toScreen(group.center);
            if (label == groupLayouts_.end() || !visible(screen)) continue;
            labels->addTextLayout(QPointF(screen.x() - label->second.width / 2.0, screen.y() + kGroupRadiusPx + kLabelGapPx),
                                  label->second.layout.get());
        }
        return;
    }
    if (scale_ < kLabelZoom) return;

    double below = (kMemoryRingRadius + kMemoryRingThickness) * scale_ + kLabelGapPx;
    for (const auto& node : nodes_) {
        auto label = labelLayouts_.find(node.id);
        auto position = currentPositions_.find(node.id);
        if (label == labelLayouts_.end() || position == currentPositions_.end()) continue;
        QPointF screen = toScreen(position->second);
        if (!visible(screen)) continue;
        QPointF at(screen.x() - label->second.width / 2.0, screen.y() + below);
        (node.ghost ? ghostLabels : labels)->addTextLayout(at, label->second.layout.get());
    }
}

QSGNode* GraphCanvasItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QSGNode* container = oldNode;
    if (container == nullptr) {
        container = new QSGNode();
        container->appendChildNode(makeColoredNode(QSGGeometry::DrawTriangles));
        auto* root = new QSGTransformNode();
        for (int layer = 0; layer < LayerCount; ++layer) {
            root->appendChildNode(makeColoredNode(layer == EdgeLayer ? QSGGeometry::DrawLines : QSGGeometry::DrawTriangles));
        }
        container->appendChildNode(root);
        container->appendChildNode(window()->createTextNode());
        container->appendChildNode(window()->createTextNode());
        backgroundDirty_ = true;
        dataDirty_ = true;
        labelsDirty_ = true;
    }
    auto* background = container->childAtIndex(0);
    auto* root = static_cast<QSGTransformNode*>(container->childAtIndex(1));
    auto* labels = static_cast<QSGTextNode*>(container->childAtIndex(2));
    auto* ghostLabels = static_cast<QSGTextNode*>(container->childAtIndex(3));

    QMatrix4x4 matrix;
    matrix.translate(static_cast<float>(offsetX_), static_cast<float>(offsetY_));
    matrix.scale(static_cast<float>(scale_));
    root->setMatrix(matrix);

    const Theme& theme = themeFor(themeMode_);
    if (backgroundDirty_) {
        upload(background, backgroundDots(static_cast<float>(width()), static_cast<float>(height()), offsetX_,
                                          offsetY_, scale_, theme.dot));
        backgroundDirty_ = false;
    }
    if (dataDirty_) {
        cullRect_ = cullRectFor(viewportInWorld(offsetX_, offsetY_, scale_, width(), height()));
        SceneVertices scene;
        if (collapsed()) {
            buildGroups(scene, theme);
        } else {
            buildHighlight(scene, theme);
            buildNodes(scene, theme);
            buildEdges(scene, theme);
        }
        upload(root->childAtIndex(HighlightLayer), scene.highlight);
        upload(root->childAtIndex(EdgeLayer), scene.edges);
        upload(root->childAtIndex(ArrowLayer), scene.arrows);
        upload(root->childAtIndex(BorderLayer), scene.borders);
        upload(root->childAtIndex(FillLayer), scene.fills);
        upload(root->childAtIndex(MemoryLayer), scene.memory);
        dataDirty_ = false;
        highlightDirty_ = false;
        labelsDirty_ = true;
    } else if (highlightDirty_) {
        SceneVertices scene;
        if (!collapsed()) buildHighlight(scene, theme);
        upload(root->childAtIndex(HighlightLayer), scene.highlight);
        highlightDirty_ = false;
    }
    if (labelsDirty_) {
        fillLabels(labels, ghostLabels, theme);
        labelsDirty_ = false;
    }
    return container;
}

void GraphCanvasItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragMoved_ = false;
        lastMousePos_ = event->position();
    } else if (event->button() == Qt::RightButton && !collapsed()) {
        int hit = hitTest((event->position().x() - offsetX_) / scale_, (event->position().y() - offsetY_) / scale_);
        if (hit >= 0) emit nodeRightClicked(nodes_[static_cast<size_t>(hit)].id);
    }
    event->accept();
}

void GraphCanvasItem::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;
    QPointF delta = event->position() - lastMousePos_;
    if (std::abs(delta.x()) > kDragThreshold || std::abs(delta.y()) > kDragThreshold) dragMoved_ = true;
    cameraMoving_ = false;
    offsetX_ += delta.x();
    offsetY_ += delta.y();
    lastMousePos_ = event->position();
    viewMoved();
    event->accept();
}

void GraphCanvasItem::mouseReleaseEvent(QMouseEvent* event) {
    if (!dragMoved_ && event->button() == Qt::LeftButton) {
        if (collapsed()) {
            QString key = groupAt(event->position().x(), event->position().y());
            if (!key.isEmpty()) emit groupClicked(key);
        } else {
            int hit = hitTest((event->position().x() - offsetX_) / scale_, (event->position().y() - offsetY_) / scale_);
            if (hit >= 0) {
                emit nodeClicked(nodes_[static_cast<size_t>(hit)].id);
            } else if (!selectedId_.isEmpty()) {
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
    QString hovered;
    if (!collapsed()) {
        int hit = hitTest((event->position().x() - offsetX_) / scale_, (event->position().y() - offsetY_) / scale_);
        if (hit >= 0) hovered = nodes_[static_cast<size_t>(hit)].id;
    }
    if (hovered != hoveredId_) {
        hoveredId_ = hovered;
        highlightDirty_ = true;
        update();
        emit nodeHovered(hoveredId_);
    }
    QString link = hovered.isEmpty() ? linkAt(event->position().x(), event->position().y()) : QString();
    if (link != hoveredLinkId_) {
        hoveredLinkId_ = link;
        emit linkHovered(hoveredLinkId_);
    }
    event->accept();
}

void GraphCanvasItem::hoverLeaveEvent(QHoverEvent* event) {
    if (!hoveredId_.isEmpty()) {
        hoveredId_.clear();
        highlightDirty_ = true;
        update();
        emit nodeHovered(QString{});
    }
    if (!hoveredLinkId_.isEmpty()) {
        hoveredLinkId_.clear();
        emit linkHovered(QString{});
    }
    event->accept();
}

void GraphCanvasItem::wheelEvent(QWheelEvent* event) {
    zoomAt(event->angleDelta().y() > 0 ? kWheelStep : 1.0 / kWheelStep, event->position().x(), event->position().y());
    event->accept();
}

void GraphCanvasItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    backgroundDirty_ = true;
    labelsDirty_ = true;
    update();
    fitOnce();
}

void registerGraphCanvasQmlType() { qmlRegisterType<GraphCanvasItem>("Atlas.Render", 1, 0, "GraphCanvas"); }

}  // namespace atlas::render
