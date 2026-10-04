#include "atlas/ui/project_suggestions_dialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace atlas::ui {

namespace {

QString formatSuggestion(const WorkspaceController::ProjectSuggestion& suggestion) {
    QString text = QString::fromStdString(suggestion.knowledgeObject.title());
    text += QString("  (%1% ready").arg(static_cast<int>(suggestion.readiness * 100));
    if (suggestion.leverage > 0) {
        text += QString(", unlocks %1 other concept%2")
                    .arg(suggestion.leverage)
                    .arg(suggestion.leverage == 1 ? "" : "s");
    }
    text += ")";

    const auto& projects = suggestion.knowledgeObject.miniProjects();
    for (const auto& project : projects) {
        text += QString("\n    \u2022 %1").arg(QString::fromStdString(project.title));
    }
    return text;
}

}  // namespace

ProjectSuggestionsDialog::ProjectSuggestionsDialog(
    const QString& topicName,
    const std::vector<WorkspaceController::ProjectSuggestion>& suggestions, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QString("Project Suggestions: %1").arg(topicName));

    auto* heading = new QLabel(
        suggestions.empty()
            ? QString("No project suggestions in \"%1\" yet - add a mini-project to a "
                      "concept, or mark fewer concepts as Mastered.")
                  .arg(topicName)
            : QString("Suggested next projects in \"%1\", ranked by readiness and how much "
                      "they unlock:")
                  .arg(topicName),
        this);
    heading->setWordWrap(true);

    auto* list = new QListWidget(this);
    for (const auto& suggestion : suggestions) {
        list->addItem(formatSuggestion(suggestion));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(heading);
    layout->addWidget(list);
    layout->addWidget(buttons);

    resize(460, 480);
}

}  // namespace atlas::ui
