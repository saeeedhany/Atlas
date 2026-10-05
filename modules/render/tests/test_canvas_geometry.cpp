#include <cmath>

#include "atlas/render/canvas_geometry.hpp"
#include "doctest.h"

using namespace atlas::render;

namespace {
float distanceTo(Vec2 point, Vec2 center) { return std::hypot(point.x - center.x, point.y - center.y); }
}

TEST_CASE("ring arcs grow with the fraction and ignore invalid input") {
    Vec2 center{0.0f, 0.0f};
    CHECK(ringArc(center, 10.0f, 2.0f, 0.0).empty());
    CHECK(ringArc(center, 10.0f, 2.0f, -1.0).empty());
    CHECK(ringArc(center, 10.0f, 2.0f, std::nan("")).empty());
    CHECK(ringArc(center, 10.0f, 2.0f, 0.5).size() == 24 * 6);
    CHECK(ringArc(center, 10.0f, 2.0f, 1.0).size() == 48 * 6);
    CHECK(ringArc(center, 10.0f, 2.0f, 2.0).size() == 48 * 6);
}

TEST_CASE("a ring arc starts at twelve o'clock and stays inside its band") {
    Vec2 center{0.0f, 0.0f};
    auto arc = ringArc(center, 10.0f, 2.0f, 1.0);
    CHECK(arc[0].x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(arc[0].y == doctest::Approx(-9.0f));
    for (const auto& point : arc) {
        CHECK(distanceTo(point, center) >= 9.0f - 1e-3f);
        CHECK(distanceTo(point, center) <= 11.0f + 1e-3f);
    }
}

TEST_CASE("a dashed line without a forward step is empty") {
    CHECK(dashedLine({0.0f, 0.0f}, {10.0f, 0.0f}, 3.0f, -3.0f).empty());
    CHECK(dashedLine({0.0f, 0.0f}, {10.0f, 0.0f}, 3.0f, -5.0f).empty());
    CHECK(dashedLine({0.0f, 0.0f}, {10.0f, 0.0f}, 3.0f, std::nanf("")).empty());
}

TEST_CASE("a dashed ring has one band per dash") {
    CHECK(dashedRing({0.0f, 0.0f}, 10.0f, 2.0f, 12).size() == 12 * 6);
}

TEST_CASE("dashed lines alternate dash and gap and stop at the end") {
    auto segments = dashedLine({0.0f, 0.0f}, {10.0f, 0.0f}, 3.0f, 2.0f);
    REQUIRE(segments.size() == 2);
    CHECK(segments[0].from.x == doctest::Approx(0.0f));
    CHECK(segments[0].to.x == doctest::Approx(3.0f));
    CHECK(segments[1].from.x == doctest::Approx(5.0f));
    CHECK(segments[1].to.x == doctest::Approx(8.0f));
    CHECK(dashedLine({1.0f, 1.0f}, {1.0f, 1.0f}, 3.0f, 2.0f).empty());
}

TEST_CASE("arrowheads sit on the target circle and point at it") {
    CHECK(trimmedEnd({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f).x == doctest::Approx(8.0f));
    auto head = arrowHead({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f, 6.0f, 3.0f);
    CHECK(head[0].x == doctest::Approx(8.0f));
    CHECK(head[0].y == doctest::Approx(0.0f));
    CHECK(head[1].x == doctest::Approx(2.0f));
    CHECK(std::abs(head[1].y) == doctest::Approx(3.0f));
    CHECK(head[2].x == doctest::Approx(2.0f));
    CHECK(head[1].y == doctest::Approx(-head[2].y));
}
