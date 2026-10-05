#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>

#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/ui/fonts.hpp"

Q_IMPORT_QML_PLUGIN(Atlas_ViewModelsPlugin)
Q_IMPORT_QML_PLUGIN(Atlas_UiPlugin)

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    atlas::render::registerGraphCanvasQmlType();
    atlas::ui::loadBundledFonts();
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
