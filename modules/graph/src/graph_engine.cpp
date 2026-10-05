#include "atlas/graph/graph_engine.hpp"

#include <algorithm>
#include <queue>
#include <set>
#include <unordered_set>
#include <utility>

namespace atlas::graph {

Result<void, GraphError> GraphEngine::addNode(KnowledgeObject node) {
    auto id = node.id();
    if (nodeIndexById_.contains(id)) {
        return Result<void, GraphError>::err(GraphError::DuplicateNode);
    }
    size_t index = nodes_.size();
    nodes_.push_back(NodeSlot{std::move(node)});
    outgoingEdgeIndices_.emplace_back();
    incomingEdgeIndices_.emplace_back();
    nodeIndexById_.emplace(id, index);
    return Result<void, GraphError>::ok();
}

Result<void, GraphError> GraphEngine::updateNode(KnowledgeObject node) {
    auto it = nodeIndexById_.find(node.id());
    if (it == nodeIndexById_.end()) {
        return Result<void, GraphError>::err(GraphError::UnknownNode);
    }
    nodes_[it->second].object = std::move(node);
    return Result<void, GraphError>::ok();
}

std::vector<KnowledgeObjectId> GraphEngine::allNodeIds() const {
    std::vector<KnowledgeObjectId> ids;
    ids.reserve(nodeIndexById_.size());
    for (const auto& [id, index] : nodeIndexById_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<RelationshipId> GraphEngine::allEdgeIds() const {
    std::vector<RelationshipId> ids;
    ids.reserve(edgeIndexById_.size());
    for (const auto& [id, index] : edgeIndexById_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<KnowledgeObjectId> GraphEngine::search(std::string_view query) const {
    if (query.empty()) return allNodeIds();

    std::vector<std::pair<int, KnowledgeObjectId>> scored;
    scored.reserve(nodeIndexById_.size());
    for (const auto& [id, index] : nodeIndexById_) {
        const auto& slot = nodes_[index];
        if (!slot.object.has_value()) continue;
        auto score = atlas::core::matchScore(*slot.object, query);
        if (score.has_value()) scored.emplace_back(*score, id);
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;  // higher score first
        // Deterministic tie-break: nodeIndexById_ is a hash map, so
        // without this, two identical searches could return ties in a
        // different order just because of iteration order - making
        // results look like they're "jumping around" for no reason.
        return a.second.toString() < b.second.toString();
    });

    std::vector<KnowledgeObjectId> result;
    result.reserve(scored.size());
    for (const auto& [score, id] : scored) result.push_back(id);
    return result;
}

bool GraphEngine::hasDuplicateEdge(const KnowledgeObjectId& source, const KnowledgeObjectId& target,
                                     RelationshipType type) const {
    auto sourceIt = nodeIndexById_.find(source);
    if (sourceIt == nodeIndexById_.end()) return false;

    for (size_t edgeIdx : outgoingEdgeIndices_[sourceIt->second]) {
        const auto& slot = edges_[edgeIdx];
        if (!slot.relationship.has_value()) continue;
        const auto& rel = *slot.relationship;
        if (rel.type() != type) continue;

        bool sameOrder = (rel.sourceId() == source && rel.targetId() == target);
        bool sameUnorderedPair =
            isSymmetric(type) && (rel.sourceId() == target && rel.targetId() == source);
        if (sameOrder || sameUnorderedPair) return true;
    }
    return false;
}

void GraphEngine::indexEdge(size_t edgeIndex) {
    const auto& rel = *edges_[edgeIndex].relationship;
    size_t sourceIdx = nodeIndexById_.at(rel.sourceId());
    size_t targetIdx = nodeIndexById_.at(rel.targetId());

    outgoingEdgeIndices_[sourceIdx].push_back(edgeIndex);
    incomingEdgeIndices_[targetIdx].push_back(edgeIndex);

    // A symmetric edge is one fact, stored once, but it must be
    // findable as a neighbor from either endpoint regardless of which
    // one happens to be `source` in storage.
    if (isSymmetric(rel.type())) {
        outgoingEdgeIndices_[targetIdx].push_back(edgeIndex);
        incomingEdgeIndices_[sourceIdx].push_back(edgeIndex);
    }
}

void GraphEngine::unindexEdge(size_t edgeIndex) {
    const auto& rel = *edges_[edgeIndex].relationship;
    size_t sourceIdx = nodeIndexById_.at(rel.sourceId());
    size_t targetIdx = nodeIndexById_.at(rel.targetId());

    auto erase = [edgeIndex](std::vector<size_t>& indices) {
        indices.erase(std::remove(indices.begin(), indices.end(), edgeIndex), indices.end());
    };
    erase(outgoingEdgeIndices_[sourceIdx]);
    erase(incomingEdgeIndices_[targetIdx]);
    if (isSymmetric(rel.type())) {
        erase(outgoingEdgeIndices_[targetIdx]);
        erase(incomingEdgeIndices_[sourceIdx]);
    }
}

Result<void, GraphError> GraphEngine::addEdge(Relationship edge) {
    if (!nodeIndexById_.contains(edge.sourceId()) || !nodeIndexById_.contains(edge.targetId())) {
        return Result<void, GraphError>::err(GraphError::UnknownNode);
    }
    if (edgeIndexById_.contains(edge.id())) {
        return Result<void, GraphError>::err(GraphError::DuplicateEdge);
    }
    if (hasDuplicateEdge(edge.sourceId(), edge.targetId(), edge.type())) {
        return Result<void, GraphError>::err(GraphError::DuplicateEdge);
    }

    size_t edgeIndex = edges_.size();
    RelationshipId id = edge.id();
    edges_.push_back(EdgeSlot{std::move(edge)});
    edgeIndexById_.emplace(id, edgeIndex);
    indexEdge(edgeIndex);
    return Result<void, GraphError>::ok();
}

bool GraphEngine::removeEdge(const RelationshipId& id) {
    auto it = edgeIndexById_.find(id);
    if (it == edgeIndexById_.end()) return false;

    size_t edgeIndex = it->second;
    unindexEdge(edgeIndex);
    edges_[edgeIndex].relationship.reset();
    edgeIndexById_.erase(it);
    return true;
}

bool GraphEngine::removeNode(const KnowledgeObjectId& id) {
    auto it = nodeIndexById_.find(id);
    if (it == nodeIndexById_.end()) return false;
    size_t nodeIndex = it->second;

    // Cascade to every edge touching this node, mirroring the
    // database's ON DELETE CASCADE. Copy first: unindexEdge mutates
    // the very vectors we'd otherwise be iterating.
    std::vector<size_t> touching = outgoingEdgeIndices_[nodeIndex];
    touching.insert(touching.end(), incomingEdgeIndices_[nodeIndex].begin(),
                     incomingEdgeIndices_[nodeIndex].end());
    std::sort(touching.begin(), touching.end());
    touching.erase(std::unique(touching.begin(), touching.end()), touching.end());

    for (size_t edgeIdx : touching) {
        if (!edges_[edgeIdx].relationship.has_value()) continue;
        RelationshipId relId = edges_[edgeIdx].relationship->id();
        unindexEdge(edgeIdx);
        edges_[edgeIdx].relationship.reset();
        edgeIndexById_.erase(relId);
    }

    nodes_[nodeIndex].object.reset();
    outgoingEdgeIndices_[nodeIndex].clear();
    incomingEdgeIndices_[nodeIndex].clear();
    nodeIndexById_.erase(it);
    return true;
}

const KnowledgeObject* GraphEngine::findNode(const KnowledgeObjectId& id) const {
    auto it = nodeIndexById_.find(id);
    if (it == nodeIndexById_.end()) return nullptr;
    const auto& slot = nodes_[it->second];
    return slot.object.has_value() ? &(*slot.object) : nullptr;
}

const Relationship* GraphEngine::findEdge(const RelationshipId& id) const {
    auto it = edgeIndexById_.find(id);
    if (it == edgeIndexById_.end()) return nullptr;
    const auto& slot = edges_[it->second];
    return slot.relationship.has_value() ? &(*slot.relationship) : nullptr;
}

std::vector<KnowledgeObjectId> GraphEngine::neighbors(const KnowledgeObjectId& id,
                                                         std::optional<RelationshipType> type,
                                                         Direction direction) const {
    std::vector<KnowledgeObjectId> result;
    auto it = nodeIndexById_.find(id);
    if (it == nodeIndexById_.end()) return result;
    size_t nodeIndex = it->second;

    // Symmetric edges are indexed into both outgoing and incoming for
    // both endpoints, so Direction::Both must dedupe edge indices
    // before resolving neighbors, or a symmetric edge would be
    // reported twice.
    std::vector<size_t> candidateEdges;
    if (direction == Direction::Outgoing || direction == Direction::Both) {
        const auto& outgoing = outgoingEdgeIndices_[nodeIndex];
        candidateEdges.insert(candidateEdges.end(), outgoing.begin(), outgoing.end());
    }
    if (direction == Direction::Incoming || direction == Direction::Both) {
        const auto& incoming = incomingEdgeIndices_[nodeIndex];
        candidateEdges.insert(candidateEdges.end(), incoming.begin(), incoming.end());
    }
    std::sort(candidateEdges.begin(), candidateEdges.end());
    candidateEdges.erase(std::unique(candidateEdges.begin(), candidateEdges.end()),
                          candidateEdges.end());

    for (size_t edgeIdx : candidateEdges) {
        const auto& slot = edges_[edgeIdx];
        if (!slot.relationship.has_value()) continue;
        const auto& rel = *slot.relationship;
        if (type.has_value() && rel.type() != *type) continue;
        result.push_back(rel.sourceId() == id ? rel.targetId() : rel.sourceId());
    }
    return result;
}

std::vector<KnowledgeObjectId> GraphEngine::dependsOn(const KnowledgeObjectId& id) const {
    return neighbors(id, RelationshipType::DependsOn, Direction::Outgoing);
}

std::vector<KnowledgeObjectId> GraphEngine::usedBy(const KnowledgeObjectId& id) const {
    return neighbors(id, RelationshipType::DependsOn, Direction::Incoming);
}

namespace {

// Shared BFS core for transitiveDependencies/transitiveDependents -
// they differ only in which direction they walk (dependsOn vs.
// usedBy), so the traversal itself is factored out once rather than
// duplicated.
template <typename NeighborFn>
std::vector<KnowledgeObjectId> transitiveClosure(const KnowledgeObjectId& id,
                                                    NeighborFn neighborsOf) {
    std::vector<KnowledgeObjectId> result;
    std::unordered_set<KnowledgeObjectId> visited;
    std::queue<KnowledgeObjectId> frontier;
    frontier.push(id);
    visited.insert(id);

    while (!frontier.empty()) {
        auto current = frontier.front();
        frontier.pop();
        for (const auto& next : neighborsOf(current)) {
            if (visited.insert(next).second) {
                result.push_back(next);
                frontier.push(next);
            }
        }
    }
    return result;
}

}  // namespace

std::vector<KnowledgeObjectId> GraphEngine::transitiveDependencies(
    const KnowledgeObjectId& id) const {
    if (!nodeIndexById_.contains(id)) return {};
    return transitiveClosure(id, [this](const KnowledgeObjectId& current) {
        return dependsOn(current);
    });
}

std::vector<KnowledgeObjectId> GraphEngine::transitiveDependents(
    const KnowledgeObjectId& id) const {
    if (!nodeIndexById_.contains(id)) return {};
    return transitiveClosure(
        id, [this](const KnowledgeObjectId& current) { return usedBy(current); });
}

Result<std::vector<KnowledgeObjectId>, GraphError> GraphEngine::topologicalOrder() const {
    std::unordered_map<KnowledgeObjectId, int> inDegree;
    inDegree.reserve(nodeIndexById_.size());
    for (const auto& [id, index] : nodeIndexById_) {
        inDegree[id] = 0;
    }
    for (const auto& [id, index] : nodeIndexById_) {
        // "id depends on dep" means dep must be ordered before id -
        // i.e. id's in-degree is the number of dependencies it has.
        inDegree[id] = static_cast<int>(dependsOn(id).size());
    }

    std::queue<KnowledgeObjectId> ready;
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) ready.push(id);
    }

    std::vector<KnowledgeObjectId> order;
    order.reserve(nodeIndexById_.size());

    while (!ready.empty()) {
        auto current = ready.front();
        ready.pop();
        order.push_back(current);
        for (const auto& dependent : usedBy(current)) {
            if (--inDegree[dependent] == 0) {
                ready.push(dependent);
            }
        }
    }

    if (order.size() != nodeIndexById_.size()) {
        return Result<std::vector<KnowledgeObjectId>, GraphError>::err(GraphError::CycleDetected);
    }
    return Result<std::vector<KnowledgeObjectId>, GraphError>::ok(std::move(order));
}

Result<std::vector<KnowledgeObjectId>, GraphError> GraphEngine::learningRoadmapFor(
    const KnowledgeObjectId& id) const {
    using Out = Result<std::vector<KnowledgeObjectId>, GraphError>;
    if (!nodeIndexById_.contains(id)) return Out::err(GraphError::UnknownNode);

    auto scope = transitiveDependencies(id);
    scope.push_back(id);
    std::unordered_set<KnowledgeObjectId> inScope(scope.begin(), scope.end());
    std::unordered_map<KnowledgeObjectId, int> waitingOn;
    for (const auto& nodeId : scope) waitingOn[nodeId] = static_cast<int>(dependsOn(nodeId).size());

    auto byIdText = [](const KnowledgeObjectId& left, const KnowledgeObjectId& right) {
        return left.toString() < right.toString();
    };
    std::set<KnowledgeObjectId, decltype(byIdText)> ready(byIdText);
    for (const auto& [nodeId, count] : waitingOn) {
        if (count == 0) ready.insert(nodeId);
    }

    std::vector<KnowledgeObjectId> roadmap;
    roadmap.reserve(scope.size());
    while (!ready.empty()) {
        auto current = *ready.begin();
        ready.erase(ready.begin());
        roadmap.push_back(current);
        for (const auto& dependent : usedBy(current)) {
            if (inScope.contains(dependent) && --waitingOn[dependent] == 0) ready.insert(dependent);
        }
    }
    if (roadmap.size() != scope.size()) return Out::err(GraphError::CycleDetected);
    return Out::ok(std::move(roadmap));
}

std::vector<GraphEngine::ProjectSuggestion> GraphEngine::suggestProjects(
    const TopicId& topicId) const {
    std::vector<ProjectSuggestion> suggestions;

    for (const auto& [id, index] : nodeIndexById_) {
        const auto& slot = nodes_[index];
        if (!slot.object.has_value()) continue;
        const auto& object = *slot.object;

        if (!object.topicId().has_value() || !(*object.topicId() == topicId)) continue;
        if (object.miniProjects().empty()) continue;
        if (object.confidence() == atlas::core::ConfidenceLevel::Mastered) continue;

        auto prerequisites = dependsOn(id);
        double readiness = 1.0;
        if (!prerequisites.empty()) {
            int readyCount = 0;
            for (const auto& prereqId : prerequisites) {
                const auto* prereq = findNode(prereqId);
                if (prereq == nullptr) continue;
                if (prereq->confidence() == atlas::core::ConfidenceLevel::Confident ||
                    prereq->confidence() == atlas::core::ConfidenceLevel::Mastered) {
                    ++readyCount;
                }
            }
            readiness = static_cast<double>(readyCount) / static_cast<double>(prerequisites.size());
        }

        int leverage = static_cast<int>(transitiveDependents(id).size());

        suggestions.push_back(ProjectSuggestion{id, readiness, leverage});
    }

    std::sort(suggestions.begin(), suggestions.end(),
              [](const ProjectSuggestion& a, const ProjectSuggestion& b) {
                  double scoreA = a.readiness * (1.0 + a.leverage);
                  double scoreB = b.readiness * (1.0 + b.leverage);
                  if (scoreA != scoreB) return scoreA > scoreB;
                  // Deterministic tie-break - same reasoning as
                  // search()'s tie-break: nodeIndexById_ is a hash map,
                  // so without this, identical inputs could produce a
                  // different order across runs.
                  return a.conceptId.toString() < b.conceptId.toString();
              });

    return suggestions;
}

}  // namespace atlas::graph
