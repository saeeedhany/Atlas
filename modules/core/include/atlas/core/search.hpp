#pragma once

#include <optional>
#include <string_view>

#include "atlas/core/knowledge_object.hpp"

namespace atlas::core {

// Case-insensitive substring search across a KnowledgeObject's main
// text fields, returning a relevance score (higher ranks first) or
// std::nullopt if the query doesn't appear anywhere. ASCII case
// folding only - no Unicode-aware case mapping, consistent with the
// rest of this codebase not having dealt with internationalization at
// all yet.
//
// An empty query is the caller's concern, not this function's: it
// always returns nullopt for an empty query (an empty substring
// trivially "matches" everything in std::string::find, which is not
// the behavior callers actually want - see GraphEngine::search for how
// "show everything, unranked" is handled instead).
//
// Deliberately scoped to Title/Definition/Problem Solved/Why It
// Exists/Notes for v1 - Examples/MiniProjects/References aren't
// searched yet. Those are comparatively rarely where the
// differentiating text lives; expand this if there's real evidence
// people need it searched too.
std::optional<int> matchScore(const KnowledgeObject& object, std::string_view query);

}  // namespace atlas::core
