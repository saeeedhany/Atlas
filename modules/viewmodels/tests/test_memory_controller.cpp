#include <QDateTime>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
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
    Database db = openTestDatabase();
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
