#include "atlas/render/node_style.hpp"
#include "doctest.h"

using namespace atlas::render;

TEST_CASE("the halo stays inside the memory ring at every degree") {
    float ringInner = kMemoryRingRadius - kMemoryRingThickness / 2;
    for (int degree : {0, 1, 3, 5, 12, 400}) CHECK(dotRadius(degree) + kNodeHalo <= ringInner);
    CHECK(dotRadius(0) == doctest::Approx(kNodeRadius));
    CHECK(dotRadius(1) > dotRadius(0));
    CHECK(dotRadius(-3) == doctest::Approx(kNodeRadius));
}

TEST_CASE("links stop outside every selection, neighbor, and hover ring") {
    float trim = linkTrim();
    CHECK(trim >= kSelectRingRadius + kSelectRingThickness / 2 + 1.0f);
    CHECK(trim > kNeighborRingRadius + kHighlightThickness / 2);
    CHECK(trim > kHoverRingRadius + kHighlightThickness / 2);
    CHECK(trim > kMemoryRingRadius + kMemoryRingThickness / 2);
}
