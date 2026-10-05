#pragma once

#include <optional>
#include <string>

namespace atlas::core {

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

}
