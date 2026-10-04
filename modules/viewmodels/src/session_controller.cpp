#include "atlas/viewmodels/session_controller.hpp"

#include <algorithm>

#include "atlas/viewmodels/concept_links_model.hpp"
#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::Certainty;
using atlas::core::Exercise;
using atlas::core::Grade;
using atlas::core::ItemKind;
using atlas::core::ItemRef;
using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;
using atlas::core::ReviewEvent;
using atlas::core::TimePoint;
using atlas::core::Uuid;
using atlas::render::EdgeMark;

namespace {

EdgeMark markFor(Grade grade) {
    if (grade == Grade::Again) return EdgeMark::Missed;
    return grade == Grade::Hard ? EdgeMark::Partial : EdgeMark::Recalled;
}

QString outcomeName(EdgeMark mark) {
    if (mark == EdgeMark::Missed) return QStringLiteral("missed");
    return mark == EdgeMark::Partial ? QStringLiteral("partial") : QStringLiteral("recalled");
}

}  // namespace

SessionController::SessionController(WorkspaceController& workspace, MemoryController& memory, MapViewModel& map,
                                     AppSettings& settings, QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), map_(&map), settings_(&settings) {}

QString SessionController::stage() const {
    switch (stage_) {
        case Stage::Rebuild: return QStringLiteral("rebuild");
        case Stage::Feedback: return QStringLiteral("feedback");
        case Stage::Explain: return QStringLiteral("explain");
        case Stage::Summary: return QStringLiteral("summary");
        case Stage::Idle: break;
    }
    return QStringLiteral("idle");
}

int SessionController::focusNumber() const { return active() ? static_cast<int>(index_) + 1 : 0; }
QString SessionController::focusId() const { return active() ? idString(current().plan.conceptId) : QString(); }
QString SessionController::focusTitle() const { return active() ? titleOf(current().plan.conceptId) : QString(); }
bool SessionController::focusIsNew() const { return active() && current().plan.isNew; }

int SessionController::recalledCount() const {
    return static_cast<int>(std::count_if(recorded_.begin(), recorded_.end(),
                                          [](const ReviewEvent& event) { return event.grade != Grade::Again; }));
}

QStringList SessionController::typeNames() const {
    QStringList names;
    for (int type = 0; type <= static_cast<int>(RelationshipType::Causes); ++type) {
        names.append(relationshipLabel(static_cast<RelationshipType>(type)));
    }
    return names;
}

QVariantList SessionController::recalled() const {
    QVariantList list;
    for (const auto& named : recalled_) {
        list.append(QVariantMap{{"id", idString(named.other)},
                                {"title", titleOf(named.other)},
                                {"typeIndex", static_cast<int>(named.type)},
                                {"focusIsSource", named.focusIsSource}});
    }
    return list;
}

bool SessionController::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

QString SessionController::titleOf(const KnowledgeObjectId& id) const {
    const auto* object = workspace_->graph().findNode(id);
    return object ? toQString(object->title()) : QString();
}

std::chrono::milliseconds SessionController::sinceStageStart() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(memory_->now() - stageStartedAt_);
}

std::vector<RelationshipId> SessionController::gradedLinks() const {
    std::vector<RelationshipId> links;
    for (const auto& item : current().plan.items) {
        if (item.kind == ItemKind::Link) links.push_back(RelationshipId(item.id));
    }
    return links;
}

bool SessionController::isNamed(const KnowledgeObjectId& id) const {
    return std::any_of(recalled_.begin(), recalled_.end(), [&](const auto& named) { return named.other == id; });
}

ReviewEvent SessionController::makeEvent(const ItemRef& item, Exercise exercise, Certainty predicted, Grade grade,
                                         TimePoint at) const {
    ReviewEvent event;
    event.id = Uuid::generate();
    event.item = item;
    event.sessionId = sessionId_;
    event.deviceId = settings_->deviceId().toStdString();
    event.reviewedAt = at;
    bool repeated = std::any_of(pending_.begin(), pending_.end(), [&](const ReviewEvent& p) { return p.item == item; });
    event.elapsedDays = repeated ? 0.0 : memory_->elapsedDaysFor(item, at);
    event.exercise = exercise;
    event.predicted = predicted;
    event.grade = grade;
    return event;
}

