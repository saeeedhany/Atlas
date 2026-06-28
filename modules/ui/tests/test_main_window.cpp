#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QStackedWidget>

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
    CHECK(buttons.size() == 6);  // New, Edit, Delete, Connect, Relationships, Graph View
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

TEST_CASE("the window shows an empty-state placeholder until the first object is created") {
    // Regression test for a real usability gap found by actually using
    // the app: an empty list and a broken window look identical (both
    // just blank), and a single new row was easy to miss against a
    // blank background with no visual cue that anything changed.
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    MainWindow window(controller);
    auto* stack = window.findChild<QStackedWidget*>();
    auto* listView = window.findChild<QListView*>();
    auto* label = window.findChild<QLabel*>();
    REQUIRE(stack != nullptr);
    REQUIRE(listView != nullptr);
    REQUIRE(label != nullptr);

    // currentWidget(), not isVisible() — isVisible() depends on the
    // top-level window actually being shown via show(), which this
    // test (correctly) never does.
    CHECK(stack->currentWidget() == label);

    REQUIRE(controller.createKnowledgeObject("First Item").hasValue());

    CHECK(stack->currentWidget() == listView);
}

TEST_CASE("the Connect button needs both a selection and a second object to exist") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    MainWindow window(controller);
    auto buttons = window.findChildren<QPushButton*>();
    QPushButton* connectButton = nullptr;
    for (auto* button : buttons) {
        if (button->text() == "Connect...") connectButton = button;
    }
    REQUIRE(connectButton != nullptr);

    CHECK(!connectButton->isEnabled());  // nothing exists yet

    REQUIRE(controller.createKnowledgeObject("Only Item").hasValue());
    auto* listView = window.findChild<QListView*>();
    listView->selectionModel()->select(listView->model()->index(0, 0),
                                         QItemSelectionModel::Select);
    CHECK(!connectButton->isEnabled());  // selected, but nothing else to connect to

    REQUIRE(controller.createKnowledgeObject("Second Item").hasValue());
    listView->selectionModel()->select(listView->model()->index(0, 0),
                                         QItemSelectionModel::Select);
    CHECK(connectButton->isEnabled());  // now there's a valid target
}
