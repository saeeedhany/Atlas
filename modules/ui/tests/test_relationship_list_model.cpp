#include <QListView>
#include <QPushButton>

#include "atlas/persistence/database.hpp"
#include "atlas/ui/relationship_list_model.hpp"
#include "atlas/ui/relationships_window.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::ui;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

}  // namespace

TEST_CASE("RelationshipListModel reflects a relationship created after construction") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    RelationshipListModel model(controller);
    CHECK(model.rowCount() == 0);

    auto a = controller.createKnowledgeObject("Stack").value();
    auto b = controller.createKnowledgeObject("Recursion").value();
    auto relId =
        controller.createRelationship(a, b, RelationshipType::Uses, std::nullopt).value();

    REQUIRE(model.rowCount() == 1);
    auto displayText = model.data(model.index(0, 0), Qt::DisplayRole).toString();
    CHECK(displayText.contains("Stack"));
    CHECK(displayText.contains("Recursion"));
    CHECK(displayText.contains("Uses"));

    auto rowId = model.idAt(0);
    REQUIRE(rowId.has_value());
    CHECK(*rowId == relId);
}

TEST_CASE("RelationshipListModel drops a row when the controller removes the relationship") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();
    auto relId =
        controller.createRelationship(a, b, RelationshipType::Causes, std::nullopt).value();

    RelationshipListModel model(controller);
    REQUIRE(model.rowCount() == 1);

    REQUIRE(controller.removeRelationship(relId).hasValue());
    CHECK(model.rowCount() == 0);
}

TEST_CASE("RelationshipListModel reflects cascaded removal when a KnowledgeObject is deleted") {
    // The exact scenario that motivated collapsing every controller
    // signal into one graphChanged — this view only listens to that
    // single signal, so it can't miss a cascade.
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto a = controller.createKnowledgeObject("A").value();
    auto b = controller.createKnowledgeObject("B").value();
    REQUIRE(controller.createRelationship(a, b, RelationshipType::DependsOn, std::nullopt)
                .hasValue());

    RelationshipListModel model(controller);
    REQUIRE(model.rowCount() == 1);

    REQUIRE(controller.removeKnowledgeObject(a).hasValue());
    CHECK(model.rowCount() == 0);
}

TEST_CASE("RelationshipsWindow constructs and exposes its list and delete button") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    RelationshipsWindow window(controller);
    CHECK(window.findChild<QListView*>() != nullptr);
    CHECK(window.findChild<QPushButton*>() != nullptr);
}
