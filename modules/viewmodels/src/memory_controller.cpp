#include "atlas/viewmodels/memory_controller.hpp"

#include <QDateTime>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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

}

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
    bool loadedFromCache = false;
    if (fresh.value()) {
        auto stored = repository_.allStates();
        if (stored.hasValue()) {
            for (auto& state : stored.value()) states_.emplace(state.item, state);
            loadedFromCache = true;
        }
    }
    if (!loadedFromCache) {
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

double MemoryController::elapsedDaysFor(const ItemRef& item, TimePoint at) const {
    auto it = states_.find(item);
    if (it == states_.end() || !it->second.lastReviewedAt) return 0.0;
    using Days = std::chrono::duration<double, std::ratio<86400>>;
    return std::max(0.0, std::chrono::duration_cast<Days>(at - *it->second.lastReviewedAt).count());
}

Result<void, ControllerFailure> MemoryController::record(std::vector<atlas::core::ReviewEvent> events) {
    using Out = Result<void, ControllerFailure>;
    if (events.empty()) return Out::ok();
    TimePoint now = clock_();
    for (auto& event : events) {
        event.reviewedAt = std::chrono::floor<std::chrono::milliseconds>(std::min(event.reviewedAt, now));
        event.elapsedDays = std::max(0.0, event.elapsedDays);
    }
    events = atlas::learning::inReplayOrder(std::move(events), now);
    auto next = states_;
    auto boost = rules_.boostFn();
    std::unordered_set<ItemRef> touched;
    for (const auto& event : events) {
        ledger_.apply(next, event, boost);
        touched.insert(event.item);
    }
    std::vector<MemoryState> changed;
    changed.reserve(touched.size());
    for (const auto& item : touched) changed.push_back(next.at(item));
    auto written = repository_.record(events, changed);
    if (!written.hasValue()) return Out::err(persistenceFailure(written.error()));
    states_ = std::move(next);
    events_.insert(events_.end(), events.begin(), events.end());
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

}
