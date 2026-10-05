#include "atlas/render/region_geometry.hpp"
#include "doctest.h"

using namespace atlas::render;

TEST_CASE("region bounds pad every member") {
    QRectF rect = regionBounds({{0, 0}, {100, 40}}, 48);
    CHECK(rect == QRectF(-48, -48, 196, 136));
    CHECK(regionBounds({{10, 10}}, 48) == QRectF(-38, -38, 96, 96));
    CHECK(regionBounds({}, 48).isEmpty());
}

TEST_CASE("region hues are stable and in range") {
    int hue = regionHue("1b9d6bcd-bbfd-4b2d-9b5d-ab8dfbbd4bed");
    CHECK(hue == regionHue("1b9d6bcd-bbfd-4b2d-9b5d-ab8dfbbd4bed"));
    for (const char* key : {"a", "b", "topic", "another topic", ""}) {
        int value = regionHue(key);
        CHECK(value >= 0);
        CHECK(value < 6);
    }
}

TEST_CASE("rounded outlines stay inside their rect and clamp the radius") {
    auto outline = roundedRectOutline(QRectF(0, 0, 100, 40), 28, 6);
    CHECK(outline.size() == 4 * 7);
    for (const auto& point : outline) {
        CHECK(point.x() >= -1e-9);
        CHECK(point.x() <= 100 + 1e-9);
        CHECK(point.y() >= -1e-9);
        CHECK(point.y() <= 40 + 1e-9);
    }
}
