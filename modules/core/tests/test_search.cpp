#include "atlas/core/search.hpp"
#include "doctest.h"

using namespace atlas::core;

namespace {

KnowledgeObject makeObject(const char* title, const char* definition = "",
                            const char* problemSolved = "", const char* whyItExists = "",
                            const char* notes = "") {
    auto result = KnowledgeObject::create(title, definition, problemSolved, whyItExists);
    REQUIRE(result.hasValue());
    auto object = std::move(result).value();
    object.setNotes(notes);
    return object;
}

}  // namespace

TEST_CASE("matchScore returns nullopt for an empty query") {
    auto object = makeObject("Recursion");
    CHECK(!matchScore(object, "").has_value());
}

TEST_CASE("matchScore returns nullopt when the query appears nowhere") {
    auto object = makeObject("Recursion", "A function calling itself");
    CHECK(!matchScore(object, "linked list").has_value());
}

TEST_CASE("matchScore matches the title") {
    auto object = makeObject("Recursion");
    CHECK(matchScore(object, "recur").has_value());
}

TEST_CASE("matchScore is case-insensitive") {
    auto object = makeObject("Recursion");
    CHECK(matchScore(object, "RECURSION").has_value());
    CHECK(matchScore(object, "ReCuRsIoN").has_value());
}

TEST_CASE("a title match ranks higher than a definition-only match") {
    auto titleMatch = makeObject("Tail Call", "something unrelated");
    auto definitionMatch = makeObject("Unrelated Title", "involves a tail call somewhere");

    auto titleScore = matchScore(titleMatch, "tail call");
    auto definitionScore = matchScore(definitionMatch, "tail call");
    REQUIRE(titleScore.has_value());
    REQUIRE(definitionScore.has_value());
    CHECK(*titleScore > *definitionScore);
}

TEST_CASE("matching in two fields scores higher than matching in only one") {
    auto bothFields = makeObject("Recursion", "Recursion is when a function calls itself");
    auto oneField = makeObject("Recursion", "Something else entirely");

    auto bothScore = matchScore(bothFields, "recursion");
    auto oneScore = matchScore(oneField, "recursion");
    REQUIRE(bothScore.has_value());
    REQUIRE(oneScore.has_value());
    CHECK(*bothScore > *oneScore);
}

TEST_CASE("matchScore checks problemSolved, whyItExists, and notes too") {
    auto problemMatch = makeObject("X", "", "solves the problem of distinctive-phrase-alpha");
    auto whyMatch = makeObject("Y", "", "", "exists because of distinctive-phrase-beta");
    auto notesMatch = makeObject("Z", "", "", "", "distinctive-phrase-gamma in my notes");

    CHECK(matchScore(problemMatch, "distinctive-phrase-alpha").has_value());
    CHECK(matchScore(whyMatch, "distinctive-phrase-beta").has_value());
    CHECK(matchScore(notesMatch, "distinctive-phrase-gamma").has_value());
}
