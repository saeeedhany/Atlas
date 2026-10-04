#pragma once

#include <QWidget>

#include <optional>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/topic.hpp"
#include "atlas/ui/workspace_controller.hpp"

class QListView;
class QPushButton;
class QLabel;
class QStackedWidget;
class QLineEdit;

namespace atlas::ui {

class KnowledgeObjectListModel;
class RelationshipsWindow;

// The list-view side of the workspace: search, the list itself,
// empty-state handling, and New/Edit/Delete/Connect/Relationships.
// Extracted out of MainWindow (which originally owned all of this
// directly) so it can be composed alongside an embedded graph canvas
// in a unified window, rather than MainWindow growing into one giant
// class that also knows about layout/splitters/canvas wiring. Single
// responsibility: this widget manages KnowledgeObject selection and
// CRUD; it has no idea a graph canvas exists.
class KnowledgeObjectPanel : public QWidget {
    Q_OBJECT

public:
    // topicId defaults to Uncategorized - see GraphWindow's
    // constructor comment for why that keeps every existing call site
    // (mostly tests, all single-arg) meaning what it already meant.
    explicit KnowledgeObjectPanel(WorkspaceController& controller,
                                   atlas::core::TopicId topicId = atlas::core::uncategorizedTopicId(),
                                   QWidget* parent = nullptr);

    // Re-scopes an existing panel in place (see GraphWindow::setTopic
    // for the matching "reuse the widget, don't reconstruct it" reasoning).
    // Clears the search box and selection - a search term or a
    // selected row from the old topic has no meaning in the new one.
    void setTopic(atlas::core::TopicId topicId);
    const atlas::core::TopicId& topic() const { return topicId_; }

signals:
    // Fires whenever the list selection changes - including to
    // nullopt when the selection is cleared (e.g. after a delete).
    // MainWindow listens to this to drive canvas highlighting; nothing
    // in this class knows or cares that a canvas exists.
    void selectionChanged(std::optional<atlas::core::KnowledgeObjectId> id);

private slots:
    void onNewClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onListSelectionChanged();
    void onConnectClicked();
    void onRelationshipsClicked();
    void onSuggestProjectsClicked();
    void onSearchTextChanged(const QString& text);
    void updateEmptyState();

private:
    void showControllerError(const QString& action, const ControllerFailure& failure);
    void updateHeading();

    WorkspaceController* controller_;
    KnowledgeObjectListModel* model_;
    atlas::core::TopicId topicId_;
    QLabel* headingLabel_;
    QLineEdit* searchEdit_;
    QListView* listView_;
    QLabel* emptyStateLabel_;
    QStackedWidget* listStack_;
    QPushButton* newButton_;
    QPushButton* editButton_;
    QPushButton* deleteButton_;
    QPushButton* connectButton_;
    QPushButton* relationshipsButton_;
    QPushButton* suggestProjectsButton_;
    RelationshipsWindow* relationshipsWindow_ = nullptr;  // lazily created, owned by this panel
};

}  // namespace atlas::ui
