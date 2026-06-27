#pragma once

#include <QDialog>

#include "atlas/core/enums.hpp"
#include "atlas/core/knowledge_object.hpp"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

namespace atlas::ui {

// Edit-only, not create-and-edit: creating a new KnowledgeObject only
// ever needs a title (see atlas-core's design notes — Title is the
// only field with a real invariant, everything else can start blank
// and be filled in progressively), so MainWindow uses a plain
// QInputDialog text prompt for that. This dialog exists for the
// "flesh out an existing concept" step, pre-filled from the object
// being edited. Two simpler, single-purpose flows instead of one
// dialog trying to serve both.
//
// Does not edit Examples, MiniProjects, or References — those are
// list-of-structs fields that need their own list-editing UI; adding
// that here would roughly double this dialog's size for a milestone
// that's supposed to be minimal. Deferred, not forgotten.
class KnowledgeObjectEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit KnowledgeObjectEditDialog(const atlas::core::KnowledgeObject& object,
                                        QWidget* parent = nullptr);

    QString title() const;
    QString definition() const;
    QString problemSolved() const;
    QString whyItExists() const;
    QString notes() const;
    atlas::core::Difficulty difficulty() const;
    atlas::core::ConfidenceLevel confidence() const;

private:
    QLineEdit* titleEdit_;
    QPlainTextEdit* definitionEdit_;
    QPlainTextEdit* problemSolvedEdit_;
    QPlainTextEdit* whyItExistsEdit_;
    QPlainTextEdit* notesEdit_;
    QComboBox* difficultyCombo_;
    QComboBox* confidenceCombo_;
};

}  // namespace atlas::ui
