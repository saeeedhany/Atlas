#include "atlas/viewmodels/map_view_model.hpp"

#include <QVariantMap>

#include <algorithm>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "atlas/render/region_geometry.hpp"
#include "atlas/viewmodels/concept_links_model.hpp"
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
    connect(workspace_, &WorkspaceController::graphChanged, this, &MapViewModel::scheduleRefresh);
    connect(workspace_, &WorkspaceController::topicsChanged, this, &MapViewModel::dropMissingScope);
    connect(workspace_, &WorkspaceController::topicsChanged, this, &MapViewModel::scheduleRefresh);
    connect(memory_, &MemoryController::memoryChanged, this, &MapViewModel::scheduleRefresh);
    connect(placements_, &PlacementController::placementsChanged, this, &MapViewModel::scheduleRefresh);
    connect(placements_, &PlacementController::failed, this, &MapViewModel::errorOccurred);
    connect(palette_, &Palette::changed, this, &MapViewModel::scheduleRefresh);
    refresh();
}

void MapViewModel::scheduleRefresh() {
    if (refreshPending_) return;
    refreshPending_ = true;
    QMetaObject::invokeMethod(this, [this] { if (refreshPending_) refresh(); }, Qt::QueuedConnection);
}

std::optional<TopicId> MapViewModel::scope() const {
    if (topicId_.isEmpty()) return std::nullopt;
    return parseId<TopicId>(topicId_);
}

void MapViewModel::dropMissingScope() {
    auto topic = scope();
    if (!topic || workspace_->findTopic(*topic)) return;
    topicId_.clear();
    emit topicIdChanged();
}

bool MapViewModel::isShown(const QString& conceptId) const {
    return std::any_of(nodes_.begin(), nodes_.end(), [&](const RenderNode& node) { return node.id == conceptId; });
}

void MapViewModel::setTopicId(const QString& topicId) {
    auto parsed = parseId<TopicId>(topicId);
    QString normalized = parsed ? idString(*parsed) : QString();
    if (normalized == topicId_) return;
    topicId_ = normalized;
    emit topicIdChanged();
    refresh();
}

void MapViewModel::openGroup(const QString& topicId) {
    if (!inSession()) setTopicId(topicId);
}

void MapViewModel::setSelectedId(const QString& conceptId) {
    auto parsed = parseId<KnowledgeObjectId>(conceptId);
    QString normalized = parsed ? idString(*parsed) : QString();
    bool known = parsed && workspace_->graph().findNode(*parsed) != nullptr;
    if (!isShown(normalized) && !(refreshPending_ && known)) normalized.clear();
    if (normalized == selectedId_) return;
    selectedId_ = normalized;
    applySelection();
    emit selectedIdChanged();
}

