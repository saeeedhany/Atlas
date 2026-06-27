#pragma once

#include <optional>
#include <string>

namespace atlas::core {

// Deliberately plain content types, kept as structs rather than raw
// strings so the UI can render each as a distinct, structured piece of
// content, and so future fields (e.g. a "completed" flag on
// MiniProject) don't require a breaking schema change.

struct Example {
    std::string description;
    std::optional<std::string> snippet;
};

struct MiniProject {
    std::string title;
    std::string description;
};

struct Reference {
    std::string title;
    std::optional<std::string> url;
};

}  // namespace atlas::core
