#pragma once

#include <optional>
#include <string_view>

namespace atlas::core {

enum class Difficulty {
    Beginner,
    Intermediate,
    Advanced,
    Expert,
};

enum class ConfidenceLevel {
    Unknown,
    Learning,
    Familiar,
    Confident,
    Mastered,
};

enum class RelationshipType {
    DependsOn,
    Uses,
    Implements,
    Solves,
    Contains,
    PartOf,
    RelatedTo,
    AlternativeTo,
    OppositeOf,
    Causes,
};

bool isSymmetric(RelationshipType type);

// Not named toString: doctest finds a free toString through ADL and breaks on string_view.
std::string_view toDisplayString(Difficulty value);
std::optional<Difficulty> difficultyFromString(std::string_view text);

std::string_view toDisplayString(ConfidenceLevel value);
std::optional<ConfidenceLevel> confidenceLevelFromString(std::string_view text);

std::string_view toDisplayString(RelationshipType value);
std::optional<RelationshipType> relationshipTypeFromString(std::string_view text);

}
