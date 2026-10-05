#include "atlas/viewmodels/session_summary.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::viewmodels;

namespace {

ReviewEvent event(Certainty predicted, Grade grade, Exercise exercise = Exercise::Rebuild, int hints = 0) {
    ReviewEvent result;
    result.predicted = predicted;
    result.grade = grade;
    result.exercise = exercise;
    result.hintsUsed = hints;
    return result;
}

}  // namespace

TEST_CASE("link prompts read naturally for every type") {
    CHECK(linkPrompt(RelationshipType::DependsOn, "B-Tree", "Tree") == "Why does B-Tree depend on Tree?");
    CHECK(linkPrompt(RelationshipType::PartOf, "TLB", "Paging") == "Why is TLB part of Paging?");
    CHECK(linkPrompt(RelationshipType::AlternativeTo, "Heap", "Tree") == "When would you pick Heap over Tree?");
    for (int type = 0; type <= static_cast<int>(RelationshipType::Causes); ++type) {
        CHECK(linkPrompt(static_cast<RelationshipType>(type), "A", "B").endsWith("?"));
    }
}

TEST_CASE("calibration lines list used levels, most certain first") {
    std::vector<ReviewEvent> events{event(Certainty::Certain, Grade::Good), event(Certainty::Certain, Grade::Again),
                                    event(Certainty::Guess, Grade::Good)};
    CHECK(calibrationLines(events) == QStringList{"Certain 2 times, right 1", "Guess 1 time, right 1"});
    CHECK(calibrationLines({}).isEmpty());
}

TEST_CASE("tips follow what happened in the session") {
    CHECK(chooseTip({event(Certainty::Certain, Grade::Again)}).source == "Butterfield & Metcalfe 2001");

    auto confused = event(Certainty::Unsure, Grade::Again);
    confused.wrongTarget = KnowledgeObjectId::generate();
    CHECK(chooseTip({confused}).source == "Rohrer & Taylor 2007");

    CHECK(chooseTip({event(Certainty::Unsure, Grade::Hard, Exercise::Rebuild, 1),
                     event(Certainty::Unsure, Grade::Good)})
              .source == "Bjork 1994");
    CHECK(chooseTip({event(Certainty::Unsure, Grade::Good)}).source == "Roediger & Karpicke 2006");
}

TEST_CASE("items are counted once and judged by their first event") {
    auto link = ItemRef::forLink(RelationshipId::generate());
    auto tree = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto missedFirst = event(Certainty::Certain, Grade::Again);
    missedFirst.item = link;
    auto laterGood = event(Certainty::Certain, Grade::Good, Exercise::Explain);
    laterGood.item = link;
    auto recalled = event(Certainty::Unsure, Grade::Hard, Exercise::Explain);
    recalled.item = tree;
    std::vector<ReviewEvent> events{missedFirst, laterGood, recalled};
    CHECK(itemsReviewed(events) == 2);
    CHECK(itemsRecalled(events) == 1);
    CHECK(itemsReviewed({}) == 0);
}
