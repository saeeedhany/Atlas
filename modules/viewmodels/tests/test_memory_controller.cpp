#include <QDateTime>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "doctest.h"
#include "raw_sql.hpp"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase(const std::string& path) {
    auto result = Database::open(path);
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

TimePoint noonToday() {
    QDateTime noon(QDate::currentDate(), QTime(12, 0));
    return TimePoint(std::chrono::milliseconds(noon.toMSecsSinceEpoch()));
}

ReviewEvent reviewOf(const KnowledgeObjectId& id, TimePoint at, Grade grade) {
    ReviewEvent event;
    event.id = Uuid::generate();
    event.item = ItemRef::forConcept(id);
    event.sessionId = Uuid::generate();
    event.deviceId = "test";
    event.reviewedAt = at;
    event.grade = grade;
    return event;
}

struct Fixture {
    QTemporaryDir dir;
    std::string path = dir.filePath("atlas.db").toStdString();
    Database db = openTestDatabase(path);
    WorkspaceController workspace{db};
    TimePoint now = noonToday();
    Clock clock = [this] { return now; };

    Fixture() { REQUIRE(workspace.load().hasValue()); }

    KnowledgeObjectId addConcept(const char* title) { return workspace.createKnowledgeObject(title).value(); }
};

}  // namespace

TEST_CASE("load rebuilds a stale cache from the log and marks it fresh") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    LearningRepository repository(f.db);
    auto corrupt = MemoryState{ItemRef::forConcept(tree)};
    corrupt.phase = Phase::Review;
    corrupt.stability = 999.0;
    corrupt.difficulty = 5.0;
    corrupt.lastReviewedAt = f.now;
    REQUIRE(repository.record({reviewOf(tree, f.now, Grade::Good)}, {corrupt}).hasValue());

    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());

    CHECK(memory.states().at(ItemRef::forConcept(tree)).stability == doctest::Approx(2.3065));
    CHECK(repository.isCacheFresh(atlas::learning::kReplayVersion).value());
}

TEST_CASE("a fresh cache is loaded as stored") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    LearningRepository repository(f.db);
    REQUIRE(repository.record({reviewOf(tree, f.now, Grade::Good)}, {}).hasValue());
    {
        MemoryController first(f.db, f.workspace, f.clock);
        REQUIRE(first.load().hasValue());
    }
    MemoryController second(f.db, f.workspace, f.clock);
    REQUIRE(second.load().hasValue());
    CHECK(second.states().size() == 1);
    CHECK(second.events().size() == 1);
}

TEST_CASE("a fresh cache with a corrupt row is rebuilt from the log") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    LearningRepository repository(f.db);
    REQUIRE(repository.record({reviewOf(tree, f.now, Grade::Good)}, {}).hasValue());
    atlas::learning::StateMap replayed;
    {
        MemoryController first(f.db, f.workspace, f.clock);
        REQUIRE(first.load().hasValue());
        replayed = first.states();
    }
    REQUIRE(repository.isCacheFresh(atlas::learning::kReplayVersion).value());
    REQUIRE(executeRawSql(f.path, "UPDATE memory_states SET phase = 'garbage';"));
    REQUIRE_FALSE(repository.allStates().hasValue());

    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());

    const auto& state = memory.states().at(ItemRef::forConcept(tree));
    const auto& expected = replayed.at(ItemRef::forConcept(tree));
    CHECK(state.phase == expected.phase);
    CHECK(state.stability == doctest::Approx(expected.stability));
    CHECK(state.difficulty == doctest::Approx(expected.difficulty));
    CHECK(state.reviewCount == expected.reviewCount);
    CHECK(repository.allStates().hasValue());
}

TEST_CASE("a fresh cache is used instead of replaying the log") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    LearningRepository repository(f.db);
    auto stored = MemoryState{ItemRef::forConcept(tree)};
    stored.phase = Phase::Review;
    stored.stability = 7.0;
    stored.difficulty = 5.0;
    stored.lastReviewedAt = f.now;
    REQUIRE(repository.record({reviewOf(tree, f.now, Grade::Good)}, {stored}).hasValue());
    REQUIRE(repository.replaceStates({stored}, atlas::learning::kReplayVersion).hasValue());

    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());

    CHECK(memory.states().at(ItemRef::forConcept(tree)).stability == doctest::Approx(7.0));
}

TEST_CASE("recall chance is empty for concepts never reviewed") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    auto hash = f.addConcept("Hash Table");
    LearningRepository repository(f.db);
    REQUIRE(repository.record({reviewOf(tree, f.now, Grade::Good)}, {}).hasValue());
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());

    CHECK(memory.recallChance(ItemRef::forConcept(tree)) == doctest::Approx(1.0));
    CHECK_FALSE(memory.recallChance(ItemRef::forConcept(hash)).has_value());
}

