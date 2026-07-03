#include <QListWidget>

#include "atlas/ui/roadmap_dialog.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::ui;

TEST_CASE("RoadmapDialog displays one numbered row per roadmap entry, target marked last") {
    std::vector<KnowledgeObject> roadmap;
    roadmap.push_back(KnowledgeObject::create("Functions").value());
    roadmap.push_back(KnowledgeObject::create("Recursion").value());
    roadmap.push_back(KnowledgeObject::create("Dynamic Programming").value());

    RoadmapDialog dialog("Dynamic Programming", roadmap);
    auto* list = dialog.findChild<QListWidget*>();
    REQUIRE(list != nullptr);
    REQUIRE(list->count() == 3);

    CHECK(list->item(0)->text().contains("Functions"));
    CHECK(list->item(1)->text().contains("Recursion"));
    CHECK(list->item(2)->text().contains("Dynamic Programming"));
    CHECK(list->item(2)->text().contains("target"));
    CHECK(!list->item(0)->text().contains("target"));
}

TEST_CASE("RoadmapDialog handles a single-entry roadmap (target with no dependencies)") {
    std::vector<KnowledgeObject> roadmap;
    roadmap.push_back(KnowledgeObject::create("Standalone").value());

    RoadmapDialog dialog("Standalone", roadmap);
    auto* list = dialog.findChild<QListWidget*>();
    REQUIRE(list != nullptr);
    REQUIRE(list->count() == 1);
    CHECK(list->item(0)->text().contains("target"));
}
