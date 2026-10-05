#include <chrono>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/note_repository.hpp"
#include "atlas/persistence/topic_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

BoardNote makeNote(const char* body) {
    BoardNote note;
    note.id = Uuid::generate();
    note.body = body;
    note.color = "clay";
    note.x = 10;
    note.y = 20;
    note.width = 180;
    note.height = 120;
    note.createdAt = std::chrono::system_clock::now();
    note.updatedAt = note.createdAt;
    return note;
}

}

TEST_CASE("notes round trip with their links") {
    auto db = openTestDatabase();
    NoteRepository notes(db);
    auto note = makeNote("Why half full?");
    REQUIRE(notes.save(note).hasValue());
    NoteLink link{"concept", Uuid::generate()};
    REQUIRE(notes.addLink(note.id, link).hasValue());
    REQUIRE(notes.addLink(note.id, link).hasValue());

    auto all = notes.findAll().value();
    REQUIRE(all.size() == 1);
    CHECK(all[0].body == "Why half full?");
    CHECK(all[0].width == doctest::Approx(180.0));
    REQUIRE(all[0].links.size() == 1);
    CHECK(all[0].links[0] == link);

    note.body = "Edited";
    note.x = 99;
    REQUIRE(notes.save(note).hasValue());
    CHECK(notes.findAll().value()[0].body == "Edited");

    REQUIRE(notes.removeLink(note.id, link).hasValue());
    CHECK(notes.findAll().value()[0].links.empty());
    REQUIRE(notes.remove(note.id).hasValue());
    CHECK(notes.findAll().value().empty());
}

TEST_CASE("deleting a linked concept or topic removes the link but keeps the note") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    TopicRepository topics(db);
    auto topic = Topic::create("OS").value();
    REQUIRE(topics.save(topic).hasValue());
    auto object = KnowledgeObject::create("Paging").value();
    REQUIRE(objects.save(object).hasValue());

    NoteRepository notes(db);
    auto note = makeNote("Linked");
    REQUIRE(notes.save(note).hasValue());
    REQUIRE(notes.addLink(note.id, NoteLink{"concept", object.id().value()}).hasValue());
    REQUIRE(notes.addLink(note.id, NoteLink{"topic", topic.id().value()}).hasValue());

    REQUIRE(objects.remove(object.id()).hasValue());
    auto after = notes.findAll().value();
    REQUIRE(after.size() == 1);
    REQUIRE(after[0].links.size() == 1);
    CHECK(after[0].links[0].kind == "topic");

    REQUIRE(topics.remove(topic.id()).hasValue());
    CHECK(notes.findAll().value()[0].links.empty());
}