bool SessionController::start() {
    if (stage_ != Stage::Idle) return fail(tr("A session is already running"));
    atlas::learning::SessionLimits limits;
    limits.newPerDay = settings_->newPerDay();
    auto plan = memory_->todayPlan(limits);
    if (plan.focuses.empty()) return fail(tr("Nothing to recall right now"));
    queue_.clear();
    for (auto& focus : plan.focuses) queue_.push_back(Attempt{std::move(focus), false});
    index_ = 0;
    sessionId_ = Uuid::generate();
    recorded_.clear();
    previousTopic_ = map_->topicId();
    map_->setTopicId(QString());
    seekFocus();
    return true;
}

void SessionController::seekFocus() {
    while (index_ < queue_.size() && workspace_->graph().findNode(current().plan.conceptId) == nullptr) ++index_;
    if (index_ < queue_.size()) enterFocus();
    else showSummary();
}

bool SessionController::itemExists(const ItemRef& item) const {
    if (item.kind == ItemKind::Link) return workspace_->graph().findEdge(RelationshipId(item.id)) != nullptr;
    return workspace_->graph().findNode(KnowledgeObjectId(item.id)) != nullptr;
}

std::unordered_set<QString> SessionController::introducedLinks() const {
    std::unordered_set<QString> links;
    const auto& focus = current().plan.conceptId;
    for (const auto& id : workspace_->graph().allEdgeIds()) {
        const auto* link = workspace_->graph().findEdge(id);
        if (link == nullptr || (link->sourceId() != focus && link->targetId() != focus)) continue;
        if (memory_->rules().isLinkReady(id, memory_->states())) links.insert(idString(id));
    }
    return links;
}

void SessionController::enterFocus() {
    recalled_.clear();
    hinted_.clear();
    pending_.clear();
    feedback_.clear();
    marks_ = SessionMarks{};
    marks_.focusId = idString(current().plan.conceptId);
    bool rebuilds = !gradedLinks().empty();
    if (rebuilds) marks_.hiddenLinks = introducedLinks();
    stage_ = rebuilds ? Stage::Rebuild : Stage::Explain;
    if (stage_ == Stage::Explain) prepareExplain();
    stageStartedAt_ = memory_->now();
    pushMarks();
    emit changed();
}

void SessionController::pushMarks() {
    map_->setSessionMarks(marks_);
    map_->setSelectedId(marks_.focusId);
}

QVariantList SessionController::candidates(const QString& query) const {
    QVariantList results;
    QString needle = query.trimmed();
    if (stage_ != Stage::Rebuild || needle.size() < kMinQueryLength) return results;
    const auto& focus = current().plan.conceptId;
    for (const auto& object : workspace_->allKnowledgeObjects()) {
        if (object.id() == focus || isNamed(object.id())) continue;
        QString title = toQString(object.title());
        if (!title.startsWith(needle, Qt::CaseInsensitive)) continue;
        results.append(QVariantMap{{"id", idString(object.id())}, {"title", title}});
        if (results.size() == kCandidateLimit) break;
    }
    return results;
}

bool SessionController::addRecalled(const QString& id, int typeIndex, bool focusIsSource) {
    auto other = parseId<KnowledgeObjectId>(id);
    if (stage_ != Stage::Rebuild || !other || *other == current().plan.conceptId || isNamed(*other)) return false;
    if (typeIndex < 0 || typeIndex > static_cast<int>(RelationshipType::Causes)) return false;
    if (workspace_->graph().findNode(*other) == nullptr) return false;
    recalled_.push_back({*other, static_cast<RelationshipType>(typeIndex), focusIsSource});
    emit changed();
    return true;
}

void SessionController::updateRecalled(int index, int typeIndex, bool focusIsSource) {
    if (stage_ != Stage::Rebuild || index < 0 || index >= static_cast<int>(recalled_.size())) return;
    if (typeIndex < 0 || typeIndex > static_cast<int>(RelationshipType::Causes)) return;
    recalled_[static_cast<size_t>(index)].type = static_cast<RelationshipType>(typeIndex);
    recalled_[static_cast<size_t>(index)].focusIsSource = focusIsSource;
    emit changed();
}

