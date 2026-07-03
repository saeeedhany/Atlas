#include <QLabel>
#include <QListWidget>

#include "atlas/ui/project_suggestions_dialog.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::ui;

namespace {

WorkspaceController::ProjectSuggestion makeSuggestion(const char* title, double readiness,
                                                         int leverage, bool withMiniProject) {
    auto object = KnowledgeObject::create(title).value();
    if (withMiniProject) {
        object.addMiniProject(MiniProject{"Build it", "A hands-on project"});
    }
    return WorkspaceController::ProjectSuggestion{object, readiness, leverage};
}

}  // namespace

TEST_CASE("ProjectSuggestionsDialog lists one row per suggestion, including its mini-projects") {
    std::vector<WorkspaceController::ProjectSuggestion> suggestions;
    suggestions.push_back(makeSuggestion("Dynamic Programming", 1.0, 2, true));
    suggestions.push_back(makeSuggestion("Recursion", 0.5, 0, true));

    ProjectSuggestionsDialog dialog("Algorithms", suggestions);
    auto* list = dialog.findChild<QListWidget*>();
    REQUIRE(list != nullptr);
    REQUIRE(list->count() == 2);

    CHECK(list->item(0)->text().contains("Dynamic Programming"));
    CHECK(list->item(0)->text().contains("100%"));
    CHECK(list->item(0)->text().contains("unlocks 2"));
    CHECK(list->item(0)->text().contains("Build it"));

    CHECK(list->item(1)->text().contains("Recursion"));
    CHECK(list->item(1)->text().contains("50%"));
}

TEST_CASE("ProjectSuggestionsDialog with zero suggestions shows an explanatory message, not a blank list") {
    std::vector<WorkspaceController::ProjectSuggestion> suggestions;

    ProjectSuggestionsDialog dialog("Empty Topic", suggestions);
    auto* list = dialog.findChild<QListWidget*>();
    auto* label = dialog.findChild<QLabel*>();
    REQUIRE(list != nullptr);
    REQUIRE(label != nullptr);
    CHECK(list->count() == 0);
    CHECK(label->text().contains("No project suggestions"));
}

TEST_CASE("a suggestion with zero leverage doesn't claim to unlock anything") {
    std::vector<WorkspaceController::ProjectSuggestion> suggestions;
    suggestions.push_back(makeSuggestion("Standalone Concept", 1.0, 0, true));

    ProjectSuggestionsDialog dialog("Topic", suggestions);
    auto* list = dialog.findChild<QListWidget*>();
    REQUIRE(list != nullptr);
    REQUIRE(list->count() == 1);
    CHECK(!list->item(0)->text().contains("unlocks"));
}
