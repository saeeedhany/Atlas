#pragma once

#include <unordered_map>
#include <unordered_set>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/graph/graph_engine.hpp"

namespace atlas::render {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct LayoutConfig {
    int iterations = 50;
    double idealEdgeLength = 80.0;
    double repulsionStrength = 4000.0;
    double dampening = 0.85;
    double maxDisplacementPerIteration = 50.0;
    double warmStartDisplacementPerIteration = 8.0;
    unsigned seed = 42;
};

struct LayoutHints {
    std::unordered_map<atlas::core::KnowledgeObjectId, Point2D> initial;
    std::unordered_set<atlas::core::KnowledgeObjectId> pinned;
};

class ForceDirectedLayout {
public:
    static std::unordered_map<atlas::core::KnowledgeObjectId, Point2D> compute(
        const atlas::graph::GraphEngine& graph, LayoutConfig config = {}, const LayoutHints& hints = {});
};

}
