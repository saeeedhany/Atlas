#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include "atlas/viewmodels/app_startup.hpp"
#include "doctest.h"

using namespace atlas::viewmodels;

TEST_CASE("a good database path starts the app") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppStartup startup(dir.filePath("atlas.db"), store, systemClock());
    CHECK(startup.failure().isEmpty());
    REQUIRE(startup.context() != nullptr);
    CHECK(startup.context()->topics().count() == 1);
}

TEST_CASE("a broken database file gives a readable failure instead of a crash") {
    QTemporaryDir dir;
    QFile file(dir.filePath("atlas.db"));
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("this is not a database, just text that fills the header and more");
    file.close();
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);

    AppStartup startup(dir.filePath("atlas.db"), store, systemClock());
    CHECK(startup.context() == nullptr);
    CHECK(startup.failure().contains("atlas.db"));
}
