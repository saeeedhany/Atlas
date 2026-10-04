#include "atlas/render/force_directed_layout.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace atlas::render {

namespace {

using atlas::core::KnowledgeObjectId;

constexpr double kMinDistance = 0.01;

struct Edge {
    size_t a;
    size_t b;
};

struct CellKey {
    long long x;
    long long y;
    bool operator==(const CellKey& other) const { return x == other.x && y == other.y; }
};

struct CellKeyHash {
    size_t operator()(const CellKey& key) const noexcept {
        return std::hash<long long>{}(key.x) ^ (std::hash<long long>{}(key.y) << 1);
    }
};

CellKey cellOf(const Point2D& point, double cellSize) {
    return CellKey{static_cast<long long>(std::floor(point.x / cellSize)),
                   static_cast<long long>(std::floor(point.y / cellSize))};
}

std::vector<KnowledgeObjectId> sortedIds(const atlas::graph::GraphEngine& graph) {
    auto ids = graph.allNodeIds();
    std::sort(ids.begin(), ids.end(), [](const auto& a, const auto& b) { return a.toString() < b.toString(); });
    return ids;
}

std::vector<Point2D> startingPositions(const std::vector<KnowledgeObjectId>& ids, const std::vector<Edge>& edges,
                                       const LayoutHints& hints, const LayoutConfig& config) {
    const size_t n = ids.size();
    double side = std::sqrt(static_cast<double>(n)) * config.idealEdgeLength;
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<double> anywhere(-side / 2.0, side / 2.0);
    std::uniform_real_distribution<double> nearby(-config.idealEdgeLength / 2.0, config.idealEdgeLength / 2.0);

    std::vector<Point2D> positions(n);
    std::vector<bool> known(n, false);
    for (size_t i = 0; i < n; ++i) {
        Point2D random{anywhere(rng), anywhere(rng)};
        auto hint = hints.initial.find(ids[i]);
        known[i] = hint != hints.initial.end();
        positions[i] = known[i] ? hint->second : random;
    }

    for (size_t i = 0; i < n; ++i) {
        if (known[i]) continue;
        Point2D sum;
        int count = 0;
        for (const auto& edge : edges) {
            size_t other = edge.a == i ? edge.b : (edge.b == i ? edge.a : n);
            if (other == n || !known[other]) continue;
            sum.x += positions[other].x;
            sum.y += positions[other].y;
            ++count;
        }
        if (count > 0) positions[i] = Point2D{sum.x / count + nearby(rng), sum.y / count + nearby(rng)};
    }
    return positions;
}

}  // namespace

std::unordered_map<KnowledgeObjectId, Point2D> ForceDirectedLayout::compute(
    const atlas::graph::GraphEngine& graph, LayoutConfig config, const LayoutHints& hints) {
    auto ids = sortedIds(graph);
    std::unordered_map<KnowledgeObjectId, Point2D> result;
    if (ids.empty()) return result;
    const size_t n = ids.size();

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

    auto positions = startingPositions(ids, edges, hints, config);
    std::vector<bool> pinned(n, false);
    for (size_t i = 0; i < n; ++i) pinned[i] = hints.pinned.contains(ids[i]);

    std::vector<Point2D> displacement(n);
    double temperature = hints.initial.empty() ? config.maxDisplacementPerIteration
                                               : config.warmStartDisplacementPerIteration;
    double cellSize = std::max(config.idealEdgeLength * 2.0, kMinDistance);

    for (int iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(displacement.begin(), displacement.end(), Point2D{0.0, 0.0});

        std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> grid;
        for (size_t i = 0; i < n; ++i) grid[cellOf(positions[i], cellSize)].push_back(i);

        for (size_t i = 0; i < n; ++i) {
            CellKey home = cellOf(positions[i], cellSize);
            for (long long dx = -1; dx <= 1; ++dx) {
                for (long long dy = -1; dy <= 1; ++dy) {
                    auto it = grid.find(CellKey{home.x + dx, home.y + dy});
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
            displacement[edge.a].x -= (dx / d) * force;
            displacement[edge.a].y -= (dy / d) * force;
        }

        for (size_t i = 0; i < n; ++i) {
            if (pinned[i]) continue;
            double magnitude = std::max(
                std::sqrt(displacement[i].x * displacement[i].x + displacement[i].y * displacement[i].y),
                kMinDistance);
            double step = std::min(magnitude, temperature);
            positions[i].x += (displacement[i].x / magnitude) * step;
            positions[i].y += (displacement[i].y / magnitude) * step;
        }

        temperature *= config.dampening;
    }

    result.reserve(n);
    for (size_t i = 0; i < n; ++i) result[ids[i]] = positions[i];
    return result;
}

}  // namespace atlas::render
