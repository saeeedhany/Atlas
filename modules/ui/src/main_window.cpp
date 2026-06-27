#include "atlas/ui/main_window.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "atlas/ui/knowledge_object_edit_dialog.hpp"
#include "atlas/ui/knowledge_object_list_model.hpp"

namespace atlas::ui {

MainWindow::MainWindow(WorkspaceController& controller, QWidget* parent)
    : QMainWindow(parent), controller_(&controller) {
    setWindowTitle("Atlas");

    model_ = new KnowledgeObjectListModel(controller, this);
    listView_ = new QListView(this);
    listView_->setModel(model_);

    newButton_ = new QPushButton("New", this);
    editButton_ = new QPushButton("Edit", this);
    deleteButton_ = new QPushButton("Delete", this);
    editButton_->setEnabled(false);
    deleteButton_->setEnabled(false);

    connect(newButton_, &QPushButton::clicked, this, &MainWindow::onNewClicked);
    connect(editButton_, &QPushButton::clicked, this, &MainWindow::onEditClicked);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(listView_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::onSelectionChanged);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(newButton_);
    buttonRow->addWidget(editButton_);
    buttonRow->addWidget(deleteButton_);
    buttonRow->addStretch();

    auto* layout = new QVBoxLayout;
    layout->addWidget(listView_);
    layout->addLayout(buttonRow);

    auto* central = new QWidget(this);
    central->setLayout(layout);
    setCentralWidget(central);

    resize(480, 600);
}

void MainWindow::onSelectionChanged() {
    bool hasSelection = listView_->selectionModel()->hasSelection();
    editButton_->setEnabled(hasSelection);
    deleteButton_->setEnabled(hasSelection);
}

void MainWindow::onNewClicked() {
    bool ok = false;
    QString title =
        QInputDialog::getText(this, "New Knowledge Object", "Title:", QLineEdit::Normal, "", &ok);
    if (!ok || title.trimmed().isEmpty()) return;

    auto result = controller_->createKnowledgeObject(title.trimmed().toStdString());
    if (!result.hasValue()) {
        showControllerError("create the knowledge object", result.error());
    }
}

void MainWindow::onEditClicked() {
    auto selected = listView_->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    auto id = model_->idAt(selected.first().row());
    if (!id.has_value()) return;

    auto object = controller_->findKnowledgeObject(*id);
    if (!object.has_value()) return;

    KnowledgeObjectEditDialog dialog(*object, this);
    if (dialog.exec() != QDialog::Accepted) return;

    KnowledgeObjectEdits edits;
    edits.title = dialog.title().toStdString();
    edits.definition = dialog.definition().toStdString();
    edits.problemSolved = dialog.problemSolved().toStdString();
    edits.whyItExists = dialog.whyItExists().toStdString();
    edits.notes = dialog.notes().toStdString();
    edits.difficulty = dialog.difficulty();
    edits.confidence = dialog.confidence();

    auto result = controller_->updateKnowledgeObject(*id, std::move(edits));
    if (!result.hasValue()) {
        showControllerError("save the changes", result.error());
    }
}

void MainWindow::onDeleteClicked() {
    auto selected = listView_->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    auto id = model_->idAt(selected.first().row());
    if (!id.has_value()) return;

    auto object = controller_->findKnowledgeObject(*id);
    if (!object.has_value()) return;  // shouldn't happen: id came from this same controller

    auto title = QString::fromStdString(object->title());
    auto confirm = QMessageBox::question(this, "Delete", QString("Delete \"%1\"?").arg(title),
                                          QMessageBox::Yes | QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    auto result = controller_->removeKnowledgeObject(*id);
    if (!result.hasValue()) {
        showControllerError("delete the knowledge object", result.error());
    }
}

void MainWindow::showControllerError(const QString& action, const ControllerFailure& failure) {
    QMessageBox::warning(
        this, "Atlas", QString("Couldn't %1: %2").arg(action, QString::fromStdString(failure.detail)));
}

}  // namespace atlas::ui
