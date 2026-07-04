#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

namespace atlas::ui {

// Generic add/edit dialog for a two-field item, shared by the
// Examples/MiniProjects/References sections of
// KnowledgeObjectEditDialog. All three of atlas-core's content types
// (Example, MiniProject, Reference) are shaped as (required string,
// second string that's either required or optional) — close enough to
// share one small dialog rather than writing three nearly-identical
// ones. field1 is always required (non-empty enforced by the OK
// button's enabled state); field2's requiredness is the caller's
// business, not this dialog's — see field2IsRequired.
class TwoFieldItemDialog : public QDialog {
    Q_OBJECT

public:
    // field2Initial/field2IsRequired: when field2 is optional (e.g.
    // Example::snippet, Reference::url), an empty field2() on return
    // means "absent," not "empty string" — the caller distinguishes
    // those the same way Example/Reference's std::optional<std::string>
    // does at the domain layer.
    TwoFieldItemDialog(const QString& dialogTitle, const QString& field1Label,
                        const QString& field1Initial, const QString& field2Label,
                        const QString& field2Initial, bool field2IsRequired,
                        QWidget* parent = nullptr);

    QString field1() const;
    QString field2() const;

private:
    void updateOkEnabled();

    QLineEdit* field1Edit_;
    QLineEdit* field2Edit_;
    bool field2IsRequired_;
    QPushButton* okButton_;
};

}  // namespace atlas::ui
