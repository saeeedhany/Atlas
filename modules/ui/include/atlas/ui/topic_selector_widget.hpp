#pragma once

#include <QWidget>

#include <vector>

#include "atlas/core/topic.hpp"
#include "atlas/ui/workspace_controller.hpp"

class QListWidget;
class QPushButton;
class QLabel;
class QStackedWidget;

namespace atlas::ui {

// The screen a person lands on when Atlas opens: every top-level
// Topic, each showing how many concepts it holds, plus "New Topic."
// Deliberately a plain QWidget, not a QMainWindow/top-level window -
// MainWindow hosts this and the workspace view (panel + canvas) as two
// pages of one QStackedWidget so switching between them can crossfade
// (see MainWindow::switchToPage) instead of spawning a second window,
// the same reasoning docs/DECISIONS.md already gives for keeping
// everything in one unified window rather than several utility windows.
class TopicSelectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit TopicSelectorWidget(WorkspaceController& controller, QWidget* parent = nullptr);

signals:
    // Double-click or Enter on a row. MainWindow owns what happens
    // next (switching pages, scoping the panel/canvas) - this class
    // only knows "a topic was picked," the same one-directional,
    // no-feedback-loop shape as KnowledgeObjectPanel::selectionChanged.
    void topicChosen(atlas::core::TopicId id);

private slots:
    void refresh();
    void onNewTopicClicked();
    void onItemActivated();
    void onContextMenuRequested(const QPoint& pos);

private:
    void updateEmptyState();

    WorkspaceController* controller_;
    QListWidget* listWidget_;
    QLabel* emptyStateLabel_;
    QStackedWidget* listStack_;
    QPushButton* newTopicButton_;

    // Parallel to listWidget_'s rows - same "plain accessor, not a Qt
    // model role" reasoning as KnowledgeObjectListModel::idAt(), and a
    // full QAbstractListModel felt like overkill at the scale a person
    // actually has topics (dozens, not thousands - see
    // WorkspaceController::allTopics()'s comment on the same point).
    std::vector<atlas::core::TopicId> rowTopicIds_;
};

}  // namespace atlas::ui
