#pragma once

#include <QDialog>

#include <vector>

#include "atlas/core/knowledge_object.hpp"

namespace atlas::ui {

// Read-only display of a learning roadmap: an ordered list of
// KnowledgeObjects where every entry appears only after all of its own
// dependencies have. Plain numbered list - no editing, no drag-to-
// reorder; the order is computed (GraphEngine::learningRoadmapFor),
// not something the user is meant to author by hand here.
class RoadmapDialog : public QDialog {
    Q_OBJECT

public:
    RoadmapDialog(const QString& targetTitle,
                   const std::vector<atlas::core::KnowledgeObject>& roadmap,
                   QWidget* parent = nullptr);
};

}  // namespace atlas::ui
