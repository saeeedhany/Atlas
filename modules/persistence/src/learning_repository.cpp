#include "atlas/persistence/learning_repository.hpp"

#include <sqlite3.h>

#include <cmath>
#include <string>
#include <utility>

#include "detail/statement.hpp"

namespace atlas::persistence {

using atlas::core::ItemRef;
using atlas::core::KnowledgeObjectId;
using atlas::core::TimePoint;
using atlas::core::Uuid;

namespace {

constexpr const char* kReplayVersionKey = "replay_version";
constexpr const char* kLastEventKey = "last_event_id";

int64_t toMillis(TimePoint time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

TimePoint fromMillis(int64_t millis) { return TimePoint(std::chrono::milliseconds(millis)); }

std::optional<int64_t> toOptionalMillis(const std::optional<TimePoint>& time) {
    if (!time) return std::nullopt;
    return toMillis(*time);
}

std::optional<TimePoint> fromOptionalMillis(std::optional<int64_t> millis) {
    if (!millis) return std::nullopt;
    return fromMillis(*millis);
}

PersistenceError malformed(const std::string& what) {
    return PersistenceError{PersistenceErrorCode::ConstraintViolation, what + " is malformed"};
}

Result<void, PersistenceError> run(detail::Statement& statement) {
    auto step = statement.step();
    if (!step.hasValue()) return Result<void, PersistenceError>::err(step.error());
    return Result<void, PersistenceError>::ok();
}

Result<void, PersistenceError> insertEvent(sqlite3* db, const ReviewEvent& event) {
    auto prepared = detail::Statement::prepare(db, R"sql(
        INSERT INTO review_events (id, item_kind, item_id, session_id, device_id, reviewed_at,
            elapsed_days, exercise, predicted, grade, hints_used, wrong_target_id, response_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )sql");
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    statement.bindText(1, event.id.toString());
    statement.bindText(2, toStorageString(event.item.kind));
    statement.bindText(3, event.item.id.toString());
    statement.bindText(4, event.sessionId.toString());
    statement.bindText(5, event.deviceId);
    statement.bindInt64(6, toMillis(event.reviewedAt));
    statement.bindDouble(7, event.elapsedDays);
    statement.bindText(8, toStorageString(event.exercise));
    statement.bindInt64(9, static_cast<int64_t>(event.predicted));
    statement.bindInt64(10, static_cast<int64_t>(event.grade));
    statement.bindInt64(11, event.hintsUsed);
    statement.bindOptionalText(12, event.wrongTarget ? std::optional<std::string>(event.wrongTarget->toString())
                                                     : std::nullopt);
    statement.bindInt64(13, event.responseTime.count());
    return run(statement);
}

Result<void, PersistenceError> upsertState(sqlite3* db, const MemoryState& state) {
    auto prepared = detail::Statement::prepare(db, R"sql(
        INSERT INTO memory_states (item_kind, item_id, phase, stability, difficulty,
            last_reviewed_at, due_at, review_count, lapse_count)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(item_kind, item_id) DO UPDATE SET
            phase = excluded.phase,
            stability = excluded.stability,
            difficulty = excluded.difficulty,
            last_reviewed_at = excluded.last_reviewed_at,
            due_at = excluded.due_at,
            review_count = excluded.review_count,
            lapse_count = excluded.lapse_count;
    )sql");
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    statement.bindText(1, toStorageString(state.item.kind));
    statement.bindText(2, state.item.id.toString());
    statement.bindText(3, toStorageString(state.phase));
    statement.bindDouble(4, state.stability);
    statement.bindDouble(5, state.difficulty);
    statement.bindOptionalInt64(6, toOptionalMillis(state.lastReviewedAt));
    statement.bindOptionalInt64(7, toOptionalMillis(state.dueAt));
    statement.bindInt64(8, state.reviewCount);
    statement.bindInt64(9, state.lapseCount);
    return run(statement);
}

Result<std::optional<std::string>, PersistenceError> singleText(sqlite3* db, const char* sql,
                                                                const char* parameter) {
    using Out = Result<std::optional<std::string>, PersistenceError>;
    auto prepared = detail::Statement::prepare(db, sql);
    if (!prepared.hasValue()) return Out::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    if (parameter != nullptr) statement.bindText(1, parameter);
    auto step = statement.step();
    if (!step.hasValue()) return Out::err(step.error());
    if (!step.value()) return Out::ok(std::nullopt);
    return Out::ok(statement.columnText(0));
}

Result<std::optional<std::string>, PersistenceError> metaValue(sqlite3* db, const char* key) {
    return singleText(db, "SELECT value FROM memory_meta WHERE key = ?;", key);
}

Result<std::optional<std::string>, PersistenceError> newestEventId(sqlite3* db) {
    return singleText(db, "SELECT id FROM review_events ORDER BY reviewed_at DESC, id DESC LIMIT 1;", nullptr);
}

Result<void, PersistenceError> setMeta(sqlite3* db, const char* key, const std::string& value) {
    auto prepared = detail::Statement::prepare(db, R"sql(
        INSERT INTO memory_meta (key, value) VALUES (?, ?)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    )sql");
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    statement.bindText(1, key);
    statement.bindText(2, value);
    return run(statement);
}

Result<void, PersistenceError> deleteMeta(sqlite3* db, const char* key) {
    auto prepared = detail::Statement::prepare(db, "DELETE FROM memory_meta WHERE key = ?;");
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    statement.bindText(1, key);
    return run(statement);
}

Result<void, PersistenceError> syncLastEventId(sqlite3* db) {
    auto newest = newestEventId(db);
    if (!newest.hasValue()) return Result<void, PersistenceError>::err(newest.error());
    if (!newest.value()) return deleteMeta(db, kLastEventKey);
    return setMeta(db, kLastEventKey, *newest.value());
}

Result<bool, PersistenceError> lastEventIdMatchesLog(sqlite3* db) {
    auto stored = metaValue(db, kLastEventKey);
    if (!stored.hasValue()) return Result<bool, PersistenceError>::err(stored.error());
    auto newest = newestEventId(db);
    if (!newest.hasValue()) return Result<bool, PersistenceError>::err(newest.error());
    return Result<bool, PersistenceError>::ok(stored.value() == newest.value());
}

Result<ReviewEvent, PersistenceError> readEvent(const detail::Statement& row) {
    using Out = Result<ReviewEvent, PersistenceError>;
    auto id = Uuid::parse(row.columnText(0));
    auto kind = atlas::core::itemKindFromString(row.columnText(1));
    auto itemId = Uuid::parse(row.columnText(2));
    auto sessionId = Uuid::parse(row.columnText(3));
    auto exercise = atlas::core::exerciseFromString(row.columnText(7));
    auto predicted = atlas::core::certaintyFromInt(row.columnInt64(8));
    auto grade = atlas::core::gradeFromInt(row.columnInt64(9));
    if (!id || !kind || !itemId || !sessionId || !exercise || !predicted || !grade) {
        return Out::err(malformed("review_events row " + row.columnText(0)));
    }

    ReviewEvent event;
    event.id = *id;
    event.item = ItemRef{*kind, *itemId};
    event.sessionId = *sessionId;
    event.deviceId = row.columnText(4);
    event.reviewedAt = fromMillis(row.columnInt64(5));
    event.elapsedDays = row.columnDouble(6);
    event.exercise = *exercise;
    event.predicted = *predicted;
    event.grade = *grade;
    event.hintsUsed = static_cast<int>(row.columnInt64(10));
    if (auto wrong = row.columnOptionalText(11)) {
        auto parsed = Uuid::parse(*wrong);
        if (!parsed) return Out::err(malformed("review_events wrong_target_id " + *wrong));
        event.wrongTarget = KnowledgeObjectId(*parsed);
    }
    event.responseTime = std::chrono::milliseconds(row.columnInt64(12));
    return Out::ok(std::move(event));
}

Result<MemoryState, PersistenceError> readState(const detail::Statement& row) {
    using Out = Result<MemoryState, PersistenceError>;
    auto kind = atlas::core::itemKindFromString(row.columnText(0));
    auto itemId = Uuid::parse(row.columnText(1));
    auto phase = atlas::core::phaseFromString(row.columnText(2));
    if (!kind || !itemId || !phase) return Out::err(malformed("memory_states row " + row.columnText(1)));

    MemoryState state{ItemRef{*kind, *itemId}};
    state.phase = *phase;
    state.stability = row.columnDouble(3);
    state.difficulty = row.columnDouble(4);
    bool invalidNumbers = !std::isfinite(state.stability) || !std::isfinite(state.difficulty) ||
                          (state.phase != atlas::core::Phase::New && state.stability <= 0.0);
    if (invalidNumbers) return Out::err(malformed("memory_states row " + row.columnText(1)));
    state.lastReviewedAt = fromOptionalMillis(row.columnOptionalInt64(5));
    state.dueAt = fromOptionalMillis(row.columnOptionalInt64(6));
    state.reviewCount = static_cast<int>(row.columnInt64(7));
    state.lapseCount = static_cast<int>(row.columnInt64(8));
    return Out::ok(std::move(state));
}

template <typename Row, typename Reader>
Result<std::vector<Row>, PersistenceError> readAll(sqlite3* db, const char* sql, Reader read) {
    using Out = Result<std::vector<Row>, PersistenceError>;
    auto prepared = detail::Statement::prepare(db, sql);
    if (!prepared.hasValue()) return Out::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    std::vector<Row> rows;
    while (true) {
        auto step = statement.step();
        if (!step.hasValue()) return Out::err(step.error());
        if (!step.value()) break;
        auto row = read(statement);
        if (!row.hasValue()) return Out::err(row.error());
        rows.push_back(std::move(row).value());
    }
    return Out::ok(std::move(rows));
}

}

LearningRepository::LearningRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> LearningRepository::record(const std::vector<ReviewEvent>& events,
                                                          const std::vector<MemoryState>& states) {
    sqlite3* db = database_->handle();
    return detail::inTransaction(db, [&]() -> Result<void, PersistenceError> {
        auto wasFresh = lastEventIdMatchesLog(db);
        if (!wasFresh.hasValue()) return Result<void, PersistenceError>::err(wasFresh.error());
        for (const auto& event : events) {
            if (auto inserted = insertEvent(db, event); !inserted.hasValue()) return inserted;
        }
        for (const auto& state : states) {
            if (auto upserted = upsertState(db, state); !upserted.hasValue()) return upserted;
        }
        if (!wasFresh.value()) return Result<void, PersistenceError>::ok();
        return syncLastEventId(db);
    });
}

Result<std::vector<ReviewEvent>, PersistenceError> LearningRepository::allEvents() {
    return readAll<ReviewEvent>(database_->handle(), R"sql(
        SELECT id, item_kind, item_id, session_id, device_id, reviewed_at, elapsed_days, exercise,
               predicted, grade, hints_used, wrong_target_id, response_ms
        FROM review_events ORDER BY reviewed_at, id;
    )sql", readEvent);
}

Result<std::vector<MemoryState>, PersistenceError> LearningRepository::allStates() {
    return readAll<MemoryState>(database_->handle(), R"sql(
        SELECT item_kind, item_id, phase, stability, difficulty, last_reviewed_at, due_at,
               review_count, lapse_count
        FROM memory_states ORDER BY item_kind, item_id;
    )sql", readState);
}

Result<void, PersistenceError> LearningRepository::replaceStates(const std::vector<MemoryState>& states,
                                                                 int replayVersion) {
    sqlite3* db = database_->handle();
    return detail::inTransaction(db, [&]() -> Result<void, PersistenceError> {
        if (auto cleared = detail::execute(db, "DELETE FROM memory_states;"); !cleared.hasValue()) return cleared;
        for (const auto& state : states) {
            if (auto upserted = upsertState(db, state); !upserted.hasValue()) return upserted;
        }
        if (auto version = setMeta(db, kReplayVersionKey, std::to_string(replayVersion)); !version.hasValue()) {
            return version;
        }
        return syncLastEventId(db);
    });
}

Result<bool, PersistenceError> LearningRepository::isCacheFresh(int replayVersion) {
    sqlite3* db = database_->handle();
    auto version = metaValue(db, kReplayVersionKey);
    if (!version.hasValue()) return Result<bool, PersistenceError>::err(version.error());
    auto stored = metaValue(db, kLastEventKey);
    if (!stored.hasValue()) return Result<bool, PersistenceError>::err(stored.error());
    auto newest = newestEventId(db);
    if (!newest.hasValue()) return Result<bool, PersistenceError>::err(newest.error());
    bool fresh = version.value() == std::to_string(replayVersion) && stored.value() == newest.value();
    return Result<bool, PersistenceError>::ok(fresh);
}

}