TEST_CASE("introducedToday counts concepts first reviewed on the local calendar day") {
    Fixture f;
    auto today = f.addConcept("Today");
    auto earlier = f.addConcept("Earlier");
    LearningRepository repository(f.db);
    auto twoDaysAgo = f.now - std::chrono::hours(48);
    REQUIRE(repository.record({reviewOf(today, f.now, Grade::Good), reviewOf(earlier, twoDaysAgo, Grade::Good),
                               reviewOf(earlier, f.now, Grade::Good)},
                              {})
                .hasValue());
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    CHECK(memory.introducedToday() == 1);
}

TEST_CASE("today's plan introduces frontier concepts on an empty log") {
    Fixture f;
    f.addConcept("A");
    f.addConcept("B");
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    auto plan = memory.todayPlan(atlas::learning::SessionLimits{});
    CHECK(plan.focuses.size() == 2);
    CHECK(plan.focuses[0].isNew);
}

TEST_CASE("memoryChanged fires on load and on every graph change") {
    Fixture f;
    MemoryController memory(f.db, f.workspace, f.clock);
    int fired = 0;
    QObject::connect(&memory, &MemoryController::memoryChanged, [&] { ++fired; });
    REQUIRE(memory.load().hasValue());
    f.addConcept("New");
    CHECK(fired == 2);
}

TEST_CASE("record writes events and states and updates memory") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    int changes = 0;
    QObject::connect(&memory, &MemoryController::memoryChanged, [&] { ++changes; });

    REQUIRE(memory.record({reviewOf(tree, f.now, Grade::Good)}).hasValue());
    CHECK(changes == 1);
    CHECK(memory.events().size() == 1);
    REQUIRE(memory.recallChance(ItemRef::forConcept(tree)).has_value());

    MemoryController reloaded(f.db, f.workspace, f.clock);
    REQUIRE(reloaded.load().hasValue());
    CHECK(reloaded.states() == memory.states());
}

TEST_CASE("record clamps future timestamps and negative gaps") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    auto event = reviewOf(tree, f.now + std::chrono::hours(5), Grade::Good);
    event.elapsedDays = -3.0;
    REQUIRE(memory.record({event}).hasValue());
    CHECK(memory.events().front().reviewedAt == f.now);
    CHECK(memory.events().front().elapsedDays == 0.0);
    CHECK(memory.states().at(ItemRef::forConcept(tree)).lastReviewedAt == f.now);
}

TEST_CASE("a failed write leaves memory untouched") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    REQUIRE(executeRawSql(f.path, "DROP TABLE review_events"));
    int changes = 0;
    QObject::connect(&memory, &MemoryController::memoryChanged, [&] { ++changes; });

    CHECK_FALSE(memory.record({reviewOf(tree, f.now, Grade::Good)}).hasValue());
    CHECK(changes == 0);
    CHECK(memory.events().empty());
    CHECK(memory.states().empty());
}

TEST_CASE("elapsed days count from the last review") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    auto item = ItemRef::forConcept(tree);
    CHECK(memory.elapsedDaysFor(item, f.now) == 0.0);
    REQUIRE(memory.record({reviewOf(tree, f.now, Grade::Good)}).hasValue());
    CHECK(memory.elapsedDaysFor(item, f.now + std::chrono::hours(36)) == doctest::Approx(1.5));
    CHECK(memory.elapsedDaysFor(item, f.now - std::chrono::hours(2)) == 0.0);
}

TEST_CASE("states after a live batch equal the states replayed from the log") {
    Fixture f;
    auto tree = f.addConcept("Tree");
    f.now += std::chrono::microseconds(1700);
    auto later = reviewOf(tree, f.now - std::chrono::hours(1) + std::chrono::microseconds(300), Grade::Good);
    auto earlier = reviewOf(tree, f.now - std::chrono::hours(30), Grade::Again);
    atlas::learning::StateMap live;
    {
        MemoryController memory(f.db, f.workspace, f.clock);
        REQUIRE(memory.load().hasValue());
        REQUIRE(memory.record({later, earlier}).hasValue());
        live = memory.states();
    }
    REQUIRE(executeRawSql(f.path, "UPDATE memory_meta SET value = '0' WHERE key = 'replay_version';"));
    LearningRepository repository(f.db);
    REQUIRE_FALSE(repository.isCacheFresh(atlas::learning::kReplayVersion).value());

    MemoryController reloaded(f.db, f.workspace, f.clock);
    REQUIRE(reloaded.load().hasValue());
    CHECK(reloaded.states() == live);
}
