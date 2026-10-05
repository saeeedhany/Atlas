#include "qml_fixture.hpp"

#include <QCoreApplication>
#include <QQmlComponent>

#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openMemoryDatabase() {
    auto opened = Database::open(":memory:");
    REQUIRE(opened.hasValue());
    return std::move(opened).value();
}

std::unique_ptr<QObject> finish(QQmlComponent& component, const QVariantMap& properties) {
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties(properties));
    REQUIRE(object != nullptr);
    return object;
}

}

QmlFixture::QmlFixture()
    : store_(dir_.filePath("settings.ini"), QSettings::IniFormat),
      database_(openMemoryDatabase()),
      context_(database_, store_, systemClock()) {
    REQUIRE(context_.load().hasValue());
    context_.provideSingletons();
    engine_.setOutputWarningsToStandardError(false);
    QObject::connect(&engine_, &QQmlEngine::warnings, [this](const QList<QQmlError>& list) {
        for (const auto& warning : list) warnings_.append(warning.toString());
    });
}

QmlFixture::~QmlFixture() {
    settle();
    INFO(warnings_.join('\n').toStdString());
    CHECK(warnings_.isEmpty());
}

std::unique_ptr<QObject> QmlFixture::create(const char* type, const QVariantMap& properties) {
    QQmlComponent component(&engine_, "Atlas.Ui", type);
    return finish(component, properties);
}

std::unique_ptr<QObject> QmlFixture::createFromData(const QByteArray& qml) {
    QQmlComponent component(&engine_);
    component.setData(qml, QUrl());
    return finish(component, {});
}

QObject* QmlFixture::child(QObject* root, const char* objectName) {
    auto* found = root->findChild<QObject*>(objectName);
    INFO(objectName);
    REQUIRE(found != nullptr);
    return found;
}

void QmlFixture::settle() {
    for (int round = 0; round < 3; ++round) QCoreApplication::processEvents();
}
