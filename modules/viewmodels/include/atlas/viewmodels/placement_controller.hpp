#pragma once

#include <QObject>
#include <QString>

#include <optional>
#include <unordered_map>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/persistence/placement_repository.hpp"
#include "atlas/render/force_directed_layout.hpp"
#include "atlas/viewmodels/controller_error.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class PlacementController : public QObject {
    Q_OBJECT

public:
    static constexpr double kMaxStartingOffset = 40.0;

    PlacementController(atlas::persistence::Database& database, WorkspaceController& workspace,
                        QObject* parent = nullptr);

    Result<void, ControllerFailure> load();
    Result<void, ControllerFailure> arrange();
    Result<void, ControllerFailure> tidy();
    Result<void, ControllerFailure> setPinned(const KnowledgeObjectId& id, bool pinned);
    Result<void, ControllerFailure> moveBy(const std::vector<KnowledgeObjectId>& ids, double dx, double dy);

    std::optional<atlas::render::Point2D> position(const KnowledgeObjectId& id) const;
    bool isPinned(const KnowledgeObjectId& id) const;

signals:
    void placementsChanged();
    void failed(const QString& message);

private:
    Result<void, ControllerFailure> save(const std::vector<atlas::core::Placement>& changed);
    void forgetRemovedConcepts();
    atlas::render::Point2D startingPointFor(const KnowledgeObjectId& id) const;

    atlas::persistence::PlacementRepository repository_;
    WorkspaceController* workspace_;
    std::unordered_map<KnowledgeObjectId, atlas::core::Placement> placements_;
    bool loaded_ = false;
};

atlas::render::Point2D startingOffsetFor(const KnowledgeObjectId& id);

}
