#pragma once

#include <QSettings>
#include <QString>

#include <memory>
#include <optional>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/app_context.hpp"

namespace atlas::viewmodels {

class AppStartup {
public:
    AppStartup(const QString& databasePath, QSettings& store, Clock clock);
    AppStartup(const AppStartup&) = delete;
    AppStartup& operator=(const AppStartup&) = delete;

    const QString& failure() const { return failure_; }
    AppContext* context() { return context_.get(); }

private:
    std::optional<atlas::persistence::Database> database_;
    std::unique_ptr<AppContext> context_;
    QString failure_;
};

}  // namespace atlas::viewmodels
