#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

#include "atlas/viewmodels/provided_singleton.hpp"

namespace atlas::viewmodels {

class AppSettings : public QObject, public ProvidedSingleton<AppSettings> {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(int newPerDay READ newPerDay WRITE setNewPerDay NOTIFY newPerDayChanged)

public:
    static constexpr int kMinNewPerDay = 1;
    static constexpr int kMaxNewPerDay = 20;
    static constexpr int kDefaultNewPerDay = 5;

    explicit AppSettings(QSettings& store, QObject* parent = nullptr);

    bool darkTheme() const;
    void setDarkTheme(bool dark);
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    int newPerDay() const;
    void setNewPerDay(int count);
    QString deviceId() const;

signals:
    void darkThemeChanged();
    void reducedMotionChanged();
    void newPerDayChanged();

private:
    QSettings* store_;
    mutable QString deviceId_;
};

}  // namespace atlas::viewmodels
