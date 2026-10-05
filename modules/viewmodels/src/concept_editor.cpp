#include "atlas/viewmodels/concept_editor.hpp"

#include <QVariantMap>

#include <algorithm>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::Difficulty;
using atlas::core::Example;
using atlas::core::ItemRef;
using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;
using atlas::core::MiniProject;
using atlas::core::Reference;
using atlas::core::TopicId;

namespace {

constexpr int kHighestDifficulty = static_cast<int>(Difficulty::Expert);

QVariantMap listItem(const std::string& primary, const std::string& secondary) {
    return QVariantMap{{"primary", toQString(primary)}, {"secondary", toQString(secondary)}};
}

struct ListItem {
    std::string primary;
    std::optional<std::string> secondary;
};

std::vector<ListItem> readList(const QVariantList& list) {
    std::vector<ListItem> items;
    for (const auto& entry : list) {
        auto map = entry.toMap();
        QString primary = map.value("primary").toString();
        QString secondary = map.value("secondary").toString();
        if (primary.isEmpty() && secondary.isEmpty()) continue;
        items.push_back({toStdString(primary), secondary.isEmpty() ? std::nullopt : std::optional(toStdString(secondary))});
    }
    return items;
}

}

ConceptEditor::ConceptEditor(WorkspaceController& workspace, MemoryController& memory,
                             PlacementController& placements, QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), placements_(&placements) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &ConceptEditor::onGraphChanged);
    connect(memory_, &MemoryController::memoryChanged, this, &ConceptEditor::loaded);
    connect(placements_, &PlacementController::placementsChanged, this, &ConceptEditor::loaded);
}

void ConceptEditor::setConceptId(const QString& conceptId) {
    if (conceptId == conceptId_ && exists_) return;
    conceptId_ = conceptId;
    load();
}

void ConceptEditor::load() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    auto object = id ? workspace_->findKnowledgeObject(*id) : std::nullopt;
    exists_ = object.has_value();
    draft_ = Draft{};
    if (object) {
        draft_.title = toQString(object->title());
        draft_.definition = toQString(object->definition());
        draft_.problemSolved = toQString(object->problemSolved());
        draft_.whyItExists = toQString(object->whyItExists());
        draft_.notes = toQString(object->notes());
        draft_.difficulty = static_cast<int>(object->difficulty());
        draft_.topicId = object->topicId() ? idString(*object->topicId()) : QString();
        for (const auto& example : object->examples()) {
            draft_.examples.append(listItem(example.description, example.snippet.value_or("")));
        }
        for (const auto& project : object->miniProjects()) {
            draft_.miniProjects.append(listItem(project.title, project.description));
        }
        for (const auto& reference : object->references()) {
            draft_.references.append(listItem(reference.title, reference.url.value_or("")));
        }
    }
    dirty_ = false;
    emit loaded();
    emit edited();
    emit roadmapChanged();
}

void ConceptEditor::onGraphChanged() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    bool stillThere = id && workspace_->findKnowledgeObject(*id).has_value();
    if (!stillThere) {
        if (!exists_) return;
        conceptId_.clear();
        load();
        return;
    }
    if (dirty_) emit roadmapChanged();
    else load();
}

void ConceptEditor::setDifficulty(int value) { assign(draft_.difficulty, std::clamp(value, 0, kHighestDifficulty)); }

double ConceptEditor::recall() const {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return -1.0;
    return memory_->recallChance(ItemRef::forConcept(*id)).value_or(-1.0);
}

bool ConceptEditor::pinned() const {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    return id && exists_ && placements_->isPinned(*id);
}

void ConceptEditor::setPinned(bool pinned) {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) {
        fail(tr("No concept is open"));
        return;
    }
    auto result = placements_->setPinned(*id, pinned);
    if (!result.hasValue()) fail(toQString(result.error().detail));
}

QStringList ConceptEditor::difficultyNames() const {
    QStringList names;
    for (int level = 0; level <= kHighestDifficulty; ++level) {
        names.append(QString::fromUtf8(atlas::core::toDisplayString(static_cast<Difficulty>(level))));
    }
    return names;
}

bool ConceptEditor::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

bool ConceptEditor::save() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return fail(tr("No concept is open"));
    auto topic = parseId<TopicId>(draft_.topicId);
    if (!topic) return fail(tr("Choose a topic"));

    KnowledgeObjectEdits edits;
    edits.title = toStdString(draft_.title);
    edits.definition = toStdString(draft_.definition);
    edits.problemSolved = toStdString(draft_.problemSolved);
    edits.whyItExists = toStdString(draft_.whyItExists);
    edits.notes = toStdString(draft_.notes);
    edits.difficulty = static_cast<Difficulty>(draft_.difficulty);
    edits.topicId = *topic;
    edits.examples.emplace();
    for (auto& entry : readList(draft_.examples)) edits.examples->push_back(Example{entry.primary, entry.secondary});
    edits.miniProjects.emplace();
    for (auto& entry : readList(draft_.miniProjects)) {
        edits.miniProjects->push_back(MiniProject{entry.primary, entry.secondary.value_or("")});
    }
    edits.references.emplace();
    for (auto& entry : readList(draft_.references)) edits.references->push_back(Reference{entry.primary, entry.secondary});

    auto updated = workspace_->updateKnowledgeObject(*id, std::move(edits));
    if (!updated.hasValue()) return fail(toQString(updated.error().detail));
    load();
    return true;
}

bool ConceptEditor::remove() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return fail(tr("No concept is open"));
    auto removed = workspace_->removeKnowledgeObject(*id);
    return removed.hasValue() || fail(toQString(removed.error().detail));
}

void ConceptEditor::revert() { load(); }

QVariantList ConceptEditor::roadmap() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return {};
    auto path = workspace_->roadmapFor(*id);
    if (!path.hasValue()) {
        if (path.error().code == WorkspaceController::RoadmapErrorCode::CycleDetected) {
            emit errorOccurred(tr("These prerequisites depend on each other in a loop"));
        }
        return {};
    }
    QVariantList steps;
    for (const auto& object : path.value()) {
        if (object.id() == *id) continue;
        steps.append(QVariantMap{{"id", idString(object.id())}, {"title", toQString(object.title())}});
    }
    return steps;
}

}
