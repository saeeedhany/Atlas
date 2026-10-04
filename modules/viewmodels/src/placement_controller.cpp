#include "atlas/viewmodels/placement_controller.hpp"

#include <iterator>

namespace atlas::viewmodels {

using atlas::core::Placement;
using atlas::render::ForceDirectedLayout;
using atlas::render::LayoutHints;

namespace {

Result<void, ControllerFailure> persistenceFailure(const atlas::persistence::PersistenceError& error) {
    return Result<void, ControllerFailure>::err({ControllerErrorCode::PersistenceFailed, error.detail});
}

}  // namespace

PlacementController::PlacementController(atlas::persistence::Database& database, WorkspaceController& workspace,
                                         QObject* parent)
    : QObject(parent), repository_(database), workspace_(&workspace) {
    connect(workspace_, &WorkspaceController::graphChanged, this, [this] {
        auto arranged = arrange();
        if (!arranged.hasValue()) emit failed(QString::fromStdString(arranged.error().detail));
    });
}

Result<void, ControllerFailure> PlacementController::load() {
    auto stored = repository_.findAll();
    if (!stored.hasValue()) return persistenceFailure(stored.error());
    placements_.clear();
    for (const auto& placement : stored.value()) placements_.insert_or_assign(placement.conceptId, placement);
    return arrange();
}

void PlacementController::forgetRemovedConcepts() {
    const auto& graph = workspace_->graph();
    for (auto it = placements_.begin(); it != placements_.end();) {
        it = graph.findNode(it->first) == nullptr ? placements_.erase(it) : std::next(it);
    }
}

Result<void, ControllerFailure> PlacementController::arrange() {
    forgetRemovedConcepts();
    const auto& graph = workspace_->graph();
    LayoutHints hints;
    bool anyUnplaced = false;
    for (const auto& id : graph.allNodeIds()) {
        auto saved = placements_.find(id);
        if (saved == placements_.end()) {
            anyUnplaced = true;
            continue;
        }
        hints.initial[id] = {saved->second.x, saved->second.y};
        hints.pinned.insert(id);
    }
    if (!anyUnplaced) return Result<void, ControllerFailure>::ok();

    std::vector<Placement> added;
    for (const auto& [id, point] : ForceDirectedLayout::compute(graph, {}, hints)) {
        if (!placements_.contains(id)) added.push_back(Placement{id, point.x, point.y, false});
    }
    return save(added);
}

Result<void, ControllerFailure> PlacementController::tidy() {
    forgetRemovedConcepts();
    LayoutHints hints;
    for (const auto& [id, placement] : placements_) {
        hints.initial[id] = {placement.x, placement.y};
        if (placement.pinned) hints.pinned.insert(id);
    }
    std::vector<Placement> moved;
    for (const auto& [id, point] : ForceDirectedLayout::compute(workspace_->graph(), {}, hints)) {
        moved.push_back(Placement{id, point.x, point.y, isPinned(id)});
    }
    return save(moved);
}

Result<void, ControllerFailure> PlacementController::setPinned(const KnowledgeObjectId& id, bool pinned) {
    auto found = placements_.find(id);
    if (found == placements_.end()) {
        return Result<void, ControllerFailure>::err({ControllerErrorCode::NotFound, "No placement for that concept"});
    }
    Placement updated = found->second;
    updated.pinned = pinned;
    return save({updated});
}

Result<void, ControllerFailure> PlacementController::save(const std::vector<Placement>& changed) {
    if (changed.empty()) return Result<void, ControllerFailure>::ok();
    auto saved = repository_.saveAll(changed);
    if (!saved.hasValue()) return persistenceFailure(saved.error());
    for (const auto& placement : changed) placements_.insert_or_assign(placement.conceptId, placement);
    emit placementsChanged();
    return Result<void, ControllerFailure>::ok();
}

std::optional<atlas::render::Point2D> PlacementController::position(const KnowledgeObjectId& id) const {
    auto found = placements_.find(id);
    if (found == placements_.end()) return std::nullopt;
    return atlas::render::Point2D{found->second.x, found->second.y};
}

bool PlacementController::isPinned(const KnowledgeObjectId& id) const {
    auto found = placements_.find(id);
    return found != placements_.end() && found->second.pinned;
}

}  // namespace atlas::viewmodels
