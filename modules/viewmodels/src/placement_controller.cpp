#include "atlas/viewmodels/placement_controller.hpp"

#include <cmath>
#include <functional>
#include <iterator>

namespace atlas::viewmodels {

using atlas::core::Placement;
using atlas::render::ForceDirectedLayout;
using atlas::render::LayoutHints;

namespace {

Result<void, ControllerFailure> persistenceFailure(const atlas::persistence::PersistenceError& error) {
    return Result<void, ControllerFailure>::err({ControllerErrorCode::PersistenceFailed, error.detail});
}

struct Centroid {
    double sumX = 0.0;
    double sumY = 0.0;
    int count = 0;

    void add(const Placement& placement) {
        sumX += placement.x;
        sumY += placement.y;
        ++count;
    }
    atlas::render::Point2D center() const { return {sumX / count, sumY / count}; }
};

}  // namespace

atlas::render::Point2D startingOffsetFor(const KnowledgeObjectId& id) {
    constexpr double kTwoPi = 6.283185307179586;
    size_t hash = std::hash<std::string>{}(id.toString());
    double angle = kTwoPi * static_cast<double>(hash % 360) / 360.0;
    double radius = PlacementController::kMaxStartingOffset *
                    (0.25 + 0.75 * static_cast<double>((hash / 360) % 101) / 100.0);
    return {radius * std::cos(angle), radius * std::sin(angle)};
}

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
    loaded_ = true;
    return arrange();
}

atlas::render::Point2D PlacementController::startingPointFor(const KnowledgeObjectId& id) const {
    const auto& graph = workspace_->graph();
    Centroid neighbors;
    for (const auto& neighbor : graph.neighbors(id, std::nullopt, atlas::graph::GraphEngine::Direction::Both)) {
        auto placed = placements_.find(neighbor);
        if (placed != placements_.end()) neighbors.add(placed->second);
    }
    Centroid peers;
    Centroid everything;
    const auto* object = graph.findNode(id);
    auto topic = object != nullptr ? object->topicId() : std::nullopt;
    for (const auto& [placedId, placement] : placements_) {
        everything.add(placement);
        const auto* placedObject = graph.findNode(placedId);
        if (topic && placedObject != nullptr && placedObject->topicId() == topic) peers.add(placement);
    }
    atlas::render::Point2D base;
    if (neighbors.count > 0) base = neighbors.center();
    else if (peers.count > 0) base = peers.center();
    else if (everything.count > 0) base = everything.center();
    auto offset = startingOffsetFor(id);
    return {base.x + offset.x, base.y + offset.y};
}

void PlacementController::forgetRemovedConcepts() {
    const auto& graph = workspace_->graph();
    for (auto it = placements_.begin(); it != placements_.end();) {
        it = graph.findNode(it->first) == nullptr ? placements_.erase(it) : std::next(it);
    }
}

Result<void, ControllerFailure> PlacementController::arrange() {
    if (!loaded_) return Result<void, ControllerFailure>::ok();
    forgetRemovedConcepts();
    const auto& graph = workspace_->graph();
    LayoutHints hints;
    std::vector<KnowledgeObjectId> unplaced;
    for (const auto& id : graph.allNodeIds()) {
        auto saved = placements_.find(id);
        if (saved == placements_.end()) {
            unplaced.push_back(id);
            continue;
        }
        hints.initial[id] = {saved->second.x, saved->second.y};
        hints.pinned.insert(id);
    }
    if (unplaced.empty()) return Result<void, ControllerFailure>::ok();
    if (!placements_.empty()) {
        for (const auto& id : unplaced) hints.initial[id] = startingPointFor(id);
    }

    std::vector<Placement> added;
    for (const auto& [id, point] : ForceDirectedLayout::compute(graph, {}, hints)) {
        if (!placements_.contains(id)) added.push_back(Placement{id, point.x, point.y, false});
    }
    return save(added);
}

Result<void, ControllerFailure> PlacementController::tidy() {
    if (!loaded_) return Result<void, ControllerFailure>::ok();
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

Result<void, ControllerFailure> PlacementController::moveBy(const std::vector<KnowledgeObjectId>& ids, double dx,
                                                            double dy) {
    std::vector<Placement> moved;
    for (const auto& id : ids) {
        auto placed = placements_.find(id);
        if (placed == placements_.end()) continue;
        Placement next = placed->second;
        next.x += dx;
        next.y += dy;
        moved.push_back(next);
    }
    return save(moved);
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
