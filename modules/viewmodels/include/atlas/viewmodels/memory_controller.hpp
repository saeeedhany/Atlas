#pragma once

#include <QObject>

#include <optional>
#include <vector>

#include "atlas/learning/memory_ledger.hpp"
#include "atlas/learning/network_rules.hpp"
#include "atlas/learning/session_planner.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/clock.hpp"
#include "atlas/viewmodels/controller_error.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class MemoryController : public QObject {
    Q_OBJECT

public:
    MemoryController(atlas::persistence::Database& database, WorkspaceController& workspace, Clock clock,
                     QObject* parent = nullptr);

    Result<void, ControllerFailure> load();

    std::optional<double> recallChance(const atlas::core::ItemRef& item) const;
    int introducedToday() const;
    atlas::learning::SessionPlan todayPlan(const atlas::learning::SessionLimits& limits) const;

    const atlas::learning::StateMap& states() const { return states_; }
    const std::vector<atlas::core::ReviewEvent>& events() const { return events_; }
    const atlas::learning::NetworkRules& rules() const { return rules_; }
    atlas::core::TimePoint now() const { return clock_(); }

signals:
    void memoryChanged();

private:
    atlas::persistence::LearningRepository repository_;
    WorkspaceController* workspace_;
    Clock clock_;
    atlas::learning::Fsrs fsrs_;
    atlas::learning::NetworkRules rules_;
    atlas::learning::SessionPlanner planner_;
    atlas::learning::MemoryLedger ledger_;
    std::vector<atlas::core::ReviewEvent> events_;
    atlas::learning::StateMap states_;
};

}  // namespace atlas::viewmodels
