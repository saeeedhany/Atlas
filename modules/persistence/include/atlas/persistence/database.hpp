#pragma once

#include <string>

#include "atlas/core/result.hpp"
#include "atlas/persistence/persistence_error.hpp"

struct sqlite3;

namespace atlas::persistence {

using atlas::core::Result;

class Database {
public:
    static Result<Database, PersistenceError> open(const std::string& path);

    ~Database();
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

private:
    friend class KnowledgeObjectRepository;
    friend class RelationshipRepository;
    friend class TopicRepository;
    friend class LearningRepository;
    friend class PlacementRepository;
    friend class NoteRepository;

    explicit Database(sqlite3* handle);
    sqlite3* handle() const { return handle_; }

    sqlite3* handle_ = nullptr;
};

}
