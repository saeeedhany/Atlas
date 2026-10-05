#include "atlas/render/region_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace atlas::render {

QRectF regionBounds(const std::vector<QPointF>& points, double padding) {
    if (points.empty()) return {};
    double left = points.front().x();
    double right = left;
    double top = points.front().y();
    double bottom = top;
    for (const auto& point : points) {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return QRectF(left - padding, top - padding, right - left + 2 * padding, bottom - top + 2 * padding);
}

int regionHue(const QString& key) {
    std::uint32_t hash = 2166136261u;
    for (QChar unit : key) {
        hash ^= unit.unicode();
        hash *= 16777619u;
    }
    return static_cast<int>(hash % 6u);
}

std::vector<QPointF> roundedRectOutline(const QRectF& rect, double radius, int cornerSegments) {
    double r = std::clamp(radius, 0.0, std::max(0.0, std::min(rect.width(), rect.height()) / 2.0));
    const QPointF centers[4] = {{rect.right() - r, rect.top() + r},
                                {rect.right() - r, rect.bottom() - r},
                                {rect.left() + r, rect.bottom() - r},
                                {rect.left() + r, rect.top() + r}};
    constexpr double kQuarter = 1.5707963267948966;
    std::vector<QPointF> outline;
    outline.reserve(static_cast<size_t>(4 * (cornerSegments + 1)));
    for (int corner = 0; corner < 4; ++corner) {
        double start = -kQuarter + corner * kQuarter;
        for (int step = 0; step <= cornerSegments; ++step) {
            double angle = start + kQuarter * step / cornerSegments;
            outline.emplace_back(centers[corner].x() + r * std::cos(angle), centers[corner].y() + r * std::sin(angle));
        }
    }
    return outline;
}

}  // namespace atlas::render
