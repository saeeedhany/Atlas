#pragma once

#include <QAbstractListModel>

#include <optional>
#include <vector>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/ui/workspace_controller.hpp"

namespace atlas::ui {

// Backs a QListView with the controller's KnowledgeObjects. Refreshes
// via a full model reset on every controller signal, rather than
// fine-grained row insert/remove — deliberately simple: at this
// milestone's data volumes a reset is imperceptible, and precise
// index-tracking logic is exactly the kind of code that's easy to get
// subtly wrong. Revisit only once a full reset is actually visible to
// a user (e.g. losing scroll position on a list of thousands of rows).
class KnowledgeObjectListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit KnowledgeObjectListModel(WorkspaceController& controller, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // Plain accessor, not a Qt model role: simpler than registering
    // KnowledgeObjectId as a QVariant-compatible metatype for a value
    // that's only ever needed by this same C++ codebase (the
    // MainWindow that owns this model), never by QML or a delegate.
    std::optional<atlas::core::KnowledgeObjectId> idAt(int row) const;

public slots:
    void refresh();

private:
    WorkspaceController* controller_;
    std::vector<atlas::core::KnowledgeObject> items_;
};

}  // namespace atlas::ui
