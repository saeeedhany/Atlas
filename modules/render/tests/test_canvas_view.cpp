#include <cmath>

#include "atlas/render/canvas_view.hpp"
#include "doctest.h"

using namespace atlas::render;

TEST_CASE("the viewport maps screen space into world space") {
    QRectF viewport = viewportInWorld(-100.0, -50.0, 2.0, 800.0, 600.0);
    CHECK(viewport.left() == doctest::Approx(50.0));
    CHECK(viewport.top() == doctest::Approx(25.0));
    CHECK(viewport.width() == doctest::Approx(400.0));
    CHECK(viewport.height() == doctest::Approx(300.0));
}

TEST_CASE("the cull rect grows the viewport by one viewport on every side") {
    QRectF cull = cullRectFor(QRectF(0, 0, 100, 50));
    CHECK(cull == QRectF(-100, -50, 300, 150));
}

TEST_CASE("segments are kept when their bounds touch the rect") {
    QRectF rect(0, 0, 10, 10);
    CHECK(segmentMayCross(rect, {-50, 5}, {50, 5}));
    CHECK(segmentMayCross(rect, {5, 5}, {6, 6}));
    CHECK_FALSE(segmentMayCross(rect, {20, 20}, {30, 40}));
}

TEST_CASE("distance to a segment clamps to its ends") {
    CHECK(distanceToSegment({5, 3}, {0, 0}, {10, 0}) == doctest::Approx(3.0));
    CHECK(distanceToSegment({-3, 4}, {0, 0}, {10, 0}) == doctest::Approx(5.0));
    CHECK(distanceToSegment({2, 2}, {1, 1}, {1, 1}) == doctest::Approx(std::sqrt(2.0)));
}
