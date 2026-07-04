#include "atlas/ui/knowledge_object_edit_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "atlas/ui/two_field_item_dialog.hpp"

namespace atlas::ui {

using atlas::core::ConfidenceLevel;
using atlas::core::Difficulty;
using atlas::core::Example;
using atlas::core::MiniProject;
using atlas::core::Reference;
using atlas::core::toDisplayString;

namespace {

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

// Builds the (list widget, add/edit/remove buttons) skeleton common to
// all three sections; the caller wires up what "add"/"edit" actually
// mean for its specific item type.
QListWidget* buildListSkeleton(QWidget* parent, QPushButton*& addButton,
                                 QPushButton*& editButton, QPushButton*& removeButton) {
    auto* list = new QListWidget(parent);
    addButton = new QPushButton("Add...", parent);
    editButton = new QPushButton("Edit...", parent);
    removeButton = new QPushButton("Remove", parent);
    editButton->setEnabled(false);
    removeButton->setEnabled(false);

    QObject::connect(list, &QListWidget::itemSelectionChanged, parent, [=]() {
        bool hasSelection = !list->selectedItems().isEmpty();
        editButton->setEnabled(hasSelection);
        removeButton->setEnabled(hasSelection);
    });

    return list;
}

}  // namespace

KnowledgeObjectEditDialog::KnowledgeObjectEditDialog(const atlas::core::KnowledgeObject& object,
                                                       QWidget* parent)
    : QDialog(parent),
      examples_(object.examples()),
      miniProjects_(object.miniProjects()),
      references_(object.references()) {
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
    auto* detailsTab = new QWidget(this);
    detailsTab->setLayout(form);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(detailsTab, "Details");
    tabs->addTab(buildExamplesSection(), "Examples");
    tabs->addTab(buildMiniProjectsSection(), "Mini Projects");
    tabs->addTab(buildReferencesSection(), "References");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    resize(520, 560);
}

QWidget* KnowledgeObjectEditDialog::buildExamplesSection() {
    auto* container = new QWidget(this);
    QPushButton* addButton;
    QPushButton* editButton;
    QPushButton* removeButton;
    examplesList_ = buildListSkeleton(container, addButton, editButton, removeButton);
    refreshExamplesList();

    connect(addButton, &QPushButton::clicked, this, [this]() {
        TwoFieldItemDialog itemDialog("Add Example", "Description", "", "Snippet (code)", "",
                                       /*field2IsRequired=*/false, this);
        if (itemDialog.exec() != QDialog::Accepted) return;
        std::optional<std::string> snippet;
        if (!itemDialog.field2().isEmpty()) snippet = itemDialog.field2().toStdString();
        examples_.push_back(Example{itemDialog.field1().toStdString(), snippet});
        refreshExamplesList();
    });
    connect(editButton, &QPushButton::clicked, this, [this]() {
        int row = examplesList_->currentRow();
        if (row < 0 || static_cast<size_t>(row) >= examples_.size()) return;
        const auto& current = examples_[static_cast<size_t>(row)];
        TwoFieldItemDialog itemDialog(
            "Edit Example", "Description", QString::fromStdString(current.description),
            "Snippet (code)", current.snippet ? QString::fromStdString(*current.snippet) : "",
            /*field2IsRequired=*/false, this);
        if (itemDialog.exec() != QDialog::Accepted) return;
        std::optional<std::string> snippet;
        if (!itemDialog.field2().isEmpty()) snippet = itemDialog.field2().toStdString();
        examples_[static_cast<size_t>(row)] = Example{itemDialog.field1().toStdString(), snippet};
        refreshExamplesList();
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        int row = examplesList_->currentRow();
        if (row < 0 || static_cast<size_t>(row) >= examples_.size()) return;
        examples_.erase(examples_.begin() + row);
        refreshExamplesList();
    });

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();

    auto* layout = new QVBoxLayout(container);
    layout->addWidget(examplesList_);
    layout->addLayout(buttonRow);
    return container;
}

QWidget* KnowledgeObjectEditDialog::buildMiniProjectsSection() {
    auto* container = new QWidget(this);
    QPushButton* addButton;
    QPushButton* editButton;
    QPushButton* removeButton;
    miniProjectsList_ = buildListSkeleton(container, addButton, editButton, removeButton);
    refreshMiniProjectsList();

    connect(addButton, &QPushButton::clicked, this, [this]() {
        TwoFieldItemDialog itemDialog("Add Mini Project", "Title", "", "Description", "",
                                       /*field2IsRequired=*/true, this);
        if (itemDialog.exec() != QDialog::Accepted) return;
        miniProjects_.push_back(
            MiniProject{itemDialog.field1().toStdString(), itemDialog.field2().toStdString()});
        refreshMiniProjectsList();
    });
    connect(editButton, &QPushButton::clicked, this, [this]() {
        int row = miniProjectsList_->currentRow();
        if (row < 0 || static_cast<size_t>(row) >= miniProjects_.size()) return;
        const auto& current = miniProjects_[static_cast<size_t>(row)];
        TwoFieldItemDialog itemDialog(
            "Edit Mini Project", "Title", QString::fromStdString(current.title), "Description",
            QString::fromStdString(current.description), /*field2IsRequired=*/true, this);
        if (itemDialog.exec() != QDialog::Accepted) return;
        miniProjects_[static_cast<size_t>(row)] =
            MiniProject{itemDialog.field1().toStdString(), itemDialog.field2().toStdString()};
        refreshMiniProjectsList();
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        int row = miniProjectsList_->currentRow();
        if (row < 0 || static_cast<size_t>(row) >= miniProjects_.size()) return;
        miniProjects_.erase(miniProjects_.begin() + row);
        refreshMiniProjectsList();
    });

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();

    auto* layout = new QVBoxLayout(container);
    layout->addWidget(miniProjectsList_);
    layout->addLayout(buttonRow);
    return container;
}

QWidget* KnowledgeObjectEditDialog::buildReferencesSection() {
    auto* container = new QWidget(this);
    QPushButton* addButton;
    QPushButton* editButton;
    QPushButton* removeButton;
    referencesList_ = buildListSkeleton(container, addButton, editButton, removeButton);
    refreshReferencesList();

    connect(addButton, &QPushButton::clicked, this, [this]() {
        TwoFieldItemDialog itemDialog("Add Reference", "Title", "", "URL", "",
                                       /*field2IsRequired=*/false, this);
        if (itemDialog.exec() != QDialog::Accepted) return;
        std::optional<std::string> url;
        if (!itemDialog.field2().isEmpty()) url = itemDialog.field2().toStdString();
        references_.push_back(Reference{itemDialog.field1().toStdString(), url});
        refreshReferencesList();
    });
    connect(editButton, &QPushButton::clicked, this, [this]() {
        int row = referencesList_->currentRow();
        if (row < 0 || static_cast<size_t>(row) >= references_.size()) return;
        const auto& current = references_[static_cast<size_t>(row)];
        TwoFieldItemDialog itemDialog(
            "Edit Reference", "Title", QString::fromStdString(current.title), "URL",
            current.url ? QString::fromStdString(*current.url) : "",
            /*field2IsRequired=*/false, this);
        if (itemDialog.exec() != QDialog::Accepted) return;
        std::optional<std::string> url;
        if (!itemDialog.field2().isEmpty()) url = itemDialog.field2().toStdString();
        references_[static_cast<size_t>(row)] = Reference{itemDialog.field1().toStdString(), url};
        refreshReferencesList();
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        int row = referencesList_->currentRow();
        if (row < 0 || static_cast<size_t>(row) >= references_.size()) return;
        references_.erase(references_.begin() + row);
        refreshReferencesList();
    });

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();

    auto* layout = new QVBoxLayout(container);
    layout->addWidget(referencesList_);
    layout->addLayout(buttonRow);
    return container;
}

void KnowledgeObjectEditDialog::refreshExamplesList() {
    examplesList_->clear();
    for (const auto& example : examples_) {
        QString text = QString::fromStdString(example.description);
        if (example.snippet) text += "  [has snippet]";
        examplesList_->addItem(text);
    }
}

void KnowledgeObjectEditDialog::refreshMiniProjectsList() {
    miniProjectsList_->clear();
    for (const auto& project : miniProjects_) {
        miniProjectsList_->addItem(QString::fromStdString(project.title));
    }
}

void KnowledgeObjectEditDialog::refreshReferencesList() {
    referencesList_->clear();
    for (const auto& reference : references_) {
        QString text = QString::fromStdString(reference.title);
        if (reference.url) text += QString("  (%1)").arg(QString::fromStdString(*reference.url));
        referencesList_->addItem(text);
    }
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
