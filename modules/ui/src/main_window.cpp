#include "atlas/ui/main_window.hpp"

#include <QActionGroup>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QMenuBar>
#include <QPropertyAnimation>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include "atlas/ui/graph_window.hpp"
#include "atlas/ui/knowledge_object_panel.hpp"
#include "atlas/ui/topic_selector_widget.hpp"

namespace atlas::ui {

namespace {

// "Atlas"/"Atlas" matches what a QSettings default-constructed from
// QApplication's organizationName/applicationName would resolve to if
// those were set — spelled out explicitly here since atlas_app's
// main() doesn't currently set them, and QSettings needs *some*
// scope. Revisit if main() ever sets QCoreApplication::setOrganizationName.
constexpr const char* kSettingsOrg = "Atlas";
constexpr const char* kSettingsApp = "Atlas";
constexpr const char* kThemeKey    = "theme";

atlas::render::ThemeMode loadSavedTheme() {
    QSettings settings(kSettingsOrg, kSettingsApp);
    auto value = settings.value(kThemeKey, "dark").toString();
    return value == "light" ? atlas::render::ThemeMode::Light : atlas::render::ThemeMode::Dark;
}

}  // namespace

MainWindow::MainWindow(WorkspaceController& controller, QWidget* parent)
    : QMainWindow(parent), controller_(&controller) {
    setWindowTitle("Atlas");

    topicSelector_ = new TopicSelectorWidget(controller, this);
    connect(topicSelector_, &TopicSelectorWidget::topicChosen, this, &MainWindow::onTopicChosen);

    panel_ = new KnowledgeObjectPanel(controller, atlas::core::uncategorizedTopicId(), this);
    canvas_ = new GraphWindow(controller, this, /*standalone=*/false);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(panel_);
    splitter->addWidget(canvas_);
    splitter->setStretchFactor(0, 1);   // panel gets 1 unit
    splitter->setStretchFactor(1, 2);   // canvas gets 2 units (stays wider by default)
    splitter->setSizes({340, 680});

    workspacePage_ = new QWidget(this);
    auto* workspaceLayout = new QVBoxLayout(workspacePage_);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->addWidget(splitter);

    // One-directional: list selection -> canvas highlight.
    // Never the reverse — canvas clicks update the canvas highlight
    // state but never jump-scroll the list or change list selection.
    connect(panel_, &KnowledgeObjectPanel::selectionChanged,
            canvas_, &GraphWindow::setSelectedKnowledgeObject);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(topicSelector_);
    stack_->addWidget(workspacePage_);
    setCentralWidget(stack_);
    resize(1020, 680);

    auto* navBar = addToolBar("Navigation");
    navBar->setMovable(false);
    backAction_ = navBar->addAction("\u25c0 Topics");  // "◀ Topics"
    backAction_->setVisible(false);
    connect(backAction_, &QAction::triggered, this, &MainWindow::onBackToTopicsClicked);

    auto* viewMenu = menuBar()->addMenu("&View");
    auto* themeMenu = viewMenu->addMenu("Theme");
    auto* darkAction = themeMenu->addAction("Dark");
    auto* lightAction = themeMenu->addAction("Light");
    darkAction->setCheckable(true);
    lightAction->setCheckable(true);
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    themeGroup->addAction(darkAction);
    themeGroup->addAction(lightAction);

    connect(darkAction, &QAction::triggered, this,
            [this] { applyTheme(atlas::render::ThemeMode::Dark); });
    connect(lightAction, &QAction::triggered, this,
            [this] { applyTheme(atlas::render::ThemeMode::Light); });

    auto savedTheme = loadSavedTheme();
    (savedTheme == atlas::render::ThemeMode::Dark ? darkAction : lightAction)->setChecked(true);
    applyTheme(savedTheme);
}

void MainWindow::onTopicChosen(atlas::core::TopicId id) {
    activeTopic_ = id;
    panel_->setTopic(id);
    canvas_->setTopic(id);

    auto topic = controller_->findTopic(id);
    setWindowTitle(topic.has_value() ? QString("Atlas — %1").arg(QString::fromStdString(topic->name()))
                                       : "Atlas");

    switchToPage(workspacePage_);
}

void MainWindow::onBackToTopicsClicked() {
    activeTopic_ = std::nullopt;
    setWindowTitle("Atlas");
    switchToPage(topicSelector_);
}

void MainWindow::switchToPage(QWidget* page) {
    if (stack_->currentWidget() == page) return;

    // The page swap itself is synchronous — deliberately never gated
    // behind animation completion, so callers (including tests) can
    // rely on stack_->currentWidget() already being `page` the moment
    // this returns, with no event-loop pumping required. The fade is a
    // best-effort visual layer on top of that, not a precondition for
    // it: a true two-sided crossfade (fading the OLD page out before
    // swapping) would mean the swap only happens once that animation's
    // finished signal fires, which needs a running event loop to ever
    // deliver — exactly the kind of timing dependency
    // docs/DECISIONS.md already steers UI tests away from elsewhere in
    // this codebase (see the empty-state/currentWidget() note).
    stack_->setCurrentWidget(page);

    auto* effect = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(effect);
    auto* fadeIn = new QPropertyAnimation(effect, "opacity", page);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    // Parented to `page`, not `this` — if the page is destroyed mid-
    // animation (shouldn't happen for these two long-lived pages, but
    // cheap insurance), the animation and effect go with it rather
    // than outliving what they were animating.
    connect(fadeIn, &QPropertyAnimation::finished, page,
            [page]() { page->setGraphicsEffect(nullptr); });
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    backAction_->setVisible(page == workspacePage_);
}

void MainWindow::applyTheme(atlas::render::ThemeMode mode) {
    themeMode_ = mode;
    const auto& theme = atlas::render::themeFor(mode);

    auto hex = [](const QColor& color) { return color.name(QColor::HexRgb); };

    // A hand-written stylesheet, not a QPalette swap: QPalette gets
    // overridden inconsistently by individual widgets' own styling
    // (e.g. KnowledgeObjectPanel's empty-state label already sets
    // "color: gray;" directly). Deliberately NOT a blanket `QWidget`
    // selector: Qt stylesheets cascade to every descendant, and the
    // edit/relationship/roadmap dialogs are all parented under this
    // window — a blanket rule would silently theme their backgrounds
    // dark while their own QLabels keep default-palette (often black)
    // text, an unreadable combination no one asked for. Scoped to the
    // specific widget classes this window actually contains; those
    // dialogs intentionally keep the OS default style until they get
    // their own theming pass.
    //
    // %1 panelBackground, %2 panelText, %3 panelAlternateBackground,
    // %4 panelBorder, %5 accent — used both for interactive-hover
    // feedback (buttons, splitter handle) and for the "this row is
    // selected" list-item background, so hover and selection read as
    // the same family of feedback rather than two competing colors.
    setStyleSheet(QString(R"(
        QMainWindow, QSplitter, QToolBar,
        atlas--ui--KnowledgeObjectPanel, atlas--ui--GraphWindow,
        atlas--ui--TopicSelectorWidget { background-color: %1; }

        QLabel#panelHeading { color: %5; padding: 2px 0px 6px 0px; }

        QListView, QListWidget {
            background-color: %1; alternate-background-color: %3;
            color: %2; border: 1px solid %4; border-radius: 6px;
            outline: 0; padding: 2px;
        }
        QListView::item, QListWidget::item {
            padding: 6px 8px; border-radius: 4px; margin: 1px 0px;
        }
        QListView::item:selected, QListWidget::item:selected {
            background-color: %5; color: %1;
        }
        QListView::item:hover:!selected, QListWidget::item:hover:!selected {
            background-color: %4;
        }

        QLineEdit {
            background-color: %3; color: %2; border: 1px solid %4;
            padding: 6px 8px; border-radius: 6px;
        }
        QLineEdit:focus { border-color: %5; }

        QPushButton {
            background-color: %3; color: %2; border: 1px solid %4;
            padding: 6px 14px; border-radius: 6px;
        }
        QPushButton:hover:!disabled { border-color: %5; }
        QPushButton:pressed:!disabled { background-color: %4; }
        QPushButton:disabled { color: %4; }

        QSplitter::handle { background-color: %4; margin: 0px 3px; }
        QSplitter::handle:hover { background-color: %5; }

        QMenuBar, QMenu, QToolBar { background-color: %3; color: %2; }
        QMenu { border: 1px solid %4; border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 6px 20px; border-radius: 4px; }
        QMenu::item:selected { background-color: %5; color: %1; }
        QMenu::item:disabled { color: %4; }
        QToolBar { border: none; padding: 4px; spacing: 4px; }

        QToolTip {
            background-color: %3; color: %2; border: 1px solid %5;
            padding: 6px 8px; border-radius: 4px;
        }

        atlas--ui--TopicSelectorWidget QLabel,
        atlas--ui--KnowledgeObjectPanel QLabel,
        atlas--ui--GraphWindow QLabel { color: %2; }
    )")
                         .arg(hex(theme.panelBackground), hex(theme.panelText),
                              hex(theme.panelAlternateBackground), hex(theme.panelBorder),
                              hex(theme.accent)));

    canvas_->setTheme(mode);

    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue(kThemeKey, mode == atlas::render::ThemeMode::Dark ? "dark" : "light");
}

}  // namespace atlas::ui
