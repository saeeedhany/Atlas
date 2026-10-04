#pragma once

#include <QDialog>

#include <vector>

#include "atlas/core/content_types.hpp"
#include "atlas/core/enums.hpp"
#include "atlas/core/knowledge_object.hpp"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QPushButton;

namespace atlas::ui {

// Edit-only, not create-and-edit: creating a new KnowledgeObject only
// ever needs a title (see atlas-core's design notes - Title is the
// only field with a real invariant, everything else can start blank
// and be filled in progressively), so MainWindow uses a plain
// QInputDialog text prompt for that. This dialog exists for the
// "flesh out an existing concept" step, pre-filled from the object
// being edited. Two simpler, single-purpose flows instead of one
// dialog trying to serve both.
//
// Examples/MiniProjects/References are each a QListWidget + Add/Edit/
// Remove buttons, working on a local std::vector<T> copy that's only
// committed back via examples()/miniProjects()/references() if the
// dialog is accepted - cancelling the dialog discards any in-progress
// list edits, same as every other field here. Add/Edit both go through
// the shared TwoFieldItemDialog rather than three near-identical
// custom dialogs - see that class's own header comment.
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

    const std::vector<atlas::core::Example>& examples() const { return examples_; }
    const std::vector<atlas::core::MiniProject>& miniProjects() const { return miniProjects_; }
    const std::vector<atlas::core::Reference>& references() const { return references_; }

private:
    // One QGroupBox-building helper per list type rather than a
    // generic "list section" abstraction - see this class's header
    // comment, and KnowledgeObjectRepository's own precedent for
    // three explicit blocks over one generic one for these same three
    // types.
    QWidget* buildExamplesSection();
    QWidget* buildMiniProjectsSection();
    QWidget* buildReferencesSection();
    void refreshExamplesList();
    void refreshMiniProjectsList();
    void refreshReferencesList();

    QLineEdit* titleEdit_;
    QPlainTextEdit* definitionEdit_;
    QPlainTextEdit* problemSolvedEdit_;
    QPlainTextEdit* whyItExistsEdit_;
    QPlainTextEdit* notesEdit_;
    QComboBox* difficultyCombo_;
    QComboBox* confidenceCombo_;

    std::vector<atlas::core::Example> examples_;
    std::vector<atlas::core::MiniProject> miniProjects_;
    std::vector<atlas::core::Reference> references_;
    QListWidget* examplesList_;
    QListWidget* miniProjectsList_;
    QListWidget* referencesList_;
};

}  // namespace atlas::ui
