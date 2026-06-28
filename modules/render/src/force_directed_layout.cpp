#include "atlas/render/force_directed_layout.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

namespace atlas::render {

namespace {

constexpr double kMinDistance = 0.01;  // avoids division blow-up when two nodes coincide

struct Edge {
    size_t a;
    size_t b;
};

// Integer grid-cell coordinates, for the uniform-grid (cell-list)
// repulsion approximation below.
struct CellKey {
    long long x;
    long long y;
    bool operator==(const CellKey& other) const { return x == other.x && y == other.y; }
};

struct CellKeyHash {
    size_t operator()(const CellKey& key) const noexcept {
        // Simple, fast, sufficient for a hash map key — not used for
        // anything security-sensitive.
        return std::hash<long long>{}(key.x) ^ (std::hash<long long>{}(key.y) << 1);
    }
};

}  // namespace

std::unordered_map<atlas::core::KnowledgeObjectId, Point2D> ForceDirectedLayout::compute(
    const atlas::graph::GraphEngine& graph, LayoutConfig config) {
    using atlas::core::KnowledgeObjectId;

    auto ids = graph.allNodeIds();
    std::unordered_map<KnowledgeObjectId, Point2D> result;
    if (ids.empty()) return result;

    const size_t n = ids.size();

    // One-time setup: every UUID hash lookup happens here, exactly
    // once per node/edge. The simulation loop below touches dense
    // arrays (and, for repulsion, a per-iteration grid rebuilt from
    // dense data) — no per-node-pair UUID hashing in the hot path.
    std::unordered_map<KnowledgeObjectId, size_t> indexOf;
    indexOf.reserve(n);
    for (size_t i = 0; i < n; ++i) indexOf[ids[i]] = i;

    std::vector<Edge> edges;
    for (size_t i = 0; i < n; ++i) {
        for (const auto& neighborId :
             graph.neighbors(ids[i], std::nullopt, atlas::graph::GraphEngine::Direction::Outgoing)) {
            edges.push_back(Edge{i, indexOf.at(neighborId)});
        }
    }

    double area = static_cast<double>(n) * config.idealEdgeLength * config.idealEdgeLength;
    double side = std::sqrt(area);
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<double> dist(-side / 2.0, side / 2.0);

    std::vector<Point2D> positions(n);
    for (size_t i = 0; i < n; ++i) positions[i] = Point2D{dist(rng), dist(rng)};

    std::vector<Point2D> displacement(n);
    double temperature = config.maxDisplacementPerIteration;

    // Repulsion decays as 1/distance^2, so contributions beyond a few
    // cell-widths are already negligible — this cell size and 3x3
    // neighborhood is the standard cell-list approximation used by
    // real force-directed layout implementations (the same idea
    // Barnes-Hut generalizes with a quadtree; a uniform grid is a
    // much simpler structure that does the same job here since the
    // graph isn't astronomically large). Found necessary, not
    // speculative: the naive O(n^2) version measured at 10.8 seconds
    // for one layout pass at 10,000 nodes — far too slow to feel like
    // part of an interactive app, even as a one-time computation.
    double cellSize = std::max(config.idealEdgeLength * 2.0, kMinDistance);

    for (int iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(displacement.begin(), displacement.end(), Point2D{0.0, 0.0});

        std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> grid;
        for (size_t i = 0; i < n; ++i) {
            CellKey key{static_cast<long long>(std::floor(positions[i].x / cellSize)),
                        static_cast<long long>(std::floor(positions[i].y / cellSize))};
            grid[key].push_back(i);
        }

        for (size_t i = 0; i < n; ++i) {
            long long cx = static_cast<long long>(std::floor(positions[i].x / cellSize));
            long long cy = static_cast<long long>(std::floor(positions[i].y / cellSize));
            for (long long dx = -1; dx <= 1; ++dx) {
                for (long long dy = -1; dy <= 1; ++dy) {
                    auto it = grid.find(CellKey{cx + dx, cy + dy});
                    if (it == grid.end()) continue;
                    for (size_t j : it->second) {
                        if (j == i) continue;
                        double ddx = positions[i].x - positions[j].x;
                        double ddy = positions[i].y - positions[j].y;
                        double d = std::max(std::sqrt(ddx * ddx + ddy * ddy), kMinDistance);
                        double force = config.repulsionStrength / (d * d);
                        displacement[i].x += (ddx / d) * force;
                        displacement[i].y += (ddy / d) * force;
                    }
                }
            }
        }

        for (const auto& edge : edges) {
            double dx = positions[edge.a].x - positions[edge.b].x;
            double dy = positions[edge.a].y - positions[edge.b].y;
            double d = std::max(std::sqrt(dx * dx + dy * dy), kMinDistance);
            double force = (d * d) / config.idealEdgeLength;
            double fx = (dx / d) * force;
            double fy = (dy / d) * force;
            displacement[edge.a].x -= fx;
            displacement[edge.a].y -= fy;
        }

        for (size_t i = 0; i < n; ++i) {
            double mag = std::max(
                std::sqrt(displacement[i].x * displacement[i].x + displacement[i].y * displacement[i].y),
                kMinDistance);
            double capped = std::min(mag, temperature);
            positions[i].x += (displacement[i].x / mag) * capped;
            positions[i].y += (displacement[i].y / mag) * capped;
        }

        temperature *= config.dampening;
    }

    result.reserve(n);
    for (size_t i = 0; i < n; ++i) result[ids[i]] = positions[i];
    return result;
}

}  // namespace atlas::render
