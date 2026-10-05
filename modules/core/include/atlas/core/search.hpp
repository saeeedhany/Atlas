#pragma once

#include <optional>
#include <string_view>

#include "atlas/core/knowledge_object.hpp"

namespace atlas::core {

std::optional<int> matchScore(const KnowledgeObject& object, std::string_view query);

}
