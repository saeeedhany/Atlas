#pragma once

#include <QSettings>

#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/clock.hpp"
#include "atlas/viewmodels/concept_editor.hpp"
#include "atlas/viewmodels/concept_links_model.hpp"
#include "atlas/viewmodels/map_view_model.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/today_view_model.hpp"
#include "atlas/viewmodels/topics_model.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class AppContext {
public:
    AppContext(atlas::persistence::Database& database, QSettings& store, Clock clock);
    ~AppContext();
    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;

    Result<void, ControllerFailure> load();
    void provideSingletons();

    WorkspaceController& workspace() { return workspace_; }
    MemoryController& memory() { return memory_; }
    PlacementController& placements() { return placements_; }
    AppSettings& settings() { return settings_; }
    Palette& palette() { return palette_; }
    TopicsModel& topics() { return topics_; }
    MapViewModel& map() { return map_; }
    ConceptEditor& conceptEditor() { return conceptEditor_; }
    ConceptLinksModel& links() { return links_; }
    TodayViewModel& today() { return today_; }

private:
    WorkspaceController workspace_;
    MemoryController memory_;
    PlacementController placements_;
    AppSettings settings_;
    Palette palette_;
    TopicsModel topics_;
    MapViewModel map_;
    ConceptEditor conceptEditor_;
    ConceptLinksModel links_;
    TodayViewModel today_;
};

}  // namespace atlas::viewmodels
