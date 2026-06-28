#include "atlas/ui/main_window.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "atlas/ui/graph_window.hpp"
#include "atlas/ui/knowledge_object_edit_dialog.hpp"
#include "atlas/ui/knowledge_object_list_model.hpp"
#include "atlas/ui/relationship_edit_dialog.hpp"
#include "atlas/ui/relationships_window.hpp"

namespace atlas::ui {

using atlas::core::KnowledgeObject;

MainWindow::MainWindow(WorkspaceController& controller, QWidget* parent)
    : QMainWindow(parent), controller_(&controller) {
    setWindowTitle("Atlas");

    model_ = new KnowledgeObjectListModel(controller, this);
    listView_ = new QListView(this);
    listView_->setModel(model_);
    listView_->setAlternatingRowColors(true);  // a single row on a blank background is easy to miss otherwise

    emptyStateLabel_ = new QLabel(
        "No knowledge objects yet.\n\nClick \"New\" below to add your first one.", this);
    emptyStateLabel_->setAlignment(Qt::AlignCenter);
    emptyStateLabel_->setStyleSheet("color: gray;");

    listStack_ = new QStackedWidget(this);
    listStack_->addWidget(listView_);
    listStack_->addWidget(emptyStateLabel_);

    connect(model_, &QAbstractItemModel::modelReset, this, &MainWindow::updateEmptyState);
    connect(model_, &QAbstractItemModel::modelReset, this, &MainWindow::onSelectionChanged);

    newButton_ = new QPushButton("New", this);
    editButton_ = new QPushButton("Edit", this);
    deleteButton_ = new QPushButton("Delete", this);
    connectButton_ = new QPushButton("Connect...", this);
    relationshipsButton_ = new QPushButton("Relationships...", this);
    graphViewButton_ = new QPushButton("Graph View", this);
    editButton_->setEnabled(false);
    deleteButton_->setEnabled(false);
    connectButton_->setEnabled(false);

    connect(newButton_, &QPushButton::clicked, this, &MainWindow::onNewClicked);
    connect(editButton_, &QPushButton::clicked, this, &MainWindow::onEditClicked);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(relationshipsButton_, &QPushButton::clicked, this,
            &MainWindow::onRelationshipsClicked);
    connect(graphViewButton_, &QPushButton::clicked, this, &MainWindow::onGraphViewClicked);
    connect(listView_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::onSelectionChanged);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(newButton_);
    buttonRow->addWidget(editButton_);
    buttonRow->addWidget(deleteButton_);
    buttonRow->addWidget(connectButton_);
    buttonRow->addWidget(relationshipsButton_);
    buttonRow->addStretch();
    buttonRow->addWidget(graphViewButton_);

    auto* layout = new QVBoxLayout;
    layout->addWidget(listStack_);
    layout->addLayout(buttonRow);

    auto* central = new QWidget(this);
    central->setLayout(layout);
    setCentralWidget(central);

    resize(480, 600);
    updateEmptyState();
}

void MainWindow::updateEmptyState() {
    if (model_->rowCount() == 0) {
        listStack_->setCurrentWidget(emptyStateLabel_);
    } else {
        listStack_->setCurrentWidget(listView_);
    }
}

void MainWindow::onSelectionChanged() {
    bool hasSelection = listView_->selectionModel()->hasSelection();
    editButton_->setEnabled(hasSelection);
    deleteButton_->setEnabled(hasSelection);
    // Connecting needs a second, different object to target — disabled
    // with fewer than 2 objects in the workspace even if one is selected.
    connectButton_->setEnabled(hasSelection && model_->rowCount() >= 2);
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

void MainWindow::onConnectClicked() {
    auto selected = listView_->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    auto sourceId = model_->idAt(selected.first().row());
    if (!sourceId.has_value()) return;

    auto source = controller_->findKnowledgeObject(*sourceId);
    if (!source.has_value()) return;

    std::vector<KnowledgeObject> candidates;
    for (const auto& object : controller_->allKnowledgeObjects()) {
        if (!(object.id() == *sourceId)) candidates.push_back(object);
    }
    if (candidates.empty()) return;  // guarded by connectButton_'s enabled state too

    RelationshipEditDialog dialog(*source, candidates, this);
    if (dialog.exec() != QDialog::Accepted) return;

    auto result = controller_->createRelationship(*sourceId, dialog.targetId(), dialog.type(),
                                                    dialog.note());
    if (!result.hasValue()) {
        showControllerError("create the relationship", result.error());
    }
}

void MainWindow::onRelationshipsClicked() {
    if (relationshipsWindow_ == nullptr) {
        relationshipsWindow_ = new RelationshipsWindow(*controller_, this);
    }
    relationshipsWindow_->show();
    relationshipsWindow_->raise();
    relationshipsWindow_->activateWindow();
}

void MainWindow::onGraphViewClicked() {
    if (graphWindow_ == nullptr) {
        graphWindow_ = new GraphWindow(*controller_, this);
        graphWindow_->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    graphWindow_->show();
    graphWindow_->raise();
    graphWindow_->activateWindow();
}

void MainWindow::showControllerError(const QString& action, const ControllerFailure& failure) {
    QMessageBox::warning(
        this, "Atlas", QString("Couldn't %1: %2").arg(action, QString::fromStdString(failure.detail)));
}

}  // namespace atlas::ui
