#include "atlas/core/uuid.hpp"

#include <array>
#include <cstring>
#include <random>

namespace atlas::core {

namespace {

std::byte hexNibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<std::byte>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<std::byte>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<std::byte>(c - 'A' + 10);
    return std::byte{0xFF};  // sentinel for "invalid"
}

}  // namespace

Uuid Uuid::generate() {
    static thread_local std::mt19937_64 engine{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;

    Uuid id;
    uint64_t high = dist(engine);
    uint64_t low = dist(engine);
    std::memcpy(id.bytes_.data(), &high, 8);
    std::memcpy(id.bytes_.data() + 8, &low, 8);

    // RFC 4122 version 4 / variant bits, so a parsed-back string is
    // indistinguishable from a UUID produced by any other v4 generator.
    id.bytes_[6] = static_cast<std::byte>((static_cast<uint8_t>(id.bytes_[6]) & 0x0F) | 0x40);
    id.bytes_[8] = static_cast<std::byte>((static_cast<uint8_t>(id.bytes_[8]) & 0x3F) | 0x80);

    return id;
}

std::optional<Uuid> Uuid::parse(std::string_view text) {
    static constexpr size_t kExpectedLength = 36;
    static constexpr std::array<size_t, 4> kDashPositions = {8, 13, 18, 23};

    if (text.size() != kExpectedLength) return std::nullopt;
    for (size_t pos : kDashPositions) {
        if (text[pos] != '-') return std::nullopt;
    }

    Uuid id;
    size_t byteIndex = 0;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '-') {
            ++i;
            continue;
        }
        if (i + 1 >= text.size()) return std::nullopt;
        std::byte high = hexNibble(text[i]);
        std::byte low = hexNibble(text[i + 1]);
        if (high == std::byte{0xFF} || low == std::byte{0xFF}) return std::nullopt;
        id.bytes_[byteIndex++] =
            static_cast<std::byte>((static_cast<uint8_t>(high) << 4) | static_cast<uint8_t>(low));
        i += 2;
    }
    if (byteIndex != 16) return std::nullopt;
    return id;
}

std::string Uuid::toString() const {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    for (size_t i = 0; i < bytes_.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        uint8_t byte = static_cast<uint8_t>(bytes_[i]);
        out.push_back(kHex[byte >> 4]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

}  // namespace atlas::core

size_t std::hash<atlas::core::Uuid>::operator()(const atlas::core::Uuid& id) const noexcept {
    // FNV-1a over the 16 raw bytes — simple, fast, good enough for an
    // in-memory hash map key. Not used for any security purpose.
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = 14695981039346656037ull;
    for (std::byte b : id.bytes()) {
        hash ^= static_cast<uint8_t>(b);
        hash *= kPrime;
    }
    return static_cast<size_t>(hash);
}
