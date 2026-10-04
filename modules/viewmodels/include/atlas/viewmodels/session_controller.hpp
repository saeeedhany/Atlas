#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <chrono>
#include <unordered_set>
#include <vector>

#include "atlas/learning/rebuild_grader.hpp"
#include "atlas/learning/session_planner.hpp"
#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/map_view_model.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/session_summary.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class SessionController : public QObject, public ProvidedSingleton<SessionController> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Session)
    QML_SINGLETON
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(int focusNumber READ focusNumber NOTIFY changed)
    Q_PROPERTY(int focusCount READ focusCount NOTIFY changed)
    Q_PROPERTY(QString focusId READ focusId NOTIFY changed)
    Q_PROPERTY(QString focusTitle READ focusTitle NOTIFY changed)
    Q_PROPERTY(bool focusIsNew READ focusIsNew NOTIFY changed)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY changed)
    Q_PROPERTY(QVariantList recalled READ recalled NOTIFY changed)
    Q_PROPERTY(int hintsUsed READ hintsUsed NOTIFY changed)
    Q_PROPERTY(QVariantList feedback READ feedback NOTIFY changed)
    Q_PROPERTY(QString prompt READ prompt NOTIFY changed)
    Q_PROPERTY(QString answerKey READ answerKey NOTIFY changed)
    Q_PROPERTY(int reviewedCount READ reviewedCount NOTIFY changed)
    Q_PROPERTY(int recalledCount READ recalledCount NOTIFY changed)
    Q_PROPERTY(QStringList calibration READ calibration NOTIFY changed)
    Q_PROPERTY(QString tip READ tip NOTIFY changed)
    Q_PROPERTY(QString tipSource READ tipSource NOTIFY changed)
    Q_PROPERTY(QStringList certaintyNames READ certaintyNameList CONSTANT)
    Q_PROPERTY(QStringList typeNames READ typeNames CONSTANT)

public:
    static constexpr int kMinQueryLength = 3;
    static constexpr int kCandidateLimit = 8;

    SessionController(WorkspaceController& workspace, MemoryController& memory, MapViewModel& map,
                      AppSettings& settings, QObject* parent = nullptr);

    QString stage() const;
    int focusNumber() const;
    int focusCount() const { return static_cast<int>(queue_.size()); }
    QString focusId() const;
    QString focusTitle() const;
    bool focusIsNew() const;
    int hiddenCount() const { return static_cast<int>(marks_.hiddenLinks.size()); }
    QVariantList recalled() const;
    int hintsUsed() const { return static_cast<int>(hinted_.size()); }
    QVariantList feedback() const { return feedback_; }
    QString prompt() const { return prompt_; }
    QString answerKey() const { return answerKey_; }
    int reviewedCount() const { return static_cast<int>(recorded_.size()); }
    int recalledCount() const;
    QStringList calibration() const { return calibrationLines(recorded_); }
    QString tip() const { return tip_.text; }
    QString tipSource() const { return tip_.source; }
    QStringList certaintyNameList() const { return certaintyNames(); }
    QStringList typeNames() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE QVariantList candidates(const QString& query) const;
    Q_INVOKABLE bool addRecalled(const QString& id, int typeIndex, bool focusIsSource);
    Q_INVOKABLE void updateRecalled(int index, int typeIndex, bool focusIsSource);
    Q_INVOKABLE void removeRecalled(int index);
    Q_INVOKABLE QString hint();
    Q_INVOKABLE bool submitRebuild(int certainty);
    Q_INVOKABLE void continueToExplain();
    Q_INVOKABLE bool submitExplain(int certainty, int grade, const QString& writtenKey);
    Q_INVOKABLE void quit();
    Q_INVOKABLE void finish();

signals:
    void changed();
    void errorOccurred(const QString& message);

private:
    enum class Stage { Idle, Rebuild, Feedback, Explain, Summary };
    enum class AnswerField { Definition, Problem, LinkNote };

    struct Attempt {
        atlas::learning::FocusPlan plan;
        bool retry = false;
    };

    bool fail(const QString& message);
    bool active() const { return stage_ != Stage::Idle && stage_ != Stage::Summary; }
    const Attempt& current() const { return queue_[index_]; }
    std::vector<atlas::core::RelationshipId> gradedLinks() const;
    std::unordered_set<QString> introducedLinks() const;
    bool itemExists(const atlas::core::ItemRef& item) const;
    void seekFocus();
    bool isNamed(const atlas::core::KnowledgeObjectId& id) const;
    QString titleOf(const atlas::core::KnowledgeObjectId& id) const;
    std::chrono::milliseconds sinceStageStart() const;
    atlas::core::ReviewEvent makeEvent(const atlas::core::ItemRef& item, atlas::core::Exercise exercise,
                                       atlas::core::Certainty predicted, atlas::core::Grade grade,
                                       atlas::core::TimePoint at) const;
    void enterFocus();
    void prepareExplain();
    bool saveAnswerKey(const QString& text);
    void advance();
    void showSummary();
    void pushMarks();

    WorkspaceController* workspace_;
    MemoryController* memory_;
    MapViewModel* map_;
    AppSettings* settings_;
    Stage stage_ = Stage::Idle;
    std::vector<Attempt> queue_;
    size_t index_ = 0;
    atlas::core::Uuid sessionId_;
    QString previousTopic_;
    std::vector<atlas::learning::RecalledLink> recalled_;
    std::vector<atlas::core::KnowledgeObjectId> hinted_;
    std::vector<atlas::core::ReviewEvent> pending_;
    std::vector<atlas::core::ReviewEvent> recorded_;
    QVariantList feedback_;
    SessionMarks marks_;
    atlas::core::ItemRef explainItem_;
    AnswerField answerField_ = AnswerField::Definition;
    QString prompt_;
    QString answerKey_;
    atlas::core::TimePoint stageStartedAt_;
    SessionTip tip_;
};

}  // namespace atlas::viewmodels
