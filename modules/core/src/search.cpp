#include "atlas/core/search.hpp"

#include <algorithm>
#include <cctype>

namespace atlas::core {

namespace {

std::string toLowerAscii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool containsCaseInsensitive(std::string_view haystack, const std::string& lowerNeedle) {
    return toLowerAscii(haystack).find(lowerNeedle) != std::string::npos;
}

constexpr int kTitleWeight = 100;
constexpr int kDefinitionWeight = 40;
constexpr int kProblemSolvedWeight = 30;
constexpr int kWhyItExistsWeight = 30;
constexpr int kNotesWeight = 20;

}

std::optional<int> matchScore(const KnowledgeObject& object, std::string_view query) {
    if (query.empty()) return std::nullopt;

    std::string lowerQuery = toLowerAscii(query);
    int score = 0;

    if (containsCaseInsensitive(object.title(), lowerQuery)) score += kTitleWeight;
    if (containsCaseInsensitive(object.definition(), lowerQuery)) score += kDefinitionWeight;
    if (containsCaseInsensitive(object.problemSolved(), lowerQuery)) score += kProblemSolvedWeight;
    if (containsCaseInsensitive(object.whyItExists(), lowerQuery)) score += kWhyItExistsWeight;
    if (containsCaseInsensitive(object.notes(), lowerQuery)) score += kNotesWeight;

    if (score == 0) return std::nullopt;
    return score;
}

}
