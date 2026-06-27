#include "atlas/persistence/database.hpp"
#include "atlas/ui/knowledge_object_list_model.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::ui;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

}  // namespace

TEST_CASE("the model starts empty for an empty workspace") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectListModel model(controller);
    CHECK(model.rowCount() == 0);
}

TEST_CASE("the model reflects an object created after construction") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectListModel model(controller);
    REQUIRE(controller.createKnowledgeObject("Binary Search").hasValue());

    REQUIRE(model.rowCount() == 1);
    auto index = model.index(0, 0);
    CHECK(model.data(index, Qt::DisplayRole).toString() == "Binary Search");
}

TEST_CASE("the model drops a row when the controller removes the object") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectListModel model(controller);
    auto id = controller.createKnowledgeObject("Temporary").value();
    REQUIRE(model.rowCount() == 1);

    REQUIRE(controller.removeKnowledgeObject(id).hasValue());
    CHECK(model.rowCount() == 0);
}

TEST_CASE("idAt returns the matching KnowledgeObjectId, and nullopt out of range") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectListModel model(controller);
    auto id = controller.createKnowledgeObject("Only Item").value();

    auto rowId = model.idAt(0);
    REQUIRE(rowId.has_value());
    CHECK(*rowId == id);

    CHECK(!model.idAt(1).has_value());
    CHECK(!model.idAt(-1).has_value());
}
