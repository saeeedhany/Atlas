#pragma once

#include <QMainWindow>

#include <optional>

#include "atlas/core/topic.hpp"
#include "atlas/render/theme.hpp"
#include "atlas/ui/workspace_controller.hpp"

class QStackedWidget;
class QAction;

namespace atlas::ui {

class KnowledgeObjectPanel;
class GraphWindow;
class TopicSelectorWidget;

// The application shell: a QStackedWidget with two pages —
// TopicSelectorWidget (the launch screen) and the workspace view (the
// panel+canvas splitter this class already owned before topics
// existed). One MainWindow, one long-lived panel_/canvas_ pair, reused
// and re-scoped per topic via KnowledgeObjectPanel::setTopic /
// GraphWindow::setTopic — not reconstructed per topic switch — so
// switching topics can crossfade a stable pair of widgets instead of
// tearing down and rebuilding the QML canvas every time.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(WorkspaceController& controller, QWidget* parent = nullptr);

private slots:
    // Applies the theme to the canvas (via GraphWindow::setTheme) and
    // to the Qt Widgets shell (a stylesheet built from
    // atlas::render::Theme's panel* colors, so the list/buttons/splitter
    // read as one cohesive surface with the canvas rather than the
    // canvas going dark while the rest of the window stays whatever the
    // OS default palette is), then persists the choice via QSettings so
    // it survives a restart.
    void applyTheme(atlas::render::ThemeMode mode);

    void onTopicChosen(atlas::core::TopicId id);
    void onBackToTopicsClicked();

private:
    // Crossfades stack_ from whatever page is current to `page`: fades
    // the current page out, swaps the stack's current widget, fades
    // the new page in. ~180ms each way — enough to read as a
    // transition, not enough to feel laggy tapping between a handful
    // of topics. No-op if `page` is already current.
    void switchToPage(QWidget* page);

    WorkspaceController* controller_;

    QStackedWidget* stack_;
    TopicSelectorWidget* topicSelector_;
    QWidget* workspacePage_;  // hosts the splitter below
    KnowledgeObjectPanel* panel_;
    GraphWindow* canvas_;

    QAction* backAction_;
    std::optional<atlas::core::TopicId> activeTopic_;
    atlas::render::ThemeMode themeMode_ = atlas::render::ThemeMode::Dark;
};

}  // namespace atlas::ui
