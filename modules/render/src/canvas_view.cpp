#include "atlas/render/canvas_view.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::render {

QRectF viewportInWorld(double offsetX, double offsetY, double scale, double width, double height) {
    return QRectF(-offsetX / scale, -offsetY / scale, width / scale, height / scale);
}

QRectF cullRectFor(const QRectF& viewport) {
    return viewport.adjusted(-viewport.width(), -viewport.height(), viewport.width(), viewport.height());
}

bool segmentMayCross(const QRectF& rect, QPointF from, QPointF to) {
    QRectF bounds = QRectF(from, to).normalized();
    return bounds.right() >= rect.left() && bounds.left() <= rect.right() && bounds.bottom() >= rect.top() &&
           bounds.top() <= rect.bottom();
}

double distanceToSegment(QPointF point, QPointF from, QPointF to) {
    QPointF segment = to - from;
    double lengthSq = segment.x() * segment.x() + segment.y() * segment.y();
    double t = lengthSq > 0.0 ? QPointF::dotProduct(point - from, segment) / lengthSq : 0.0;
    QPointF closest = from + segment * std::clamp(t, 0.0, 1.0);
    QPointF delta = point - closest;
    return std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
}

}  // namespace atlas::render
