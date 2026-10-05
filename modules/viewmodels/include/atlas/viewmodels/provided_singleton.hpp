#pragma once

#include <QJSEngine>

class QQmlEngine;

namespace atlas::viewmodels {

template <typename T>
class ProvidedSingleton {
public:
    static void provide(T* instance) { instance_ = instance; }

    static T* create(QQmlEngine*, QJSEngine*) {
        Q_ASSERT_X(instance_ != nullptr, "ProvidedSingleton", "provide() must run before QML loads");
        QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
        return instance_;
    }

private:
    static inline T* instance_ = nullptr;
};

}
