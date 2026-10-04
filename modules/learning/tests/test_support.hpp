#pragma once

#include <chrono>
#include <optional>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/memory.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/graph/graph_engine.hpp"
#include "doctest.h"

namespace atlas::learning::testing {

using namespace atlas::core;
using atlas::graph::GraphEngine;

inline TimePoint day(double days) {
    auto offset = std::chrono::duration<double, std::ratio<86400>>(20000.0 + days);
    return TimePoint{} + std::chrono::duration_cast<std::chrono::system_clock::duration>(offset);
}

inline KnowledgeObjectId addConcept(GraphEngine& graph, const char* title,
                                    std::optional<TopicId> topic = std::nullopt) {
    auto object = KnowledgeObject::create(title).value();
    if (topic) object.assignToTopic(*topic);
    auto id = object.id();
    REQUIRE(graph.addNode(std::move(object)).hasValue());
    return id;
}

inline RelationshipId addLink(GraphEngine& graph, const KnowledgeObjectId& source,
                              const KnowledgeObjectId& target, RelationshipType type) {
    auto link = Relationship::create(source, target, type).value();
    auto id = link.id();
    REQUIRE(graph.addEdge(std::move(link)).hasValue());
    return id;
}

inline ReviewEvent event(const ItemRef& item, TimePoint at, Grade grade,
                         Exercise exercise = Exercise::Rebuild,
                         Certainty predicted = Certainty::Unsure,
                         std::chrono::milliseconds response = std::chrono::milliseconds(10000)) {
    ReviewEvent result;
    result.id = Uuid::generate();
    result.item = item;
    result.sessionId = Uuid::generate();
    result.deviceId = "test";
    result.reviewedAt = at;
    result.exercise = exercise;
    result.predicted = predicted;
    result.grade = grade;
    result.responseTime = response;
    return result;
}

}  // namespace atlas::learning::testing
