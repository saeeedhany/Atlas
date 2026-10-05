#pragma once

#include <algorithm>

namespace atlas::render {

inline constexpr float kNodeRadius = 5.5f;
inline constexpr float kNodeHalo = 1.5f;
inline constexpr float kDegreeGrowth = 0.5f;
inline constexpr int kMaxDegreeGrowth = 3;
inline constexpr float kMemoryRingRadius = 9.5f;
inline constexpr float kMemoryRingThickness = 1.6f;
inline constexpr float kSelectRingRadius = 13.5f;
inline constexpr float kSelectRingThickness = 2.0f;
inline constexpr float kNeighborRingRadius = 12.5f;
inline constexpr float kHoverRingRadius = 12.5f;
inline constexpr float kHighlightThickness = 1.5f;
inline constexpr float kHintRingRadius = 16.0f;
inline constexpr float kLinkGap = 1.0f;

constexpr float dotRadius(int degree) {
    return kNodeRadius + static_cast<float>(std::clamp(degree, 0, kMaxDegreeGrowth)) * kDegreeGrowth;
}

constexpr float linkTrim() { return kSelectRingRadius + kSelectRingThickness / 2 + kLinkGap; }

}  // namespace atlas::render
