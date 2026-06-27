// All tests in this binary — including the WorkspaceController and
// list-model tests, which are plain QObjects and don't strictly need
// it — share one QApplication, constructed here. MainWindow tests do
// need a real QApplication (Qt asserts when constructing a QWidget
// without one), and only one QApplication may exist per process, so
// every test in this binary runs under the same instance.
//
// QT_QPA_PLATFORM=offscreen lets Qt construct and exercise widgets
// without a real display server — exactly what a headless build/CI
// machine needs. Set before constructing QApplication, not after.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QApplication>

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
