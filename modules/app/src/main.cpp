#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include "atlas/persistence/database.hpp"
#include "atlas/ui/main_window.hpp"
#include "atlas/ui/workspace_controller.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    std::string dbPath = (dataDir + "/atlas.db").toStdString();

    auto databaseResult = atlas::persistence::Database::open(dbPath);
    if (!databaseResult.hasValue()) {
        QMessageBox::critical(nullptr, "Atlas",
                               QString("Failed to open database at %1:\n%2")
                                   .arg(QString::fromStdString(dbPath),
                                        QString::fromStdString(databaseResult.error().detail)));
        return 1;
    }
    auto database = std::move(databaseResult).value();

    atlas::ui::WorkspaceController controller(database);
    auto loadResult = controller.load();
    if (!loadResult.hasValue()) {
        QMessageBox::critical(nullptr, "Atlas",
                               QString("Failed to load data:\n%1")
                                   .arg(QString::fromStdString(loadResult.error().detail)));
        return 1;
    }

    atlas::ui::MainWindow window(controller);
    window.show();

    return app.exec();
}
