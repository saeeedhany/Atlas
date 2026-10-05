#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace atlas::core {

class Uuid {
public:
    static Uuid generate();
    static std::optional<Uuid> parse(std::string_view text);

    std::string toString() const;

    bool operator==(const Uuid&) const = default;

    const std::array<std::byte, 16>& bytes() const { return bytes_; }

private:
    std::array<std::byte, 16> bytes_{};
};

}

namespace std {
template <>
struct hash<atlas::core::Uuid> {
    size_t operator()(const atlas::core::Uuid& id) const noexcept;
};
}
