#include "atlas/viewmodels/app_context.hpp"

namespace atlas::viewmodels {

AppContext::AppContext(atlas::persistence::Database& database, QSettings& store, Clock clock)
    : workspace_(database),
      memory_(database, workspace_, std::move(clock)),
      placements_(database, workspace_),
      settings_(store),
      palette_(settings_),
      topics_(workspace_),
      map_(workspace_, memory_, placements_, palette_),
      conceptEditor_(workspace_, memory_, placements_),
      links_(workspace_, memory_),
      today_(workspace_, memory_, settings_) {}

AppContext::~AppContext() {
    AppSettings::provide(nullptr);
    Palette::provide(nullptr);
    TopicsModel::provide(nullptr);
    MapViewModel::provide(nullptr);
    ConceptEditor::provide(nullptr);
    ConceptLinksModel::provide(nullptr);
    TodayViewModel::provide(nullptr);
}

Result<void, ControllerFailure> AppContext::load() {
    if (auto loaded = workspace_.load(); !loaded.hasValue()) return loaded;
    if (auto placed = placements_.load(); !placed.hasValue()) return placed;
    if (auto remembered = memory_.load(); !remembered.hasValue()) return remembered;
    topics_.refresh();
    return Result<void, ControllerFailure>::ok();
}

void AppContext::provideSingletons() {
    AppSettings::provide(&settings_);
    Palette::provide(&palette_);
    TopicsModel::provide(&topics_);
    MapViewModel::provide(&map_);
    ConceptEditor::provide(&conceptEditor_);
    ConceptLinksModel::provide(&links_);
    TodayViewModel::provide(&today_);
}

}  // namespace atlas::viewmodels
