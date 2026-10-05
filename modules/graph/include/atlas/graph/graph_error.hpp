#pragma once

namespace atlas::graph {

enum class GraphError {
    UnknownNode,
    DuplicateNode,
    DuplicateEdge,
    CycleDetected,
};

}
