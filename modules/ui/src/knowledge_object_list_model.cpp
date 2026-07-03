#include "atlas/ui/knowledge_object_list_model.hpp"

namespace atlas::ui {

KnowledgeObjectListModel::KnowledgeObjectListModel(WorkspaceController& controller,
                                                     QObject* parent)
    : QAbstractListModel(parent), controller_(&controller) {
    connect(controller_, &WorkspaceController::graphChanged, this,
            &KnowledgeObjectListModel::refresh);
    refresh();
}

int KnowledgeObjectListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(items_.size());
}

QVariant KnowledgeObjectListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        static_cast<size_t>(index.row()) >= items_.size()) {
        return {};
    }
    const auto& object = items_[static_cast<size_t>(index.row())];
    if (role == Qt::DisplayRole) {
        return QString::fromStdString(object.title());
    }
    if (role == Qt::ToolTipRole) {
        return object.definition().empty() ? QString("(no definition yet)")
                                            : QString::fromStdString(object.definition());
    }
    return {};
}

std::optional<atlas::core::KnowledgeObjectId> KnowledgeObjectListModel::idAt(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= items_.size()) return std::nullopt;
    return items_[static_cast<size_t>(row)].id();
}

void KnowledgeObjectListModel::setSearchQuery(QString query) {
    searchQuery_ = std::move(query);
    refresh();
}

void KnowledgeObjectListModel::setTopicFilter(std::optional<atlas::core::TopicId> topicId) {
    topicFilter_ = topicId;
    refresh();
}

void KnowledgeObjectListModel::refresh() {
    beginResetModel();
    items_ = searchQuery_.isEmpty() ? controller_->allKnowledgeObjects()
                                      : controller_->search(searchQuery_.toStdString());
    if (topicFilter_.has_value()) {
        std::erase_if(items_, [this](const atlas::core::KnowledgeObject& object) {
            return !object.topicId().has_value() || *object.topicId() != *topicFilter_;
        });
    }
    endResetModel();
}

}  // namespace atlas::ui
