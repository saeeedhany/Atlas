#include "atlas/viewmodels/map_view_model.hpp"

#include <QVariantMap>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::ItemRef;
using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipType;
using atlas::core::TopicId;
using atlas::render::GraphCanvasItem;
using atlas::render::RenderEdge;
using atlas::render::RenderNode;

MapViewModel::MapViewModel(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                           Palette& palette, QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), placements_(&placements), palette_(&palette) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &MapViewModel::refresh);
    connect(workspace_, &WorkspaceController::topicsChanged, this, &MapViewModel::refresh);
    connect(memory_, &MemoryController::memoryChanged, this, &MapViewModel::refresh);
    connect(placements_, &PlacementController::placementsChanged, this, &MapViewModel::refresh);
    connect(placements_, &PlacementController::failed, this, &MapViewModel::errorOccurred);
    connect(palette_, &Palette::changed, this, &MapViewModel::refresh);
    refresh();
}

std::optional<TopicId> MapViewModel::scope() const {
    if (topicId_.isEmpty()) return std::nullopt;
    return parseId<TopicId>(topicId_);
}

void MapViewModel::setTopicId(const QString& topicId) {
    QString normalized = parseId<TopicId>(topicId) ? topicId : QString();
    if (normalized == topicId_) return;
    topicId_ = normalized;
    emit topicIdChanged();
    refresh();
}

void MapViewModel::setSelectedId(const QString& conceptId) {
    if (conceptId == selectedId_) return;
    selectedId_ = conceptId;
    applySelection();
    emit selectedIdChanged();
}

void MapViewModel::refresh() {
    std::unordered_map<std::string, std::string> topicNames;
    for (const auto& topic : workspace_->allTopics()) topicNames.emplace(topic.id().toString(), topic.name());

    auto topic = scope();
    auto objects = workspace_->allKnowledgeObjects();
    std::unordered_set<KnowledgeObjectId> members;
    for (const auto& object : objects) {
        if (!topic || object.topicId() == topic) members.insert(object.id());
    }

    std::unordered_set<KnowledgeObjectId> ghosts;
    edges_.clear();
    for (const auto& link : workspace_->allRelationships()) {
        bool sourceIn = members.contains(link.sourceId());
        bool targetIn = members.contains(link.targetId());
        if (!sourceIn && !targetIn) continue;
        if (!sourceIn) ghosts.insert(link.sourceId());
        if (!targetIn) ghosts.insert(link.targetId());
        RenderEdge edge;
        edge.sourceId = idString(link.sourceId());
        edge.targetId = idString(link.targetId());
        edge.directed = !atlas::core::isSymmetric(link.type());
        edge.contrast = link.type() == RelationshipType::AlternativeTo || link.type() == RelationshipType::OppositeOf;
        edge.ghost = !sourceIn || !targetIn;
        edges_.push_back(edge);
    }

    nodes_.clear();
    for (const auto& object : objects) {
        bool member = members.contains(object.id());
        if (!member && !ghosts.contains(object.id())) continue;
        RenderNode node;
        node.id = idString(object.id());
        node.label = toQString(object.title());
        if (auto point = placements_->position(object.id())) {
            node.x = point->x;
            node.y = point->y;
        }
        node.color = palette_->nodeFill();
        node.recall = memory_->recallChance(ItemRef::forConcept(object.id())).value_or(-1.0);
        node.ghost = !member;
        if (object.topicId()) {
            node.groupKey = idString(*object.topicId());
            auto name = topicNames.find(object.topicId()->toString());
            if (name != topicNames.end()) node.groupLabel = toQString(name->second);
        }
        nodes_.push_back(std::move(node));
    }
    conceptCount_ = static_cast<int>(members.size());

    if (!selectedId_.isEmpty()) {
        auto selected = parseId<KnowledgeObjectId>(selectedId_);
        if (!selected || workspace_->graph().findNode(*selected) == nullptr) {
            selectedId_.clear();
            emit selectedIdChanged();
        }
    }
    pushToCanvas();
    emit sceneChanged();
}

void MapViewModel::pushToCanvas() {
    if (!canvas_) return;
    canvas_->setTheme(palette_->mode());
    canvas_->setGraphData(nodes_, edges_);
    applySelection();
}

void MapViewModel::applySelection() {
    if (!canvas_) return;
    auto selected = parseId<KnowledgeObjectId>(selectedId_);
    if (!selected) {
        canvas_->clearHighlight();
        return;
    }
    std::unordered_set<QString> neighbors;
    for (const auto& neighbor :
         workspace_->graph().neighbors(*selected, std::nullopt, atlas::graph::GraphEngine::Direction::Both)) {
        neighbors.insert(idString(neighbor));
    }
    canvas_->setHighlight(selectedId_, neighbors);
}

void MapViewModel::attach(QQuickItem* item) {
    auto* canvas = qobject_cast<GraphCanvasItem*>(item);
    if (canvas == nullptr) {
        emit errorOccurred(tr("The map canvas is unavailable"));
        return;
    }
    canvas_ = canvas;
    connect(canvas, &GraphCanvasItem::nodeClicked, this, &MapViewModel::setSelectedId, Qt::UniqueConnection);
    connect(canvas, &GraphCanvasItem::groupClicked, this, &MapViewModel::setTopicId, Qt::UniqueConnection);
    pushToCanvas();
}

QVariantList MapViewModel::search(const QString& query) const {
    QVariantList results;
    auto topic = scope();
    for (const auto& object : workspace_->search(toStdString(query))) {
        if (topic && object.topicId() != topic) continue;
        results.append(QVariantMap{{"id", idString(object.id())}, {"title", toQString(object.title())}});
        if (results.size() == kSearchLimit) break;
    }
    return results;
}

QString MapViewModel::createConcept(const QString& title) {
    auto created = workspace_->createKnowledgeObject(toStdString(title), scope().value_or(atlas::core::uncategorizedTopicId()));
    if (!created.hasValue()) {
        emit errorOccurred(toQString(created.error().detail));
        return {};
    }
    QString id = idString(created.value());
    setSelectedId(id);
    return id;
}

void MapViewModel::tidy() {
    auto tidied = placements_->tidy();
    if (!tidied.hasValue()) emit errorOccurred(toQString(tidied.error().detail));
}

}  // namespace atlas::viewmodels
