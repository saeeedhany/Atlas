#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "atlas/render/theme.hpp"
#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"

namespace atlas::viewmodels {

class Palette : public QObject, public ProvidedSingleton<Palette> {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceRaised READ surfaceRaised NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)
    Q_PROPERTY(QColor nodeFill READ nodeFill NOTIFY changed)
    Q_PROPERTY(QColor ringStrong READ ringStrong NOTIFY changed)
    Q_PROPERTY(QColor ringMedium READ ringMedium NOTIFY changed)
    Q_PROPERTY(QColor ringWeak READ ringWeak NOTIFY changed)
    Q_PROPERTY(QColor ringNew READ ringNew NOTIFY changed)
    Q_PROPERTY(QColor ringTrack READ ringTrack NOTIFY changed)
    Q_PROPERTY(QColor outlineStrong READ outlineStrong NOTIFY changed)
    Q_PROPERTY(QColor textFaint READ textFaint NOTIFY changed)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY changed)
    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY changed)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY changed)
    Q_PROPERTY(QColor link READ link NOTIFY changed)

public:
    explicit Palette(AppSettings& settings, QObject* parent = nullptr);

    atlas::render::ThemeMode mode() const;
    Q_INVOKABLE QColor ringColor(double recall) const;

    QColor background() const { return theme().background; }
    QColor surface() const { return theme().surface; }
    QColor surfaceRaised() const { return theme().surfaceRaised; }
    QColor border() const { return theme().border; }
    QColor text() const { return theme().text; }
    QColor textMuted() const { return theme().textMuted; }
    QColor accent() const { return theme().accent; }
    QColor danger() const { return theme().danger; }
    QColor nodeFill() const { return theme().nodeFill; }
    QColor ringStrong() const { return theme().ringStrong; }
    QColor ringMedium() const { return theme().ringMedium; }
    QColor ringWeak() const { return theme().ringWeak; }
    QColor ringNew() const { return theme().ringNew; }
    QColor ringTrack() const { return theme().ringTrack; }
    QColor outlineStrong() const { return theme().outlineStrong; }
    QColor textFaint() const { return theme().textFaint; }
    QColor secondary() const { return theme().secondary; }
    QColor tertiary() const { return theme().tertiary; }
    QColor onPrimary() const { return theme().onPrimary; }
    QColor link() const { return theme().link; }
    Q_INVOKABLE QColor regionTint(int hue) const;
    Q_INVOKABLE QColor noteFill(const QString& color) const;
    Q_INVOKABLE QColor noteText(const QString& color) const;

signals:
    void changed();

private:
    const atlas::render::Theme& theme() const;

    AppSettings* settings_;
};

}  // namespace atlas::viewmodels
