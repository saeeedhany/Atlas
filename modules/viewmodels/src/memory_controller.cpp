#include "atlas/viewmodels/memory_controller.hpp"

#include <QDateTime>

#include <unordered_map>

#include "atlas/learning/rebuild_grader.hpp"

namespace atlas::viewmodels {

using atlas::core::ItemKind;
using atlas::core::ItemRef;
using atlas::core::MemoryState;
using atlas::core::Phase;
using atlas::core::TimePoint;

namespace {

QDate localDate(TimePoint time) {
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(millis).date();
}

ControllerFailure persistenceFailure(const atlas::persistence::PersistenceError& error) {
    return ControllerFailure{ControllerErrorCode::PersistenceFailed, error.detail};
}

}  // namespace

MemoryController::MemoryController(atlas::persistence::Database& database, WorkspaceController& workspace,
                                   Clock clock, QObject* parent)
    : QObject(parent),
      repository_(database),
      workspace_(&workspace),
      clock_(std::move(clock)),
      rules_(workspace.graph(), fsrs_),
      planner_(workspace.graph(), rules_, fsrs_),
      ledger_(fsrs_) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &MemoryController::memoryChanged);
}

Result<void, ControllerFailure> MemoryController::load() {
    using Out = Result<void, ControllerFailure>;
    auto events = repository_.allEvents();
    if (!events.hasValue()) return Out::err(persistenceFailure(events.error()));
    events_ = std::move(events).value();

    auto fresh = repository_.isCacheFresh(atlas::learning::kReplayVersion);
    if (!fresh.hasValue()) return Out::err(persistenceFailure(fresh.error()));

    states_.clear();
    if (fresh.value()) {
        auto stored = repository_.allStates();
        if (!stored.hasValue()) return Out::err(persistenceFailure(stored.error()));
        for (auto& state : stored.value()) states_.emplace(state.item, state);
    } else {
        states_ = ledger_.replay(events_, clock_(), rules_.boostFn());
        std::vector<MemoryState> rebuilt;
        rebuilt.reserve(states_.size());
        for (const auto& entry : states_) rebuilt.push_back(entry.second);
        auto replaced = repository_.replaceStates(rebuilt, atlas::learning::kReplayVersion);
        if (!replaced.hasValue()) return Out::err(persistenceFailure(replaced.error()));
    }

    emit memoryChanged();
    return Out::ok();
}

std::optional<double> MemoryController::recallChance(const ItemRef& item) const {
    auto it = states_.find(item);
    if (it == states_.end() || it->second.phase == Phase::New) return std::nullopt;
    return fsrs_.recallChance(it->second, clock_());
}

int MemoryController::introducedToday() const {
    std::unordered_map<ItemRef, TimePoint> firstReview;
    for (const auto& event : events_) {
        if (event.item.kind != ItemKind::Concept) continue;
        auto [it, inserted] = firstReview.try_emplace(event.item, event.reviewedAt);
        if (!inserted && event.reviewedAt < it->second) it->second = event.reviewedAt;
    }
    QDate today = localDate(clock_());
    int count = 0;
    for (const auto& entry : firstReview) {
        if (localDate(entry.second) == today) ++count;
    }
    return count;
}

atlas::learning::SessionPlan MemoryController::todayPlan(const atlas::learning::SessionLimits& limits) const {
    return planner_.plan(states_, clock_(), introducedToday(), limits,
                         atlas::learning::medianRebuildResponse(events_));
}

}  // namespace atlas::viewmodels
