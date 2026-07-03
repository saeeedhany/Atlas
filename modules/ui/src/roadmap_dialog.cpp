#include "atlas/ui/roadmap_dialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace atlas::ui {

RoadmapDialog::RoadmapDialog(const QString& targetTitle,
                               const std::vector<atlas::core::KnowledgeObject>& roadmap,
                               QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QString("Roadmap: %1").arg(targetTitle));

    auto* heading = new QLabel(
        QString("Suggested learning order to reach \"%1\":").arg(targetTitle), this);
    heading->setWordWrap(true);

    auto* list = new QListWidget(this);
    int position = 1;
    for (const auto& object : roadmap) {
        bool isTarget = (position == static_cast<int>(roadmap.size()));
        QString label = QString("%1. %2").arg(position).arg(QString::fromStdString(object.title()));
        if (isTarget) label += "  \u2190 target";
        list->addItem(label);
        ++position;
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(heading);
    layout->addWidget(list);
    layout->addWidget(buttons);

    resize(420, 480);
}

}  // namespace atlas::ui
