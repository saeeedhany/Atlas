#pragma once

#include <unordered_map>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/graph/graph_engine.hpp"

namespace atlas::render {

// A 2D position. Deliberately not in atlas-core: a KnowledgeObject has
// no intrinsic position — position is a presentation artifact, computed
// fresh by layout and not persisted anywhere in the schema. Keeping it
// out of the domain model preserves the domain/presentation boundary.
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct LayoutConfig {
    int iterations = 50;
    double idealEdgeLength = 80.0;
    double repulsionStrength = 4000.0;
    // Multiplies displacement each iteration, starting near 1 and
    // implicitly cooling as iterations proceed (see implementation) —
    // standard simulated-annealing trick to help the layout settle
    // instead of oscillating forever.
    double dampening = 0.85;
    double maxDisplacementPerIteration = 50.0;
    unsigned seed = 42;  // fixed default: layout is reproducible, not random, run to run
};

// Force-directed (Fruchterman-Reingold-style) layout. Computed once
// per structural graph change, not continuously simulated every frame
// — see M4 design notes on why continuous simulation is the wrong
// algorithm at 10k+ nodes without spatial partitioning this milestone
// deliberately doesn't build yet.
//
// Pure function of the graph's current structure: no hidden state
// carried between calls. Calling this twice on an unchanged graph with
// the same config produces the same result (same seed, same math).
class ForceDirectedLayout {
public:
    static std::unordered_map<atlas::core::KnowledgeObjectId, Point2D> compute(
        const atlas::graph::GraphEngine& graph, LayoutConfig config = {});
};

}  // namespace atlas::render
