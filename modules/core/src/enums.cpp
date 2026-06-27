#include "atlas/core/enums.hpp"

namespace atlas::core {

bool isSymmetric(RelationshipType type) {
    switch (type) {
        case RelationshipType::RelatedTo:
        case RelationshipType::AlternativeTo:
        case RelationshipType::OppositeOf:
            return true;
        case RelationshipType::DependsOn:
        case RelationshipType::Uses:
        case RelationshipType::Implements:
        case RelationshipType::Solves:
        case RelationshipType::Contains:
        case RelationshipType::PartOf:
        case RelationshipType::Causes:
            return false;
    }
    return false;  // unreachable; silences -Wreturn-type on some compilers
}

std::string_view toDisplayString(Difficulty value) {
    switch (value) {
        case Difficulty::Beginner: return "Beginner";
        case Difficulty::Intermediate: return "Intermediate";
        case Difficulty::Advanced: return "Advanced";
        case Difficulty::Expert: return "Expert";
    }
    return "Beginner";  // unreachable
}

std::optional<Difficulty> difficultyFromString(std::string_view text) {
    if (text == "Beginner") return Difficulty::Beginner;
    if (text == "Intermediate") return Difficulty::Intermediate;
    if (text == "Advanced") return Difficulty::Advanced;
    if (text == "Expert") return Difficulty::Expert;
    return std::nullopt;
}

std::string_view toDisplayString(ConfidenceLevel value) {
    switch (value) {
        case ConfidenceLevel::Unknown: return "Unknown";
        case ConfidenceLevel::Learning: return "Learning";
        case ConfidenceLevel::Familiar: return "Familiar";
        case ConfidenceLevel::Confident: return "Confident";
        case ConfidenceLevel::Mastered: return "Mastered";
    }
    return "Unknown";  // unreachable
}

std::optional<ConfidenceLevel> confidenceLevelFromString(std::string_view text) {
    if (text == "Unknown") return ConfidenceLevel::Unknown;
    if (text == "Learning") return ConfidenceLevel::Learning;
    if (text == "Familiar") return ConfidenceLevel::Familiar;
    if (text == "Confident") return ConfidenceLevel::Confident;
    if (text == "Mastered") return ConfidenceLevel::Mastered;
    return std::nullopt;
}

std::string_view toDisplayString(RelationshipType value) {
    switch (value) {
        case RelationshipType::DependsOn: return "DependsOn";
        case RelationshipType::Uses: return "Uses";
        case RelationshipType::Implements: return "Implements";
        case RelationshipType::Solves: return "Solves";
        case RelationshipType::Contains: return "Contains";
        case RelationshipType::PartOf: return "PartOf";
        case RelationshipType::RelatedTo: return "RelatedTo";
        case RelationshipType::AlternativeTo: return "AlternativeTo";
        case RelationshipType::OppositeOf: return "OppositeOf";
        case RelationshipType::Causes: return "Causes";
    }
    return "RelatedTo";  // unreachable
}

std::optional<RelationshipType> relationshipTypeFromString(std::string_view text) {
    if (text == "DependsOn") return RelationshipType::DependsOn;
    if (text == "Uses") return RelationshipType::Uses;
    if (text == "Implements") return RelationshipType::Implements;
    if (text == "Solves") return RelationshipType::Solves;
    if (text == "Contains") return RelationshipType::Contains;
    if (text == "PartOf") return RelationshipType::PartOf;
    if (text == "RelatedTo") return RelationshipType::RelatedTo;
    if (text == "AlternativeTo") return RelationshipType::AlternativeTo;
    if (text == "OppositeOf") return RelationshipType::OppositeOf;
    if (text == "Causes") return RelationshipType::Causes;
    return std::nullopt;
}

}  // namespace atlas::core
