#include "atlas/viewmodels/today_view_model.hpp"

#include <algorithm>

namespace atlas::viewmodels {

namespace {
constexpr long long kSecondsPerMinute = 60;
}

TodayViewModel::TodayViewModel(WorkspaceController& workspace, MemoryController& memory, AppSettings& settings,
                               QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), settings_(&settings) {
    connect(memory_, &MemoryController::memoryChanged, this, &TodayViewModel::refresh);
    connect(settings_, &AppSettings::newPerDayChanged, this, &TodayViewModel::refresh);
    refresh();
}

void TodayViewModel::refresh() {
    atlas::learning::SessionLimits limits;
    limits.newPerDay = settings_->newPerDay();
    auto plan = memory_->todayPlan(limits);

    dueCount_ = 0;
    newCount_ = 0;
    itemCount_ = 0;
    for (const auto& focus : plan.focuses) {
        (focus.isNew ? newCount_ : dueCount_) += 1;
        itemCount_ += static_cast<int>(focus.items.size());
    }
    long long seconds = plan.estimatedDuration.count();
    estimatedMinutes_ = plan.focuses.empty()
                            ? 0
                            : static_cast<int>(std::max(1LL, (seconds + kSecondsPerMinute - 1) / kSecondsPerMinute));

    auto objects = workspace_->allKnowledgeObjects();
    conceptCount_ = static_cast<int>(objects.size());
    learnedCount_ = static_cast<int>(std::count_if(objects.begin(), objects.end(), [this](const auto& object) {
        return memory_->recallChance(atlas::core::ItemRef::forConcept(object.id())).has_value();
    }));
    emit changed();
}

}
