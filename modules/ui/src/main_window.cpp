#include "atlas/ui/main_window.hpp"

#include <QActionGroup>
#include <QApplication>
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
    // (e.g. KnowledgeObjectPanel's empty-state label sets "color:
    // gray;" directly, which still wins due to higher specificity).
    //
    // Applied via qApp->setStyleSheet(), not this->setStyleSheet():
    // Qt style sheets set on one widget only cascade to descendants
    // that are part of the *same* top-level window. RelationshipsWindow
    // (Qt::Window) and every QDialog (KnowledgeObjectEditDialog,
    // RelationshipEditDialog, RoadmapDialog, ProjectSuggestionsDialog,
    // TwoFieldItemDialog — QDialog is inherently its own top-level
    // window regardless of parent) are each a separate top-level
    // window from MainWindow's perspective, so a stylesheet set on
    // `this` never reached them — the actual cause of "the theme
    // doesn't affect all windows." An application-level stylesheet
    // applies process-wide regardless of top-level-window boundaries,
    // which is the only mechanism that actually fixes this without
    // hand-applying the same stylesheet string in every dialog's own
    // constructor (repeating it there would drift out of sync the
    // first time only one of the copies gets edited).
    //
    // Now that every window is in scope, QDialog and every input
    // widget dialogs actually use (QComboBox, QPlainTextEdit,
    // QTabWidget) are styled too — previously absent entirely, so
    // those widgets rendered in the OS default style even inside an
    // otherwise-dark-themed dialog.
    //
    // %1 panelBackground, %2 panelText, %3 panelAlternateBackground,
    // %4 panelBorder, %5 accent.
    qApp->setStyleSheet(QString(R"(
        QMainWindow, QDialog, QSplitter, QToolBar, QWidget {
            background-color: %1; color: %2;
        }
        QLabel { color: %2; }

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

        QLineEdit, QPlainTextEdit {
            background-color: %3; color: %2; border: 1px solid %4;
            padding: 6px 8px; border-radius: 6px;
        }
        QLineEdit:focus, QPlainTextEdit:focus { border-color: %5; }

        QComboBox {
            background-color: %3; color: %2; border: 1px solid %4;
            padding: 6px 8px; border-radius: 6px;
        }
        QComboBox:focus { border-color: %5; }
        QComboBox QAbstractItemView {
            background-color: %3; color: %2; border: 1px solid %4;
            selection-background-color: %5; selection-color: %1;
            outline: 0;
        }

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

        QTabWidget::pane { border: 1px solid %4; border-radius: 6px; top: -1px; }
        QTabBar::tab {
            background-color: %1; color: %2; border: 1px solid %4;
            padding: 6px 14px; border-top-left-radius: 6px; border-top-right-radius: 6px;
        }
        QTabBar::tab:selected { background-color: %3; border-color: %5; }
        QTabBar::tab:!selected:hover { background-color: %4; }

        QDialogButtonBox QPushButton { min-width: 70px; }

        QToolTip {
            background-color: %3; color: %2; border: 1px solid %5;
            padding: 6px 8px; border-radius: 4px;
        }
    )")
                             .arg(hex(theme.panelBackground), hex(theme.panelText),
                                  hex(theme.panelAlternateBackground), hex(theme.panelBorder),
                                  hex(theme.accent)));

    canvas_->setTheme(mode);

    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue(kThemeKey, mode == atlas::render::ThemeMode::Dark ? "dark" : "light");
}

}  // namespace atlas::ui
