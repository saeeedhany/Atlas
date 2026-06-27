#include <QListView>
#include <QPushButton>

#include "atlas/persistence/database.hpp"
#include "atlas/ui/main_window.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::ui;

TEST_CASE("MainWindow constructs under the offscreen platform and exposes its core widgets") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    MainWindow window(controller);
    CHECK(window.findChild<QListView*>() != nullptr);

    auto buttons = window.findChildren<QPushButton*>();
    CHECK(buttons.size() == 3);  // New, Edit, Delete
}

TEST_CASE("creating a KnowledgeObject through the controller is reflected in the open window") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    MainWindow window(controller);
    auto* listView = window.findChild<QListView*>();
    REQUIRE(listView != nullptr);
    REQUIRE(listView->model() != nullptr);

    REQUIRE(controller.createKnowledgeObject("Visible In Window").hasValue());
    CHECK(listView->model()->rowCount() == 1);
}
