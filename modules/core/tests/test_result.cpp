#include <string>
#include <utility>

#include "atlas/core/result.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("Result<T,E>::ok constructs a successful result") {
    auto result = Result<int, std::string>::ok(42);
    CHECK(result.hasValue());
    CHECK(static_cast<bool>(result));
    CHECK(result.value() == 42);
}

TEST_CASE("Result<T,E>::err constructs a failed result") {
    auto result = Result<int, std::string>::err("something went wrong");
    CHECK(!result.hasValue());
    CHECK(!static_cast<bool>(result));
    CHECK(result.error() == "something went wrong");
}

TEST_CASE("value() on a non-const lvalue Result returns a mutable reference, not a copy") {
    // This is the overload that was missing. Without it, this would
    // have silently bound to the const& overload instead, and the
    // mutation below would have failed to compile (can't assign
    // through a const reference) rather than working as a real
    // in-place mutation.
    auto result = Result<std::string, int>::ok("original");
    result.value() += " mutated";
    CHECK(result.value() == "original mutated");
}

TEST_CASE("std::move(namedResult).value() actually moves, not copies") {
    auto result = Result<std::string, int>::ok(std::string(500, 'x'));
    std::string moved = std::move(result).value();
    CHECK(moved.size() == 500);
}

TEST_CASE("value() on a non-const lvalue enables a genuine move out via std::move(ref)") {
    // The specific bug this overload fixes: code that does
    // `for (auto& x : namedResult.value()) { use(std::move(x)); }`
    // needs namedResult.value() itself to return non-const here, or
    // `x` becomes const and std::move(x) is a no-op copy in disguise.
    auto result = Result<std::string, int>::ok(std::string(500, 'y'));
    std::string moved = std::move(result.value());
    CHECK(moved.size() == 500);
    CHECK(result.value().empty());  // really moved-from, not copied
}

TEST_CASE("error() on a non-const lvalue Result returns a mutable reference") {
    auto result = Result<int, std::string>::err("original error");
    result.error() += " mutated";
    CHECK(result.error() == "original error mutated");
}

TEST_CASE("Result<void,E>::ok and ::err round-trip correctly") {
    auto ok = Result<void, std::string>::ok();
    CHECK(ok.hasValue());

    auto err = Result<void, std::string>::err("failed");
    CHECK(!err.hasValue());
    CHECK(err.error() == "failed");
}
