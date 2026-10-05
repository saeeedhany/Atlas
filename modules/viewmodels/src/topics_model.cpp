#include "atlas/viewmodels/topics_model.hpp"

#include <QVariantMap>

#include <algorithm>
#include <unordered_map>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::KnowledgeObject;
using atlas::core::TopicId;

TopicsModel::TopicsModel(WorkspaceController& workspace, MemoryController& memory, QObject* parent)
    : QAbstractListModel(parent), workspace_(&workspace), memory_(&memory) {
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

QVariantList TopicsModel::entries() const {
    QVariantList list;
    for (const auto& row : rows_) {
        list.append(QVariantMap{{"topicId", row.id}, {"name", row.name}, {"conceptCount", row.conceptCount},
                                {"uncategorized", row.uncategorized}});
    }
    return list;
}

QVariantList TopicsModel::suggestProjects(const QString& topicId) const {
    auto id = parseId<TopicId>(topicId);
    if (!id) return {};
    struct Idea {
        KnowledgeObject object;
        double readiness;
        int leverage;
        double score() const { return readiness * (1 + leverage); }
    };
    const auto& graph = workspace_->graph();
    const auto& rules = memory_->rules();
    auto now = memory_->now();
    std::vector<Idea> ranked;
    for (const auto& object : workspace_->allKnowledgeObjects()) {
        if (object.topicId() != id || object.miniProjects().empty()) continue;
        if (rules.isSolid(object.id(), now, memory_->states())) continue;
        auto prerequisites = graph.dependsOn(object.id());
        auto solid = std::count_if(prerequisites.begin(), prerequisites.end(), [&](const auto& prerequisite) {
            return rules.isSolid(prerequisite, now, memory_->states());
        });
        double readiness = prerequisites.empty() ? 1.0 : static_cast<double>(solid) / prerequisites.size();
        ranked.push_back(Idea{object, readiness, rules.leverage(object.id())});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Idea& a, const Idea& b) {
        if (a.score() != b.score()) return a.score() > b.score();
        return a.object.id().toString() < b.object.id().toString();
    });
    QVariantList ideas;
    for (const auto& idea : ranked) {
        QVariantList projects;
        for (const auto& project : idea.object.miniProjects()) {
            projects.append(QVariantMap{{"title", toQString(project.title)}, {"description", toQString(project.description)}});
        }
        ideas.append(QVariantMap{{"id", idString(idea.object.id())},
                                 {"title", toQString(idea.object.title())},
                                 {"readiness", idea.readiness},
                                 {"leverage", idea.leverage},
                                 {"projects", projects}});
    }
    return ideas;
}

}  // namespace atlas::viewmodels
