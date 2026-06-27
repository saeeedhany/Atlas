#pragma once

namespace atlas::graph {

enum class GraphError {
    UnknownNode,      // addEdge referenced a source/target id not present in the graph
    DuplicateNode,    // addNode called with an id already present
    DuplicateEdge,    // addEdge with an id already present, or a (source, target,
                      // type) pair already represented (including, for symmetric
                      // types, the same pair stored in the opposite order)
    CycleDetected,    // topologicalOrder() found a cycle in the DependsOn subgraph
};

}  // namespace atlas::graph
