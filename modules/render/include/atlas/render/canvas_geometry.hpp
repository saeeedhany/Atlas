#pragma once

#include <array>
#include <vector>

namespace atlas::render {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct LineSegment {
    Vec2 from;
    Vec2 to;
};

std::vector<Vec2> ringArc(Vec2 center, float radius, float thickness, double fraction);
std::vector<Vec2> dashedRing(Vec2 center, float radius, float thickness, int dashCount);
std::vector<LineSegment> dashedLine(Vec2 from, Vec2 to, float dash, float gap);
Vec2 trimmedEnd(Vec2 from, Vec2 to, float radius);
std::array<Vec2, 3> arrowHead(Vec2 from, Vec2 to, float targetRadius, float length, float halfWidth);

}
