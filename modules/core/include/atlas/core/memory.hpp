#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "atlas/core/strong_id.hpp"
#include "atlas/core/uuid.hpp"

namespace atlas::core {

using TimePoint = std::chrono::system_clock::time_point;

enum class ItemKind { Concept, Link };
enum class Grade { Again = 1, Hard = 2, Good = 3, Easy = 4 };
enum class Certainty { Guess = 1, Unsure = 2, FairlySure = 3, Certain = 4 };
enum class Exercise { Rebuild, Explain };
enum class Phase { New, Learning, Review, Relearning };

struct ItemRef {
    ItemKind kind = ItemKind::Concept;
    Uuid id;

    static ItemRef forConcept(const KnowledgeObjectId& conceptId) {
        return ItemRef{ItemKind::Concept, conceptId.value()};
    }
    static ItemRef forLink(const RelationshipId& linkId) {
        return ItemRef{ItemKind::Link, linkId.value()};
    }

    bool operator==(const ItemRef&) const = default;
};

struct ReviewEvent {
    Uuid id;
    ItemRef item;
    Uuid sessionId;
    std::string deviceId;
    TimePoint reviewedAt;
    double elapsedDays = 0.0;
    Exercise exercise = Exercise::Rebuild;
    Certainty predicted = Certainty::Unsure;
    Grade grade = Grade::Good;
    int hintsUsed = 0;
    std::optional<KnowledgeObjectId> wrongTarget;
    std::chrono::milliseconds responseTime{0};
};

struct MemoryState {
    ItemRef item;
    Phase phase = Phase::New;
    double stability = 0.0;
    double difficulty = 0.0;
    std::optional<TimePoint> lastReviewedAt;
    std::optional<TimePoint> dueAt;
    int reviewCount = 0;
    int lapseCount = 0;

    bool operator==(const MemoryState&) const = default;
};

struct Placement {
    KnowledgeObjectId conceptId;
    double x = 0.0;
    double y = 0.0;
    bool pinned = false;
};

std::string_view toStorageString(ItemKind kind);
std::optional<ItemKind> itemKindFromString(std::string_view text);

std::string_view toStorageString(Exercise exercise);
std::optional<Exercise> exerciseFromString(std::string_view text);

std::string_view toStorageString(Phase phase);
std::optional<Phase> phaseFromString(std::string_view text);

std::optional<Grade> gradeFromInt(int64_t value);
std::optional<Certainty> certaintyFromInt(int64_t value);

}  // namespace atlas::core

namespace std {
template <>
struct hash<atlas::core::ItemRef> {
    size_t operator()(const atlas::core::ItemRef& item) const noexcept {
        return std::hash<atlas::core::Uuid>{}(item.id) ^ static_cast<size_t>(item.kind);
    }
};
}  // namespace std
