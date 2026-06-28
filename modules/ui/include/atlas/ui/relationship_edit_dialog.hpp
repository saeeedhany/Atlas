#pragma once

#include <QDialog>

#include <optional>
#include <string>
#include <vector>

#include "atlas/core/enums.hpp"
#include "atlas/core/knowledge_object.hpp"

class QComboBox;
class QLineEdit;

namespace atlas::ui {

// Creates a relationship FROM a fixed source TO a user-chosen target.
// Source is fixed (shown as a read-only label) rather than also
// editable here — it's whatever row was selected in MainWindow's list,
// the same "use the existing, proven selection mechanism" choice
// described in this milestone's design notes, rather than building
// canvas-click node picking. `candidateTargets` is expected to already
// exclude the source itself, so this dialog has no way to construct a
// self-loop by construction, not just by validation.
class RelationshipEditDialog : public QDialog {
    Q_OBJECT

public:
    RelationshipEditDialog(const atlas::core::KnowledgeObject& source,
                             const std::vector<atlas::core::KnowledgeObject>& candidateTargets,
                             QWidget* parent = nullptr);

    atlas::core::KnowledgeObjectId targetId() const;
    atlas::core::RelationshipType type() const;
    std::optional<std::string> note() const;

private:
    QComboBox* targetCombo_;
    QComboBox* typeCombo_;
    QLineEdit* noteEdit_;
};

}  // namespace atlas::ui
