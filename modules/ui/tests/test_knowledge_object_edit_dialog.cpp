#include <QListWidget>
#include <QPushButton>

#include "atlas/ui/knowledge_object_edit_dialog.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::ui;

TEST_CASE("KnowledgeObjectEditDialog pre-fills its lists from the object being edited") {
    auto object = KnowledgeObject::create("Recursion").value();
    object.addExample({"Factorial", std::nullopt});
    object.addMiniProject({"Implement it", "From scratch"});
    object.addReference({"CLRS", std::nullopt});

    KnowledgeObjectEditDialog dialog(object);
    REQUIRE(dialog.examples().size() == 1);
    REQUIRE(dialog.miniProjects().size() == 1);
    REQUIRE(dialog.references().size() == 1);
    CHECK(dialog.examples().front().description == "Factorial");
    CHECK(dialog.miniProjects().front().title == "Implement it");
    CHECK(dialog.references().front().title == "CLRS");
}

TEST_CASE("KnowledgeObjectEditDialog's Edit/Remove buttons start disabled with nothing selected") {
    auto object = KnowledgeObject::create("Recursion").value();
    object.addExample({"Factorial", std::nullopt});

    KnowledgeObjectEditDialog dialog(object);
    // Every section's Edit/Remove pair starts disabled - three
    // sections, so three of each, all disabled until a row is
    // selected in that section's list.
    auto editButtons = dialog.findChildren<QPushButton*>();
    int disabledEditOrRemove = 0;
    for (auto* button : editButtons) {
        if ((button->text() == "Edit..." || button->text() == "Remove") && !button->isEnabled()) {
            ++disabledEditOrRemove;
        }
    }
    CHECK(disabledEditOrRemove == 6);  // 2 buttons x 3 sections
}

TEST_CASE("KnowledgeObjectEditDialog starts with an empty object's lists all empty") {
    auto object = KnowledgeObject::create("Blank Concept").value();
    KnowledgeObjectEditDialog dialog(object);
    CHECK(dialog.examples().empty());
    CHECK(dialog.miniProjects().empty());
    CHECK(dialog.references().empty());
}

// Note: an end-to-end "click Add, fill the nested TwoFieldItemDialog,
// verify examples() updates" test was attempted here and removed.
// QApplication::activeModalWidget() proved unreliable under the
// `offscreen` Qt platform used for this whole test binary - the test
// was flaky, not the dialog. Every other dialog test in this codebase
// (RoadmapDialog, RelationshipEditDialog, ProjectSuggestionsDialog)
// deliberately constructs and inspects without driving a nested modal
// for the same reason. The pieces that matter are covered separately:
// TwoFieldItemDialog's own accessors (test_two_field_item_dialog.cpp)
// and this dialog's pre-fill/initial-state behavior above - the only
// untested seam is the literal button-click -> exec() call itself,
// which is a single trivial line per section.
