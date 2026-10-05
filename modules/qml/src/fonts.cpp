#include "atlas/ui/fonts.hpp"

#include <QFontDatabase>

namespace atlas::ui {

QStringList loadBundledFonts() {
    QStringList families;
    for (const char* file : {"Inter.ttf", "Newsreader.ttf", "IBMPlexMono-Regular.ttf"}) {
        int id = QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Atlas/Ui/fonts/") + file);
        if (id >= 0) families.append(QFontDatabase::applicationFontFamilies(id));
    }
    return families;
}

}  // namespace atlas::ui
