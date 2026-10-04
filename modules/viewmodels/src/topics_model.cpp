#include "atlas/viewmodels/topics_model.hpp"

#include <unordered_map>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::TopicId;

TopicsModel::TopicsModel(WorkspaceController& workspace, QObject* parent)
    : QAbstractListModel(parent), workspace_(&workspace) {
    connect(workspace_, &WorkspaceController::topicsChanged, this, &TopicsModel::refresh);
    connect(workspace_, &WorkspaceController::graphChanged, this, &TopicsModel::refresh);
    refresh();
}

void TopicsModel::refresh() {
    auto topics = workspace_->topics();
    if (!topics.hasValue()) {
        emit errorOccurred(toQString(topics.error().detail));
        return;
    }
    std::unordered_map<TopicId, int> conceptCounts;
    for (const auto& object : workspace_->allKnowledgeObjects()) {
        if (object.topicId()) ++conceptCounts[*object.topicId()];
    }
    beginResetModel();
    rows_.clear();
    for (const auto& topic : topics.value()) {
        auto counted = conceptCounts.find(topic.id());
        rows_.push_back(Row{idString(topic.id()), toQString(topic.name()),
                            counted == conceptCounts.end() ? 0 : counted->second,
                            topic.id() == atlas::core::uncategorizedTopicId()});
    }
    endResetModel();
    emit countChanged();
}

int TopicsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant TopicsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const Row& row = rows_[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return row.id;
        case NameRole:
        case Qt::DisplayRole: return row.name;
        case ConceptCountRole: return row.conceptCount;
        case IsUncategorizedRole: return row.uncategorized;
        default: return {};
    }
}

QHash<int, QByteArray> TopicsModel::roleNames() const {
    return {{IdRole, "topicId"}, {NameRole, "name"}, {ConceptCountRole, "conceptCount"},
            {IsUncategorizedRole, "uncategorized"}};
}

bool TopicsModel::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

QString TopicsModel::createTopic(const QString& name) {
    auto created = workspace_->createTopic(toStdString(name));
    if (!created.hasValue()) {
        fail(toQString(created.error().detail));
        return {};
    }
    return idString(created.value());
}

bool TopicsModel::rename(const QString& id, const QString& name) {
    auto topic = parseId<TopicId>(id);
    if (!topic) return fail(tr("Unknown topic"));
    auto renamed = workspace_->renameTopic(*topic, toStdString(name));
    return renamed.hasValue() || fail(toQString(renamed.error().detail));
}

bool TopicsModel::remove(const QString& id) {
    auto topic = parseId<TopicId>(id);
    if (!topic) return fail(tr("Unknown topic"));
    auto removed = workspace_->removeTopic(*topic);
    return removed.hasValue() || fail(toQString(removed.error().detail));
}

QString TopicsModel::nameOf(const QString& id) const {
    for (const auto& row : rows_) {
        if (row.id == id) return row.name;
    }
    return {};
}

}  // namespace atlas::viewmodels
