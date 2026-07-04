#include "atlas/ui/topic_selector_widget.hpp"

#include <QListWidget>
#include <QMetaObject>

#include "atlas/persistence/database.hpp"
#include "atlas/ui/workspace_controller.hpp"
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

TEST_CASE("TopicSelectorWidget lists every topic, including the seeded Uncategorized one") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    controller.createTopic("Operating Systems");
    controller.createTopic("Databases");

    TopicSelectorWidget widget(controller);
    auto* listWidget = widget.findChild<QListWidget*>();
    REQUIRE(listWidget != nullptr);
    CHECK(listWidget->count() == 3);  // Uncategorized + the two created above
}

TEST_CASE("TopicSelectorWidget refreshes when the controller reports topicsChanged") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    TopicSelectorWidget widget(controller);
    auto* listWidget = widget.findChild<QListWidget*>();
    REQUIRE(listWidget != nullptr);
    CHECK(listWidget->count() == 1);  // just Uncategorized

    controller.createTopic("Compilers");
    CHECK(listWidget->count() == 2);
}

TEST_CASE("TopicSelectorWidget's row label reflects the topic's member count") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto topicId = controller.createTopic("Operating Systems").value();

    TopicSelectorWidget widget(controller);
    auto* listWidget = widget.findChild<QListWidget*>();
    REQUIRE(listWidget != nullptr);

    // Find the "Operating Systems" row specifically rather than
    // assuming row order — list order follows allTopics()'s
    // alphabetical sort, which is an implementation detail this test
    // shouldn't depend on.
    QListWidgetItem* osItem = nullptr;
    for (int i = 0; i < listWidget->count(); ++i) {
        if (listWidget->item(i)->text().startsWith("Operating Systems")) {
            osItem = listWidget->item(i);
        }
    }
    REQUIRE(osItem != nullptr);
    CHECK(osItem->text().contains("(0)"));

    controller.createKnowledgeObject("Paging", topicId);

    osItem = nullptr;
    for (int i = 0; i < listWidget->count(); ++i) {
        if (listWidget->item(i)->text().startsWith("Operating Systems")) {
            osItem = listWidget->item(i);
        }
    }
    REQUIRE(osItem != nullptr);
    CHECK(osItem->text().contains("(1)"));
}

TEST_CASE("Activating a row emits topicChosen with that row's TopicId") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());
    auto topicId = controller.createTopic("Operating Systems").value();

    TopicSelectorWidget widget(controller);
    auto* listWidget = widget.findChild<QListWidget*>();
    REQUIRE(listWidget != nullptr);

    std::optional<atlas::core::TopicId> emitted;
    QObject::connect(&widget, &TopicSelectorWidget::topicChosen, [&](auto id) { emitted = id; });

    int row = -1;
    for (int i = 0; i < listWidget->count(); ++i) {
        if (listWidget->item(i)->text().startsWith("Operating Systems")) row = i;
    }
    REQUIRE(row >= 0);
    listWidget->setCurrentRow(row);
    // QListWidget::itemActivated is a protected Qt signal — not
    // callable from outside QListWidget, even to simulate activation
    // for a test. Invoking TopicSelectorWidget's own (private) slot by
    // name through the meta-object system is the standard Qt way
    // around that: Q_OBJECT registers slots for invocation regardless
    // of their C++ access specifier, which is the whole point of the
    // mechanism. onItemActivated() takes no arguments — it reads
    // listWidget_->currentRow() itself — so there's also no QVariant
    // marshalling to worry about here.
    QMetaObject::invokeMethod(&widget, "onItemActivated");

    REQUIRE(emitted.has_value());
    CHECK(*emitted == topicId);
}

TEST_CASE("the topic list has a custom context menu policy, so right-click can offer Rename/Delete") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    TopicSelectorWidget widget(controller);
    auto* listWidget = widget.findChild<QListWidget*>();
    REQUIRE(listWidget != nullptr);
    CHECK(listWidget->contextMenuPolicy() == Qt::CustomContextMenu);
}

TEST_CASE("requesting a context menu at a position with no item does nothing, not a crash") {
    // Deliberately does not attempt to drive the resulting QMenu::exec()
    // for a right-click that DOES land on an item — QMenu::exec() is a
    // blocking modal call, and the equivalent technique for
    // TwoFieldItemDialog's own modal (QApplication::activeModalWidget()
    // inside a QTimer::singleShot) proved unreliable under the
    // `offscreen` Qt platform this whole binary runs under (see
    // docs/DECISIONS.md and the comment left in
    // test_knowledge_object_edit_dialog.cpp). The Rename/Delete
    // business logic itself is already covered directly at the
    // controller level (WorkspaceController's renameTopic/removeTopic
    // tests); this test only confirms the empty-space guard clause
    // doesn't crash.
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    TopicSelectorWidget widget(controller);
    auto* listWidget = widget.findChild<QListWidget*>();
    REQUIRE(listWidget != nullptr);

    QMetaObject::invokeMethod(&widget, "onContextMenuRequested", Q_ARG(QPoint, QPoint(5000, 5000)));
    CHECK(true);  // reaching this line without crashing is the assertion
}