void SessionController::removeRecalled(int index) {
    if (stage_ != Stage::Rebuild || index < 0 || index >= static_cast<int>(recalled_.size())) return;
    recalled_.erase(recalled_.begin() + index);
    emit changed();
}

QString SessionController::hint() {
    if (stage_ != Stage::Rebuild) return {};
    const auto& focus = current().plan.conceptId;
    for (const auto& linkId : gradedLinks()) {
        const auto* link = workspace_->graph().findEdge(linkId);
        if (link == nullptr) continue;
        auto other = link->sourceId() == focus ? link->targetId() : link->sourceId();
        if (isNamed(other) || std::find(hinted_.begin(), hinted_.end(), other) != hinted_.end()) continue;
        hinted_.push_back(other);
        marks_.hinted.insert(idString(other));
        pushMarks();
        emit changed();
        return idString(other);
    }
    return {};
}

bool SessionController::submitRebuild(int certainty) {
    if (stage_ != Stage::Rebuild) return false;
    auto predicted = atlas::core::certaintyFromInt(certainty);
    if (!predicted) return fail(tr("Choose how sure you are first"));
    const auto& focus = current().plan.conceptId;
    auto neighbors = workspace_->graph().neighbors(focus, std::nullopt, atlas::graph::GraphEngine::Direction::Both);
    std::vector<KnowledgeObjectId> gradedEnds;
    for (const auto& linkId : gradedLinks()) {
        const auto* link = workspace_->graph().findEdge(linkId);
        if (link != nullptr) gradedEnds.push_back(link->sourceId() == focus ? link->targetId() : link->sourceId());
    }
    std::vector<atlas::learning::RecalledLink> judged;
    for (const auto& named : recalled_) {
        bool linked = std::find(neighbors.begin(), neighbors.end(), named.other) != neighbors.end();
        bool graded = std::find(gradedEnds.begin(), gradedEnds.end(), named.other) != gradedEnds.end();
        if (!linked || graded) judged.push_back(named);
    }
    atlas::learning::RebuildAnswer answer{focus, *predicted, judged, hinted_, sinceStageStart()};
    atlas::learning::RebuildGrader grader(workspace_->graph());
    auto graded = grader.grade(answer, gradedLinks(), atlas::learning::medianRebuildResponse(memory_->events()));

    TimePoint now = memory_->now();
    pending_.clear();
    feedback_.clear();
    for (const auto& result : graded) {
        const auto* link = workspace_->graph().findEdge(result.link);
        if (link == nullptr) continue;
        auto event = makeEvent(ItemRef::forLink(result.link), Exercise::Rebuild, *predicted, result.grade, now);
        event.hintsUsed = result.hintsUsed;
        event.wrongTarget = result.wrongTarget;
        event.responseTime = answer.responseTime;
        pending_.push_back(event);
        auto mark = markFor(result.grade);
        marks_.outcomes[idString(result.link)] = mark;
        auto other = link->sourceId() == focus ? link->targetId() : link->sourceId();
        feedback_.append(QVariantMap{
            {"title", titleOf(other)}, {"typeName", relationshipLabel(link->type())}, {"outcome", outcomeName(mark)}});
    }
    for (const auto& named : recalled_) {
        if (std::find(neighbors.begin(), neighbors.end(), named.other) != neighbors.end()) continue;
        marks_.confused.push_back(idString(named.other));
        feedback_.append(QVariantMap{{"title", titleOf(named.other)}, {"typeName", QString()}, {"outcome", "confused"}});
    }
    marks_.hinted.clear();
    marks_.hiddenLinks.clear();
    stage_ = Stage::Feedback;
    pushMarks();
    emit changed();
    return true;
}

void SessionController::continueToExplain() {
    if (stage_ != Stage::Feedback) return;
    prepareExplain();
    stage_ = Stage::Explain;
    stageStartedAt_ = memory_->now();
    emit changed();
}

