#include "atlas/ui/topic_selector_widget.hpp"

#include <QAction>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace atlas::ui {

TopicSelectorWidget::TopicSelectorWidget(WorkspaceController& controller, QWidget* parent)
    : QWidget(parent), controller_(&controller) {
    auto* heading = new QLabel("Topics", this);
    heading->setObjectName("panelHeading");
    auto headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    headingFont.setBold(true);
    heading->setFont(headingFont);

    listWidget_ = new QListWidget(this);
    listWidget_->setAlternatingRowColors(true);
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    emptyStateLabel_ = new QLabel(
        "No topics yet.\n\nClick \"New Topic\" below to start one — \"OS,\" \"Databases,\" "
        "whatever you're building a map of.",
        this);
    emptyStateLabel_->setAlignment(Qt::AlignCenter);
    emptyStateLabel_->setStyleSheet("color: gray;");

    listStack_ = new QStackedWidget(this);
    listStack_->addWidget(listWidget_);
    listStack_->addWidget(emptyStateLabel_);

    newTopicButton_ = new QPushButton("New Topic", this);

    connect(newTopicButton_, &QPushButton::clicked, this, &TopicSelectorWidget::onNewTopicClicked);
    connect(listWidget_, &QListWidget::itemActivated, this, &TopicSelectorWidget::onItemActivated);
    connect(listWidget_, &QListWidget::customContextMenuRequested, this,
            &TopicSelectorWidget::onContextMenuRequested);
    connect(controller_, &WorkspaceController::topicsChanged, this, &TopicSelectorWidget::refresh);
    // A topic's member count (shown per row) depends on
    // KnowledgeObjects, not just Topics — a concept getting created,
    // deleted, or moved between topics needs to update these counts
    // too, even though no Topic itself changed.
    connect(controller_, &WorkspaceController::graphChanged, this, &TopicSelectorWidget::refresh);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(heading);
    layout->addWidget(listStack_, /*stretch=*/1);
    layout->addWidget(newTopicButton_, 0, Qt::AlignLeft);

    refresh();
}

void TopicSelectorWidget::refresh() {
    listWidget_->clear();
    rowTopicIds_.clear();

    for (const auto& topic : controller_->allTopics()) {
        auto memberCount = controller_->knowledgeObjectsInTopic(topic.id()).size();
        auto label = QString("%1  (%2)").arg(QString::fromStdString(topic.name())).arg(memberCount);
        listWidget_->addItem(label);
        rowTopicIds_.push_back(topic.id());
    }

    updateEmptyState();
}

void TopicSelectorWidget::updateEmptyState() {
    listStack_->setCurrentWidget(listWidget_->count() == 0 ? static_cast<QWidget*>(emptyStateLabel_)
                                                             : static_cast<QWidget*>(listWidget_));
}

void TopicSelectorWidget::onNewTopicClicked() {
    bool ok = false;
    QString name =
        QInputDialog::getText(this, "New Topic", "Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    auto result = controller_->createTopic(name.trimmed().toStdString());
    if (!result.hasValue()) {
        QMessageBox::warning(this, "Atlas",
                              QString("Couldn't create the topic: %1")
                                  .arg(QString::fromStdString(result.error().detail)));
        return;
    }
    emit topicChosen(result.value());
}

void TopicSelectorWidget::onItemActivated() {
    int row = listWidget_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= rowTopicIds_.size()) return;
    emit topicChosen(rowTopicIds_[static_cast<size_t>(row)]);
}

void TopicSelectorWidget::onContextMenuRequested(const QPoint& pos) {
    auto* item = listWidget_->itemAt(pos);
    if (item == nullptr) return;
    int row = listWidget_->row(item);
    if (row < 0 || static_cast<size_t>(row) >= rowTopicIds_.size()) return;
    auto topicId = rowTopicIds_[static_cast<size_t>(row)];

    auto topic = controller_->findTopic(topicId);
    if (!topic.has_value()) return;

    QMenu menu(this);
    QAction* renameAction = menu.addAction("Rename...");
    QAction* deleteAction = menu.addAction("Delete...");
    // The Uncategorized topic can be renamed but never deleted — same
    // rule WorkspaceController::removeTopic already enforces (it's
    // where migrated and not-yet-sorted concepts live and would have
    // nowhere to go). Disabling it here is just a friendlier way to
    // say "no" than letting the click through and showing an error.
    deleteAction->setEnabled(topicId != atlas::core::uncategorizedTopicId());

    QAction* chosen = menu.exec(listWidget_->viewport()->mapToGlobal(pos));
    if (chosen == renameAction) {
        bool ok = false;
        QString newName = QInputDialog::getText(this, "Rename Topic", "Name:", QLineEdit::Normal,
                                                   QString::fromStdString(topic->name()), &ok);
        if (!ok || newName.trimmed().isEmpty()) return;

        auto result = controller_->renameTopic(topicId, newName.trimmed().toStdString());
        if (!result.hasValue()) {
            QMessageBox::warning(this, "Atlas",
                                  QString("Couldn't rename the topic: %1")
                                      .arg(QString::fromStdString(result.error().detail)));
        }
    } else if (chosen == deleteAction) {
        auto confirm = QMessageBox::question(
            this, "Delete Topic",
            QString("Delete \"%1\"? This can't be undone.").arg(QString::fromStdString(topic->name())),
            QMessageBox::Yes | QMessageBox::No);
        if (confirm != QMessageBox::Yes) return;

        auto result = controller_->removeTopic(topicId);
        if (!result.hasValue()) {
            QMessageBox::warning(this, "Atlas",
                                  QString("Couldn't delete the topic: %1")
                                      .arg(QString::fromStdString(result.error().detail)));
        }
    }
}

}  // namespace atlas::ui
