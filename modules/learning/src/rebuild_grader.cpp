#include "atlas/learning/rebuild_grader.hpp"

#include <algorithm>

#include "atlas/learning/network_rules.hpp"

namespace atlas::learning {

using atlas::core::Exercise;

namespace {

constexpr size_t kMinRebuildsForEasy = 20;

KnowledgeObjectId otherEnd(const Relationship& link, const KnowledgeObjectId& focus) {
    return link.sourceId() == focus ? link.targetId() : link.sourceId();
}

bool contains(const std::vector<KnowledgeObjectId>& ids, const KnowledgeObjectId& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool sharesNeighbor(const GraphEngine& graph, const KnowledgeObjectId& a, const KnowledgeObjectId& b,
                    const KnowledgeObjectId& excluded) {
    auto neighborsOfB = graph.neighbors(b, std::nullopt, GraphEngine::Direction::Both);
    for (const auto& neighbor : graph.neighbors(a, std::nullopt, GraphEngine::Direction::Both)) {
        if (neighbor != excluded && contains(neighborsOfB, neighbor)) return true;
    }
    return false;
}

Grade gradeMatch(const RebuildAnswer& answer, const Relationship& link, const RecalledLink& recalled,
                 bool hinted, std::optional<std::chrono::milliseconds> medianResponse) {
    bool typeRight = recalled.type == link.type();
    bool directionRight = atlas::core::isSymmetric(link.type()) ||
                          recalled.focusIsSource == (link.sourceId() == answer.focus);
    if (hinted || !typeRight || !directionRight) return Grade::Hard;
    bool fast = medianResponse.has_value() && answer.responseTime < *medianResponse;
    return answer.predicted == Certainty::Certain && fast ? Grade::Easy : Grade::Good;
}

}  // namespace

std::optional<std::chrono::milliseconds> medianRebuildResponse(const std::vector<ReviewEvent>& events) {
    std::vector<std::chrono::milliseconds> times;
    for (const auto& event : events) {
        if (event.exercise == Exercise::Rebuild) times.push_back(event.responseTime);
    }
    if (times.size() < kMinRebuildsForEasy) return std::nullopt;
    auto middle = times.begin() + static_cast<long>(times.size() / 2);
    std::nth_element(times.begin(), middle, times.end());
    return *middle;
}

RebuildGrader::RebuildGrader(const GraphEngine& graph) : graph_(&graph) {}

std::vector<GradedLink> RebuildGrader::grade(const RebuildAnswer& answer,
                                             const std::vector<RelationshipId>& hidden,
                                             std::optional<std::chrono::milliseconds> medianResponse) const {
    std::vector<const Relationship*> links;
    std::vector<KnowledgeObjectId> targets;
    for (const auto& id : hidden) {
        const Relationship* link = graph_->findEdge(id);
        if (link == nullptr) continue;
        links.push_back(link);
        targets.push_back(otherEnd(*link, answer.focus));
    }

    std::vector<std::optional<size_t>> matches(links.size());
    std::vector<bool> used(answer.recalled.size(), false);
    for (bool requireType : {true, false}) {
        for (size_t h = 0; h < links.size(); ++h) {
            if (matches[h]) continue;
            for (size_t i = 0; i < answer.recalled.size(); ++i) {
                const auto& recalled = answer.recalled[i];
                if (used[i] || recalled.other != targets[h]) continue;
                if (requireType && recalled.type != links[h]->type()) continue;
                matches[h] = i;
                used[i] = true;
                break;
            }
        }
    }

    std::vector<GradedLink> graded;
    for (size_t h = 0; h < links.size(); ++h) {
        bool hinted = contains(answer.hinted, targets[h]);
        GradedLink result{links[h]->id(), Grade::Again, hinted ? 1 : 0, std::nullopt};
        if (matches[h]) {
            result.grade = gradeMatch(answer, *links[h], answer.recalled[*matches[h]], hinted, medianResponse);
        }
        graded.push_back(result);
    }

    for (size_t i = 0; i < answer.recalled.size(); ++i) {
        const auto& named = answer.recalled[i].other;
        if (!used[i] && !contains(targets, named)) attachWrongTarget(graded, targets, named, answer.focus);
    }
    return graded;
}

void RebuildGrader::attachWrongTarget(std::vector<GradedLink>& graded,
                                      const std::vector<KnowledgeObjectId>& targets,
                                      const KnowledgeObjectId& wrong, const KnowledgeObjectId& focus) const {
    auto partners = contrastPartners(*graph_, wrong);
    GradedLink* byNeighbor = nullptr;
    for (size_t h = 0; h < graded.size(); ++h) {
        auto& candidate = graded[h];
        if (candidate.grade != Grade::Again || candidate.wrongTarget) continue;
        if (contains(partners, targets[h])) {
            candidate.wrongTarget = wrong;
            return;
        }
        if (byNeighbor == nullptr && sharesNeighbor(*graph_, targets[h], wrong, focus)) {
            byNeighbor = &candidate;
        }
    }
    if (byNeighbor != nullptr) byNeighbor->wrongTarget = wrong;
}

}  // namespace atlas::learning
