#pragma once

#include <QWidget>

#include "atlas/ui/workspace_controller.hpp"

class QQuickWidget;

namespace atlas::render {
class GraphCanvasItem;
}  // namespace atlas::render

namespace atlas::ui {

// Hosts the Qt Quick graph canvas inside the Qt Widgets shell via
// QQuickWidget. Kept as its own top-level window rather than merged
// into MainWindow's layout — deliberately: this is M4's spike, and
// not disturbing M3's already-verified list view while validating the
// canvas separately is a smaller, safer change than integrating both
// into one window layout on the first attempt. Unifying them into one
// cohesive window is future UI-polish work, not this milestone's job.
class GraphWindow : public QWidget {
    Q_OBJECT

public:
    explicit GraphWindow(WorkspaceController& controller, QWidget* parent = nullptr);

    // The canvas item lives in the QML scene graph rooted at the
    // QQuickWidget's rootObject(), not in this QWidget's own child
    // hierarchy — QWidget::findChild() from GraphWindow itself will
    // never find it. This is the correct way to reach it.
    atlas::render::GraphCanvasItem* canvasItem() const;

private slots:
    void refreshGraph();

private:
    WorkspaceController* controller_;
    QQuickWidget* quickWidget_;
};

}  // namespace atlas::ui
