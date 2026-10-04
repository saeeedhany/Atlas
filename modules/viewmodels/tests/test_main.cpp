#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QGuiApplication>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Atlas_ViewModelsPlugin)

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
