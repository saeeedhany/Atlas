#include <QSettings>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/today_view_model.hpp"
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

struct Fixture {
    Database db = openTestDatabase();
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    QTemporaryDir dir;
    QSettings store{dir.filePath("settings.ini"), QSettings::IniFormat};
    AppSettings settings{store};
    TimePoint now = std::chrono::system_clock::now();
    MemoryController memory{db, workspace, [this] { return now; }};
    TodayViewModel today{workspace, memory, settings};

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(memory.load().hasValue());
    }

    void reviewNow(const KnowledgeObjectId& id) {
        ReviewEvent review;
        review.id = Uuid::generate();
        review.item = ItemRef::forConcept(id);
        review.sessionId = Uuid::generate();
        review.deviceId = "test";
        review.reviewedAt = now;
        review.grade = Grade::Good;
        auto state = atlas::learning::Fsrs{}.review(atlas::learning::MemoryState{review.item}, Grade::Good, now);
        REQUIRE(LearningRepository(db).record({review}, {state}).hasValue());
    }
};

}  // namespace

TEST_CASE("a new database is empty, not caught up") {
    Fixture f;
    CHECK(f.today.empty());
    CHECK_FALSE(f.today.caughtUp());
    CHECK(f.today.estimatedMinutes() == 0);
}

TEST_CASE("new concepts are offered up to the daily limit") {
    Fixture f;
    f.workspace.createKnowledgeObject("A");
    f.workspace.createKnowledgeObject("B");
    CHECK(f.today.newCount() == 2);
    CHECK(f.today.itemCount() == 2);
    CHECK(f.today.estimatedMinutes() == 2);
    f.settings.setNewPerDay(1);
    CHECK(f.today.newCount() == 1);
}

TEST_CASE("after learning everything today the user is caught up, later items come due") {
    Fixture f;
    auto a = f.workspace.createKnowledgeObject("A").value();
    auto b = f.workspace.createKnowledgeObject("B").value();
    f.reviewNow(a);
    f.reviewNow(b);
    REQUIRE(f.memory.load().hasValue());
    CHECK(f.today.learnedCount() == 2);
    CHECK(f.today.caughtUp());

    f.now += std::chrono::hours(24 * 10);
    f.today.refresh();
    CHECK(f.today.dueCount() == 2);
    CHECK_FALSE(f.today.caughtUp());
}
