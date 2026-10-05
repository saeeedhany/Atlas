#include "atlas/viewmodels/app_startup.hpp"

namespace atlas::viewmodels {

AppStartup::AppStartup(const QString& databasePath, QSettings& store, Clock clock) {
    auto opened = atlas::persistence::Database::open(databasePath.toStdString());
    if (!opened.hasValue()) {
        failure_ = QString("The database at %1 could not be opened: %2")
                       .arg(databasePath, QString::fromStdString(opened.error().detail));
        return;
    }
    database_.emplace(std::move(opened).value());
    context_ = std::make_unique<AppContext>(*database_, store, std::move(clock));
    if (auto loaded = context_->load(); !loaded.hasValue()) {
        failure_ = QString("The data in %1 could not be loaded: %2")
                       .arg(databasePath, QString::fromStdString(loaded.error().detail));
        context_.reset();
        return;
    }
    context_->provideSingletons();
}

}
