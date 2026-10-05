#pragma once

#include <string>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/core/result.hpp"
#include "atlas/core/uuid.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

struct NoteLink {
    std::string kind;
    atlas::core::Uuid targetId;

    bool operator==(const NoteLink&) const = default;
};

struct BoardNote {
    atlas::core::Uuid id;
    std::string body;
    std::string color;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    atlas::core::TimePoint createdAt;
    atlas::core::TimePoint updatedAt;
    std::vector<NoteLink> links{};
};

class NoteRepository {
public:
    explicit NoteRepository(Database& database);

    Result<void, PersistenceError> save(const BoardNote& note);
    Result<void, PersistenceError> remove(const atlas::core::Uuid& id);
    Result<void, PersistenceError> addLink(const atlas::core::Uuid& noteId, const NoteLink& link);
    Result<void, PersistenceError> removeLink(const atlas::core::Uuid& noteId, const NoteLink& link);
    Result<std::vector<BoardNote>, PersistenceError> findAll();

private:
    Database* database_;
};

}  // namespace atlas::persistence
