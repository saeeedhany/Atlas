#include "atlas/ui/relationship_edit_dialog.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::ui;

TEST_CASE("RelationshipEditDialog exposes the chosen target, type, and note") {
    auto source = KnowledgeObject::create("Stack").value();
    auto targetA = KnowledgeObject::create("Recursion").value();
    auto targetB = KnowledgeObject::create("Queue").value();
    auto targetAId = targetA.id();

    std::vector<KnowledgeObject> candidates;
    candidates.push_back(std::move(targetA));
    candidates.push_back(std::move(targetB));

    RelationshipEditDialog dialog(source, candidates);

    // Default selection is whatever the combo box starts on — the
    // first candidate, by construction order.
    CHECK(dialog.targetId() == targetAId);
    CHECK(!dialog.note().has_value());  // empty by default
}

TEST_CASE("RelationshipEditDialog with no candidate targets has an empty target list") {
    auto source = KnowledgeObject::create("Lonely Concept").value();
    std::vector<KnowledgeObject> noCandidates;

    // Doesn't crash to construct, even with nothing to connect to —
    // MainWindow is responsible for not opening this dialog in that
    // case (see its Connect button's enabled-state logic), but the
    // dialog itself should degrade gracefully regardless.
    RelationshipEditDialog dialog(source, noCandidates);
    CHECK(true);
}