void SessionController::prepareExplain() {
    const auto& plan = current().plan;
    explainItem_ = ItemRef::forConcept(plan.conceptId);
    double weakest = 2.0;
    for (const auto& item : plan.items) {
        double recall = memory_->recallChance(item).value_or(0.0);
        bool weaker = recall < weakest || (recall == weakest && item.kind == ItemKind::Concept);
        if (weaker) {
            weakest = recall;
            explainItem_ = item;
        }
    }
    if (explainItem_.kind == ItemKind::Link) {
        const auto* link = workspace_->graph().findEdge(RelationshipId(explainItem_.id));
        answerField_ = AnswerField::LinkNote;
        prompt_ = link ? linkPrompt(link->type(), titleOf(link->sourceId()), titleOf(link->targetId())) : QString();
        answerKey_ = link ? toQString(link->note().value_or("")) : QString();
        return;
    }
    const auto* object = workspace_->graph().findNode(plan.conceptId);
    auto state = memory_->states().find(explainItem_);
    int reviews = state == memory_->states().end() ? 0 : state->second.reviewCount;
    bool askProblem = reviews % 2 == 1;
    answerField_ = askProblem ? AnswerField::Problem : AnswerField::Definition;
    QString title = titleOf(plan.conceptId);
    prompt_ = askProblem ? tr("What problem does %1 solve?").arg(title) : tr("What is %1?").arg(title);
    answerKey_ = object ? toQString(askProblem ? object->problemSolved() : object->definition()) : QString();
}

bool SessionController::saveAnswerKey(const QString& text) {
    if (answerField_ == AnswerField::LinkNote) {
        auto saved = workspace_->setRelationshipNote(RelationshipId(explainItem_.id), toStdString(text));
        if (!saved.hasValue()) return fail(toQString(saved.error().detail));
        return true;
    }
    KnowledgeObjectEdits edits;
    if (answerField_ == AnswerField::Problem) edits.problemSolved = toStdString(text);
    else edits.definition = toStdString(text);
    auto saved = workspace_->updateKnowledgeObject(current().plan.conceptId, std::move(edits));
    if (!saved.hasValue()) return fail(toQString(saved.error().detail));
    return true;
}

bool SessionController::submitExplain(int certainty, int grade, const QString& writtenKey) {
    if (stage_ != Stage::Explain) return false;
    auto predicted = atlas::core::certaintyFromInt(certainty);
    auto graded = atlas::core::gradeFromInt(grade);
    if (!predicted || !graded) return fail(tr("Choose how sure you were and how it went"));
    QString written = writtenKey.trimmed();
    if (answerKey_.isEmpty() && !written.isEmpty() && saveAnswerKey(written)) answerKey_ = written;

    auto events = pending_;
    auto explain = makeEvent(explainItem_, Exercise::Explain, *predicted, *graded, memory_->now());
    explain.responseTime = sinceStageStart();
    events.push_back(explain);
    std::erase_if(events, [this](const ReviewEvent& event) { return !itemExists(event.item); });
    if (!events.empty()) {
        if (auto saved = memory_->record(events); !saved.hasValue()) return fail(toQString(saved.error().detail));
    }

    recorded_.insert(recorded_.end(), events.begin(), events.end());
    bool surprised = std::any_of(events.begin(), events.end(), [](const ReviewEvent& event) {
        return event.predicted == Certainty::Certain && event.grade == Grade::Again;
    });
    if (surprised && !current().retry) {
        auto again = current().plan;
        queue_.push_back(Attempt{std::move(again), true});
    }
    advance();
    return true;
}

void SessionController::advance() {
    ++index_;
    seekFocus();
}

void SessionController::showSummary() {
    stage_ = Stage::Summary;
    tip_ = chooseTip(recorded_);
    marks_ = SessionMarks{};
    map_->clearSessionMarks();
    map_->setSelectedId(QString());
    emit changed();
}

void SessionController::quit() {
    if (!active()) return;
    pending_.clear();
    if (recorded_.empty()) finish();
    else showSummary();
}

void SessionController::finish() {
    if (stage_ == Stage::Idle) return;
    stage_ = Stage::Idle;
    queue_.clear();
    index_ = 0;
    recalled_.clear();
    hinted_.clear();
    pending_.clear();
    feedback_.clear();
    marks_ = SessionMarks{};
    map_->clearSessionMarks();
    map_->setSelectedId(QString());
    map_->setTopicId(previousTopic_);
    emit changed();
}

}  // namespace atlas::viewmodels
