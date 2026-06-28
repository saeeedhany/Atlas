#include "atlas/ui/relationships_window.hpp"

#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "atlas/ui/relationship_list_model.hpp"

namespace atlas::ui {

RelationshipsWindow::RelationshipsWindow(WorkspaceController& controller, QWidget* parent)
    : QWidget(parent), controller_(&controller) {
    setWindowTitle("Atlas - Relationships");

    model_ = new RelationshipListModel(controller, this);
    listView_ = new QListView(this);
    listView_->setModel(model_);
    listView_->setAlternatingRowColors(true);

    deleteButton_ = new QPushButton("Delete", this);
    deleteButton_->setEnabled(false);

    connect(deleteButton_, &QPushButton::clicked, this, &RelationshipsWindow::onDeleteClicked);
    connect(listView_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &RelationshipsWindow::onSelectionChanged);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(listView_);
    layout->addWidget(deleteButton_);

    resize(480, 400);
}

void RelationshipsWindow::onSelectionChanged() {
    deleteButton_->setEnabled(listView_->selectionModel()->hasSelection());
}

void RelationshipsWindow::onDeleteClicked() {
    auto selected = listView_->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    auto id = model_->idAt(selected.first().row());
    if (!id.has_value()) return;

    auto confirm = QMessageBox::question(this, "Delete", "Delete this relationship?",
                                          QMessageBox::Yes | QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    auto result = controller_->removeRelationship(*id);
    if (!result.hasValue()) {
        QMessageBox::warning(this, "Atlas",
                              QString("Couldn't delete: %1")
                                  .arg(QString::fromStdString(result.error().detail)));
    }
}

}  // namespace atlas::ui
