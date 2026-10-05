#include "atlas/core/memory.hpp"

namespace atlas::core {

std::string_view toStorageString(ItemKind kind) {
    return kind == ItemKind::Concept ? "concept" : "link";
}

std::optional<ItemKind> itemKindFromString(std::string_view text) {
    if (text == "concept") return ItemKind::Concept;
    if (text == "link") return ItemKind::Link;
    return std::nullopt;
}

std::string_view toStorageString(Exercise exercise) {
    return exercise == Exercise::Rebuild ? "rebuild" : "explain";
}

std::optional<Exercise> exerciseFromString(std::string_view text) {
    if (text == "rebuild") return Exercise::Rebuild;
    if (text == "explain") return Exercise::Explain;
    return std::nullopt;
}

std::string_view toStorageString(Phase phase) {
    switch (phase) {
        case Phase::New: return "new";
        case Phase::Learning: return "learning";
        case Phase::Review: return "review";
        case Phase::Relearning: return "relearning";
    }
    return "new";
}

std::optional<Phase> phaseFromString(std::string_view text) {
    if (text == "new") return Phase::New;
    if (text == "learning") return Phase::Learning;
    if (text == "review") return Phase::Review;
    if (text == "relearning") return Phase::Relearning;
    return std::nullopt;
}

std::optional<Grade> gradeFromInt(int64_t value) {
    if (value < 1 || value > 4) return std::nullopt;
    return static_cast<Grade>(value);
}

std::optional<Certainty> certaintyFromInt(int64_t value) {
    if (value < 1 || value > 4) return std::nullopt;
    return static_cast<Certainty>(value);
}

}
