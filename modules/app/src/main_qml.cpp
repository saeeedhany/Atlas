#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>

#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/viewmodels/app_startup.hpp"

Q_IMPORT_QML_PLUGIN(Atlas_ViewModelsPlugin)
Q_IMPORT_QML_PLUGIN(Atlas_UiPlugin)

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("atlas_app");
    QGuiApplication::setApplicationDisplayName("Atlas");
    atlas::render::registerGraphCanvasQmlType();

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QSettings store(dataDir + "/settings.ini", QSettings::IniFormat);
    atlas::viewmodels::AppStartup startup(dataDir + "/atlas.db", store, atlas::viewmodels::systemClock());

    QQmlApplicationEngine engine;
    if (startup.failure().isEmpty()) {
        engine.loadFromModule("Atlas.Ui", "Main");
    } else {
        engine.setInitialProperties({{"message", startup.failure()}});
        engine.loadFromModule("Atlas.Ui", "StartupError");
    }
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
