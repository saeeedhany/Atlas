#pragma once

#include "atlas/core/uuid.hpp"

namespace atlas::core {

// Wraps a Uuid with a phantom Tag type so IDs belonging to different
// entities (KnowledgeObjectId vs RelationshipId) are distinct types at
// compile time, even though they share an identical representation.
// One implementation, no per-entity boilerplate duplication.
template <typename Tag>
class StrongId {
public:
    explicit StrongId(Uuid value) : value_(value) {}

    static StrongId generate() { return StrongId(Uuid::generate()); }

    const Uuid& value() const { return value_; }
    std::string toString() const { return value_.toString(); }

    bool operator==(const StrongId&) const = default;

private:
    Uuid value_;
};

struct KnowledgeObjectTag {};
struct RelationshipTag {};

using KnowledgeObjectId = StrongId<KnowledgeObjectTag>;
using RelationshipId = StrongId<RelationshipTag>;

}  // namespace atlas::core

namespace std {
template <typename Tag>
struct hash<atlas::core::StrongId<Tag>> {
    size_t operator()(const atlas::core::StrongId<Tag>& id) const noexcept {
        return std::hash<atlas::core::Uuid>{}(id.value());
    }
};
}  // namespace std
