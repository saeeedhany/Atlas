#pragma once

#include <optional>
#include <string_view>

namespace atlas::core {

// Intrinsic difficulty of a concept — a property of the concept itself,
// independent of any one user's mastery of it.
enum class Difficulty {
    Beginner,
    Intermediate,
    Advanced,
    Expert,
};

// A user's self-assessed mastery of a concept — independent of the
// concept's intrinsic Difficulty. Two separate axes on purpose: a
// Beginner-difficulty concept can still have low Confidence if it was
// never practiced.
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

// Symmetric types describe a mutual relationship (if A is RelatedTo B,
// B is equally RelatedTo A). Directional types do not. The graph engine
// (M2) uses this to index a single stored edge in both directions,
// rather than the storage layer duplicating it as two rows.
bool isSymmetric(RelationshipType type);

// Canonical string names for storage and display. Deliberately named
// strings rather than integer ordinals: a future reordering of an enum
// (an innocuous-looking refactor) would silently change the meaning of
// every existing stored row if we used ordinals. Renaming a value is a
// visible, deliberate action and is the only thing that can break this.
//
// Named toDisplayString rather than toString on purpose: doctest (our
// test framework) auto-detects a free function named exactly toString
// via ADL for printing values in assertion failures, and got confused
// by the std::string_view return. Don't rename this back to toString.
std::string_view toDisplayString(Difficulty value);
std::optional<Difficulty> difficultyFromString(std::string_view text);

std::string_view toDisplayString(ConfidenceLevel value);
std::optional<ConfidenceLevel> confidenceLevelFromString(std::string_view text);

std::string_view toDisplayString(RelationshipType value);
std::optional<RelationshipType> relationshipTypeFromString(std::string_view text);

}  // namespace atlas::core
