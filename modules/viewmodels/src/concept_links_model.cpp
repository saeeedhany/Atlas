#include "atlas/viewmodels/concept_links_model.hpp"

#include <QVariantMap>

#include <algorithm>
#include <string>
#include <unordered_map>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::ItemRef;
using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;

namespace {
constexpr int kTypeCount = static_cast<int>(RelationshipType::Causes) + 1;
}

QString relationshipLabel(RelationshipType type) {
    switch (type) {
        case RelationshipType::DependsOn: return QStringLiteral("depends on");
        case RelationshipType::Uses: return QStringLiteral("uses");
        case RelationshipType::Implements: return QStringLiteral("implements");
        case RelationshipType::Solves: return QStringLiteral("solves");
        case RelationshipType::Contains: return QStringLiteral("contains");
        case RelationshipType::PartOf: return QStringLiteral("part of");
        case RelationshipType::RelatedTo: return QStringLiteral("related to");
        case RelationshipType::AlternativeTo: return QStringLiteral("alternative to");
        case RelationshipType::OppositeOf: return QStringLiteral("opposite of");
        case RelationshipType::Causes: return QStringLiteral("causes");
    }
    return {};
}

ConceptLinksModel::ConceptLinksModel(WorkspaceController& workspace, MemoryController& memory, QObject* parent)
    : QAbstractListModel(parent), workspace_(&workspace), memory_(&memory) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &ConceptLinksModel::refresh);
    connect(memory_, &MemoryController::memoryChanged, this, &ConceptLinksModel::refresh);
}

void ConceptLinksModel::setConceptId(const QString& conceptId) {
    if (conceptId == conceptId_) return;
    conceptId_ = conceptId;
    emit conceptIdChanged();
    refresh();
}

void ConceptLinksModel::refresh() {
    beginResetModel();
    rows_.clear();
    auto self = parseId<KnowledgeObjectId>(conceptId_);
    if (self) {
        const auto& graph = workspace_->graph();
        for (const auto& link : workspace_->allRelationships()) {
            bool outgoing = link.sourceId() == *self;
            if (!outgoing && link.targetId() != *self) continue;
            const auto& otherId = outgoing ? link.targetId() : link.sourceId();
            const auto* other = graph.findNode(otherId);
            Row row;
            row.linkId = idString(link.id());
            row.otherId = idString(otherId);
            row.otherTitle = other ? toQString(other->title()) : QString();
            row.typeName = relationshipLabel(link.type());
            row.outgoing = outgoing;
            row.symmetric = atlas::core::isSymmetric(link.type());
            row.note = toQString(link.note().value_or(""));
            row.recall = memory_->recallChance(ItemRef::forLink(link.id())).value_or(-1.0);
            rows_.push_back(row);
        }
        std::sort(rows_.begin(), rows_.end(), [](const Row& a, const Row& b) {
            if (a.otherTitle != b.otherTitle) return a.otherTitle < b.otherTitle;
            return a.typeName < b.typeName;
        });
    }
    endResetModel();
    emit countChanged();
}

int ConceptLinksModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant ConceptLinksModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const Row& row = rows_[static_cast<size_t>(index.row())];
    switch (role) {
        case LinkIdRole: return row.linkId;
        case OtherIdRole: return row.otherId;
        case OtherTitleRole:
        case Qt::DisplayRole: return row.otherTitle;
        case TypeNameRole: return row.typeName;
        case OutgoingRole: return row.outgoing;
        case SymmetricRole: return row.symmetric;
        case NoteRole: return row.note;
        case RecallRole: return row.recall;
        default: return {};
    }
}

QHash<int, QByteArray> ConceptLinksModel::roleNames() const {
    return {{LinkIdRole, "linkId"},   {OtherIdRole, "otherId"},     {OtherTitleRole, "otherTitle"},
            {TypeNameRole, "typeName"}, {OutgoingRole, "outgoing"}, {SymmetricRole, "symmetric"},
            {NoteRole, "note"},       {RecallRole, "recall"}};
}

QStringList ConceptLinksModel::typeNames() const {
    QStringList names;
    for (int type = 0; type < kTypeCount; ++type) names.append(relationshipLabel(static_cast<RelationshipType>(type)));
    return names;
}

bool ConceptLinksModel::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

bool ConceptLinksModel::add(const QString& otherId, int typeIndex, bool outgoing, const QString& note) {
    auto self = parseId<KnowledgeObjectId>(conceptId_);
    auto other = parseId<KnowledgeObjectId>(otherId);
    if (!self || !other) return fail(tr("Unknown concept"));
    if (typeIndex < 0 || typeIndex >= kTypeCount) return fail(tr("Unknown link type"));
    auto type = static_cast<RelationshipType>(typeIndex);
    std::optional<std::string> text = note.isEmpty() ? std::nullopt : std::optional(toStdString(note));
    auto created = outgoing ? workspace_->createRelationship(*self, *other, type, text)
                            : workspace_->createRelationship(*other, *self, type, text);
    return created.hasValue() || fail(toQString(created.error().detail));
}

bool ConceptLinksModel::remove(const QString& linkId) {
    auto id = parseId<RelationshipId>(linkId);
    if (!id) return fail(tr("Unknown link"));
    auto removed = workspace_->removeRelationship(*id);
    return removed.hasValue() || fail(toQString(removed.error().detail));
}

QVariantList ConceptLinksModel::candidates(const QString& query) {
    if (auto topics = workspace_->topics(); topics.hasValue()) {
        topicNames_.clear();
        for (const auto& topic : topics.value()) topicNames_.emplace(topic.id().toString(), topic.name());
    } else {
        emit errorOccurred(toQString(topics.error().detail));
    }
    QVariantList results;
    for (const auto& object : workspace_->search(toStdString(query))) {
        QString id = idString(object.id());
        if (id == conceptId_) continue;
        QString topic;
        if (object.topicId()) {
            auto name = topicNames_.find(object.topicId()->toString());
            if (name != topicNames_.end()) topic = toQString(name->second);
        }
        results.append(QVariantMap{{"id", id}, {"title", toQString(object.title())}, {"topic", topic}});
        if (results.size() == kCandidateLimit) break;
    }
    return results;
}

}
