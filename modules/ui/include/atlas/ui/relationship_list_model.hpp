#pragma once

#include <QAbstractListModel>

#include <optional>
#include <vector>

#include "atlas/core/relationship.hpp"
#include "atlas/ui/workspace_controller.hpp"

namespace atlas::ui {

// Mirrors KnowledgeObjectListModel's structure and its "full reset on
// every change" simplicity — see that class's header comment for the
// rationale, which applies identically here.
class RelationshipListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit RelationshipListModel(WorkspaceController& controller, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    std::optional<atlas::core::RelationshipId> idAt(int row) const;

public slots:
    void refresh();

private:
    WorkspaceController* controller_;
    std::vector<atlas::core::Relationship> items_;
};

}  // namespace atlas::ui
