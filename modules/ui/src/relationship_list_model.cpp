#include "atlas/ui/relationship_list_model.hpp"

namespace atlas::ui {

using atlas::core::toDisplayString;

namespace {

QString describe(WorkspaceController& controller, const atlas::core::Relationship& relationship) {
    auto source = controller.findKnowledgeObject(relationship.sourceId());
    auto target = controller.findKnowledgeObject(relationship.targetId());
    QString sourceTitle =
        source.has_value() ? QString::fromStdString(source->title()) : QString("(unknown)");
    QString targetTitle =
        target.has_value() ? QString::fromStdString(target->title()) : QString("(unknown)");
    QString typeName = QString::fromStdString(std::string(toDisplayString(relationship.type())));
    return QString("%1 \u2014[%2]\u2192 %3").arg(sourceTitle, typeName, targetTitle);
}

}  // namespace

RelationshipListModel::RelationshipListModel(WorkspaceController& controller, QObject* parent)
    : QAbstractListModel(parent), controller_(&controller) {
    connect(controller_, &WorkspaceController::graphChanged, this,
            &RelationshipListModel::refresh);
    refresh();
}

int RelationshipListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(items_.size());
}

QVariant RelationshipListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        static_cast<size_t>(index.row()) >= items_.size()) {
        return {};
    }
    const auto& relationship = items_[static_cast<size_t>(index.row())];
    if (role == Qt::DisplayRole) {
        return describe(*controller_, relationship);
    }
    if (role == Qt::ToolTipRole && relationship.note().has_value()) {
        return QString::fromStdString(*relationship.note());
    }
    return {};
}

std::optional<atlas::core::RelationshipId> RelationshipListModel::idAt(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= items_.size()) return std::nullopt;
    return items_[static_cast<size_t>(row)].id();
}

void RelationshipListModel::refresh() {
    beginResetModel();
    items_ = controller_->allRelationships();
    endResetModel();
}

}  // namespace atlas::ui
