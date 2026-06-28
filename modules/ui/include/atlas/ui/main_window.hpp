#pragma once

#include <QMainWindow>

#include "atlas/ui/workspace_controller.hpp"

class QListView;
class QPushButton;
class QLabel;
class QStackedWidget;

namespace atlas::ui {

class KnowledgeObjectListModel;
class GraphWindow;
class RelationshipsWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(WorkspaceController& controller, QWidget* parent = nullptr);

private slots:
    void onNewClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onSelectionChanged();
    void onGraphViewClicked();
    void onConnectClicked();
    void onRelationshipsClicked();
    void updateEmptyState();

private:
    void showControllerError(const QString& action, const ControllerFailure& failure);

    WorkspaceController* controller_;
    KnowledgeObjectListModel* model_;
    QListView* listView_;
    QLabel* emptyStateLabel_;
    QStackedWidget* listStack_;
    QPushButton* newButton_;
    QPushButton* editButton_;
    QPushButton* deleteButton_;
    QPushButton* connectButton_;
    QPushButton* relationshipsButton_;
    QPushButton* graphViewButton_;
    GraphWindow* graphWindow_ = nullptr;            // lazily created, owned by this window
    RelationshipsWindow* relationshipsWindow_ = nullptr;  // lazily created, owned by this window
};

}  // namespace atlas::ui
