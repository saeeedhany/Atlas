#pragma once

#include <QDialog>

#include <vector>

#include "atlas/ui/workspace_controller.hpp"

namespace atlas::ui {

// Read-only display of ranked project suggestions for a Topic - same
// "computed, not authored, no editing" posture as RoadmapDialog. Each
// row shows the concept, why it was suggested (readiness/leverage,
// not just a bare rank - see GraphEngine::ProjectSuggestion's own doc
// comment on why those are exposed separately), and its mini-projects.
class ProjectSuggestionsDialog : public QDialog {
    Q_OBJECT

public:
    ProjectSuggestionsDialog(const QString& topicName,
                               const std::vector<WorkspaceController::ProjectSuggestion>& suggestions,
                               QWidget* parent = nullptr);
};

}  // namespace atlas::ui
