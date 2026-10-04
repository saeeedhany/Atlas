#pragma once

#include <QQmlEngine>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariantMap>

#include <memory>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/app_context.hpp"

class QmlFixture {
public:
    QmlFixture();
    ~QmlFixture();

    atlas::viewmodels::AppContext& context() { return context_; }
    std::unique_ptr<QObject> create(const char* type, const QVariantMap& properties = {});
    std::unique_ptr<QObject> createFromData(const QByteArray& qml);
    static QObject* child(QObject* root, const char* objectName);
    static void settle();

private:
    QTemporaryDir dir_;
    QSettings store_;
    atlas::persistence::Database database_;
    atlas::viewmodels::AppContext context_;
    QStringList warnings_;
    QQmlEngine engine_;
};
