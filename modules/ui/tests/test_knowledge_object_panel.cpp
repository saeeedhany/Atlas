#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QStackedWidget>

#include "atlas/persistence/database.hpp"
#include "atlas/ui/knowledge_object_panel.hpp"
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

TEST_CASE("KnowledgeObjectPanel constructs and exposes its core widgets") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    CHECK(panel.findChild<QListView*>() != nullptr);
    CHECK(panel.findChild<QLineEdit*>() != nullptr);

    auto buttons = panel.findChildren<QPushButton*>();
    CHECK(buttons.size() == 6);  // New, Edit, Delete, Connect, Relationships, Suggest Projects
}

TEST_CASE("creating a KnowledgeObject is reflected in the panel's list") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto* listView = panel.findChild<QListView*>();
    REQUIRE(listView != nullptr);

    REQUIRE(controller.createKnowledgeObject("Visible In Panel").hasValue());
    CHECK(listView->model()->rowCount() == 1);
}

TEST_CASE("the panel shows an empty-state placeholder until the first object is created") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto* stack = panel.findChild<QStackedWidget*>();
    auto* listView = panel.findChild<QListView*>();
    auto* label = panel.findChild<QLabel*>("emptyStateLabel");
    REQUIRE(stack != nullptr);
    REQUIRE(listView != nullptr);
    REQUIRE(label != nullptr);

    CHECK(stack->currentWidget() == label);

    REQUIRE(controller.createKnowledgeObject("First Item").hasValue());

    CHECK(stack->currentWidget() == listView);
}

TEST_CASE("typing in the search box filters the list and shows a search-specific empty message") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto* searchEdit = panel.findChild<QLineEdit*>();
    auto* stack = panel.findChild<QStackedWidget*>();
    auto* listView = panel.findChild<QListView*>();
    auto* label = panel.findChild<QLabel*>("emptyStateLabel");
    REQUIRE(searchEdit != nullptr);
    REQUIRE(stack != nullptr);

    REQUIRE(controller.createKnowledgeObject("Recursion").hasValue());
    REQUIRE(controller.createKnowledgeObject("Linked List").hasValue());
    CHECK(stack->currentWidget() == listView);
    CHECK(listView->model()->rowCount() == 2);

    searchEdit->setText("recursion");
    CHECK(listView->model()->rowCount() == 1);

    searchEdit->setText("no such concept exists");
    CHECK(listView->model()->rowCount() == 0);
    CHECK(stack->currentWidget() == label);
    CHECK(label->text().contains("No matches"));

    searchEdit->clear();
    CHECK(listView->model()->rowCount() == 2);
}

TEST_CASE("the Connect button needs both a selection and a second object to exist") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto buttons = panel.findChildren<QPushButton*>();
    QPushButton* connectButton = nullptr;
    for (auto* button : buttons) {
        if (button->text() == "Connect...") connectButton = button;
    }
    REQUIRE(connectButton != nullptr);

    CHECK(!connectButton->isEnabled());  // nothing exists yet

    REQUIRE(controller.createKnowledgeObject("Only Item").hasValue());
    auto* listView = panel.findChild<QListView*>();
    listView->selectionModel()->select(listView->model()->index(0, 0),
                                        QItemSelectionModel::Select);
    CHECK(!connectButton->isEnabled());  // selected, but nothing else to connect to

    REQUIRE(controller.createKnowledgeObject("Second Item").hasValue());
    listView->selectionModel()->select(listView->model()->index(0, 0),
                                        QItemSelectionModel::Select);
    CHECK(connectButton->isEnabled());  // now there's a valid target
}

TEST_CASE("panel emits selectionChanged with an id when a row is selected") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto* listView = panel.findChild<QListView*>();
    REQUIRE(listView != nullptr);

    std::optional<atlas::core::KnowledgeObjectId> emitted;
    QObject::connect(&panel, &KnowledgeObjectPanel::selectionChanged,
                     [&](auto id) { emitted = id; });

    auto id = controller.createKnowledgeObject("Emitter Test").value();
    listView->selectionModel()->select(listView->model()->index(0, 0),
                                        QItemSelectionModel::Select);

    REQUIRE(emitted.has_value());
    CHECK(*emitted == id);
}

TEST_CASE("panel emits selectionChanged with nullopt when selection is cleared") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto* listView = panel.findChild<QListView*>();
    REQUIRE(listView != nullptr);

    std::optional<atlas::core::KnowledgeObjectId> emitted = atlas::core::KnowledgeObjectId::generate();
    QObject::connect(&panel, &KnowledgeObjectPanel::selectionChanged,
                     [&](auto id) { emitted = id; });

    REQUIRE(controller.createKnowledgeObject("ClearMe").hasValue());
    listView->selectionModel()->select(listView->model()->index(0, 0),
                                        QItemSelectionModel::Select);
    listView->selectionModel()->clearSelection();

    CHECK(!emitted.has_value());
}

TEST_CASE("the Suggest Projects button exists and needs no selection to be usable") {
    // Unlike Connect (needs a row selected) or Edit/Delete (need a row
    // selected), suggestions are scoped to the whole topic - there's
    // no reason to gate this button on list selection state.
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);
    auto buttons = panel.findChildren<QPushButton*>();
    QPushButton* suggestButton = nullptr;
    for (auto* button : buttons) {
        if (button->text() == "Suggest Projects...") suggestButton = button;
    }
    REQUIRE(suggestButton != nullptr);
    CHECK(suggestButton->isEnabled());
}

TEST_CASE("the panel's heading defaults to the Uncategorized topic's name") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    KnowledgeObjectPanel panel(controller);  // default topicId is uncategorizedTopicId()
    auto* heading = panel.findChild<QLabel*>("panelHeading");
    REQUIRE(heading != nullptr);

    auto uncategorized = controller.findTopic(atlas::core::uncategorizedTopicId());
    REQUIRE(uncategorized.has_value());
    CHECK(heading->text() == QString::fromStdString(uncategorized->name()));
}

TEST_CASE("setTopic updates the heading to the newly scoped topic's name") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto topicId = controller.createTopic("Operating Systems").value();

    KnowledgeObjectPanel panel(controller);
    auto* heading = panel.findChild<QLabel*>("panelHeading");
    REQUIRE(heading != nullptr);

    panel.setTopic(topicId);
    CHECK(heading->text() == "Operating Systems");
}

TEST_CASE("renaming the currently-viewed topic updates the heading live") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto topicId = controller.createTopic("Old Name").value();

    KnowledgeObjectPanel panel(controller, topicId);
    auto* heading = panel.findChild<QLabel*>("panelHeading");
    REQUIRE(heading != nullptr);
    CHECK(heading->text() == "Old Name");

    REQUIRE(controller.renameTopic(topicId, "New Name").hasValue());
    CHECK(heading->text() == "New Name");
}
