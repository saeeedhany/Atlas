#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class TodayViewModel : public QObject, public ProvidedSingleton<TodayViewModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Today)
    QML_SINGLETON
    Q_PROPERTY(int dueCount READ dueCount NOTIFY changed)
    Q_PROPERTY(int newCount READ newCount NOTIFY changed)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY changed)
    Q_PROPERTY(int estimatedMinutes READ estimatedMinutes NOTIFY changed)
    Q_PROPERTY(int learnedCount READ learnedCount NOTIFY changed)
    Q_PROPERTY(int conceptCount READ conceptCount NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)
    Q_PROPERTY(bool caughtUp READ caughtUp NOTIFY changed)

public:
    TodayViewModel(WorkspaceController& workspace, MemoryController& memory, AppSettings& settings,
                   QObject* parent = nullptr);

    int dueCount() const { return dueCount_; }
    int newCount() const { return newCount_; }
    int itemCount() const { return itemCount_; }
    int estimatedMinutes() const { return estimatedMinutes_; }
    int learnedCount() const { return learnedCount_; }
    int conceptCount() const { return conceptCount_; }
    bool empty() const { return conceptCount_ == 0; }
    bool caughtUp() const { return conceptCount_ > 0 && dueCount_ + newCount_ == 0; }

    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    WorkspaceController* workspace_;
    MemoryController* memory_;
    AppSettings* settings_;
    int dueCount_ = 0;
    int newCount_ = 0;
    int itemCount_ = 0;
    int estimatedMinutes_ = 0;
    int learnedCount_ = 0;
    int conceptCount_ = 0;
};

}  // namespace atlas::viewmodels