void MapViewModel::refresh() {
    refreshPending_ = false;
    if (auto topics = workspace_->topics(); topics.hasValue()) {
        topicNames_.clear();
        for (const auto& topic : topics.value()) topicNames_.emplace(topic.id().toString(), topic.name());
    } else {
        emit errorOccurred(toQString(topics.error().detail));
    }

    auto topic = scope();
    auto objects = workspace_->allKnowledgeObjects();
    std::unordered_set<KnowledgeObjectId> members;
    for (const auto& object : objects) {
        if (!topic || object.topicId() == topic) members.insert(object.id());
    }

    auto relationships = workspace_->allRelationships();
    std::unordered_set<KnowledgeObjectId> ghosts;
    for (const auto& link : relationships) {
        bool sourceIn = members.contains(link.sourceId());
        bool targetIn = members.contains(link.targetId());
        if (sourceIn && !targetIn) ghosts.insert(link.targetId());
        if (targetIn && !sourceIn) ghosts.insert(link.sourceId());
    }

    std::unordered_set<KnowledgeObjectId> shown;
    nodes_.clear();
    for (const auto& object : objects) {
        bool member = members.contains(object.id());
        if (!member && !ghosts.contains(object.id())) continue;
        auto point = placements_->position(object.id());
        if (!point) continue;
        shown.insert(object.id());
        RenderNode node;
        node.id = idString(object.id());
        node.label = toQString(object.title());
        node.x = point->x;
        node.y = point->y;
        node.color = palette_->nodeFill();
        node.recall = memory_->recallChance(ItemRef::forConcept(object.id())).value_or(-1.0);
        node.ghost = !member;
        node.hinted = marks_.hinted.contains(node.id);
        if (object.topicId()) {
            if (!topic) node.groupKey = idString(*object.topicId());
            auto name = topicNames_.find(object.topicId()->toString());
            if (name != topicNames_.end()) node.groupLabel = toQString(name->second);
        }
        nodes_.push_back(std::move(node));
    }

    edges_.clear();
    for (const auto& link : relationships) {
        if (!shown.contains(link.sourceId()) || !shown.contains(link.targetId())) continue;
        bool sourceIn = members.contains(link.sourceId());
        bool targetIn = members.contains(link.targetId());
        if (!sourceIn && !targetIn) continue;
        RenderEdge edge;
        edge.id = idString(link.id());
        edge.sourceId = idString(link.sourceId());
        edge.targetId = idString(link.targetId());
        edge.directed = !atlas::core::isSymmetric(link.type());
        edge.contrast = link.type() == RelationshipType::AlternativeTo || link.type() == RelationshipType::OppositeOf;
        edge.ghost = !sourceIn || !targetIn;
        if (auto outcome = marks_.outcomes.find(edge.id); outcome != marks_.outcomes.end()) edge.mark = outcome->second;
        else if (marks_.hiddenLinks.contains(edge.id)) edge.mark = atlas::render::EdgeMark::Hidden;
        edges_.push_back(edge);
    }
    for (const auto& wrongId : marks_.confused) {
        auto focus = parseId<KnowledgeObjectId>(marks_.focusId);
        auto wrong = parseId<KnowledgeObjectId>(wrongId);
        if (!focus || !wrong || !shown.contains(*focus) || !shown.contains(*wrong)) continue;
        RenderEdge edge;
        edge.id = QStringLiteral("confused:") + wrongId;
        edge.sourceId = marks_.focusId;
        edge.targetId = wrongId;
        edge.directed = false;
        edge.mark = atlas::render::EdgeMark::Confused;
        edges_.push_back(edge);
    }
    for (auto& node : nodes_) {
        node.degree = static_cast<int>(std::count_if(edges_.begin(), edges_.end(), [&](const RenderEdge& edge) {
            return edge.sourceId == node.id || edge.targetId == node.id;
        }));
    }
    buildRegions(objects);
    conceptCount_ = static_cast<int>(members.size());

    if (!selectedId_.isEmpty() && !isShown(selectedId_)) {
        selectedId_.clear();
        emit selectedIdChanged();
    }
    pushToCanvas();
    emit sceneChanged();
}

void MapViewModel::buildRegions(const std::vector<atlas::core::KnowledgeObject>& objects) {
    std::unordered_map<std::string, std::vector<QPointF>> members;
    std::unordered_map<std::string, std::vector<double>> recalls;
    for (const auto& object : objects) {
        auto topicId = object.topicId();
        if (!topicId || *topicId == atlas::core::uncategorizedTopicId()) continue;
        QString id = idString(object.id());
        auto node = std::find_if(nodes_.begin(), nodes_.end(), [&](const RenderNode& n) { return n.id == id; });
        if (node == nodes_.end() || node->ghost) continue;
        members[topicId->toString()].emplace_back(node->x, node->y);
        if (node->recall >= 0.0) recalls[topicId->toString()].push_back(node->recall);
    }
    regions_.clear();
    renderRegions_.clear();
    for (const auto& [topicKey, points] : members) {
        QString topicId = toQString(topicKey);
        QRectF rect = atlas::render::regionBounds(points, kRegionPadding);
        int hue = atlas::render::regionHue(topicId);
        QString caption = points.size() == 1 ? tr("1 concept") : tr("%1 concepts").arg(points.size());
        if (auto learned = recalls.find(topicKey); learned != recalls.end() && !learned->second.empty()) {
            double mean = std::accumulate(learned->second.begin(), learned->second.end(), 0.0) / learned->second.size();
            caption += QStringLiteral("  \u00b7  %1%").arg(qRound(mean * 100.0));
        }
        auto name = topicNames_.find(topicKey);
        regions_.append(QVariantMap{{"topicId", topicId},
                                    {"name", name != topicNames_.end() ? toQString(name->second) : QString()},
                                    {"caption", caption},
                                    {"x", rect.x()},
                                    {"y", rect.y()},
                                    {"width", rect.width()},
                                    {"height", rect.height()},
                                    {"hue", hue}});
        renderRegions_.push_back(atlas::render::RenderRegion{topicId, rect, hue});
    }
    std::sort(regions_.begin(), regions_.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value("name").toString() < b.toMap().value("name").toString();
    });
}

