#include "atlas/persistence/note_repository.hpp"

#include <sqlite3.h>

#include <unordered_map>
#include <utility>

#include "detail/statement.hpp"

namespace atlas::persistence {

using atlas::core::TimePoint;
using atlas::core::Uuid;

namespace {

int64_t toMillis(TimePoint time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

TimePoint fromMillis(int64_t millis) { return TimePoint(std::chrono::milliseconds(millis)); }

Result<void, PersistenceError> run(sqlite3* db, const char* sql, const std::vector<std::string>& texts) {
    auto prepared = detail::Statement::prepare(db, sql);
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    for (size_t i = 0; i < texts.size(); ++i) statement.bindText(static_cast<int>(i) + 1, texts[i]);
    auto step = statement.step();
    if (!step.hasValue()) return Result<void, PersistenceError>::err(step.error());
    return Result<void, PersistenceError>::ok();
}

}  // namespace

NoteRepository::NoteRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> NoteRepository::save(const BoardNote& note) {
    auto prepared = detail::Statement::prepare(database_->handle(), R"sql(
        INSERT INTO board_notes (id, body, color, x, y, width, height, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET body = excluded.body, color = excluded.color, x = excluded.x,
            y = excluded.y, width = excluded.width, height = excluded.height, updated_at = excluded.updated_at;
    )sql");
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    statement.bindText(1, note.id.toString());
    statement.bindText(2, note.body);
    statement.bindText(3, note.color);
    statement.bindDouble(4, note.x);
    statement.bindDouble(5, note.y);
    statement.bindDouble(6, note.width);
    statement.bindDouble(7, note.height);
    statement.bindInt64(8, toMillis(note.createdAt));
    statement.bindInt64(9, toMillis(note.updatedAt));
    auto step = statement.step();
    if (!step.hasValue()) return Result<void, PersistenceError>::err(step.error());
    return Result<void, PersistenceError>::ok();
}

Result<void, PersistenceError> NoteRepository::remove(const Uuid& id) {
    return run(database_->handle(), "DELETE FROM board_notes WHERE id = ?;", {id.toString()});
}

Result<void, PersistenceError> NoteRepository::addLink(const Uuid& noteId, const NoteLink& link) {
    return run(database_->handle(),
               "INSERT OR IGNORE INTO note_links (note_id, target_kind, target_id) VALUES (?, ?, ?);",
               {noteId.toString(), link.kind, link.targetId.toString()});
}

Result<void, PersistenceError> NoteRepository::removeLink(const Uuid& noteId, const NoteLink& link) {
    return run(database_->handle(), "DELETE FROM note_links WHERE note_id = ? AND target_kind = ? AND target_id = ?;",
               {noteId.toString(), link.kind, link.targetId.toString()});
}

Result<std::vector<BoardNote>, PersistenceError> NoteRepository::findAll() {
    using Out = Result<std::vector<BoardNote>, PersistenceError>;
    sqlite3* db = database_->handle();
    auto prepared = detail::Statement::prepare(
        db, "SELECT id, body, color, x, y, width, height, created_at, updated_at FROM board_notes ORDER BY created_at, id;");
    if (!prepared.hasValue()) return Out::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    std::vector<BoardNote> notes;
    std::unordered_map<std::string, size_t> indexById;
    while (true) {
        auto step = statement.step();
        if (!step.hasValue()) return Out::err(step.error());
        if (!step.value()) break;
        auto id = Uuid::parse(statement.columnText(0));
        if (!id) continue;
        BoardNote note;
        note.id = *id;
        note.body = statement.columnText(1);
        note.color = statement.columnText(2);
        note.x = statement.columnDouble(3);
        note.y = statement.columnDouble(4);
        note.width = statement.columnDouble(5);
        note.height = statement.columnDouble(6);
        note.createdAt = fromMillis(statement.columnInt64(7));
        note.updatedAt = fromMillis(statement.columnInt64(8));
        indexById.emplace(statement.columnText(0), notes.size());
        notes.push_back(std::move(note));
    }
    auto linkStatement = detail::Statement::prepare(
        db, "SELECT note_id, target_kind, target_id FROM note_links ORDER BY note_id, target_kind, target_id;");
    if (!linkStatement.hasValue()) return Out::err(std::move(linkStatement).error());
    auto links = std::move(linkStatement).value();
    while (true) {
        auto step = links.step();
        if (!step.hasValue()) return Out::err(step.error());
        if (!step.value()) break;
        auto owner = indexById.find(links.columnText(0));
        auto target = Uuid::parse(links.columnText(2));
        if (owner == indexById.end() || !target) continue;
        notes[owner->second].links.push_back(NoteLink{links.columnText(1), *target});
    }
    return Out::ok(std::move(notes));
}

}  // namespace atlas::persistence
