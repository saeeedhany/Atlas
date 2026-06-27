#include "atlas/ui/knowledge_object_edit_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace atlas::ui {

using atlas::core::ConfidenceLevel;
using atlas::core::Difficulty;
using atlas::core::toDisplayString;

namespace {

// Populates a combo box with every enumerator of a small enum, using
// toDisplayString() as the label and the enum's int value as the
// item's user data — so the selection can be read back without a
// separate string-to-enum parse step in this file.
template <typename EnumT>
void populateEnumCombo(QComboBox* combo, std::initializer_list<EnumT> values) {
    for (EnumT value : values) {
        combo->addItem(QString::fromStdString(std::string(toDisplayString(value))),
                        static_cast<int>(value));
    }
}

template <typename EnumT>
EnumT selectedEnumValue(const QComboBox* combo) {
    return static_cast<EnumT>(combo->currentData().toInt());
}

template <typename EnumT>
void selectEnumValue(QComboBox* combo, EnumT value) {
    int index = combo->findData(static_cast<int>(value));
    if (index >= 0) combo->setCurrentIndex(index);
}

}  // namespace

KnowledgeObjectEditDialog::KnowledgeObjectEditDialog(const atlas::core::KnowledgeObject& object,
                                                       QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QString("Edit: %1").arg(QString::fromStdString(object.title())));

    titleEdit_ = new QLineEdit(QString::fromStdString(object.title()), this);
    definitionEdit_ = new QPlainTextEdit(QString::fromStdString(object.definition()), this);
    problemSolvedEdit_ = new QPlainTextEdit(QString::fromStdString(object.problemSolved()), this);
    whyItExistsEdit_ = new QPlainTextEdit(QString::fromStdString(object.whyItExists()), this);
    notesEdit_ = new QPlainTextEdit(QString::fromStdString(object.notes()), this);

    difficultyCombo_ = new QComboBox(this);
    populateEnumCombo(difficultyCombo_, {Difficulty::Beginner, Difficulty::Intermediate,
                                          Difficulty::Advanced, Difficulty::Expert});
    selectEnumValue(difficultyCombo_, object.difficulty());

    confidenceCombo_ = new QComboBox(this);
    populateEnumCombo(confidenceCombo_,
                       {ConfidenceLevel::Unknown, ConfidenceLevel::Learning,
                        ConfidenceLevel::Familiar, ConfidenceLevel::Confident,
                        ConfidenceLevel::Mastered});
    selectEnumValue(confidenceCombo_, object.confidence());

    auto* form = new QFormLayout;
    form->addRow("Title", titleEdit_);
    form->addRow("Definition", definitionEdit_);
    form->addRow("Problem it solves", problemSolvedEdit_);
    form->addRow("Why it exists", whyItExistsEdit_);
    form->addRow("Notes", notesEdit_);
    form->addRow("Difficulty", difficultyCombo_);
    form->addRow("Confidence", confidenceCombo_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(480, 480);
}

QString KnowledgeObjectEditDialog::title() const { return titleEdit_->text(); }
QString KnowledgeObjectEditDialog::definition() const { return definitionEdit_->toPlainText(); }
QString KnowledgeObjectEditDialog::problemSolved() const {
    return problemSolvedEdit_->toPlainText();
}
QString KnowledgeObjectEditDialog::whyItExists() const { return whyItExistsEdit_->toPlainText(); }
QString KnowledgeObjectEditDialog::notes() const { return notesEdit_->toPlainText(); }

Difficulty KnowledgeObjectEditDialog::difficulty() const {
    return selectedEnumValue<Difficulty>(difficultyCombo_);
}
ConfidenceLevel KnowledgeObjectEditDialog::confidence() const {
    return selectedEnumValue<ConfidenceLevel>(confidenceCombo_);
}

}  // namespace atlas::ui
