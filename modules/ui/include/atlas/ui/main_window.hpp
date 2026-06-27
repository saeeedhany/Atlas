#pragma once

#include <QMainWindow>

#include "atlas/ui/workspace_controller.hpp"

class QListView;
class QPushButton;

namespace atlas::ui {

class KnowledgeObjectListModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(WorkspaceController& controller, QWidget* parent = nullptr);

private slots:
    void onNewClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onSelectionChanged();

private:
    void showControllerError(const QString& action, const ControllerFailure& failure);

    WorkspaceController* controller_;
    KnowledgeObjectListModel* model_;
    QListView* listView_;
    QPushButton* newButton_;
    QPushButton* editButton_;
    QPushButton* deleteButton_;
};

}  // namespace atlas::ui
