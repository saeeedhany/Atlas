#include "atlas/render/canvas_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kStartAngle = -kPi / 2.0;
constexpr int kRingSegments = 48;

Vec2 polar(Vec2 center, float radius, double angle) {
    return Vec2{center.x + radius * static_cast<float>(std::cos(angle)),
                center.y + radius * static_cast<float>(std::sin(angle))};
}

void appendBand(std::vector<Vec2>& out, Vec2 center, float inner, float outer, double from, double to) {
    Vec2 innerFrom = polar(center, inner, from);
    Vec2 outerFrom = polar(center, outer, from);
    Vec2 innerTo = polar(center, inner, to);
    Vec2 outerTo = polar(center, outer, to);
    out.insert(out.end(), {innerFrom, outerFrom, outerTo, innerFrom, outerTo, innerTo});
}

float lengthOf(Vec2 vector) { return std::sqrt(vector.x * vector.x + vector.y * vector.y); }

}

std::vector<Vec2> ringArc(Vec2 center, float radius, float thickness, double fraction) {
    std::vector<Vec2> out;
    if (!(fraction > 0.0)) return out;
    fraction = std::min(fraction, 1.0);
    int segments = static_cast<int>(std::ceil(fraction * kRingSegments));
    double sweep = 2.0 * kPi * fraction;
    float inner = radius - thickness / 2.0f;
    float outer = radius + thickness / 2.0f;
    for (int i = 0; i < segments; ++i) {
        appendBand(out, center, inner, outer, kStartAngle + sweep * i / segments,
                   kStartAngle + sweep * (i + 1) / segments);
    }
    return out;
}

std::vector<Vec2> dashedRing(Vec2 center, float radius, float thickness, int dashCount) {
    std::vector<Vec2> out;
    if (dashCount <= 0) return out;
    float inner = radius - thickness / 2.0f;
    float outer = radius + thickness / 2.0f;
    double period = 2.0 * kPi / dashCount;
    for (int i = 0; i < dashCount; ++i) {
        double from = kStartAngle + period * i;
        appendBand(out, center, inner, outer, from, from + period / 2.0);
    }
    return out;
}

std::vector<LineSegment> dashedLine(Vec2 from, Vec2 to, float dash, float gap) {
    std::vector<LineSegment> out;
    Vec2 delta{to.x - from.x, to.y - from.y};
    float total = lengthOf(delta);
    if (total <= 0.0f || dash <= 0.0f || !(dash + gap > 0.0f)) return out;
    Vec2 unit{delta.x / total, delta.y / total};
    for (float start = 0.0f; start < total; start += dash + gap) {
        float end = std::min(start + dash, total);
        out.push_back({{from.x + unit.x * start, from.y + unit.y * start}, {from.x + unit.x * end, from.y + unit.y * end}});
    }
    return out;
}

Vec2 trimmedEnd(Vec2 from, Vec2 to, float radius) {
    Vec2 delta{to.x - from.x, to.y - from.y};
    float length = lengthOf(delta);
    if (length <= 0.0f) return to;
    return Vec2{to.x - delta.x / length * radius, to.y - delta.y / length * radius};
}

std::array<Vec2, 3> arrowHead(Vec2 from, Vec2 to, float targetRadius, float length, float halfWidth) {
    Vec2 delta{to.x - from.x, to.y - from.y};
    float distance = lengthOf(delta);
    if (distance <= 0.0f) return {to, to, to};
    Vec2 unit{delta.x / distance, delta.y / distance};
    Vec2 tip = trimmedEnd(from, to, targetRadius);
    Vec2 base{tip.x - unit.x * length, tip.y - unit.y * length};
    Vec2 normal{-unit.y, unit.x};
    return {tip, Vec2{base.x + normal.x * halfWidth, base.y + normal.y * halfWidth},
            Vec2{base.x - normal.x * halfWidth, base.y - normal.y * halfWidth}};
}

}
