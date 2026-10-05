#pragma once

#include "atlas/core/uuid.hpp"

namespace atlas::core {

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
struct TopicTag {};

using KnowledgeObjectId = StrongId<KnowledgeObjectTag>;
using RelationshipId = StrongId<RelationshipTag>;
using TopicId = StrongId<TopicTag>;

}

namespace std {
template <typename Tag>
struct hash<atlas::core::StrongId<Tag>> {
    size_t operator()(const atlas::core::StrongId<Tag>& id) const noexcept {
        return std::hash<atlas::core::Uuid>{}(id.value());
    }
};
}
