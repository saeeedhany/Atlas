#pragma once

#include <QWidget>

#include "atlas/ui/workspace_controller.hpp"

class QListView;
class QPushButton;

namespace atlas::ui {

class RelationshipListModel;

// Mirrors MainWindow's list-and-delete pattern, scoped to
// relationships. A separate window rather than folded into MainWindow
// or GraphWindow — same "smaller, safer change than unifying
// everything on the first attempt" reasoning as GraphWindow itself.
class RelationshipsWindow : public QWidget {
    Q_OBJECT

public:
    explicit RelationshipsWindow(WorkspaceController& controller, QWidget* parent = nullptr);

private slots:
    void onDeleteClicked();
    void onSelectionChanged();

private:
    WorkspaceController* controller_;
    RelationshipListModel* model_;
    QListView* listView_;
    QPushButton* deleteButton_;
};

}  // namespace atlas::ui
