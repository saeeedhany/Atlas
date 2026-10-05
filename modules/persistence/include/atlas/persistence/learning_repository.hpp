#pragma once

#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/core/result.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::MemoryState;
using atlas::core::ReviewEvent;

class LearningRepository {
public:
    explicit LearningRepository(Database& database);

    Result<void, PersistenceError> record(const std::vector<ReviewEvent>& events,
                                          const std::vector<MemoryState>& states);
    Result<std::vector<ReviewEvent>, PersistenceError> allEvents();
    Result<std::vector<MemoryState>, PersistenceError> allStates();
    Result<void, PersistenceError> replaceStates(const std::vector<MemoryState>& states, int replayVersion);
    Result<bool, PersistenceError> isCacheFresh(int replayVersion);

private:
    Database* database_;
};

}
