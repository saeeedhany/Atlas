#include "atlas/persistence/placement_repository.hpp"

#include <sqlite3.h>

#include <utility>

#include "detail/statement.hpp"

namespace atlas::persistence {

using atlas::core::KnowledgeObjectId;
using atlas::core::Uuid;

PlacementRepository::PlacementRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> PlacementRepository::saveAll(const std::vector<Placement>& placements) {
    sqlite3* db = database_->handle();
    return detail::inTransaction(db, [&]() -> Result<void, PersistenceError> {
        for (const auto& placement : placements) {
            auto prepared = detail::Statement::prepare(db, R"sql(
                INSERT INTO node_placements (concept_id, x, y, pinned) VALUES (?, ?, ?, ?)
                ON CONFLICT(concept_id) DO UPDATE SET x = excluded.x, y = excluded.y, pinned = excluded.pinned;
            )sql");
            if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
            auto statement = std::move(prepared).value();
            statement.bindText(1, placement.conceptId.toString());
            statement.bindDouble(2, placement.x);
            statement.bindDouble(3, placement.y);
            statement.bindInt64(4, placement.pinned ? 1 : 0);
            auto step = statement.step();
            if (!step.hasValue()) return Result<void, PersistenceError>::err(step.error());
        }
        return Result<void, PersistenceError>::ok();
    });
}

Result<std::vector<Placement>, PersistenceError> PlacementRepository::findAll() {
    using Out = Result<std::vector<Placement>, PersistenceError>;
    auto prepared = detail::Statement::prepare(
        database_->handle(), "SELECT concept_id, x, y, pinned FROM node_placements ORDER BY concept_id;");
    if (!prepared.hasValue()) return Out::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();

    std::vector<Placement> placements;
    while (true) {
        auto step = statement.step();
        if (!step.hasValue()) return Out::err(step.error());
        if (!step.value()) break;
        auto id = Uuid::parse(statement.columnText(0));
        if (!id) {
            return Out::err(PersistenceError{PersistenceErrorCode::ConstraintViolation,
                                             "node_placements row " + statement.columnText(0) + " is malformed"});
        }
        placements.push_back(Placement{KnowledgeObjectId(*id), statement.columnDouble(1),
                                       statement.columnDouble(2), statement.columnInt64(3) != 0});
    }
    return Out::ok(std::move(placements));
}

}
