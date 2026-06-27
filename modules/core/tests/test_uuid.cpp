#include "atlas/core/uuid.hpp"
#include "doctest.h"

using atlas::core::Uuid;

TEST_CASE("Uuid::generate produces distinct values") {
    auto a = Uuid::generate();
    auto b = Uuid::generate();
    CHECK(!(a == b));
}

TEST_CASE("Uuid round-trips through toString/parse") {
    auto original = Uuid::generate();
    auto text = original.toString();
    auto parsed = Uuid::parse(text);
    REQUIRE(parsed.has_value());
    CHECK(*parsed == original);
}

TEST_CASE("Uuid::parse rejects malformed input") {
    CHECK(!Uuid::parse("not-a-uuid").has_value());
    CHECK(!Uuid::parse("").has_value());
    CHECK(!Uuid::parse("123e4567e89b12d3a456426614174000").has_value());  // missing dashes
}

TEST_CASE("Uuid::toString produces canonical 8-4-4-4-12 format") {
    auto id = Uuid::generate();
    auto text = id.toString();
    CHECK(text.size() == 36);
    CHECK(text[8] == '-');
    CHECK(text[13] == '-');
    CHECK(text[18] == '-');
    CHECK(text[23] == '-');
}
