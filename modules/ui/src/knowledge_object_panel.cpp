#include "atlas/ui/knowledge_object_panel.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "atlas/ui/knowledge_object_edit_dialog.hpp"
#include "atlas/ui/knowledge_object_list_model.hpp"
#include "atlas/ui/project_suggestions_dialog.hpp"
#include "atlas/ui/relationship_edit_dialog.hpp"
#include "atlas/ui/relationships_window.hpp"

namespace atlas::ui {

using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;

KnowledgeObjectPanel::KnowledgeObjectPanel(WorkspaceController& controller,
                                             atlas::core::TopicId topicId, QWidget* parent)
    : QWidget(parent), controller_(&controller), topicId_(topicId) {
    headingLabel_ = new QLabel(this);
    headingLabel_->setObjectName("panelHeading");
    auto headingFont = headingLabel_->font();
    headingFont.setPointSize(headingFont.pointSize() + 4);
    headingFont.setBold(true);
    headingLabel_->setFont(headingFont);

    model_ = new KnowledgeObjectListModel(controller, this);
    model_->setTopicFilter(topicId_);
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("Search...");
    listView_ = new QListView(this);
    listView_->setModel(model_);
    listView_->setAlternatingRowColors(true);  // a single row on a blank background is easy to miss otherwise

    emptyStateLabel_ = new QLabel(
        "No knowledge objects yet.\n\nClick \"New\" below to add your first one.", this);
    emptyStateLabel_->setObjectName("emptyStateLabel");
    emptyStateLabel_->setAlignment(Qt::AlignCenter);
    emptyStateLabel_->setStyleSheet("color: gray;");

    listStack_ = new QStackedWidget(this);
    listStack_->addWidget(listView_);
    listStack_->addWidget(emptyStateLabel_);

    connect(searchEdit_, &QLineEdit::textChanged, this, &KnowledgeObjectPanel::onSearchTextChanged);
    connect(model_, &QAbstractItemModel::modelReset, this, &KnowledgeObjectPanel::updateEmptyState);
    connect(model_, &QAbstractItemModel::modelReset, this,
            &KnowledgeObjectPanel::onListSelectionChanged);

    newButton_ = new QPushButton("New", this);
    editButton_ = new QPushButton("Edit", this);
    deleteButton_ = new QPushButton("Delete", this);
    connectButton_ = new QPushButton("Connect...", this);
    relationshipsButton_ = new QPushButton("Relationships...", this);
    suggestProjectsButton_ = new QPushButton("Suggest Projects...", this);
    editButton_->setEnabled(false);
    deleteButton_->setEnabled(false);
    connectButton_->setEnabled(false);

    connect(newButton_, &QPushButton::clicked, this, &KnowledgeObjectPanel::onNewClicked);
    connect(editButton_, &QPushButton::clicked, this, &KnowledgeObjectPanel::onEditClicked);
    connect(deleteButton_, &QPushButton::clicked, this, &KnowledgeObjectPanel::onDeleteClicked);
    connect(connectButton_, &QPushButton::clicked, this, &KnowledgeObjectPanel::onConnectClicked);
    connect(relationshipsButton_, &QPushButton::clicked, this,
            &KnowledgeObjectPanel::onRelationshipsClicked);
    connect(suggestProjectsButton_, &QPushButton::clicked, this,
            &KnowledgeObjectPanel::onSuggestProjectsClicked);
    connect(listView_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &KnowledgeObjectPanel::onListSelectionChanged);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(newButton_);
    buttonRow->addWidget(editButton_);
    buttonRow->addWidget(deleteButton_);
    buttonRow->addWidget(connectButton_);
    buttonRow->addWidget(relationshipsButton_);
    buttonRow->addWidget(suggestProjectsButton_);
    buttonRow->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(headingLabel_);
    layout->addWidget(searchEdit_);
    layout->addWidget(listStack_);
    layout->addLayout(buttonRow);

    // Renaming the active topic (via TopicSelectorWidget's context
    // menu) should update this heading immediately, even though
    // nothing about this panel's own KnowledgeObject data changed -
    // same reasoning as TopicSelectorWidget itself listening for
    // topicsChanged to refresh its per-row member counts.
    connect(controller_, &WorkspaceController::topicsChanged, this,
            &KnowledgeObjectPanel::updateHeading);

    updateEmptyState();
    updateHeading();
}

void KnowledgeObjectPanel::setTopic(atlas::core::TopicId topicId) {
    if (topicId_ == topicId) return;
    topicId_ = topicId;
    searchEdit_->clear();  // also triggers onSearchTextChanged -> model_->setSearchQuery("")
    model_->setTopicFilter(topicId_);
    updateHeading();
}

void KnowledgeObjectPanel::updateHeading() {
    auto topic = controller_->findTopic(topicId_);
    headingLabel_->setText(topic.has_value() ? QString::fromStdString(topic->name())
                                               : "Knowledge Objects");
}

void KnowledgeObjectPanel::onSearchTextChanged(const QString& text) {
    model_->setSearchQuery(text);
}

void KnowledgeObjectPanel::updateEmptyState() {
    if (model_->rowCount() != 0) {
        listStack_->setCurrentWidget(listView_);
        return;
    }
    if (model_->currentQuery().isEmpty()) {
        emptyStateLabel_->setText(
            "No knowledge objects yet.\n\nClick \"New\" below to add your first one.");
    } else {
        emptyStateLabel_->setText(QString("No matches for \"%1\".").arg(model_->currentQuery()));
    }
    listStack_->setCurrentWidget(emptyStateLabel_);
}

void KnowledgeObjectPanel::onListSelectionChanged() {
    bool hasSelection = listView_->selectionModel()->hasSelection();
    editButton_->setEnabled(hasSelection);
    deleteButton_->setEnabled(hasSelection);
    // Connecting needs a second, different object to target - disabled
    // with fewer than 2 objects in the workspace even if one is selected.
    connectButton_->setEnabled(hasSelection && model_->rowCount() >= 2);

    if (!hasSelection) {
        emit selectionChanged(std::nullopt);
        return;
    }
    auto selected = listView_->selectionModel()->selectedIndexes();
    auto id = model_->idAt(selected.first().row());
    emit selectionChanged(id);
}

void KnowledgeObjectPanel::onNewClicked() {
    bool ok = false;
    QString title =
        QInputDialog::getText(this, "New Knowledge Object", "Title:", QLineEdit::Normal, "", &ok);
    if (!ok || title.trimmed().isEmpty()) return;

    auto result = controller_->createKnowledgeObject(title.trimmed().toStdString(), topicId_);
    if (!result.hasValue()) {
        showControllerError("create the knowledge object", result.error());
    }
}

void KnowledgeObjectPanel::onEditClicked() {
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
    edits.examples = dialog.examples();
    edits.miniProjects = dialog.miniProjects();
    edits.references = dialog.references();

    auto result = controller_->updateKnowledgeObject(*id, std::move(edits));
    if (!result.hasValue()) {
        showControllerError("save the changes", result.error());
    }
}

void KnowledgeObjectPanel::onDeleteClicked() {
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

void KnowledgeObjectPanel::onConnectClicked() {
    auto selected = listView_->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    auto sourceId = model_->idAt(selected.first().row());
    if (!sourceId.has_value()) return;

    auto source = controller_->findKnowledgeObject(*sourceId);
    if (!source.has_value()) return;

    std::vector<KnowledgeObject> candidates;
    for (const auto& object : controller_->knowledgeObjectsInTopic(topicId_)) {
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

void KnowledgeObjectPanel::onRelationshipsClicked() {
    if (relationshipsWindow_ == nullptr) {
        relationshipsWindow_ = new RelationshipsWindow(*controller_, this);
    }
    relationshipsWindow_->show();
    relationshipsWindow_->raise();
    relationshipsWindow_->activateWindow();
}

void KnowledgeObjectPanel::onSuggestProjectsClicked() {
    auto topic = controller_->findTopic(topicId_);
    QString topicName = topic.has_value() ? QString::fromStdString(topic->name()) : "This Topic";

    auto suggestions = controller_->suggestProjects(topicId_);
    ProjectSuggestionsDialog dialog(topicName, suggestions, this);
    dialog.exec();
}

void KnowledgeObjectPanel::showControllerError(const QString& action,
                                                 const ControllerFailure& failure) {
    QMessageBox::warning(
        this, "Atlas", QString("Couldn't %1: %2").arg(action, QString::fromStdString(failure.detail)));
}

}  // namespace atlas::ui
