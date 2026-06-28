#include "atlas/ui/relationship_edit_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace atlas::ui {

using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipType;
using atlas::core::toDisplayString;
using atlas::core::Uuid;

namespace {

constexpr RelationshipType kAllTypes[] = {
    RelationshipType::DependsOn,    RelationshipType::Uses,         RelationshipType::Implements,
    RelationshipType::Solves,       RelationshipType::Contains,     RelationshipType::PartOf,
    RelationshipType::RelatedTo,    RelationshipType::AlternativeTo, RelationshipType::OppositeOf,
    RelationshipType::Causes,
};

}  // namespace

RelationshipEditDialog::RelationshipEditDialog(
    const atlas::core::KnowledgeObject& source,
    const std::vector<atlas::core::KnowledgeObject>& candidateTargets, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("New Relationship");

    auto* fromLabel =
        new QLabel(QString("From: %1").arg(QString::fromStdString(source.title())), this);

    targetCombo_ = new QComboBox(this);
    for (const auto& candidate : candidateTargets) {
        // The target's id, round-tripped as a string: simpler than
        // registering KnowledgeObjectId as a QVariant-compatible
        // metatype for a value only ever read back by this same file.
        targetCombo_->addItem(QString::fromStdString(candidate.title()),
                                QString::fromStdString(candidate.id().toString()));
    }

    typeCombo_ = new QComboBox(this);
    for (RelationshipType type : kAllTypes) {
        typeCombo_->addItem(QString::fromStdString(std::string(toDisplayString(type))),
                              static_cast<int>(type));
    }

    noteEdit_ = new QLineEdit(this);

    auto* form = new QFormLayout;
    form->addRow(fromLabel);
    form->addRow("To", targetCombo_);
    form->addRow("Type", typeCombo_);
    form->addRow("Note (optional)", noteEdit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

KnowledgeObjectId RelationshipEditDialog::targetId() const {
    auto uuidText = targetCombo_->currentData().toString().toStdString();
    auto parsed = Uuid::parse(uuidText);
    // candidateTargets always come from valid, already-persisted
    // KnowledgeObjects, so this should always parse; there's no
    // reasonable fallback if it somehow didn't, so this isn't guarded
    // any more defensively than that assumption already implies.
    return KnowledgeObjectId(*parsed);
}

RelationshipType RelationshipEditDialog::type() const {
    return static_cast<RelationshipType>(typeCombo_->currentData().toInt());
}

std::optional<std::string> RelationshipEditDialog::note() const {
    QString text = noteEdit_->text().trimmed();
    if (text.isEmpty()) return std::nullopt;
    return text.toStdString();
}

}  // namespace atlas::ui
