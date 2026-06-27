#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace atlas::core {

// A 128-bit UUID (version 4, randomly generated).
//
// Hand-rolled rather than pulling in a dependency: atlas-core has zero
// external dependencies by design (see M0 requirements, FR5). UUIDs
// were chosen over sequential integers specifically so KnowledgeObjects
// and Relationships created in different sessions, imports, or plugins
// never collide when merged.
class Uuid {
public:
    static Uuid generate();
    static std::optional<Uuid> parse(std::string_view text);

    std::string toString() const;

    bool operator==(const Uuid&) const = default;

    // Exposed for hashing (e.g. std::unordered_map keys in atlas-graph).
    const std::array<std::byte, 16>& bytes() const { return bytes_; }

private:
    std::array<std::byte, 16> bytes_{};
};

}  // namespace atlas::core

namespace std {
template <>
struct hash<atlas::core::Uuid> {
    size_t operator()(const atlas::core::Uuid& id) const noexcept;
};
}  // namespace std