void MapViewModel::setSessionMarks(SessionMarks marks) {
    marks_ = std::move(marks);
    refresh();
}

void MapViewModel::clearSessionMarks() {
    marks_ = SessionMarks{};
    refresh();
}

void MapViewModel::pushToCanvas() {
    if (!canvas_) return;
    canvas_->setTheme(palette_->mode());
    canvas_->setRegions(renderRegions_);
    canvas_->setGraphData(nodes_, edges_);
    applySelection();
}

void MapViewModel::applySelection() {
    if (!canvas_) return;
    if (inSession()) {
        canvas_->setHighlight(selectedId_, {});
        return;
    }
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
    if (canvas_ && canvas_ != canvas) disconnect(canvas_, nullptr, this, nullptr);
    canvas_ = canvas;
    connect(canvas, &GraphCanvasItem::nodeClicked, this, &MapViewModel::setSelectedId, Qt::UniqueConnection);
    connect(canvas, &GraphCanvasItem::groupClicked, this, &MapViewModel::openGroup, Qt::UniqueConnection);
    if (refreshPending_) refresh();
    else pushToCanvas();
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

QVariantMap MapViewModel::conceptInfo(const QString& id) const {
    auto conceptId = parseId<KnowledgeObjectId>(id);
    const auto* object = conceptId ? workspace_->graph().findNode(*conceptId) : nullptr;
    if (object == nullptr) return {};
    QString topic;
    if (object->topicId()) {
        auto name = topicNames_.find(object->topicId()->toString());
        if (name != topicNames_.end()) topic = toQString(name->second);
    }
    return {{"title", toQString(object->title())},
            {"recall", memory_->recallChance(ItemRef::forConcept(*conceptId)).value_or(-1.0)},
            {"topic", topic},
            {"definition", toQString(object->definition())}};
}

QVariantMap MapViewModel::linkInfo(const QString& linkId) const {
    auto id = parseId<atlas::core::RelationshipId>(linkId);
    const auto* link = id ? workspace_->graph().findEdge(*id) : nullptr;
    if (link == nullptr) return {};
    const auto* source = workspace_->graph().findNode(link->sourceId());
    const auto* target = workspace_->graph().findNode(link->targetId());
    return {{"sourceId", idString(link->sourceId())},
            {"targetId", idString(link->targetId())},
            {"source", source ? toQString(source->title()) : QString()},
            {"target", target ? toQString(target->title()) : QString()},
            {"typeName", relationshipLabel(link->type())},
            {"note", toQString(link->note().value_or(""))}};
}

QString MapViewModel::createConcept(const QString& title) {
    auto created = workspace_->createKnowledgeObject(toStdString(title), scope().value_or(atlas::core::uncategorizedTopicId()));
    if (!created.hasValue()) {
        emit errorOccurred(toQString(created.error().detail));
        return {};
    }
    QString id = idString(created.value());
    refresh();
    setSelectedId(id);
    return id;
}

void MapViewModel::tidy() {
    auto tidied = placements_->tidy();
    if (!tidied.hasValue()) emit errorOccurred(toQString(tidied.error().detail));
}

bool MapViewModel::moveTopic(const QString& topicId, double dx, double dy) {
    auto topic = parseId<TopicId>(topicId);
    if (!topic) return false;
    std::vector<KnowledgeObjectId> ids;
    for (const auto& object : workspace_->knowledgeObjectsInTopic(*topic)) ids.push_back(object.id());
    if (ids.empty()) return false;
    auto moved = placements_->moveBy(ids, dx, dy);
    if (!moved.hasValue()) {
        emit errorOccurred(toQString(moved.error().detail));
        return false;
    }
    return true;
}

}  // namespace atlas::viewmodels
