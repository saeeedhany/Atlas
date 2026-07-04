#include "atlas/ui/two_field_item_dialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace atlas::ui {

TwoFieldItemDialog::TwoFieldItemDialog(const QString& dialogTitle, const QString& field1Label,
                                         const QString& field1Initial, const QString& field2Label,
                                         const QString& field2Initial, bool field2IsRequired,
                                         QWidget* parent)
    : QDialog(parent), field2IsRequired_(field2IsRequired) {
    setWindowTitle(dialogTitle);

    field1Edit_ = new QLineEdit(field1Initial, this);
    field2Edit_ = new QLineEdit(field2Initial, this);

    auto* form = new QFormLayout;
    form->addRow(field1Label, field1Edit_);
    form->addRow(field2Label + (field2IsRequired ? "" : " (optional)"), field2Edit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    okButton_ = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(field1Edit_, &QLineEdit::textChanged, this, &TwoFieldItemDialog::updateOkEnabled);
    connect(field2Edit_, &QLineEdit::textChanged, this, &TwoFieldItemDialog::updateOkEnabled);
    updateOkEnabled();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void TwoFieldItemDialog::updateOkEnabled() {
    // field1 is always required; field2 only blocks OK when the
    // caller said it must be present (e.g. MiniProject::description).
    bool ok = !field1Edit_->text().trimmed().isEmpty();
    if (field2IsRequired_) ok = ok && !field2Edit_->text().trimmed().isEmpty();
    okButton_->setEnabled(ok);
}

QString TwoFieldItemDialog::field1() const { return field1Edit_->text().trimmed(); }
QString TwoFieldItemDialog::field2() const { return field2Edit_->text().trimmed(); }

}  // namespace atlas::ui
