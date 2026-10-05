#pragma once

#include <optional>
#include <vector>

#include "atlas/core/result.hpp"
#include "atlas/core/topic.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::Result;
using atlas::core::Topic;
using atlas::core::TopicId;

class TopicRepository {
public:
    explicit TopicRepository(Database& database);

    Result<void, PersistenceError> save(const Topic& topic);

    Result<std::optional<Topic>, PersistenceError> findById(const TopicId& id);

    Result<std::vector<Topic>, PersistenceError> findAll();

    Result<void, PersistenceError> remove(const TopicId& id);

private:
    Database* database_;
};

}
