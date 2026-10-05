#pragma once

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

namespace atlas::ui {

// QML reserves the word "transient", so the property is declared here.
class PanelBase : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool transient READ isTransient WRITE setTransient NOTIFY transientChanged FINAL)

public:
    explicit PanelBase(QQuickItem* parent = nullptr) : QQuickItem(parent) {}

    bool isTransient() const { return transient_; }
    void setTransient(bool value) {
        if (transient_ == value) return;
        transient_ = value;
        emit transientChanged();
    }

signals:
    void transientChanged();

private:
    bool transient_ = false;
};

}  // namespace atlas::ui
