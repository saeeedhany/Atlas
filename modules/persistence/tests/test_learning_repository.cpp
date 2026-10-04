#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/persistence/relationship_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

TimePoint at(int64_t millis) { return TimePoint{} + std::chrono::milliseconds(1'700'000'000'000 + millis); }

ReviewEvent makeEvent(const ItemRef& item, int64_t millis, Grade grade) {
    ReviewEvent event;
    event.id = Uuid::generate();
    event.item = item;
    event.sessionId = Uuid::generate();
    event.deviceId = "desk";
    event.reviewedAt = at(millis);
    event.elapsedDays = 1.5;
    event.exercise = Exercise::Explain;
    event.predicted = Certainty::FairlySure;
    event.grade = grade;
    event.hintsUsed = 2;
    event.responseTime = std::chrono::milliseconds(4200);
    return event;
}

MemoryState makeState(const ItemRef& item) {
    MemoryState state{item};
    state.phase = Phase::Review;
    state.stability = 2.3065;
    state.difficulty = 2.1181039705;
    state.lastReviewedAt = at(0);
    state.dueAt = at(5000);
    state.reviewCount = 1;
    return state;
}

}  // namespace

TEST_CASE("events and states round-trip with every field") {
    auto db = openTestDatabase();
    LearningRepository repository(db);
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto later = makeEvent(item, 2000, Grade::Again);
    later.wrongTarget = KnowledgeObjectId::generate();
    auto earlier = makeEvent(item, 1000, Grade::Good);
    auto state = makeState(item);

    REQUIRE(repository.record({later, earlier}, {state}).hasValue());

    auto events = repository.allEvents().value();
    REQUIRE(events.size() == 2);
    CHECK(events[0].id == earlier.id);
    CHECK(events[1].wrongTarget == later.wrongTarget);
    CHECK(events[0].item == item);
    CHECK(events[0].sessionId == earlier.sessionId);
    CHECK(events[0].deviceId == "desk");
    CHECK(events[0].reviewedAt == at(1000));
    CHECK(events[0].elapsedDays == 1.5);
    CHECK(events[0].exercise == Exercise::Explain);
    CHECK(events[0].predicted == Certainty::FairlySure);
    CHECK(events[0].grade == Grade::Good);
    CHECK(events[0].hintsUsed == 2);
    CHECK(events[0].responseTime == std::chrono::milliseconds(4200));
    CHECK_FALSE(events[0].wrongTarget.has_value());

    auto states = repository.allStates().value();
    REQUIRE(states.size() == 1);
    CHECK(states[0] == state);
}

TEST_CASE("recording a state again overwrites it") {
    auto db = openTestDatabase();
    LearningRepository repository(db);
    auto item = ItemRef::forLink(RelationshipId::generate());
    auto state = makeState(item);
    REQUIRE(repository.record({}, {state}).hasValue());
    state.stability = 9.0;
    state.dueAt.reset();
    REQUIRE(repository.record({}, {state}).hasValue());
    auto states = repository.allStates().value();
    REQUIRE(states.size() == 1);
    CHECK(states[0] == state);
}

TEST_CASE("a failed record writes nothing") {
    auto db = openTestDatabase();
    LearningRepository repository(db);
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto event = makeEvent(item, 0, Grade::Good);
    REQUIRE(repository.record({event}, {}).hasValue());

    auto result = repository.record({makeEvent(item, 10, Grade::Good), event}, {makeState(item)});
    CHECK_FALSE(result.hasValue());
    CHECK(repository.allEvents().value().size() == 1);
    CHECK(repository.allStates().value().empty());
}

TEST_CASE("the cache is fresh only after a rebuild with the same replay version") {
    auto db = openTestDatabase();
    LearningRepository repository(db);
    CHECK_FALSE(repository.isCacheFresh(1).value());

    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    REQUIRE(repository.record({makeEvent(item, 0, Grade::Good)}, {}).hasValue());
    REQUIRE(repository.replaceStates({makeState(item)}, 1).hasValue());
    CHECK(repository.isCacheFresh(1).value());
    CHECK_FALSE(repository.isCacheFresh(2).value());

    REQUIRE(repository.record({makeEvent(item, 100, Grade::Good)}, {makeState(item)}).hasValue());
    CHECK(repository.isCacheFresh(1).value());
}

TEST_CASE("deleting a concept removes its learning data and that of its links") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);
    LearningRepository learning(db);

    auto a = KnowledgeObject::create("A").value();
    auto b = KnowledgeObject::create("B").value();
    auto c = KnowledgeObject::create("C").value();
    REQUIRE(objects.save(a).hasValue());
    REQUIRE(objects.save(b).hasValue());
    REQUIRE(objects.save(c).hasValue());
    auto link = Relationship::create(a.id(), b.id(), RelationshipType::DependsOn).value();
    REQUIRE(relationships.save(link).hasValue());

    auto conceptItem = ItemRef::forConcept(a.id());
    auto linkItem = ItemRef::forLink(link.id());
    auto keptItem = ItemRef::forConcept(c.id());
    auto confused = makeEvent(keptItem, 30, Grade::Again);
    confused.wrongTarget = a.id();
    REQUIRE(learning.record({makeEvent(conceptItem, 10, Grade::Good), makeEvent(linkItem, 20, Grade::Good), confused},
                            {makeState(conceptItem), makeState(linkItem), makeState(keptItem)})
                .hasValue());

    REQUIRE(objects.remove(a.id()).hasValue());

    auto events = learning.allEvents().value();
    REQUIRE(events.size() == 1);
    CHECK(events[0].item == keptItem);
    CHECK_FALSE(events[0].wrongTarget.has_value());
    auto states = learning.allStates().value();
    REQUIRE(states.size() == 1);
    CHECK(states[0].item == keptItem);
}

TEST_CASE("deleting a relationship removes only its own learning data") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    RelationshipRepository relationships(db);
    LearningRepository learning(db);

    auto a = KnowledgeObject::create("A").value();
    auto b = KnowledgeObject::create("B").value();
    REQUIRE(objects.save(a).hasValue());
    REQUIRE(objects.save(b).hasValue());
    auto link = Relationship::create(a.id(), b.id(), RelationshipType::Uses).value();
    REQUIRE(relationships.save(link).hasValue());
    auto conceptItem = ItemRef::forConcept(a.id());
    REQUIRE(learning.record({makeEvent(conceptItem, 0, Grade::Good), makeEvent(ItemRef::forLink(link.id()), 5, Grade::Good)},
                            {makeState(conceptItem), makeState(ItemRef::forLink(link.id()))})
                .hasValue());

    REQUIRE(relationships.remove(link.id()).hasValue());

    CHECK(learning.allEvents().value().size() == 1);
    CHECK(learning.allStates().value().size() == 1);
    CHECK(learning.allStates().value()[0].item == conceptItem);
}
