# View Models and Canvas Implementation Plan (Plan 2 of 4)

**Goal:** Build the tested C++ layer the Qt Quick UI will sit on: a new `atlas-viewmodels` QML module (controllers, list models, singletons), Graphite colors, and a map canvas that draws memory rings, labels, arrowheads, dashed contrast links, ghost nodes, and collapses topics when zoomed out.

**Architecture:** `atlas-viewmodels` is a static library declared with `qt_add_qml_module` (URI `Atlas.ViewModels`). App level objects are created in C++ and handed to QML as provided singletons. The canvas stays a hand built scene graph item in `atlas-render`. No QML screen is written in this plan; Plan 3 builds the screens on these APIs.

**Tech Stack:** C++20, Qt 6.11 (Core, Gui, Qml, Quick), SQLite, doctest.

**Spec:** `docs/specs/2026-10-04-atlas-learning-design.md`

**Plan series (updated):**
1. Done: learning engine.
2. This plan: view models and canvas (C++ only).
3. QML shell: Theme and Motion singletons, components, Today, Map, concept panel, settings, the new `atlas` executable.
4. Sessions (rebuild, explain, summary), parity check, delete `atlas-ui`, README.

## Global Constraints

- C++20, `CMAKE_CXX_EXTENSIONS OFF`. Every target builds with `-Wall -Wextra -Wpedantic -Werror` and passes under ASan + UBSan (`asan` preset).
- Requires Qt 6.7 or newer (developed on 6.11).
- Fallible boundaries return `atlas::core::Result<T, E>`. No exceptions. View models report failures through an `errorOccurred(QString)` signal, never silently.
- Database writes happen before in-memory updates (existing `WorkspaceController` invariant).
- Classes exposed to QML as singletons derive from `ProvidedSingleton<T>` and must not be default-constructible: QML only uses the provided instance when `T` has no default constructor.
- Every header containing a `QML_ELEMENT` class is listed in the module's `SOURCES`, and `include/atlas/viewmodels` is a PRIVATE include directory (generated registration code includes headers by bare file name). Header file names in that folder must be unique.
- Ring bands: recall 0.80 and above is strong (lime), 0.50 to 0.80 medium (soft white), below 0.50 weak (rose), not introduced is dashed gray.
- The self-rated Confidence field is never exposed by a view model.
- Code style: readable names, small units. Comments only when necessary, one line maximum.
- Never write the long dash character anywhere (code, docs, commits). Use "-" only when needed.
- Commit messages have no co-author or generated-by trailers.

## Review Focus

1. A stale or corrupt memory cache at startup must be rebuilt from the log, never trusted. Test in Task 2.
2. A concept deleted while it is selected or open in the editor must clear the selection and the editor, not leave a dangling id. Tests in Task 7 and Task 9.
3. First run with an empty database must give an empty map and an "empty" Today state with no errors. Tests in Task 7 and Task 11.
4. Very long concept titles must be shortened on the map, not overflow. Test in Task 6.
5. Switching theme mid-session must recolor both the palette and the canvas. Tests in Task 4 and Task 7.

## File Map

```
CMakeLists.txt                                        add modules/viewmodels (UI block)
modules/viewmodels/CMakeLists.txt                     qt_add_qml_module Atlas.ViewModels
modules/viewmodels/include/atlas/viewmodels/
  controller_error.hpp, workspace_controller.hpp      moved from atlas-ui
  clock.hpp                                           Clock type, systemClock()
  memory_controller.hpp                               log load, cache rebuild, recall, today plan
  placement_controller.hpp                            saved positions, arrange, tidy, pins
  provided_singleton.hpp                              QML singleton from an existing instance
  app_settings.hpp, palette.hpp                       settings and Graphite colors (singletons)
  ids.hpp                                             id <-> QString helpers
  map_view_model.hpp                                  map scene for the canvas (singleton MapView)
  topics_model.hpp                                    topic list model (singleton Topics)
  concept_editor.hpp                                  concept fields (singleton Concept)
  concept_links_model.hpp                             links of one concept (singleton ConceptLinks)
  today_view_model.hpp                                session card numbers (singleton Today)
  app_context.hpp                                     owns and wires everything, provides singletons
modules/viewmodels/src/*.cpp                          one per header
modules/viewmodels/tests/                             doctest, QGuiApplication offscreen
modules/ui/include/atlas/ui/workspace_controller.hpp  forwarding header (until Plan 4)
modules/ui/include/atlas/ui/controller_error.hpp      forwarding header (until Plan 4)
modules/render/include/atlas/render/canvas_geometry.hpp   Qt free ring, dash, arrow geometry
modules/render/include/atlas/render/theme.hpp         Graphite tokens, ring bands
modules/render/include/atlas/render/graph_canvas_item.hpp rings, labels, arrows, ghosts, groups
modules/render/tests/test_canvas_geometry.cpp
modules/render/tests/test_graph_canvas_item.cpp, test_canvas_main.cpp
docs/DECISIONS.md
```

---

### Task 1: atlas-viewmodels module and the WorkspaceController move

**Files:**
- Create: `modules/viewmodels/CMakeLists.txt`, `modules/viewmodels/tests/CMakeLists.txt`, `modules/viewmodels/tests/test_main.cpp`
- Move: `modules/ui/include/atlas/ui/workspace_controller.hpp` to `modules/viewmodels/include/atlas/viewmodels/workspace_controller.hpp`; `controller_error.hpp` likewise; `modules/ui/src/workspace_controller.cpp` to `modules/viewmodels/src/`; `modules/ui/tests/test_workspace_controller.cpp` to `modules/viewmodels/tests/`
- Create: forwarding headers at the two old `atlas/ui` header paths
- Modify: `CMakeLists.txt`, `modules/ui/CMakeLists.txt`, `modules/ui/tests/CMakeLists.txt`

**Interfaces:**
- Produces: namespace `atlas::viewmodels` with `WorkspaceController`, `KnowledgeObjectEdits`, `ControllerFailure`, `ControllerErrorCode` (same API as before). CMake targets `atlas_viewmodels`, `atlas_viewmodelsplugin`, QML module `Atlas.ViewModels`. Test target `atlas_viewmodels_tests`.

- [ ] **Step 1: Move the files with history**

```bash
mkdir -p modules/viewmodels/include/atlas/viewmodels modules/viewmodels/src modules/viewmodels/tests
git mv modules/ui/include/atlas/ui/workspace_controller.hpp modules/viewmodels/include/atlas/viewmodels/workspace_controller.hpp
git mv modules/ui/include/atlas/ui/controller_error.hpp modules/viewmodels/include/atlas/viewmodels/controller_error.hpp
git mv modules/ui/src/workspace_controller.cpp modules/viewmodels/src/workspace_controller.cpp
git mv modules/ui/tests/test_workspace_controller.cpp modules/viewmodels/tests/test_workspace_controller.cpp
```

- [ ] **Step 2: Rename the namespace and strip the old essay comments**

```bash
cd modules/viewmodels
sed -i 's/namespace atlas::ui/namespace atlas::viewmodels/; s#atlas/ui/#atlas/viewmodels/#g' include/atlas/viewmodels/*.hpp src/workspace_controller.cpp
sed -i 's/using namespace atlas::ui;/using namespace atlas::viewmodels;/; s#atlas/ui/#atlas/viewmodels/#g' tests/test_workspace_controller.cpp
sed -i '/^\s*\/\//d' include/atlas/viewmodels/*.hpp src/workspace_controller.cpp tests/test_workspace_controller.cpp
for f in include/atlas/viewmodels/*.hpp src/workspace_controller.cpp tests/test_workspace_controller.cpp; do cat -s "$f" > "$f.tmp" && mv "$f.tmp" "$f"; done
grep -nP '\x{2014}' include/atlas/viewmodels/*.hpp src/workspace_controller.cpp tests/test_workspace_controller.cpp
cd ../..
```

The last `grep` must print nothing. If it prints a line (a trailing comment after code), delete that trailing comment by hand.

- [ ] **Step 3: Create the forwarding headers**

`modules/ui/include/atlas/ui/workspace_controller.hpp`:

```cpp
#pragma once

#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::ui {
using atlas::viewmodels::KnowledgeObjectEdits;
using atlas::viewmodels::WorkspaceController;
}  // namespace atlas::ui
```

`modules/ui/include/atlas/ui/controller_error.hpp`:

```cpp
#pragma once

#include "atlas/viewmodels/controller_error.hpp"

namespace atlas::ui {
using atlas::viewmodels::ControllerErrorCode;
using atlas::viewmodels::ControllerFailure;
}  // namespace atlas::ui
```

- [ ] **Step 4: Build files**

`modules/viewmodels/CMakeLists.txt`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

set(CMAKE_AUTOMOC ON)

qt_add_library(atlas_viewmodels STATIC)
qt_add_qml_module(atlas_viewmodels
    URI Atlas.ViewModels
    VERSION 1.0
    SOURCES
        include/atlas/viewmodels/controller_error.hpp
        include/atlas/viewmodels/workspace_controller.hpp
        src/workspace_controller.cpp
)

target_include_directories(atlas_viewmodels
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/atlas/viewmodels
)
target_compile_features(atlas_viewmodels PUBLIC cxx_std_20)
target_link_libraries(atlas_viewmodels PUBLIC
    atlas_core atlas_persistence atlas_graph atlas_learning atlas_render atlas_render_canvas
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

`modules/viewmodels/tests/CMakeLists.txt`:

```cmake
add_executable(atlas_viewmodels_tests
    test_main.cpp
    test_workspace_controller.cpp
)

target_link_libraries(atlas_viewmodels_tests PRIVATE atlas_viewmodels atlas_viewmodelsplugin)
target_include_directories(atlas_viewmodels_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)

add_test(NAME atlas_viewmodels_tests COMMAND atlas_viewmodels_tests)
set_tests_properties(atlas_viewmodels_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

`modules/viewmodels/tests/test_main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QGuiApplication>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Atlas_ViewModelsPlugin)

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
```

Root `CMakeLists.txt`: inside `if (ATLAS_BUILD_UI)`, add `add_subdirectory(modules/viewmodels)` as the first line (before `modules/ui`).

`modules/ui/CMakeLists.txt`: remove the lines `include/atlas/ui/workspace_controller.hpp` and `src/workspace_controller.cpp` from `add_library(atlas_ui ...)`, and add `atlas_viewmodels` as the first item of `target_link_libraries(atlas_ui PUBLIC ...)`.

`modules/ui/tests/CMakeLists.txt`: remove the line `test_workspace_controller.cpp`.

- [ ] **Step 5: Build and run everything**

Run: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
Expected: zero warnings; `atlas_viewmodels_tests` runs the moved controller tests and passes; `atlas_ui_tests` still passes through the forwarding headers.

- [ ] **Step 6: Commit**

```bash
git add -A modules/viewmodels modules/ui CMakeLists.txt
git commit -m "refactor: move WorkspaceController into a new atlas-viewmodels module"
```

---

### Task 2: Clock and MemoryController

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/clock.hpp`, `src/clock.cpp`
- Create: `modules/viewmodels/include/atlas/viewmodels/memory_controller.hpp`, `src/memory_controller.cpp`
- Test: `modules/viewmodels/tests/test_memory_controller.cpp`
- Modify: both viewmodels `CMakeLists.txt` files (add the new headers and sources to `SOURCES`, the test to the executable)

**Interfaces:**
- Consumes: `LearningRepository` (`allEvents`, `isCacheFresh`, `allStates`, `replaceStates`), `MemoryLedger::replay`, `NetworkRules`, `SessionPlanner`, `medianRebuildResponse`, `kReplayVersion`, `WorkspaceController::graph()` and `graphChanged`.
- Produces:
  - `using Clock = std::function<atlas::core::TimePoint()>; Clock systemClock();`
  - `class MemoryController : public QObject` with `MemoryController(Database&, WorkspaceController&, Clock, QObject* parent = nullptr)`, `Result<void, ControllerFailure> load()`, `std::optional<double> recallChance(const ItemRef&) const` (nullopt when not introduced), `int introducedToday() const`, `SessionPlan todayPlan(const SessionLimits&) const`, `const StateMap& states() const`, `const std::vector<ReviewEvent>& events() const`, `const NetworkRules& rules() const`, `TimePoint now() const`, signal `memoryChanged()` (also emitted on every `graphChanged`).

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_memory_controller.cpp`

```cpp
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

    KnowledgeObjectId concept(const char* title) { return workspace.createKnowledgeObject(title).value(); }
};

}  // namespace

TEST_CASE("load rebuilds a stale cache from the log and marks it fresh") {
    Fixture f;
    auto tree = f.concept("Tree");
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
    auto tree = f.concept("Tree");
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
    auto tree = f.concept("Tree");
    auto hash = f.concept("Hash Table");
    LearningRepository repository(f.db);
    REQUIRE(repository.record({reviewOf(tree, f.now, Grade::Good)}, {}).hasValue());
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());

    CHECK(memory.recallChance(ItemRef::forConcept(tree)) == doctest::Approx(1.0));
    CHECK_FALSE(memory.recallChance(ItemRef::forConcept(hash)).has_value());
}

TEST_CASE("introducedToday counts concepts first reviewed on the local calendar day") {
    Fixture f;
    auto today = f.concept("Today");
    auto earlier = f.concept("Earlier");
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
    f.concept("A");
    f.concept("B");
    MemoryController memory(f.db, f.workspace, f.clock);
    REQUIRE(memory.load().hasValue());
    auto plan = memory.todayPlan(atlas::learning::SessionLimits{});
    CHECK(plan.focuses.size() == 2);
    CHECK(plan.focuses[0].isNew);
}

TEST_CASE("memoryChanged fires on load and on every graph change") {
    Fixture f;
    MemoryController memory(f.db, f.workspace, f.clock);
    int signals = 0;
    QObject::connect(&memory, &MemoryController::memoryChanged, [&] { ++signals; });
    REQUIRE(memory.load().hasValue());
    f.concept("New");
    CHECK(signals == 2);
}
```

Add `test_memory_controller.cpp` to the test executable, and `include/atlas/viewmodels/clock.hpp`, `include/atlas/viewmodels/memory_controller.hpp`, `src/clock.cpp`, `src/memory_controller.cpp` to `SOURCES`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `memory_controller.hpp` not found.

- [ ] **Step 3: Create `clock.hpp` and `clock.cpp`**

```cpp
#pragma once

#include <functional>

#include "atlas/core/memory.hpp"

namespace atlas::viewmodels {

using Clock = std::function<atlas::core::TimePoint()>;

Clock systemClock();

}  // namespace atlas::viewmodels
```

```cpp
#include "atlas/viewmodels/clock.hpp"

namespace atlas::viewmodels {

Clock systemClock() {
    return [] { return std::chrono::system_clock::now(); };
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `memory_controller.hpp`**

```cpp
#pragma once

#include <QObject>

#include <optional>
#include <vector>

#include "atlas/learning/memory_ledger.hpp"
#include "atlas/learning/network_rules.hpp"
#include "atlas/learning/session_planner.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/clock.hpp"
#include "atlas/viewmodels/controller_error.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class MemoryController : public QObject {
    Q_OBJECT

public:
    MemoryController(atlas::persistence::Database& database, WorkspaceController& workspace, Clock clock,
                     QObject* parent = nullptr);

    Result<void, ControllerFailure> load();

    std::optional<double> recallChance(const atlas::core::ItemRef& item) const;
    int introducedToday() const;
    atlas::learning::SessionPlan todayPlan(const atlas::learning::SessionLimits& limits) const;

    const atlas::learning::StateMap& states() const { return states_; }
    const std::vector<atlas::core::ReviewEvent>& events() const { return events_; }
    const atlas::learning::NetworkRules& rules() const { return rules_; }
    atlas::core::TimePoint now() const { return clock_(); }

signals:
    void memoryChanged();

private:
    atlas::persistence::LearningRepository repository_;
    WorkspaceController* workspace_;
    Clock clock_;
    atlas::learning::Fsrs fsrs_;
    atlas::learning::NetworkRules rules_;
    atlas::learning::SessionPlanner planner_;
    atlas::learning::MemoryLedger ledger_;
    std::vector<atlas::core::ReviewEvent> events_;
    atlas::learning::StateMap states_;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Create `memory_controller.cpp`**

```cpp
#include "atlas/viewmodels/memory_controller.hpp"

#include <QDateTime>

#include <unordered_map>

#include "atlas/learning/rebuild_grader.hpp"

namespace atlas::viewmodels {

using atlas::core::ItemKind;
using atlas::core::ItemRef;
using atlas::core::MemoryState;
using atlas::core::Phase;
using atlas::core::TimePoint;

namespace {

QDate localDate(TimePoint time) {
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(millis).date();
}

ControllerFailure persistenceFailure(const atlas::persistence::PersistenceError& error) {
    return ControllerFailure{ControllerErrorCode::PersistenceFailed, error.detail};
}

}  // namespace

MemoryController::MemoryController(atlas::persistence::Database& database, WorkspaceController& workspace,
                                   Clock clock, QObject* parent)
    : QObject(parent),
      repository_(database),
      workspace_(&workspace),
      clock_(std::move(clock)),
      rules_(workspace.graph(), fsrs_),
      planner_(workspace.graph(), rules_, fsrs_),
      ledger_(fsrs_) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &MemoryController::memoryChanged);
}

Result<void, ControllerFailure> MemoryController::load() {
    using Out = Result<void, ControllerFailure>;
    auto events = repository_.allEvents();
    if (!events.hasValue()) return Out::err(persistenceFailure(events.error()));
    events_ = std::move(events).value();

    auto fresh = repository_.isCacheFresh(atlas::learning::kReplayVersion);
    if (!fresh.hasValue()) return Out::err(persistenceFailure(fresh.error()));

    states_.clear();
    if (fresh.value()) {
        auto stored = repository_.allStates();
        if (!stored.hasValue()) return Out::err(persistenceFailure(stored.error()));
        for (auto& state : stored.value()) states_.emplace(state.item, state);
    } else {
        states_ = ledger_.replay(events_, clock_(), rules_.boostFn());
        std::vector<MemoryState> rebuilt;
        rebuilt.reserve(states_.size());
        for (const auto& entry : states_) rebuilt.push_back(entry.second);
        auto replaced = repository_.replaceStates(rebuilt, atlas::learning::kReplayVersion);
        if (!replaced.hasValue()) return Out::err(persistenceFailure(replaced.error()));
    }

    emit memoryChanged();
    return Out::ok();
}

std::optional<double> MemoryController::recallChance(const ItemRef& item) const {
    auto it = states_.find(item);
    if (it == states_.end() || it->second.phase == Phase::New) return std::nullopt;
    return fsrs_.recallChance(it->second, clock_());
}

int MemoryController::introducedToday() const {
    std::unordered_map<ItemRef, TimePoint> firstReview;
    for (const auto& event : events_) {
        if (event.item.kind != ItemKind::Concept) continue;
        auto [it, inserted] = firstReview.try_emplace(event.item, event.reviewedAt);
        if (!inserted && event.reviewedAt < it->second) it->second = event.reviewedAt;
    }
    QDate today = localDate(clock_());
    int count = 0;
    for (const auto& entry : firstReview) {
        if (localDate(entry.second) == today) ++count;
    }
    return count;
}

atlas::learning::SessionPlan MemoryController::todayPlan(const atlas::learning::SessionLimits& limits) const {
    return planner_.plan(states_, clock_(), introducedToday(), limits,
                         atlas::learning::medianRebuildResponse(events_));
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 6: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): load the review log and rebuild stale memory caches"
```

---

### Task 3: PlacementController

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/placement_controller.hpp`, `src/placement_controller.cpp`
- Test: `modules/viewmodels/tests/test_placement_controller.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: `PlacementRepository` (`saveAll`, `findAll`), `ForceDirectedLayout::compute(graph, config, hints)`, `LayoutHints`, `Point2D`, `WorkspaceController::graph()` and `graphChanged`.
- Produces: `class PlacementController : public QObject` with `PlacementController(Database&, WorkspaceController&, QObject* parent = nullptr)`, `Result<void, ControllerFailure> load()`, `arrange()`, `tidy()`, `setPinned(const KnowledgeObjectId&, bool)`, `std::optional<atlas::render::Point2D> position(const KnowledgeObjectId&) const`, `bool isPinned(const KnowledgeObjectId&) const`, signals `placementsChanged()` and `failed(QString message)`.
- Contract (from DECISIONS): saved positions never move during `load` or `arrange`; only unplaced concepts get physics; `tidy` is the only full layout pass and keeps user pinned concepts fixed.

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_placement_controller.cpp`

```cpp
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/placement_repository.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
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
    KnowledgeObjectId a = concept("A");
    KnowledgeObjectId b = concept("B");
    KnowledgeObjectId c = concept("C");

    KnowledgeObjectId concept(const char* title) {
        REQUIRE(loaded);
        return workspace.createKnowledgeObject(title).value();
    }
};

bool samePoint(const atlas::render::Point2D& left, const atlas::render::Point2D& right) {
    return left.x == right.x && left.y == right.y;
}

}  // namespace

TEST_CASE("load places every concept and saves the positions") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    for (const auto& id : {f.a, f.b, f.c}) CHECK(placements.position(id).has_value());
    PlacementRepository repository(f.db);
    CHECK(repository.findAll().value().size() == 3);
}

TEST_CASE("adding a concept never moves saved concepts") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    auto before = *placements.position(f.a);
    auto added = f.workspace.createKnowledgeObject("D").value();
    REQUIRE(f.workspace.createRelationship(added, f.a, RelationshipType::DependsOn, std::nullopt).hasValue());
    CHECK(samePoint(*placements.position(f.a), before));
    CHECK(placements.position(added).has_value());
}

TEST_CASE("removing a concept forgets its position") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(f.workspace.removeKnowledgeObject(f.c).hasValue());
    CHECK_FALSE(placements.position(f.c).has_value());
}

TEST_CASE("positions survive a restart") {
    Fixture f;
    atlas::render::Point2D saved;
    {
        PlacementController first(f.db, f.workspace);
        REQUIRE(first.load().hasValue());
        saved = *first.position(f.b);
    }
    PlacementController second(f.db, f.workspace);
    REQUIRE(second.load().hasValue());
    CHECK(samePoint(*second.position(f.b), saved));
}

TEST_CASE("tidy keeps pinned concepts exactly where they are") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(f.workspace.createRelationship(f.a, f.b, RelationshipType::DependsOn, std::nullopt).hasValue());
    REQUIRE(placements.setPinned(f.a, true).hasValue());
    auto pinnedBefore = *placements.position(f.a);
    REQUIRE(placements.tidy().hasValue());
    CHECK(placements.isPinned(f.a));
    CHECK(samePoint(*placements.position(f.a), pinnedBefore));
}

TEST_CASE("placementsChanged fires when new concepts are placed") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    int signals = 0;
    QObject::connect(&placements, &PlacementController::placementsChanged, [&] { ++signals; });
    REQUIRE(placements.load().hasValue());
    f.workspace.createKnowledgeObject("E");
    CHECK(signals == 2);
}
```

The fixture's `loaded` member runs `WorkspaceController::load()` before the concept members are created.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `placement_controller.hpp` not found.

- [ ] **Step 3: Create `placement_controller.hpp`**

```cpp
#pragma once

#include <QObject>
#include <QString>

#include <optional>
#include <unordered_map>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/persistence/placement_repository.hpp"
#include "atlas/render/force_directed_layout.hpp"
#include "atlas/viewmodels/controller_error.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class PlacementController : public QObject {
    Q_OBJECT

public:
    PlacementController(atlas::persistence::Database& database, WorkspaceController& workspace,
                        QObject* parent = nullptr);

    Result<void, ControllerFailure> load();
    Result<void, ControllerFailure> arrange();
    Result<void, ControllerFailure> tidy();
    Result<void, ControllerFailure> setPinned(const KnowledgeObjectId& id, bool pinned);

    std::optional<atlas::render::Point2D> position(const KnowledgeObjectId& id) const;
    bool isPinned(const KnowledgeObjectId& id) const;

signals:
    void placementsChanged();
    void failed(const QString& message);

private:
    Result<void, ControllerFailure> save(const std::vector<atlas::core::Placement>& changed);
    void forgetRemovedConcepts();

    atlas::persistence::PlacementRepository repository_;
    WorkspaceController* workspace_;
    std::unordered_map<KnowledgeObjectId, atlas::core::Placement> placements_;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `placement_controller.cpp`**

```cpp
#include "atlas/viewmodels/placement_controller.hpp"

#include <iterator>

namespace atlas::viewmodels {

using atlas::core::Placement;
using atlas::render::ForceDirectedLayout;
using atlas::render::LayoutHints;

namespace {

Result<void, ControllerFailure> persistenceFailure(const atlas::persistence::PersistenceError& error) {
    return Result<void, ControllerFailure>::err({ControllerErrorCode::PersistenceFailed, error.detail});
}

}  // namespace

PlacementController::PlacementController(atlas::persistence::Database& database, WorkspaceController& workspace,
                                         QObject* parent)
    : QObject(parent), repository_(database), workspace_(&workspace) {
    connect(workspace_, &WorkspaceController::graphChanged, this, [this] {
        auto arranged = arrange();
        if (!arranged.hasValue()) emit failed(QString::fromStdString(arranged.error().detail));
    });
}

Result<void, ControllerFailure> PlacementController::load() {
    auto stored = repository_.findAll();
    if (!stored.hasValue()) return persistenceFailure(stored.error());
    placements_.clear();
    for (const auto& placement : stored.value()) placements_.insert_or_assign(placement.conceptId, placement);
    return arrange();
}

void PlacementController::forgetRemovedConcepts() {
    const auto& graph = workspace_->graph();
    for (auto it = placements_.begin(); it != placements_.end();) {
        it = graph.findNode(it->first) == nullptr ? placements_.erase(it) : std::next(it);
    }
}

Result<void, ControllerFailure> PlacementController::arrange() {
    forgetRemovedConcepts();
    const auto& graph = workspace_->graph();
    LayoutHints hints;
    bool anyUnplaced = false;
    for (const auto& id : graph.allNodeIds()) {
        auto saved = placements_.find(id);
        if (saved == placements_.end()) {
            anyUnplaced = true;
            continue;
        }
        hints.initial[id] = {saved->second.x, saved->second.y};
        hints.pinned.insert(id);
    }
    if (!anyUnplaced) return Result<void, ControllerFailure>::ok();

    std::vector<Placement> added;
    for (const auto& [id, point] : ForceDirectedLayout::compute(graph, {}, hints)) {
        if (!placements_.contains(id)) added.push_back(Placement{id, point.x, point.y, false});
    }
    return save(added);
}

Result<void, ControllerFailure> PlacementController::tidy() {
    forgetRemovedConcepts();
    LayoutHints hints;
    for (const auto& [id, placement] : placements_) {
        hints.initial[id] = {placement.x, placement.y};
        if (placement.pinned) hints.pinned.insert(id);
    }
    std::vector<Placement> moved;
    for (const auto& [id, point] : ForceDirectedLayout::compute(workspace_->graph(), {}, hints)) {
        moved.push_back(Placement{id, point.x, point.y, isPinned(id)});
    }
    return save(moved);
}

Result<void, ControllerFailure> PlacementController::setPinned(const KnowledgeObjectId& id, bool pinned) {
    auto found = placements_.find(id);
    if (found == placements_.end()) {
        return Result<void, ControllerFailure>::err({ControllerErrorCode::NotFound, "No placement for that concept"});
    }
    Placement updated = found->second;
    updated.pinned = pinned;
    return save({updated});
}

Result<void, ControllerFailure> PlacementController::save(const std::vector<Placement>& changed) {
    if (changed.empty()) return Result<void, ControllerFailure>::ok();
    auto saved = repository_.saveAll(changed);
    if (!saved.hasValue()) return persistenceFailure(saved.error());
    for (const auto& placement : changed) placements_.insert_or_assign(placement.conceptId, placement);
    emit placementsChanged();
    return Result<void, ControllerFailure>::ok();
}

std::optional<atlas::render::Point2D> PlacementController::position(const KnowledgeObjectId& id) const {
    auto found = placements_.find(id);
    if (found == placements_.end()) return std::nullopt;
    return atlas::render::Point2D{found->second.x, found->second.y};
}

bool PlacementController::isPinned(const KnowledgeObjectId& id) const {
    auto found = placements_.find(id);
    return found != placements_.end() && found->second.pinned;
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): keep concepts where they were placed"
```

---

### Task 4: Graphite colors, AppSettings, Palette, provided singletons

**Files:**
- Modify: `modules/render/include/atlas/render/theme.hpp`, rewrite `modules/render/src/theme.cpp`
- Create: `modules/viewmodels/include/atlas/viewmodels/provided_singleton.hpp`
- Create: `app_settings.hpp`/`.cpp`, `palette.hpp`/`.cpp` (viewmodels)
- Test: `modules/viewmodels/tests/test_appearance.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Produces (render): new `Theme` fields `surface, surfaceRaised, border, text, textMuted, nodeFill, label, ringStrong, ringMedium, ringWeak, ringNew, ringTrack, danger`; `enum class RingBand { New, Weak, Medium, Strong }`; `kStrongRecall = 0.80`, `kMediumRecall = 0.50`; `RingBand ringBand(double recall)`; `QColor ringColor(const Theme&, double recall)`.
- Produces (viewmodels): `template <typename T> class ProvidedSingleton` with `static void provide(T*)` and `static T* create(QQmlEngine*, QJSEngine*)`.
- `AppSettings(QSettings& store, QObject* parent = nullptr)`, QML singleton `AppSettings`, properties `darkTheme` (default true), `reducedMotion` (default false), `newPerDay` (default 5, clamped 1 to 20).
- `Palette(AppSettings&, QObject* parent = nullptr)`, QML singleton `Palette`, read-only color properties `background, surface, surfaceRaised, border, text, textMuted, accent, danger, nodeFill, ringStrong, ringMedium, ringWeak, ringNew, ringTrack`, signal `changed()`, `ThemeMode mode() const`, `Q_INVOKABLE QColor ringColor(double recall) const`.

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_appearance.cpp`

```cpp
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>
#include <QTemporaryDir>

#include <cmath>
#include <memory>

#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "doctest.h"

using namespace atlas::viewmodels;
using atlas::render::RingBand;

TEST_CASE("settings persist and newPerDay is clamped") {
    QTemporaryDir dir;
    {
        QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
        AppSettings settings(store);
        CHECK(settings.darkTheme());
        CHECK_FALSE(settings.reducedMotion());
        CHECK(settings.newPerDay() == 5);
        settings.setDarkTheme(false);
        settings.setReducedMotion(true);
        settings.setNewPerDay(50);
        CHECK(settings.newPerDay() == 20);
        settings.setNewPerDay(0);
        CHECK(settings.newPerDay() == 1);
    }
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings reopened(store);
    CHECK_FALSE(reopened.darkTheme());
    CHECK(reopened.reducedMotion());
    CHECK(reopened.newPerDay() == 1);
}

TEST_CASE("the palette follows the theme setting") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings settings(store);
    Palette palette(settings);
    int changes = 0;
    QObject::connect(&palette, &Palette::changed, [&] { ++changes; });

    QColor darkBackground = palette.background();
    settings.setDarkTheme(false);
    CHECK(palette.background() != darkBackground);
    CHECK(palette.mode() == atlas::render::ThemeMode::Light);
    CHECK(changes == 1);
}

TEST_CASE("ring bands follow the recall thresholds") {
    CHECK(atlas::render::ringBand(-1.0) == RingBand::New);
    CHECK(atlas::render::ringBand(0.2) == RingBand::Weak);
    CHECK(atlas::render::ringBand(0.5) == RingBand::Medium);
    CHECK(atlas::render::ringBand(0.79) == RingBand::Medium);
    CHECK(atlas::render::ringBand(0.8) == RingBand::Strong);
    CHECK(atlas::render::ringBand(std::nan("")) == RingBand::New);
}

TEST_CASE("QML sees the provided settings and palette instances") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings settings(store);
    Palette palette(settings);
    settings.setNewPerDay(7);
    AppSettings::provide(&settings);
    Palette::provide(&palette);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml\nimport Atlas.ViewModels\n"
                      "QtObject { property int perDay: AppSettings.newPerDay; property color bg: Palette.background }",
                      QUrl());
    std::unique_ptr<QObject> object(component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(object != nullptr);
    CHECK(object->property("perDay").toInt() == 7);
    CHECK(object->property("bg").value<QColor>() == palette.background());
}
```


- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `app_settings.hpp` not found.

- [ ] **Step 3: Extend `modules/render/include/atlas/render/theme.hpp`**

Replace the file with:

```cpp
#pragma once

#include <QColor>

#include <array>

namespace atlas::render {

enum class ThemeMode {
    Dark,
    Light,
};

struct Theme {
    QColor background;
    QColor dot;
    QColor edge;
    QColor edgeDimmed;
    QColor selectedRing;
    QColor neighborRing;
    QColor hoverRing;
    QColor nodeBorder;
    std::array<QColor, 4> nodeDifficulty;

    QColor panelBackground;
    QColor panelAlternateBackground;
    QColor panelText;
    QColor panelBorder;
    QColor accent;

    QColor surface;
    QColor surfaceRaised;
    QColor border;
    QColor text;
    QColor textMuted;
    QColor nodeFill;
    QColor label;
    QColor ringStrong;
    QColor ringMedium;
    QColor ringWeak;
    QColor ringNew;
    QColor ringTrack;
    QColor danger;
};

const Theme& themeFor(ThemeMode mode);

enum class RingBand { New, Weak, Medium, Strong };

inline constexpr double kStrongRecall = 0.80;
inline constexpr double kMediumRecall = 0.50;

RingBand ringBand(double recall);
QColor ringColor(const Theme& theme, double recall);

}  // namespace atlas::render
```

- [ ] **Step 4: Rewrite `modules/render/src/theme.cpp`**

The `panel*` and `nodeDifficulty` values are unchanged (only the Widgets UI uses them until Plan 4).

```cpp
#include "atlas/render/theme.hpp"

namespace atlas::render {

namespace {

constexpr QRgb kBlack = 0x000000;
constexpr QRgb kBrown = 0x1F150C;
constexpr QRgb kCoffee = 0x412D15;
constexpr QRgb kBeige = 0xE1DCC9;

QColor withAlpha(QRgb rgb, int alpha) {
    QColor color(rgb);
    color.setAlpha(alpha);
    return color;
}

Theme makeDark() {
    Theme theme;
    theme.background = QColor(0x18181b);
    theme.dot = withAlpha(0xe4e4e7, 14);
    theme.edge = QColor(0x52525b);
    theme.edgeDimmed = withAlpha(0x52525b, 80);
    theme.selectedRing = QColor(0xe4e4e7);
    theme.neighborRing = withAlpha(0xa1a1aa, 160);
    theme.hoverRing = withAlpha(0xe4e4e7, 120);
    theme.nodeBorder = QColor(0x3f3f46);
    theme.nodeDifficulty = {QColor(100, 200, 120), QColor(100, 160, 220), QColor(230, 170, 60), QColor(220, 90, 90)};

    theme.panelBackground = QColor(kCoffee);
    theme.panelAlternateBackground = QColor(kCoffee).lighter(122);
    theme.panelText = QColor(kBeige);
    theme.panelBorder = QColor(kBrown);
    theme.accent = QColor(0xa3e635);

    theme.surface = QColor(0x1f1f23);
    theme.surfaceRaised = QColor(0x27272a);
    theme.border = QColor(0x2e2e33);
    theme.text = QColor(0xe4e4e7);
    theme.textMuted = QColor(0xa1a1aa);
    theme.nodeFill = QColor(0x27272a);
    theme.label = QColor(0xa1a1aa);
    theme.ringStrong = QColor(0xa3e635);
    theme.ringMedium = QColor(0xe4e4e7);
    theme.ringWeak = QColor(0xfb7185);
    theme.ringNew = QColor(0x52525b);
    theme.ringTrack = QColor(0x2e2e33);
    theme.danger = QColor(0xfb7185);
    return theme;
}

Theme makeLight() {
    Theme theme;
    theme.background = QColor(0xfafafa);
    theme.dot = withAlpha(0x18181b, 18);
    theme.edge = QColor(0xa1a1aa);
    theme.edgeDimmed = withAlpha(0xa1a1aa, 70);
    theme.selectedRing = QColor(0x18181b);
    theme.neighborRing = withAlpha(0x52525b, 160);
    theme.hoverRing = withAlpha(0x18181b, 110);
    theme.nodeBorder = QColor(0xd4d4d8);
    theme.nodeDifficulty = {QColor(60, 140, 80), QColor(50, 100, 165), QColor(180, 120, 20), QColor(175, 60, 60)};

    theme.panelBackground = QColor(kBeige);
    theme.panelAlternateBackground = QColor(kBeige).darker(107);
    theme.panelText = QColor(kBlack);
    theme.panelBorder = QColor(kCoffee);
    theme.accent = QColor(0x4d7c0f);

    theme.surface = QColor(0xf4f4f5);
    theme.surfaceRaised = QColor(0xffffff);
    theme.border = QColor(0xe4e4e7);
    theme.text = QColor(0x18181b);
    theme.textMuted = QColor(0x52525b);
    theme.nodeFill = QColor(0xffffff);
    theme.label = QColor(0x52525b);
    theme.ringStrong = QColor(0x4d7c0f);
    theme.ringMedium = QColor(0x71717a);
    theme.ringWeak = QColor(0xe11d48);
    theme.ringNew = QColor(0xa1a1aa);
    theme.ringTrack = QColor(0xe4e4e7);
    theme.danger = QColor(0xe11d48);
    return theme;
}

}  // namespace

const Theme& themeFor(ThemeMode mode) {
    static const Theme dark = makeDark();
    static const Theme light = makeLight();
    return mode == ThemeMode::Dark ? dark : light;
}

RingBand ringBand(double recall) {
    if (!(recall >= 0.0)) return RingBand::New;
    if (recall >= kStrongRecall) return RingBand::Strong;
    if (recall >= kMediumRecall) return RingBand::Medium;
    return RingBand::Weak;
}

QColor ringColor(const Theme& theme, double recall) {
    switch (ringBand(recall)) {
        case RingBand::Strong: return theme.ringStrong;
        case RingBand::Medium: return theme.ringMedium;
        case RingBand::Weak: return theme.ringWeak;
        case RingBand::New: return theme.ringNew;
    }
    return theme.ringNew;
}

}  // namespace atlas::render
```

- [ ] **Step 5: Create `provided_singleton.hpp`**

```cpp
#pragma once

#include <QJSEngine>

class QQmlEngine;

namespace atlas::viewmodels {

// QML calls create() only when T is not default-constructible.
template <typename T>
class ProvidedSingleton {
public:
    static void provide(T* instance) { instance_ = instance; }

    static T* create(QQmlEngine*, QJSEngine*) {
        Q_ASSERT_X(instance_ != nullptr, "ProvidedSingleton", "provide() must run before QML loads");
        QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
        return instance_;
    }

private:
    static inline T* instance_ = nullptr;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 6: Create `app_settings.hpp` and `app_settings.cpp`**

```cpp
#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

#include "atlas/viewmodels/provided_singleton.hpp"

namespace atlas::viewmodels {

class AppSettings : public QObject, public ProvidedSingleton<AppSettings> {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(int newPerDay READ newPerDay WRITE setNewPerDay NOTIFY newPerDayChanged)

public:
    static constexpr int kMinNewPerDay = 1;
    static constexpr int kMaxNewPerDay = 20;
    static constexpr int kDefaultNewPerDay = 5;

    explicit AppSettings(QSettings& store, QObject* parent = nullptr);

    bool darkTheme() const;
    void setDarkTheme(bool dark);
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    int newPerDay() const;
    void setNewPerDay(int count);

signals:
    void darkThemeChanged();
    void reducedMotionChanged();
    void newPerDayChanged();

private:
    QSettings* store_;
};

}  // namespace atlas::viewmodels
```

```cpp
#include "atlas/viewmodels/app_settings.hpp"

#include <algorithm>

namespace atlas::viewmodels {

namespace {
constexpr auto kDarkThemeKey = "appearance/darkTheme";
constexpr auto kReducedMotionKey = "appearance/reducedMotion";
constexpr auto kNewPerDayKey = "learning/newPerDay";
}  // namespace

AppSettings::AppSettings(QSettings& store, QObject* parent) : QObject(parent), store_(&store) {}

bool AppSettings::darkTheme() const { return store_->value(kDarkThemeKey, true).toBool(); }

void AppSettings::setDarkTheme(bool dark) {
    if (dark == darkTheme()) return;
    store_->setValue(kDarkThemeKey, dark);
    emit darkThemeChanged();
}

bool AppSettings::reducedMotion() const { return store_->value(kReducedMotionKey, false).toBool(); }

void AppSettings::setReducedMotion(bool reduced) {
    if (reduced == reducedMotion()) return;
    store_->setValue(kReducedMotionKey, reduced);
    emit reducedMotionChanged();
}

int AppSettings::newPerDay() const {
    return std::clamp(store_->value(kNewPerDayKey, kDefaultNewPerDay).toInt(), kMinNewPerDay, kMaxNewPerDay);
}

void AppSettings::setNewPerDay(int count) {
    int clamped = std::clamp(count, kMinNewPerDay, kMaxNewPerDay);
    if (clamped == newPerDay()) return;
    store_->setValue(kNewPerDayKey, clamped);
    emit newPerDayChanged();
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 7: Create `palette.hpp` and `palette.cpp`**

```cpp
#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "atlas/render/theme.hpp"
#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"

namespace atlas::viewmodels {

class Palette : public QObject, public ProvidedSingleton<Palette> {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceRaised READ surfaceRaised NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)
    Q_PROPERTY(QColor nodeFill READ nodeFill NOTIFY changed)
    Q_PROPERTY(QColor ringStrong READ ringStrong NOTIFY changed)
    Q_PROPERTY(QColor ringMedium READ ringMedium NOTIFY changed)
    Q_PROPERTY(QColor ringWeak READ ringWeak NOTIFY changed)
    Q_PROPERTY(QColor ringNew READ ringNew NOTIFY changed)
    Q_PROPERTY(QColor ringTrack READ ringTrack NOTIFY changed)

public:
    explicit Palette(AppSettings& settings, QObject* parent = nullptr);

    atlas::render::ThemeMode mode() const;
    Q_INVOKABLE QColor ringColor(double recall) const;

    QColor background() const { return theme().background; }
    QColor surface() const { return theme().surface; }
    QColor surfaceRaised() const { return theme().surfaceRaised; }
    QColor border() const { return theme().border; }
    QColor text() const { return theme().text; }
    QColor textMuted() const { return theme().textMuted; }
    QColor accent() const { return theme().accent; }
    QColor danger() const { return theme().danger; }
    QColor nodeFill() const { return theme().nodeFill; }
    QColor ringStrong() const { return theme().ringStrong; }
    QColor ringMedium() const { return theme().ringMedium; }
    QColor ringWeak() const { return theme().ringWeak; }
    QColor ringNew() const { return theme().ringNew; }
    QColor ringTrack() const { return theme().ringTrack; }

signals:
    void changed();

private:
    const atlas::render::Theme& theme() const;

    AppSettings* settings_;
};

}  // namespace atlas::viewmodels
```

```cpp
#include "atlas/viewmodels/palette.hpp"

namespace atlas::viewmodels {

using atlas::render::ThemeMode;

Palette::Palette(AppSettings& settings, QObject* parent) : QObject(parent), settings_(&settings) {
    connect(settings_, &AppSettings::darkThemeChanged, this, &Palette::changed);
}

ThemeMode Palette::mode() const { return settings_->darkTheme() ? ThemeMode::Dark : ThemeMode::Light; }

const atlas::render::Theme& Palette::theme() const { return atlas::render::themeFor(mode()); }

QColor Palette::ringColor(double recall) const { return atlas::render::ringColor(theme(), recall); }

}  // namespace atlas::viewmodels
```

Add the four new headers and two sources to `SOURCES`, and `test_appearance.cpp` to the test executable.

- [ ] **Step 8: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS, including `atlas_ui_tests` (Widgets still compile with the extended Theme).

- [ ] **Step 9: Commit**

```bash
git add modules/render modules/viewmodels
git commit -m "feat: add Graphite colors, app settings, and the palette singleton"
```

---

### Task 5: Canvas geometry helpers (Qt free)

**Files:**
- Create: `modules/render/include/atlas/render/canvas_geometry.hpp`, `modules/render/src/canvas_geometry.cpp`
- Test: `modules/render/tests/test_canvas_geometry.cpp`
- Modify: `modules/render/CMakeLists.txt` (add `src/canvas_geometry.cpp` to `atlas_render`), `modules/render/tests/CMakeLists.txt` (add the test to `atlas_render_tests`)

**Interfaces:**
- Produces (namespace `atlas::render`): `struct Vec2 { float x = 0.0f; float y = 0.0f; };`, `struct LineSegment { Vec2 from; Vec2 to; };`, `std::vector<Vec2> ringArc(Vec2 center, float radius, float thickness, double fraction)` (triangle list, 6 vertices per segment, 48 segments for a full ring, starts at 12 o'clock, clockwise on screen), `std::vector<Vec2> dashedRing(Vec2 center, float radius, float thickness, int dashCount)`, `std::vector<LineSegment> dashedLine(Vec2 from, Vec2 to, float dash, float gap)`, `Vec2 trimmedEnd(Vec2 from, Vec2 to, float radius)` (the point `radius` before `to` on the way from `from`), `std::array<Vec2, 3> arrowHead(Vec2 from, Vec2 to, float targetRadius, float length, float halfWidth)` (tip first).

- [ ] **Step 1: Write the failing test** `modules/render/tests/test_canvas_geometry.cpp`

```cpp
#include <cmath>

#include "atlas/render/canvas_geometry.hpp"
#include "doctest.h"

using namespace atlas::render;

namespace {
float distanceTo(Vec2 point, Vec2 center) { return std::hypot(point.x - center.x, point.y - center.y); }
}  // namespace

TEST_CASE("ring arcs grow with the fraction and ignore invalid input") {
    Vec2 center{0.0f, 0.0f};
    CHECK(ringArc(center, 10.0f, 2.0f, 0.0).empty());
    CHECK(ringArc(center, 10.0f, 2.0f, -1.0).empty());
    CHECK(ringArc(center, 10.0f, 2.0f, std::nan("")).empty());
    CHECK(ringArc(center, 10.0f, 2.0f, 0.5).size() == 24 * 6);
    CHECK(ringArc(center, 10.0f, 2.0f, 1.0).size() == 48 * 6);
    CHECK(ringArc(center, 10.0f, 2.0f, 2.0).size() == 48 * 6);
}

TEST_CASE("a ring arc starts at twelve o'clock and stays inside its band") {
    Vec2 center{0.0f, 0.0f};
    auto arc = ringArc(center, 10.0f, 2.0f, 1.0);
    CHECK(arc[0].x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(arc[0].y == doctest::Approx(-9.0f));
    for (const auto& point : arc) {
        CHECK(distanceTo(point, center) >= 9.0f - 1e-3f);
        CHECK(distanceTo(point, center) <= 11.0f + 1e-3f);
    }
}

TEST_CASE("a dashed ring has one band per dash") {
    CHECK(dashedRing({0.0f, 0.0f}, 10.0f, 2.0f, 12).size() == 12 * 6);
}

TEST_CASE("dashed lines alternate dash and gap and stop at the end") {
    auto segments = dashedLine({0.0f, 0.0f}, {10.0f, 0.0f}, 3.0f, 2.0f);
    REQUIRE(segments.size() == 2);
    CHECK(segments[0].from.x == doctest::Approx(0.0f));
    CHECK(segments[0].to.x == doctest::Approx(3.0f));
    CHECK(segments[1].from.x == doctest::Approx(5.0f));
    CHECK(segments[1].to.x == doctest::Approx(8.0f));
    CHECK(dashedLine({1.0f, 1.0f}, {1.0f, 1.0f}, 3.0f, 2.0f).empty());
}

TEST_CASE("arrowheads sit on the target circle and point at it") {
    CHECK(trimmedEnd({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f).x == doctest::Approx(8.0f));
    auto head = arrowHead({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f, 6.0f, 3.0f);
    CHECK(head[0].x == doctest::Approx(8.0f));
    CHECK(head[0].y == doctest::Approx(0.0f));
    CHECK(head[1].x == doctest::Approx(2.0f));
    CHECK(std::abs(head[1].y) == doctest::Approx(3.0f));
    CHECK(head[2].x == doctest::Approx(2.0f));
    CHECK(head[1].y == doctest::Approx(-head[2].y));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `canvas_geometry.hpp` not found.

- [ ] **Step 3: Create `canvas_geometry.hpp`**

```cpp
#pragma once

#include <array>
#include <vector>

namespace atlas::render {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct LineSegment {
    Vec2 from;
    Vec2 to;
};

std::vector<Vec2> ringArc(Vec2 center, float radius, float thickness, double fraction);
std::vector<Vec2> dashedRing(Vec2 center, float radius, float thickness, int dashCount);
std::vector<LineSegment> dashedLine(Vec2 from, Vec2 to, float dash, float gap);
Vec2 trimmedEnd(Vec2 from, Vec2 to, float radius);
std::array<Vec2, 3> arrowHead(Vec2 from, Vec2 to, float targetRadius, float length, float halfWidth);

}  // namespace atlas::render
```

- [ ] **Step 4: Create `canvas_geometry.cpp`**

```cpp
#include "atlas/render/canvas_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kStartAngle = -kPi / 2.0;
constexpr int kRingSegments = 48;

Vec2 polar(Vec2 center, float radius, double angle) {
    return Vec2{center.x + radius * static_cast<float>(std::cos(angle)),
                center.y + radius * static_cast<float>(std::sin(angle))};
}

void appendBand(std::vector<Vec2>& out, Vec2 center, float inner, float outer, double from, double to) {
    Vec2 innerFrom = polar(center, inner, from);
    Vec2 outerFrom = polar(center, outer, from);
    Vec2 innerTo = polar(center, inner, to);
    Vec2 outerTo = polar(center, outer, to);
    out.insert(out.end(), {innerFrom, outerFrom, outerTo, innerFrom, outerTo, innerTo});
}

float lengthOf(Vec2 vector) { return std::sqrt(vector.x * vector.x + vector.y * vector.y); }

}  // namespace

std::vector<Vec2> ringArc(Vec2 center, float radius, float thickness, double fraction) {
    std::vector<Vec2> out;
    if (!(fraction > 0.0)) return out;
    fraction = std::min(fraction, 1.0);
    int segments = static_cast<int>(std::ceil(fraction * kRingSegments));
    double sweep = 2.0 * kPi * fraction;
    float inner = radius - thickness / 2.0f;
    float outer = radius + thickness / 2.0f;
    for (int i = 0; i < segments; ++i) {
        appendBand(out, center, inner, outer, kStartAngle + sweep * i / segments,
                   kStartAngle + sweep * (i + 1) / segments);
    }
    return out;
}

std::vector<Vec2> dashedRing(Vec2 center, float radius, float thickness, int dashCount) {
    std::vector<Vec2> out;
    if (dashCount <= 0) return out;
    float inner = radius - thickness / 2.0f;
    float outer = radius + thickness / 2.0f;
    double period = 2.0 * kPi / dashCount;
    for (int i = 0; i < dashCount; ++i) {
        double from = kStartAngle + period * i;
        appendBand(out, center, inner, outer, from, from + period / 2.0);
    }
    return out;
}

std::vector<LineSegment> dashedLine(Vec2 from, Vec2 to, float dash, float gap) {
    std::vector<LineSegment> out;
    Vec2 delta{to.x - from.x, to.y - from.y};
    float total = lengthOf(delta);
    if (total <= 0.0f || dash <= 0.0f) return out;
    Vec2 unit{delta.x / total, delta.y / total};
    for (float start = 0.0f; start < total; start += dash + gap) {
        float end = std::min(start + dash, total);
        out.push_back({{from.x + unit.x * start, from.y + unit.y * start}, {from.x + unit.x * end, from.y + unit.y * end}});
    }
    return out;
}

Vec2 trimmedEnd(Vec2 from, Vec2 to, float radius) {
    Vec2 delta{to.x - from.x, to.y - from.y};
    float length = lengthOf(delta);
    if (length <= 0.0f) return to;
    return Vec2{to.x - delta.x / length * radius, to.y - delta.y / length * radius};
}

std::array<Vec2, 3> arrowHead(Vec2 from, Vec2 to, float targetRadius, float length, float halfWidth) {
    Vec2 delta{to.x - from.x, to.y - from.y};
    float distance = lengthOf(delta);
    if (distance <= 0.0f) return {to, to, to};
    Vec2 unit{delta.x / distance, delta.y / distance};
    Vec2 tip = trimmedEnd(from, to, targetRadius);
    Vec2 base{tip.x - unit.x * length, tip.y - unit.y * length};
    Vec2 normal{-unit.y, unit.x};
    return {tip, Vec2{base.x + normal.x * halfWidth, base.y + normal.y * halfWidth},
            Vec2{base.x - normal.x * halfWidth, base.y - normal.y * halfWidth}};
}

}  // namespace atlas::render
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_render_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/render
git commit -m "feat(render): add ring, dash, and arrow geometry"
```

---

### Task 6: Canvas with memory rings, labels, arrows, ghosts, and topic collapse

**Files:**
- Rewrite: `modules/render/include/atlas/render/graph_canvas_item.hpp`, `modules/render/src/graph_canvas_item.cpp`
- Create: `modules/render/tests/test_canvas_main.cpp`, `modules/render/tests/test_graph_canvas_item.cpp`
- Modify: `modules/render/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 4 `Theme` fields and `ringBand`/`ringColor`; Task 5 geometry.
- Produces:
  - `RenderNode` gains `QString label; double recall = -1.0; bool ghost = false; QString groupKey; QString groupLabel;` (existing `id, x, y, color` kept; an invalid `color` falls back to `theme.nodeFill`).
  - `RenderEdge` gains `bool directed = true; bool contrast = false; bool ghost = false;`.
  - `struct RenderGroup { QString key; QString label; QPointF center; double meanRecall = -1.0; int memberCount = 0; }`, `QString elideLabel(const QString&, int maxChars = 28)`, `std::vector<RenderGroup> summarizeGroups(const std::vector<RenderNode>&, const std::unordered_map<QString, QPointF>& positions)` (ghosts and empty keys skipped, sorted by key).
  - `GraphCanvasItem`: `nodes()`, `edges()`, property `zoom` (`setZoom`, clamped 0.05 to 10), `Q_INVOKABLE zoomBy(double)`, property `collapsed` (zoom below `kCollapseZoom` = 0.35), `Q_INVOKABLE QString groupAt(double screenX, double screenY) const`, signals `groupClicked(QString key)` and `zoomChanged()`; existing `setGraphData`, `setTheme`, `setHighlight`, `clearHighlight`, `nodeClicked`, `nodeRightClicked`, `nodeHovered` keep their signatures (the Widgets `GraphWindow` still uses them).
- Drawing rules: memory ring at radius 12.5 with a full track plus an arc whose length is the recall chance, colored by band; not introduced shows a 12 dash gray ring; directed links get arrowheads; contrast links are dashed; ghosts are drawn at 35% opacity; labels appear at zoom 0.55 and above in a 11 px monospace font under each node; below zoom 0.35 each topic is one disc with its mean recall ring and a "Name (count)" label, links hidden, and clicking it emits `groupClicked`.
- Text layouts are created on the GUI thread (`setGraphData`) and only added to text nodes on the render thread.

- [ ] **Step 1: Write the failing test**

`modules/render/tests/test_canvas_main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QGuiApplication>

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
```

`modules/render/tests/test_graph_canvas_item.cpp`:

```cpp
#include "atlas/render/graph_canvas_item.hpp"
#include "doctest.h"

using namespace atlas::render;

namespace {

RenderNode makeNode(const char* id, double x, double y, const char* group, double recall = -1.0, bool ghost = false) {
    RenderNode node;
    node.id = id;
    node.x = x;
    node.y = y;
    node.label = id;
    node.recall = recall;
    node.ghost = ghost;
    node.groupKey = group;
    node.groupLabel = QStringLiteral("Topic %1").arg(group);
    return node;
}

}  // namespace

TEST_CASE("long labels are shortened with an ellipsis") {
    CHECK(elideLabel("Tree") == "Tree");
    QString shortened = elideLabel(QString(40, QChar('x')));
    CHECK(shortened.size() == 28);
    CHECK(shortened.endsWith(QChar(0x2026)));
}

TEST_CASE("groups summarize members, skip ghosts, and average learned recall only") {
    std::vector<RenderNode> nodes{makeNode("a", 0, 0, "t1", 0.9), makeNode("b", 10, 0, "t1"),
                                  makeNode("c", 100, 100, "t2", 0.4), makeNode("g", 500, 500, "t1", 1.0, true)};
    std::unordered_map<QString, QPointF> positions{{"a", {0, 0}}, {"b", {10, 0}}, {"c", {100, 100}}, {"g", {500, 500}}};
    auto groups = summarizeGroups(nodes, positions);
    REQUIRE(groups.size() == 2);
    CHECK(groups[0].key == "t1");
    CHECK(groups[0].label == "Topic t1");
    CHECK(groups[0].memberCount == 2);
    CHECK(groups[0].center == QPointF(5, 0));
    CHECK(groups[0].meanRecall == doctest::Approx(0.9));
    CHECK(groups[1].meanRecall == doctest::Approx(0.4));
}

TEST_CASE("a group with no learned members has no recall and falls back to node coordinates") {
    auto groups = summarizeGroups({makeNode("a", 3, 4, "t1")}, {});
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].meanRecall < 0.0);
    CHECK(groups[0].center == QPointF(3, 4));
}

TEST_CASE("zoom is clamped and collapses below the threshold") {
    GraphCanvasItem canvas;
    int changes = 0;
    QObject::connect(&canvas, &GraphCanvasItem::zoomChanged, [&] { ++changes; });
    CHECK_FALSE(canvas.collapsed());
    canvas.setZoom(0.2);
    CHECK(canvas.collapsed());
    canvas.setZoom(100.0);
    CHECK(canvas.zoom() == doctest::Approx(10.0));
    canvas.zoomBy(0.5);
    CHECK(canvas.zoom() == doctest::Approx(5.0));
    CHECK(changes == 3);
}

TEST_CASE("graph data keeps memory, ghost, and link style fields") {
    GraphCanvasItem canvas;
    RenderEdge edge;
    edge.sourceId = "a";
    edge.targetId = "b";
    edge.directed = false;
    edge.contrast = true;
    edge.ghost = true;
    canvas.setGraphData({makeNode("a", 0, 0, "t1", 0.7), makeNode("b", 1, 1, "t1", -1.0, true)}, {edge});
    REQUIRE(canvas.nodes().size() == 2);
    CHECK(canvas.nodes()[0].recall == doctest::Approx(0.7));
    CHECK(canvas.nodes()[1].ghost);
    CHECK(canvas.edges()[0].contrast);
    CHECK_FALSE(canvas.edges()[0].directed);
}

TEST_CASE("groupAt finds the collapsed topic under the cursor") {
    GraphCanvasItem canvas;
    canvas.setGraphData({makeNode("a", 100, 100, "t1"), makeNode("b", 100, 100, "t1")}, {});
    CHECK(canvas.groupAt(20, 20).isEmpty());
    canvas.setZoom(0.2);
    CHECK(canvas.groupAt(22, 20) == "t1");
    CHECK(canvas.groupAt(200, 200).isEmpty());
}
```

Append to `modules/render/tests/CMakeLists.txt`:

```cmake
if (ATLAS_BUILD_UI)
    add_executable(atlas_render_canvas_tests
        test_canvas_main.cpp
        test_graph_canvas_item.cpp
    )
    target_link_libraries(atlas_render_canvas_tests PRIVATE atlas_render_canvas)
    target_include_directories(atlas_render_canvas_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    add_test(NAME atlas_render_canvas_tests COMMAND atlas_render_canvas_tests)
    set_tests_properties(atlas_render_canvas_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --preset dev && cmake --build --preset dev`
Expected: FAIL, `elideLabel` and `summarizeGroups` are not declared.

- [ ] **Step 3: Replace `graph_canvas_item.hpp`**

```cpp
#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QTextLayout>
#include <QTimer>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atlas/render/canvas_geometry.hpp"
#include "atlas/render/theme.hpp"

class QSGTextNode;

namespace atlas::render {

struct RenderNode {
    QString id;
    double x = 0.0;
    double y = 0.0;
    QColor color;
    QString label;
    double recall = -1.0;
    bool ghost = false;
    QString groupKey;
    QString groupLabel;
};

struct RenderEdge {
    QString sourceId;
    QString targetId;
    bool directed = true;
    bool contrast = false;
    bool ghost = false;
};

struct RenderGroup {
    QString key;
    QString label;
    QPointF center;
    double meanRecall = -1.0;
    int memberCount = 0;
};

QString elideLabel(const QString& text, int maxChars = 28);
std::vector<RenderGroup> summarizeGroups(const std::vector<RenderNode>& nodes,
                                         const std::unordered_map<QString, QPointF>& positions);

struct SceneVertices;

class GraphCanvasItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(bool collapsed READ collapsed NOTIFY zoomChanged)

public:
    static constexpr double kCollapseZoom = 0.35;
    static constexpr double kLabelZoom = 0.55;

    explicit GraphCanvasItem(QQuickItem* parent = nullptr);

    void setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges);
    const std::vector<RenderNode>& nodes() const { return nodes_; }
    const std::vector<RenderEdge>& edges() const { return edges_; }

    void setTheme(ThemeMode mode);
    ThemeMode theme() const { return themeMode_; }

    void setHighlight(const QString& selectedId, const std::unordered_set<QString>& neighborIds);
    void clearHighlight();

    double zoom() const { return scale_; }
    void setZoom(double zoom);
    Q_INVOKABLE void zoomBy(double factor);
    bool collapsed() const { return scale_ < kCollapseZoom; }
    Q_INVOKABLE QString groupAt(double screenX, double screenY) const;

signals:
    void nodeClicked(QString id);
    void nodeRightClicked(QString id);
    void nodeHovered(QString id);
    void groupClicked(QString key);
    void zoomChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct LabelLayout {
        std::shared_ptr<QTextLayout> layout;
        qreal width = 0.0;
    };

    static LabelLayout makeLabel(const QString& text);
    int hitTest(double worldX, double worldY) const;
    Vec2 positionOf(const QString& id) const;
    QPointF toScreen(QPointF world) const;
    void tickAnimation();
    void buildNodes(SceneVertices& out, const Theme& theme) const;
    void buildEdges(SceneVertices& out, const Theme& theme) const;
    void buildGroups(SceneVertices& out, const Theme& theme) const;
    void fillLabels(QSGTextNode* labels, QSGTextNode* ghostLabels, const Theme& theme) const;

    std::vector<RenderNode> nodes_;
    std::vector<RenderEdge> edges_;
    std::unordered_map<QString, LabelLayout> labelLayouts_;
    std::unordered_map<QString, LabelLayout> groupLayouts_;

    std::unordered_map<QString, QPointF> targetPositions_;
    std::unordered_map<QString, QPointF> currentPositions_;
    QTimer* animationTimer_;

    QString selectedId_;
    std::unordered_set<QString> neighborIds_;
    QString hoveredId_;

    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
    double scale_ = 1.0;

    bool dragging_ = false;
    bool dragMoved_ = false;
    QPointF lastMousePos_;

    ThemeMode themeMode_ = ThemeMode::Dark;
    bool backgroundDirty_ = true;
    bool dataDirty_ = true;
    bool labelsDirty_ = true;
};

void registerGraphCanvasQmlType();

}  // namespace atlas::render
```

- [ ] **Step 4: Replace `graph_canvas_item.cpp`**

```cpp
#include "atlas/render/graph_canvas_item.hpp"

#include <QFont>
#include <QHoverEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGTextNode>
#include <QSGTransformNode>
#include <QSGVertexColorMaterial>
#include <QTextLine>
#include <QWheelEvent>
#include <QtQml/qqml.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <utility>

namespace atlas::render {

struct ColoredVertex {
    float x;
    float y;
    QColor color;
};

struct SceneVertices {
    std::vector<ColoredVertex> highlight;
    std::vector<ColoredVertex> edges;
    std::vector<ColoredVertex> arrows;
    std::vector<ColoredVertex> borders;
    std::vector<ColoredVertex> fills;
    std::vector<ColoredVertex> memory;
};

namespace {

constexpr float kNodeRadius = 8.0f;
constexpr float kNodeBorderWidth = 2.2f;
constexpr float kSelectRingRadius = 15.0f;
constexpr float kNeighborRingRadius = 13.0f;
constexpr float kHoverRingRadius = 12.0f;
constexpr float kMemoryRingRadius = 12.5f;
constexpr float kMemoryRingThickness = 2.0f;
constexpr int kNewRingDashes = 12;
constexpr float kArrowLength = 6.0f;
constexpr float kArrowHalfWidth = 3.0f;
constexpr float kDashLength = 4.0f;
constexpr float kDashGap = 3.0f;
constexpr int kCircleSegments = 16;
constexpr double kMinScale = 0.05;
constexpr double kMaxScale = 10.0;
constexpr double kDragThreshold = 4.0;
constexpr double kWheelStep = 1.1;
constexpr float kGroupRadiusPx = 20.0f;
constexpr float kLabelGapPx = 6.0f;
constexpr double kCullMarginPx = 60.0;
constexpr int kGhostAlphaPercent = 35;
constexpr int kDimmedAlpha = 60;
constexpr int kDimmedRingAlphaPercent = 40;
constexpr double kEaseFactor = 0.2;
constexpr double kAnimationEpsilonSq = 0.01;
constexpr int kAnimationIntervalMs = 16;
constexpr float kBaseDotSpacingWorld = 40.0f;
constexpr float kMinDotSpacingPx = 18.0f;
constexpr float kMaxDotSpacingPx = 72.0f;
constexpr float kDotRadius = 1.3f;
constexpr int kLabelPixelSize = 11;

enum Layer { HighlightLayer, EdgeLayer, ArrowLayer, BorderLayer, FillLayer, MemoryLayer, LayerCount };

float effectiveDotSpacingPx(double scale) {
    float spacing = static_cast<float>(kBaseDotSpacingWorld * scale);
    while (spacing < kMinDotSpacingPx) spacing *= 2.0f;
    while (spacing > kMaxDotSpacingPx) spacing *= 0.5f;
    return spacing;
}

QColor faded(QColor color, int percent) {
    color.setAlpha(color.alpha() * percent / 100);
    return color;
}

void appendTriangles(std::vector<ColoredVertex>& out, const std::vector<Vec2>& points, const QColor& color) {
    for (const auto& point : points) out.push_back({point.x, point.y, color});
}

void appendDisc(std::vector<ColoredVertex>& out, Vec2 center, float radius, const QColor& color) {
    constexpr float kTwoPi = 6.283185307179586f;
    for (int i = 0; i < kCircleSegments; ++i) {
        float from = kTwoPi * static_cast<float>(i) / static_cast<float>(kCircleSegments);
        float to = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);
        out.push_back({center.x, center.y, color});
        out.push_back({center.x + radius * std::cos(from), center.y + radius * std::sin(from), color});
        out.push_back({center.x + radius * std::cos(to), center.y + radius * std::sin(to), color});
    }
}

void appendMemoryRing(std::vector<ColoredVertex>& out, Vec2 center, float radius, float thickness, double recall,
                      const Theme& theme, int alphaPercent) {
    if (ringBand(recall) == RingBand::New) {
        appendTriangles(out, dashedRing(center, radius, thickness, kNewRingDashes), faded(theme.ringNew, alphaPercent));
        return;
    }
    appendTriangles(out, ringArc(center, radius, thickness, 1.0), faded(theme.ringTrack, alphaPercent));
    appendTriangles(out, ringArc(center, radius, thickness, recall), faded(ringColor(theme, recall), alphaPercent));
}

std::vector<ColoredVertex> backgroundDots(float width, float height, double offsetX, double offsetY, double scale,
                                          const QColor& color) {
    std::vector<ColoredVertex> out;
    if (width <= 0.0f || height <= 0.0f) return out;
    float spacing = effectiveDotSpacingPx(scale);
    float phaseX = std::fmod(static_cast<float>(offsetX), spacing);
    if (phaseX < 0.0f) phaseX += spacing;
    float phaseY = std::fmod(static_cast<float>(offsetY), spacing);
    if (phaseY < 0.0f) phaseY += spacing;
    int columns = static_cast<int>(width / spacing) + 2;
    int rows = static_cast<int>(height / spacing) + 2;
    for (int column = -1; column < columns - 1; ++column) {
        for (int row = -1; row < rows - 1; ++row) {
            float cx = phaseX + static_cast<float>(column) * spacing;
            float cy = phaseY + static_cast<float>(row) * spacing;
            out.insert(out.end(), {{cx - kDotRadius, cy - kDotRadius, color}, {cx + kDotRadius, cy - kDotRadius, color},
                                   {cx + kDotRadius, cy + kDotRadius, color}, {cx - kDotRadius, cy - kDotRadius, color},
                                   {cx + kDotRadius, cy + kDotRadius, color}, {cx - kDotRadius, cy + kDotRadius, color}});
        }
    }
    return out;
}

QSGGeometryNode* makeColoredNode(QSGGeometry::DrawingMode mode) {
    auto* node = new QSGGeometryNode();
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
    geometry->setDrawingMode(mode);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(new QSGVertexColorMaterial());
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void upload(QSGNode* node, const std::vector<ColoredVertex>& vertices) {
    auto* geometryNode = static_cast<QSGGeometryNode*>(node);
    auto* geometry = geometryNode->geometry();
    geometry->allocate(static_cast<int>(vertices.size()));
    auto* data = geometry->vertexDataAsColoredPoint2D();
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& vertex = vertices[i];
        data[i].set(vertex.x, vertex.y, static_cast<uchar>(vertex.color.red()), static_cast<uchar>(vertex.color.green()),
                    static_cast<uchar>(vertex.color.blue()), static_cast<uchar>(vertex.color.alpha()));
    }
    geometryNode->markDirty(QSGNode::DirtyGeometry);
}

QFont labelFont() {
    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(kLabelPixelSize);
    return font;
}

}  // namespace

QString elideLabel(const QString& text, int maxChars) {
    if (text.size() <= maxChars) return text;
    return text.left(maxChars - 1) + QChar(0x2026);
}

std::vector<RenderGroup> summarizeGroups(const std::vector<RenderNode>& nodes,
                                         const std::unordered_map<QString, QPointF>& positions) {
    struct Accumulator {
        QString label;
        double sumX = 0.0;
        double sumY = 0.0;
        int count = 0;
        double recallSum = 0.0;
        int learned = 0;
    };
    std::map<QString, Accumulator> byKey;
    for (const auto& node : nodes) {
        if (node.ghost || node.groupKey.isEmpty()) continue;
        auto& group = byKey[node.groupKey];
        group.label = node.groupLabel;
        auto found = positions.find(node.id);
        QPointF point = found != positions.end() ? found->second : QPointF(node.x, node.y);
        group.sumX += point.x();
        group.sumY += point.y();
        ++group.count;
        if (node.recall >= 0.0) {
            group.recallSum += node.recall;
            ++group.learned;
        }
    }
    std::vector<RenderGroup> groups;
    for (const auto& [key, group] : byKey) {
        groups.push_back(RenderGroup{key, group.label, QPointF(group.sumX / group.count, group.sumY / group.count),
                                     group.learned > 0 ? group.recallSum / group.learned : -1.0, group.count});
    }
    return groups;
}

GraphCanvasItem::GraphCanvasItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    animationTimer_ = new QTimer(this);
    animationTimer_->setInterval(kAnimationIntervalMs);
    connect(animationTimer_, &QTimer::timeout, this, &GraphCanvasItem::tickAnimation);
}

GraphCanvasItem::LabelLayout GraphCanvasItem::makeLabel(const QString& text) {
    LabelLayout label;
    label.layout = std::make_shared<QTextLayout>(text, labelFont());
    label.layout->beginLayout();
    QTextLine line = label.layout->createLine();
    if (line.isValid()) {
        line.setLineWidth(10000.0);
        label.width = line.naturalTextWidth();
    }
    label.layout->endLayout();
    return label;
}

void GraphCanvasItem::setGraphData(std::vector<RenderNode> nodes, std::vector<RenderEdge> edges) {
    nodes_ = std::move(nodes);
    edges_ = std::move(edges);

    std::unordered_map<QString, QPointF> targets;
    targets.reserve(nodes_.size());
    for (const auto& node : nodes_) targets.emplace(node.id, QPointF(node.x, node.y));
    for (const auto& [id, target] : targets) {
        if (!currentPositions_.contains(id)) currentPositions_[id] = target;
    }
    for (auto it = currentPositions_.begin(); it != currentPositions_.end();) {
        it = targets.contains(it->first) ? std::next(it) : currentPositions_.erase(it);
    }
    targetPositions_ = std::move(targets);

    labelLayouts_.clear();
    for (const auto& node : nodes_) {
        if (!node.label.isEmpty()) labelLayouts_.emplace(node.id, makeLabel(elideLabel(node.label)));
    }
    groupLayouts_.clear();
    for (const auto& group : summarizeGroups(nodes_, targetPositions_)) {
        groupLayouts_.emplace(group.key,
                              makeLabel(QStringLiteral("%1 (%2)").arg(elideLabel(group.label)).arg(group.memberCount)));
    }

    dataDirty_ = true;
    labelsDirty_ = true;
    update();
    if (!animationTimer_->isActive()) animationTimer_->start();
}

void GraphCanvasItem::tickAnimation() {
    bool stillMoving = false;
    for (auto& [id, current] : currentPositions_) {
        auto target = targetPositions_.find(id);
        if (target == targetPositions_.end()) continue;
        QPointF delta = target->second - current;
        if (delta.x() * delta.x() + delta.y() * delta.y() <= kAnimationEpsilonSq) {
            current = target->second;
            continue;
        }
        stillMoving = true;
        current += delta * kEaseFactor;
    }
    dataDirty_ = true;
    update();
    if (!stillMoving) animationTimer_->stop();
}

void GraphCanvasItem::setHighlight(const QString& selectedId, const std::unordered_set<QString>& neighborIds) {
    selectedId_ = selectedId;
    neighborIds_ = neighborIds;
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::clearHighlight() {
    selectedId_.clear();
    neighborIds_.clear();
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::setTheme(ThemeMode mode) {
    if (themeMode_ == mode) return;
    themeMode_ = mode;
    backgroundDirty_ = true;
    dataDirty_ = true;
    update();
}

void GraphCanvasItem::setZoom(double zoom) {
    double clamped = std::clamp(zoom, kMinScale, kMaxScale);
    if (clamped == scale_) return;
    scale_ = clamped;
    backgroundDirty_ = true;
    dataDirty_ = true;
    update();
    emit zoomChanged();
}

void GraphCanvasItem::zoomBy(double factor) { setZoom(scale_ * factor); }

Vec2 GraphCanvasItem::positionOf(const QString& id) const {
    auto found = currentPositions_.find(id);
    if (found == currentPositions_.end()) return {};
    return Vec2{static_cast<float>(found->second.x()), static_cast<float>(found->second.y())};
}

QPointF GraphCanvasItem::toScreen(QPointF world) const {
    return QPointF(world.x() * scale_ + offsetX_, world.y() * scale_ + offsetY_);
}

QString GraphCanvasItem::groupAt(double screenX, double screenY) const {
    if (!collapsed()) return {};
    for (const auto& group : summarizeGroups(nodes_, currentPositions_)) {
        QPointF center = toScreen(group.center);
        double dx = screenX - center.x();
        double dy = screenY - center.y();
        if (dx * dx + dy * dy <= kGroupRadiusPx * kGroupRadiusPx) return group.key;
    }
    return {};
}

int GraphCanvasItem::hitTest(double worldX, double worldY) const {
    double radiusSq = kSelectRingRadius * kSelectRingRadius;
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        auto found = currentPositions_.find(nodes_[static_cast<size_t>(i)].id);
        if (found == currentPositions_.end()) continue;
        double dx = worldX - found->second.x();
        double dy = worldY - found->second.y();
        if (dx * dx + dy * dy <= radiusSq) return i;
    }
    return -1;
}

void GraphCanvasItem::buildNodes(SceneVertices& out, const Theme& theme) const {
    bool hasHighlight = !selectedId_.isEmpty();
    for (const auto& node : nodes_) {
        Vec2 center = positionOf(node.id);
        bool isSelected = node.id == selectedId_;
        bool isNeighbor = hasHighlight && neighborIds_.contains(node.id);
        bool isHovered = !isSelected && node.id == hoveredId_;
        if (isSelected) appendDisc(out.highlight, center, kSelectRingRadius, theme.selectedRing);
        else if (isNeighbor) appendDisc(out.highlight, center, kNeighborRingRadius, theme.neighborRing);
        else if (isHovered) appendDisc(out.highlight, center, kHoverRingRadius, theme.hoverRing);

        int alpha = node.ghost ? kGhostAlphaPercent : 100;
        bool dimmed = hasHighlight && !isSelected && !isNeighbor;
        QColor border = faded(theme.nodeBorder, alpha);
        QColor fill = faded(node.color.isValid() ? node.color : theme.nodeFill, alpha);
        if (dimmed) {
            border.setAlpha(std::min(border.alpha(), kDimmedAlpha));
            fill.setAlpha(std::min(fill.alpha(), kDimmedAlpha));
        }
        appendDisc(out.borders, center, kNodeRadius + kNodeBorderWidth, border);
        appendDisc(out.fills, center, kNodeRadius, fill);
        appendMemoryRing(out.memory, center, kMemoryRingRadius, kMemoryRingThickness, node.recall, theme,
                         dimmed ? kDimmedRingAlphaPercent : alpha);
    }
}

void GraphCanvasItem::buildEdges(SceneVertices& out, const Theme& theme) const {
    bool hasHighlight = !selectedId_.isEmpty();
    float trim = kNodeRadius + kNodeBorderWidth + 1.0f;
    for (const auto& edge : edges_) {
        Vec2 from = positionOf(edge.sourceId);
        Vec2 to = positionOf(edge.targetId);
        bool touchesSelection = edge.sourceId == selectedId_ || edge.targetId == selectedId_;
        QColor color = hasHighlight && !touchesSelection ? theme.edgeDimmed : theme.edge;
        if (edge.ghost) color = faded(color, kGhostAlphaPercent);

        Vec2 start = trimmedEnd(to, from, trim);
        Vec2 end = trimmedEnd(from, to, edge.directed ? trim + kArrowLength : trim);
        std::vector<LineSegment> segments =
            edge.contrast ? dashedLine(start, end, kDashLength, kDashGap) : std::vector<LineSegment>{{start, end}};
        for (const auto& segment : segments) {
            out.edges.push_back({segment.from.x, segment.from.y, color});
            out.edges.push_back({segment.to.x, segment.to.y, color});
        }
        if (edge.directed) {
            for (const auto& point : arrowHead(from, to, trim, kArrowLength, kArrowHalfWidth)) {
                out.arrows.push_back({point.x, point.y, color});
            }
        }
    }
}

void GraphCanvasItem::buildGroups(SceneVertices& out, const Theme& theme) const {
    float pixel = 1.0f / static_cast<float>(scale_);
    float radius = kGroupRadiusPx * pixel;
    for (const auto& group : summarizeGroups(nodes_, currentPositions_)) {
        Vec2 center{static_cast<float>(group.center.x()), static_cast<float>(group.center.y())};
        appendDisc(out.borders, center, radius + kNodeBorderWidth * pixel, theme.nodeBorder);
        appendDisc(out.fills, center, radius, theme.nodeFill);
        appendMemoryRing(out.memory, center, radius + 4.0f * pixel, 3.0f * pixel, group.meanRecall, theme, 100);
    }
}

void GraphCanvasItem::fillLabels(QSGTextNode* labels, QSGTextNode* ghostLabels, const Theme& theme) const {
    labels->clear();
    ghostLabels->clear();
    labels->setColor(theme.label);
    ghostLabels->setColor(faded(theme.label, kGhostAlphaPercent));

    auto visible = [this](QPointF screen) {
        return screen.x() > -kCullMarginPx && screen.x() < width() + kCullMarginPx && screen.y() > -kCullMarginPx &&
               screen.y() < height() + kCullMarginPx;
    };

    if (collapsed()) {
        for (const auto& group : summarizeGroups(nodes_, currentPositions_)) {
            auto label = groupLayouts_.find(group.key);
            QPointF screen = toScreen(group.center);
            if (label == groupLayouts_.end() || !visible(screen)) continue;
            labels->addTextLayout(QPointF(screen.x() - label->second.width / 2.0, screen.y() + kGroupRadiusPx + kLabelGapPx),
                                  label->second.layout.get());
        }
        return;
    }
    if (scale_ < kLabelZoom) return;

    double below = (kMemoryRingRadius + kMemoryRingThickness) * scale_ + kLabelGapPx;
    for (const auto& node : nodes_) {
        auto label = labelLayouts_.find(node.id);
        auto position = currentPositions_.find(node.id);
        if (label == labelLayouts_.end() || position == currentPositions_.end()) continue;
        QPointF screen = toScreen(position->second);
        if (!visible(screen)) continue;
        QPointF at(screen.x() - label->second.width / 2.0, screen.y() + below);
        (node.ghost ? ghostLabels : labels)->addTextLayout(at, label->second.layout.get());
    }
}

QSGNode* GraphCanvasItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QSGNode* container = oldNode;
    if (container == nullptr) {
        container = new QSGNode();
        container->appendChildNode(makeColoredNode(QSGGeometry::DrawTriangles));
        auto* root = new QSGTransformNode();
        for (int layer = 0; layer < LayerCount; ++layer) {
            root->appendChildNode(makeColoredNode(layer == EdgeLayer ? QSGGeometry::DrawLines : QSGGeometry::DrawTriangles));
        }
        container->appendChildNode(root);
        container->appendChildNode(window()->createTextNode());
        container->appendChildNode(window()->createTextNode());
        backgroundDirty_ = true;
        dataDirty_ = true;
        labelsDirty_ = true;
    }
    auto* background = container->childAtIndex(0);
    auto* root = static_cast<QSGTransformNode*>(container->childAtIndex(1));
    auto* labels = static_cast<QSGTextNode*>(container->childAtIndex(2));
    auto* ghostLabels = static_cast<QSGTextNode*>(container->childAtIndex(3));

    QMatrix4x4 matrix;
    matrix.translate(static_cast<float>(offsetX_), static_cast<float>(offsetY_));
    matrix.scale(static_cast<float>(scale_));
    root->setMatrix(matrix);

    const Theme& theme = themeFor(themeMode_);
    if (backgroundDirty_) {
        upload(background, backgroundDots(static_cast<float>(width()), static_cast<float>(height()), offsetX_,
                                          offsetY_, scale_, theme.dot));
        backgroundDirty_ = false;
    }
    if (dataDirty_) {
        SceneVertices scene;
        if (collapsed()) {
            buildGroups(scene, theme);
        } else {
            buildNodes(scene, theme);
            buildEdges(scene, theme);
        }
        upload(root->childAtIndex(HighlightLayer), scene.highlight);
        upload(root->childAtIndex(EdgeLayer), scene.edges);
        upload(root->childAtIndex(ArrowLayer), scene.arrows);
        upload(root->childAtIndex(BorderLayer), scene.borders);
        upload(root->childAtIndex(FillLayer), scene.fills);
        upload(root->childAtIndex(MemoryLayer), scene.memory);
        dataDirty_ = false;
        labelsDirty_ = true;
    }
    if (labelsDirty_) {
        fillLabels(labels, ghostLabels, theme);
        labelsDirty_ = false;
    }
    return container;
}

void GraphCanvasItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragMoved_ = false;
        lastMousePos_ = event->position();
    } else if (event->button() == Qt::RightButton && !collapsed()) {
        int hit = hitTest((event->position().x() - offsetX_) / scale_, (event->position().y() - offsetY_) / scale_);
        if (hit >= 0) emit nodeRightClicked(nodes_[static_cast<size_t>(hit)].id);
    }
    event->accept();
}

void GraphCanvasItem::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;
    QPointF delta = event->position() - lastMousePos_;
    if (std::abs(delta.x()) > kDragThreshold || std::abs(delta.y()) > kDragThreshold) dragMoved_ = true;
    offsetX_ += delta.x();
    offsetY_ += delta.y();
    lastMousePos_ = event->position();
    backgroundDirty_ = true;
    labelsDirty_ = true;
    update();
    event->accept();
}

void GraphCanvasItem::mouseReleaseEvent(QMouseEvent* event) {
    if (!dragMoved_ && event->button() == Qt::LeftButton) {
        if (collapsed()) {
            QString key = groupAt(event->position().x(), event->position().y());
            if (!key.isEmpty()) emit groupClicked(key);
        } else {
            int hit = hitTest((event->position().x() - offsetX_) / scale_, (event->position().y() - offsetY_) / scale_);
            if (hit >= 0) {
                emit nodeClicked(nodes_[static_cast<size_t>(hit)].id);
            } else if (!selectedId_.isEmpty()) {
                clearHighlight();
                emit nodeClicked(QString{});
            }
        }
    }
    dragging_ = false;
    dragMoved_ = false;
    event->accept();
}

void GraphCanvasItem::hoverMoveEvent(QHoverEvent* event) {
    QString hovered;
    if (!collapsed()) {
        int hit = hitTest((event->position().x() - offsetX_) / scale_, (event->position().y() - offsetY_) / scale_);
        if (hit >= 0) hovered = nodes_[static_cast<size_t>(hit)].id;
    }
    if (hovered != hoveredId_) {
        hoveredId_ = hovered;
        dataDirty_ = true;
        update();
        emit nodeHovered(hoveredId_);
    }
    event->accept();
}

void GraphCanvasItem::hoverLeaveEvent(QHoverEvent* event) {
    if (!hoveredId_.isEmpty()) {
        hoveredId_.clear();
        dataDirty_ = true;
        update();
        emit nodeHovered(QString{});
    }
    event->accept();
}

void GraphCanvasItem::wheelEvent(QWheelEvent* event) {
    zoomBy(event->angleDelta().y() > 0 ? kWheelStep : 1.0 / kWheelStep);
    event->accept();
}

void GraphCanvasItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    backgroundDirty_ = true;
    labelsDirty_ = true;
    update();
}

void registerGraphCanvasQmlType() { qmlRegisterType<GraphCanvasItem>("Atlas.Render", 1, 0, "GraphCanvas"); }

}  // namespace atlas::render
```

- [ ] **Step 5: Run all tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS, including `atlas_render_canvas_tests` and `atlas_ui_tests` (the Widgets `GraphWindow` still builds against the unchanged method signatures).

- [ ] **Step 6: Smoke run the Widgets app on screen**

Run: `./build/dev/modules/app/atlas_app` for a few seconds if a display is available, open a topic with links, zoom in and out. Expected: rings, labels, arrows and topic discs draw without warnings on the terminal. If no display is available, write "not run: no display" in the report.

- [ ] **Step 7: Commit**

```bash
git add modules/render
git commit -m "feat(render): draw memory rings, labels, arrows, ghosts, and collapsed topics"
```

---

### Task 7: Id helpers and MapViewModel

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/ids.hpp`, `src/ids.cpp`
- Create: `modules/viewmodels/include/atlas/viewmodels/map_view_model.hpp`, `src/map_view_model.cpp`
- Test: `modules/viewmodels/tests/test_map_view_model.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: Tasks 2 to 6.
- Produces:
  - `QString toQString(const std::string&)`, `std::string toStdString(const QString&)`, `template <typename Tag> QString idString(const StrongId<Tag>&)`, `template <typename Id> std::optional<Id> parseId(const QString&)`.
  - QML singleton `MapView` (`MapViewModel(WorkspaceController&, MemoryController&, PlacementController&, Palette&, QObject* parent = nullptr)`): properties `topicId` (empty means all topics; an unparseable id is treated as empty), `selectedId`, `conceptCount`; `Q_INVOKABLE void attach(QQuickItem* canvas)`, `QVariantList search(const QString& query) const` (up to 8 `{id, title}` in scope), `QString createConcept(const QString& title)` (files into the current topic, or Uncategorized when all topics are shown; selects it; returns its id or empty on failure), `void tidy()`, `void refresh()`; C++ accessors `nodes()`, `edges()`; signals `topicIdChanged`, `selectedIdChanged`, `sceneChanged`, `errorOccurred(QString)`.
  - Scene rules: in a topic scope, concepts of other topics linked to a member appear as ghost nodes and their links as ghost links; `directed` is false for symmetric types; `contrast` is true for AlternativeTo and OppositeOf; node `recall` is the concept's recall chance or -1; `groupKey`/`groupLabel` are the concept's topic id and name; selection is cleared when its concept disappears.

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_map_view_model.cpp`

```cpp
#include <QSettings>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/learning_repository.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/map_view_model.hpp"
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
    Palette palette{settings};
    TimePoint now = std::chrono::system_clock::now();
    MemoryController memory{db, workspace, [this] { return now; }};
    PlacementController placements{db, workspace};
    MapViewModel map{workspace, memory, placements, palette};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(placements.load().hasValue());
        REQUIRE(memory.load().hasValue());
        QObject::connect(&map, &MapViewModel::errorOccurred, [this] { ++errors; });
    }

    TopicId topic(const char* name) { return workspace.createTopic(name).value(); }
    KnowledgeObjectId concept(const char* title, TopicId topicId) {
        return workspace.createKnowledgeObject(title, topicId).value();
    }
    const atlas::render::RenderNode* node(const KnowledgeObjectId& id) const {
        for (const auto& node : map.nodes()) {
            if (node.id == idString(id)) return &node;
        }
        return nullptr;
    }
};

}  // namespace

TEST_CASE("ids round trip and reject garbage") {
    auto id = KnowledgeObjectId::generate();
    CHECK(parseId<KnowledgeObjectId>(idString(id)) == id);
    CHECK_FALSE(parseId<KnowledgeObjectId>("not an id").has_value());
}

TEST_CASE("an empty database gives an empty scene without errors") {
    Fixture f;
    CHECK(f.map.nodes().empty());
    CHECK(f.map.conceptCount() == 0);
    CHECK(f.errors == 0);
}

TEST_CASE("a topic scope shows outside neighbors as ghosts") {
    Fixture f;
    auto os = f.topic("Operating Systems");
    auto databases = f.topic("Databases");
    auto paging = f.concept("Paging", os);
    auto indexing = f.concept("Indexing", databases);
    REQUIRE(f.workspace.createRelationship(indexing, paging, RelationshipType::Uses, std::nullopt).hasValue());

    CHECK(f.map.nodes().size() == 2);
    CHECK_FALSE(f.node(indexing)->ghost);

    f.map.setTopicId(idString(os));
    CHECK(f.map.conceptCount() == 1);
    REQUIRE(f.node(indexing) != nullptr);
    CHECK(f.node(indexing)->ghost);
    CHECK_FALSE(f.node(paging)->ghost);
    CHECK(f.node(paging)->groupLabel == "Operating Systems");
    REQUIRE(f.map.edges().size() == 1);
    CHECK(f.map.edges()[0].ghost);
    CHECK(f.map.edges()[0].directed);
}

TEST_CASE("links carry their style and nodes carry recall") {
    Fixture f;
    auto topic = uncategorizedTopicId();
    auto btree = f.concept("B-Tree", topic);
    auto hash = f.concept("Hash Table", topic);
    REQUIRE(f.workspace.createRelationship(btree, hash, RelationshipType::AlternativeTo, std::nullopt).hasValue());
    CHECK(f.map.edges()[0].contrast);
    CHECK_FALSE(f.map.edges()[0].directed);
    CHECK(f.node(btree)->recall < 0.0);

    ReviewEvent review;
    review.id = Uuid::generate();
    review.item = ItemRef::forConcept(btree);
    review.sessionId = Uuid::generate();
    review.deviceId = "test";
    review.reviewedAt = f.now;
    review.grade = Grade::Good;
    REQUIRE(LearningRepository(f.db).record({review}, {}).hasValue());
    REQUIRE(f.memory.load().hasValue());
    CHECK(f.node(btree)->recall == doctest::Approx(1.0));
}

TEST_CASE("deleting the selected concept clears the selection") {
    Fixture f;
    auto tree = f.concept("Tree", uncategorizedTopicId());
    int selectionChanges = 0;
    QObject::connect(&f.map, &MapViewModel::selectedIdChanged, [&] { ++selectionChanges; });
    f.map.setSelectedId(idString(tree));
    REQUIRE(f.workspace.removeKnowledgeObject(tree).hasValue());
    CHECK(f.map.selectedId().isEmpty());
    CHECK(selectionChanges == 2);
}

TEST_CASE("the attached canvas follows the scene and the theme") {
    Fixture f;
    f.concept("Tree", uncategorizedTopicId());
    atlas::render::GraphCanvasItem canvas;
    f.map.attach(&canvas);
    CHECK(canvas.nodes().size() == 1);
    f.settings.setDarkTheme(false);
    CHECK(canvas.theme() == atlas::render::ThemeMode::Light);
    f.concept("Hash", uncategorizedTopicId());
    CHECK(canvas.nodes().size() == 2);
}

TEST_CASE("clicking a collapsed topic opens it and clicking a node selects it") {
    Fixture f;
    auto os = f.topic("OS");
    auto paging = f.concept("Paging", os);
    atlas::render::GraphCanvasItem canvas;
    f.map.attach(&canvas);
    emit canvas.groupClicked(idString(os));
    CHECK(f.map.topicId() == idString(os));
    emit canvas.nodeClicked(idString(paging));
    CHECK(f.map.selectedId() == idString(paging));
}

TEST_CASE("search and new concepts stay inside the topic scope") {
    Fixture f;
    auto os = f.topic("OS");
    f.concept("Paging", os);
    f.concept("Indexing", uncategorizedTopicId());
    f.map.setTopicId(idString(os));

    auto results = f.map.search("");
    REQUIRE(results.size() == 1);
    CHECK(results[0].toMap().value("title").toString() == "Paging");

    QString created = f.map.createConcept("Segmentation");
    REQUIRE_FALSE(created.isEmpty());
    auto object = f.workspace.findKnowledgeObject(*parseId<KnowledgeObjectId>(created));
    REQUIRE(object.has_value());
    CHECK(object->topicId() == os);
    CHECK(f.map.selectedId() == created);
}

TEST_CASE("bad input is reported, not ignored") {
    Fixture f;
    f.map.setTopicId("garbage");
    CHECK(f.map.topicId().isEmpty());
    CHECK(f.map.createConcept("").isEmpty());
    CHECK(f.errors == 1);
    QQuickItem notACanvas;
    f.map.attach(&notACanvas);
    CHECK(f.errors == 2);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `map_view_model.hpp` not found.

- [ ] **Step 3: Create `ids.hpp` and `ids.cpp`**

```cpp
#pragma once

#include <QString>

#include <optional>
#include <string>

#include "atlas/core/strong_id.hpp"

namespace atlas::viewmodels {

QString toQString(const std::string& text);
std::string toStdString(const QString& text);

template <typename Tag>
QString idString(const atlas::core::StrongId<Tag>& id) {
    return toQString(id.toString());
}

template <typename Id>
std::optional<Id> parseId(const QString& text) {
    auto uuid = atlas::core::Uuid::parse(toStdString(text));
    if (!uuid) return std::nullopt;
    return Id(*uuid);
}

}  // namespace atlas::viewmodels
```

```cpp
#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

QString toQString(const std::string& text) { return QString::fromStdString(text); }

std::string toStdString(const QString& text) { return text.toStdString(); }

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `map_view_model.hpp`**

```cpp
#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <optional>
#include <vector>

#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class MapViewModel : public QObject, public ProvidedSingleton<MapViewModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(MapView)
    QML_SINGLETON
    Q_PROPERTY(QString topicId READ topicId WRITE setTopicId NOTIFY topicIdChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(int conceptCount READ conceptCount NOTIFY sceneChanged)

public:
    static constexpr int kSearchLimit = 8;

    MapViewModel(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                 Palette& palette, QObject* parent = nullptr);

    QString topicId() const { return topicId_; }
    void setTopicId(const QString& topicId);
    QString selectedId() const { return selectedId_; }
    void setSelectedId(const QString& conceptId);
    int conceptCount() const { return conceptCount_; }

    Q_INVOKABLE void attach(QQuickItem* canvas);
    Q_INVOKABLE QVariantList search(const QString& query) const;
    Q_INVOKABLE QString createConcept(const QString& title);
    Q_INVOKABLE void tidy();
    Q_INVOKABLE void refresh();

    const std::vector<atlas::render::RenderNode>& nodes() const { return nodes_; }
    const std::vector<atlas::render::RenderEdge>& edges() const { return edges_; }

signals:
    void topicIdChanged();
    void selectedIdChanged();
    void sceneChanged();
    void errorOccurred(const QString& message);

private:
    std::optional<atlas::core::TopicId> scope() const;
    void pushToCanvas();
    void applySelection();

    WorkspaceController* workspace_;
    MemoryController* memory_;
    PlacementController* placements_;
    Palette* palette_;
    QPointer<atlas::render::GraphCanvasItem> canvas_;
    QString topicId_;
    QString selectedId_;
    std::vector<atlas::render::RenderNode> nodes_;
    std::vector<atlas::render::RenderEdge> edges_;
    int conceptCount_ = 0;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Create `map_view_model.cpp`**

```cpp
#include "atlas/viewmodels/map_view_model.hpp"

#include <QVariantMap>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::ItemRef;
using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipType;
using atlas::core::TopicId;
using atlas::render::GraphCanvasItem;
using atlas::render::RenderEdge;
using atlas::render::RenderNode;

MapViewModel::MapViewModel(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                           Palette& palette, QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), placements_(&placements), palette_(&palette) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &MapViewModel::refresh);
    connect(workspace_, &WorkspaceController::topicsChanged, this, &MapViewModel::refresh);
    connect(memory_, &MemoryController::memoryChanged, this, &MapViewModel::refresh);
    connect(placements_, &PlacementController::placementsChanged, this, &MapViewModel::refresh);
    connect(placements_, &PlacementController::failed, this, &MapViewModel::errorOccurred);
    connect(palette_, &Palette::changed, this, &MapViewModel::refresh);
    refresh();
}

std::optional<TopicId> MapViewModel::scope() const {
    if (topicId_.isEmpty()) return std::nullopt;
    return parseId<TopicId>(topicId_);
}

void MapViewModel::setTopicId(const QString& topicId) {
    QString normalized = parseId<TopicId>(topicId) ? topicId : QString();
    if (normalized == topicId_) return;
    topicId_ = normalized;
    emit topicIdChanged();
    refresh();
}

void MapViewModel::setSelectedId(const QString& conceptId) {
    if (conceptId == selectedId_) return;
    selectedId_ = conceptId;
    applySelection();
    emit selectedIdChanged();
}

void MapViewModel::refresh() {
    std::unordered_map<std::string, std::string> topicNames;
    for (const auto& topic : workspace_->allTopics()) topicNames.emplace(topic.id().toString(), topic.name());

    auto topic = scope();
    auto objects = workspace_->allKnowledgeObjects();
    std::unordered_set<KnowledgeObjectId> members;
    for (const auto& object : objects) {
        if (!topic || object.topicId() == topic) members.insert(object.id());
    }

    std::unordered_set<KnowledgeObjectId> ghosts;
    edges_.clear();
    for (const auto& link : workspace_->allRelationships()) {
        bool sourceIn = members.contains(link.sourceId());
        bool targetIn = members.contains(link.targetId());
        if (!sourceIn && !targetIn) continue;
        if (!sourceIn) ghosts.insert(link.sourceId());
        if (!targetIn) ghosts.insert(link.targetId());
        RenderEdge edge;
        edge.sourceId = idString(link.sourceId());
        edge.targetId = idString(link.targetId());
        edge.directed = !atlas::core::isSymmetric(link.type());
        edge.contrast = link.type() == RelationshipType::AlternativeTo || link.type() == RelationshipType::OppositeOf;
        edge.ghost = !sourceIn || !targetIn;
        edges_.push_back(edge);
    }

    nodes_.clear();
    for (const auto& object : objects) {
        bool member = members.contains(object.id());
        if (!member && !ghosts.contains(object.id())) continue;
        RenderNode node;
        node.id = idString(object.id());
        node.label = toQString(object.title());
        if (auto point = placements_->position(object.id())) {
            node.x = point->x;
            node.y = point->y;
        }
        node.color = palette_->nodeFill();
        node.recall = memory_->recallChance(ItemRef::forConcept(object.id())).value_or(-1.0);
        node.ghost = !member;
        if (object.topicId()) {
            node.groupKey = idString(*object.topicId());
            auto name = topicNames.find(object.topicId()->toString());
            if (name != topicNames.end()) node.groupLabel = toQString(name->second);
        }
        nodes_.push_back(std::move(node));
    }
    conceptCount_ = static_cast<int>(members.size());

    if (!selectedId_.isEmpty()) {
        auto selected = parseId<KnowledgeObjectId>(selectedId_);
        if (!selected || workspace_->graph().findNode(*selected) == nullptr) {
            selectedId_.clear();
            emit selectedIdChanged();
        }
    }
    pushToCanvas();
    emit sceneChanged();
}

void MapViewModel::pushToCanvas() {
    if (!canvas_) return;
    canvas_->setTheme(palette_->mode());
    canvas_->setGraphData(nodes_, edges_);
    applySelection();
}

void MapViewModel::applySelection() {
    if (!canvas_) return;
    auto selected = parseId<KnowledgeObjectId>(selectedId_);
    if (!selected) {
        canvas_->clearHighlight();
        return;
    }
    std::unordered_set<QString> neighbors;
    for (const auto& neighbor :
         workspace_->graph().neighbors(*selected, std::nullopt, atlas::graph::GraphEngine::Direction::Both)) {
        neighbors.insert(idString(neighbor));
    }
    canvas_->setHighlight(selectedId_, neighbors);
}

void MapViewModel::attach(QQuickItem* item) {
    auto* canvas = qobject_cast<GraphCanvasItem*>(item);
    if (canvas == nullptr) {
        emit errorOccurred(tr("The map canvas is unavailable"));
        return;
    }
    canvas_ = canvas;
    connect(canvas, &GraphCanvasItem::nodeClicked, this, &MapViewModel::setSelectedId, Qt::UniqueConnection);
    connect(canvas, &GraphCanvasItem::groupClicked, this, &MapViewModel::setTopicId, Qt::UniqueConnection);
    pushToCanvas();
}

QVariantList MapViewModel::search(const QString& query) const {
    QVariantList results;
    auto topic = scope();
    for (const auto& object : workspace_->search(toStdString(query))) {
        if (topic && object.topicId() != topic) continue;
        results.append(QVariantMap{{"id", idString(object.id())}, {"title", toQString(object.title())}});
        if (results.size() == kSearchLimit) break;
    }
    return results;
}

QString MapViewModel::createConcept(const QString& title) {
    auto created = workspace_->createKnowledgeObject(toStdString(title), scope().value_or(atlas::core::uncategorizedTopicId()));
    if (!created.hasValue()) {
        emit errorOccurred(toQString(created.error().detail));
        return {};
    }
    QString id = idString(created.value());
    setSelectedId(id);
    return id;
}

void MapViewModel::tidy() {
    auto tidied = placements_->tidy();
    if (!tidied.hasValue()) emit errorOccurred(toQString(tidied.error().detail));
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 6: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): build the map scene with ghosts, recall, and selection"
```

---

### Task 8: TopicsModel

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/topics_model.hpp`, `src/topics_model.cpp`
- Test: `modules/viewmodels/tests/test_topics_model.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: `WorkspaceController` topic API and signals, Task 7 id helpers.
- Produces: QML singleton `Topics` (`TopicsModel(WorkspaceController&, QObject* parent = nullptr)`), a `QAbstractListModel` sorted by name with roles `topicId`, `name`, `conceptCount`, `uncategorized`; property `count`; `Q_INVOKABLE QString create(const QString& name)` (new id or empty), `bool rename(const QString& id, const QString& name)`, `bool remove(const QString& id)`, `QString nameOf(const QString& id) const`, `void refresh()`; signals `countChanged()`, `errorOccurred(QString)`.

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_topics_model.cpp`

```cpp
#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/topics_model.hpp"
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
    TopicsModel topics{workspace};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        QObject::connect(&topics, &TopicsModel::errorOccurred, [this] { ++errors; });
    }

    QVariant at(int row, TopicsModel::Role role) const { return topics.data(topics.index(row), role); }
};

}  // namespace

TEST_CASE("the seeded Uncategorized topic is listed and marked") {
    Fixture f;
    REQUIRE(f.topics.count() == 1);
    CHECK(f.at(0, TopicsModel::NameRole).toString() == "Uncategorized");
    CHECK(f.at(0, TopicsModel::IsUncategorizedRole).toBool());
    CHECK(f.topics.roleNames().value(TopicsModel::ConceptCountRole) == "conceptCount");
}

TEST_CASE("create, rename, and remove keep the list sorted and in sync") {
    Fixture f;
    QString databases = f.topics.create("Databases");
    REQUIRE_FALSE(databases.isEmpty());
    CHECK(f.topics.count() == 2);
    CHECK(f.at(0, TopicsModel::NameRole).toString() == "Databases");

    CHECK(f.topics.rename(databases, "Storage"));
    CHECK(f.topics.nameOf(databases) == "Storage");

    CHECK(f.topics.remove(databases));
    CHECK(f.topics.count() == 1);
    CHECK(f.errors == 0);
}

TEST_CASE("concept counts follow the graph") {
    Fixture f;
    QString os = f.topics.create("OS");
    f.workspace.createKnowledgeObject("Paging", *parseId<TopicId>(os));
    CHECK(f.at(0, TopicsModel::ConceptCountRole).toInt() == 1);
}

TEST_CASE("refused operations are reported") {
    Fixture f;
    CHECK_FALSE(f.topics.remove(idString(uncategorizedTopicId())));
    CHECK(f.topics.create("").isEmpty());
    CHECK_FALSE(f.topics.rename("garbage", "Name"));
    QString os = f.topics.create("OS");
    f.workspace.createKnowledgeObject("Paging", *parseId<TopicId>(os));
    CHECK_FALSE(f.topics.remove(os));
    CHECK(f.errors == 4);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `topics_model.hpp` not found.

- [ ] **Step 3: Create `topics_model.hpp`**

```cpp
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class TopicsModel : public QAbstractListModel, public ProvidedSingleton<TopicsModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Topics)
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, ConceptCountRole, IsUncategorizedRole };

    explicit TopicsModel(WorkspaceController& workspace, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return static_cast<int>(rows_.size()); }

    Q_INVOKABLE QString create(const QString& name);
    Q_INVOKABLE bool rename(const QString& id, const QString& name);
    Q_INVOKABLE bool remove(const QString& id);
    Q_INVOKABLE QString nameOf(const QString& id) const;
    Q_INVOKABLE void refresh();

signals:
    void countChanged();
    void errorOccurred(const QString& message);

private:
    struct Row {
        QString id;
        QString name;
        int conceptCount = 0;
        bool uncategorized = false;
    };

    bool fail(const QString& message);

    WorkspaceController* workspace_;
    std::vector<Row> rows_;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `topics_model.cpp`**

```cpp
#include "atlas/viewmodels/topics_model.hpp"

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::TopicId;

TopicsModel::TopicsModel(WorkspaceController& workspace, QObject* parent)
    : QAbstractListModel(parent), workspace_(&workspace) {
    connect(workspace_, &WorkspaceController::topicsChanged, this, &TopicsModel::refresh);
    connect(workspace_, &WorkspaceController::graphChanged, this, &TopicsModel::refresh);
    refresh();
}

void TopicsModel::refresh() {
    beginResetModel();
    rows_.clear();
    for (const auto& topic : workspace_->allTopics()) {
        rows_.push_back(Row{idString(topic.id()), toQString(topic.name()),
                            static_cast<int>(workspace_->knowledgeObjectsInTopic(topic.id()).size()),
                            topic.id() == atlas::core::uncategorizedTopicId()});
    }
    endResetModel();
    emit countChanged();
}

int TopicsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant TopicsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const Row& row = rows_[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return row.id;
        case NameRole:
        case Qt::DisplayRole: return row.name;
        case ConceptCountRole: return row.conceptCount;
        case IsUncategorizedRole: return row.uncategorized;
        default: return {};
    }
}

QHash<int, QByteArray> TopicsModel::roleNames() const {
    return {{IdRole, "topicId"}, {NameRole, "name"}, {ConceptCountRole, "conceptCount"},
            {IsUncategorizedRole, "uncategorized"}};
}

bool TopicsModel::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

QString TopicsModel::create(const QString& name) {
    auto created = workspace_->createTopic(toStdString(name));
    if (!created.hasValue()) {
        fail(toQString(created.error().detail));
        return {};
    }
    return idString(created.value());
}

bool TopicsModel::rename(const QString& id, const QString& name) {
    auto topic = parseId<TopicId>(id);
    if (!topic) return fail(tr("Unknown topic"));
    auto renamed = workspace_->renameTopic(*topic, toStdString(name));
    return renamed.hasValue() || fail(toQString(renamed.error().detail));
}

bool TopicsModel::remove(const QString& id) {
    auto topic = parseId<TopicId>(id);
    if (!topic) return fail(tr("Unknown topic"));
    auto removed = workspace_->removeTopic(*topic);
    return removed.hasValue() || fail(toQString(removed.error().detail));
}

QString TopicsModel::nameOf(const QString& id) const {
    for (const auto& row : rows_) {
        if (row.id == id) return row.name;
    }
    return {};
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): expose topics as a list model"
```

---

### Task 9: ConceptEditor

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/concept_editor.hpp`, `src/concept_editor.cpp`
- Test: `modules/viewmodels/tests/test_concept_editor.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: `WorkspaceController::findKnowledgeObject`, `updateKnowledgeObject`, `removeKnowledgeObject`, `graphChanged`; `MemoryController::recallChance`, `memoryChanged`; `PlacementController::isPinned`, `setPinned`, `placementsChanged`; id helpers.
- Produces: QML singleton `Concept` (`ConceptEditor(WorkspaceController&, MemoryController&, PlacementController&, QObject* parent = nullptr)`). Properties: `conceptId` (write loads it), `exists`, `dirty`, editable `title`, `definition`, `problemSolved`, `whyItExists`, `notes`, `difficulty` (0 Beginner to 3 Expert), `topicId`, `examples`, `miniProjects`, `references` (each a list of `{primary, secondary}` maps: example description/snippet, project title/description, reference title/url; an empty `secondary` means none), read-only `recall` (-1 when not learned), `pinned` (writes immediately), constant `difficultyNames`. `Q_INVOKABLE bool save()`, `bool remove()`, `void revert()`. Signals `loaded()`, `edited()`, `errorOccurred(QString)`.
- Rules: Confidence is never exposed; rows whose primary and secondary are both empty are dropped on save; an edited draft survives unrelated graph changes, but a concept deleted anywhere clears the editor (`exists` false).

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_concept_editor.cpp`

```cpp
#include <QVariantMap>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/concept_editor.hpp"
#include "atlas/viewmodels/ids.hpp"
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

QVariantMap item(const QString& primary, const QString& secondary) {
    return QVariantMap{{"primary", primary}, {"secondary", secondary}};
}

struct Fixture {
    Database db = openTestDatabase();
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    MemoryController memory{db, workspace, systemClock()};
    PlacementController placements{db, workspace};
    ConceptEditor editor{workspace, memory, placements};
    KnowledgeObjectId tree = workspace.createKnowledgeObject("Tree").value();
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(placements.load().hasValue());
        REQUIRE(memory.load().hasValue());
        QObject::connect(&editor, &ConceptEditor::errorOccurred, [this] { ++errors; });
        editor.setConceptId(idString(tree));
    }
};

}  // namespace

TEST_CASE("loading a concept fills the fields") {
    Fixture f;
    CHECK(f.editor.exists());
    CHECK(f.editor.title() == "Tree");
    CHECK(f.editor.difficulty() == 0);
    CHECK(f.editor.topicId() == idString(uncategorizedTopicId()));
    CHECK(f.editor.recall() < 0.0);
    CHECK_FALSE(f.editor.dirty());
    CHECK(f.editor.difficultyNames().size() == 4);
}

TEST_CASE("edits are saved, including lists with optional parts") {
    Fixture f;
    f.editor.setDefinition("A hierarchy of nodes");
    f.editor.setDifficulty(2);
    f.editor.setExamples({item("File system", "/usr/bin"), item("DOM", "")});
    f.editor.setMiniProjects({item("Build a trie", "Autocomplete")});
    f.editor.setReferences({item("CLRS", ""), item("", "")});
    CHECK(f.editor.dirty());
    REQUIRE(f.editor.save());
    CHECK_FALSE(f.editor.dirty());

    auto saved = f.workspace.findKnowledgeObject(f.tree);
    REQUIRE(saved.has_value());
    CHECK(saved->definition() == "A hierarchy of nodes");
    CHECK(saved->difficulty() == Difficulty::Advanced);
    REQUIRE(saved->examples().size() == 2);
    CHECK(saved->examples()[0].snippet == "/usr/bin");
    CHECK_FALSE(saved->examples()[1].snippet.has_value());
    CHECK(saved->miniProjects()[0].description == "Autocomplete");
    REQUIRE(saved->references().size() == 1);
    CHECK_FALSE(saved->references()[0].url.has_value());
}

TEST_CASE("an empty title is refused and reported") {
    Fixture f;
    f.editor.setTitle("");
    CHECK_FALSE(f.editor.save());
    CHECK(f.errors == 1);
    CHECK(f.workspace.findKnowledgeObject(f.tree)->title() == "Tree");
}

TEST_CASE("revert drops unsaved edits") {
    Fixture f;
    f.editor.setNotes("draft");
    f.editor.revert();
    CHECK(f.editor.notes().isEmpty());
    CHECK_FALSE(f.editor.dirty());
}

TEST_CASE("an unsaved draft survives other graph changes") {
    Fixture f;
    f.editor.setNotes("draft");
    f.workspace.createKnowledgeObject("Hash Table");
    CHECK(f.editor.notes() == "draft");
}

TEST_CASE("a concept deleted anywhere clears the editor") {
    Fixture f;
    f.editor.setNotes("draft");
    REQUIRE(f.workspace.removeKnowledgeObject(f.tree).hasValue());
    CHECK_FALSE(f.editor.exists());
    CHECK(f.editor.title().isEmpty());
}

TEST_CASE("remove deletes the concept") {
    Fixture f;
    REQUIRE(f.editor.remove());
    CHECK_FALSE(f.workspace.findKnowledgeObject(f.tree).has_value());
    CHECK_FALSE(f.editor.exists());
}

TEST_CASE("pinning writes through to the placement") {
    Fixture f;
    f.editor.setPinned(true);
    CHECK(f.placements.isPinned(f.tree));
    CHECK(f.editor.pinned());
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `concept_editor.hpp` not found.

- [ ] **Step 3: Create `concept_editor.hpp`**

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class ConceptEditor : public QObject, public ProvidedSingleton<ConceptEditor> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Concept)
    QML_SINGLETON
    Q_PROPERTY(QString conceptId READ conceptId WRITE setConceptId NOTIFY loaded)
    Q_PROPERTY(bool exists READ exists NOTIFY loaded)
    Q_PROPERTY(bool dirty READ dirty NOTIFY edited)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY edited)
    Q_PROPERTY(QString definition READ definition WRITE setDefinition NOTIFY edited)
    Q_PROPERTY(QString problemSolved READ problemSolved WRITE setProblemSolved NOTIFY edited)
    Q_PROPERTY(QString whyItExists READ whyItExists WRITE setWhyItExists NOTIFY edited)
    Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY edited)
    Q_PROPERTY(int difficulty READ difficulty WRITE setDifficulty NOTIFY edited)
    Q_PROPERTY(QString topicId READ topicId WRITE setTopicId NOTIFY edited)
    Q_PROPERTY(QVariantList examples READ examples WRITE setExamples NOTIFY edited)
    Q_PROPERTY(QVariantList miniProjects READ miniProjects WRITE setMiniProjects NOTIFY edited)
    Q_PROPERTY(QVariantList references READ references WRITE setReferences NOTIFY edited)
    Q_PROPERTY(double recall READ recall NOTIFY loaded)
    Q_PROPERTY(bool pinned READ pinned WRITE setPinned NOTIFY loaded)
    Q_PROPERTY(QStringList difficultyNames READ difficultyNames CONSTANT)

public:
    ConceptEditor(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                  QObject* parent = nullptr);

    QString conceptId() const { return conceptId_; }
    void setConceptId(const QString& conceptId);
    bool exists() const { return exists_; }
    bool dirty() const { return dirty_; }

    QString title() const { return draft_.title; }
    void setTitle(const QString& value) { assign(draft_.title, value); }
    QString definition() const { return draft_.definition; }
    void setDefinition(const QString& value) { assign(draft_.definition, value); }
    QString problemSolved() const { return draft_.problemSolved; }
    void setProblemSolved(const QString& value) { assign(draft_.problemSolved, value); }
    QString whyItExists() const { return draft_.whyItExists; }
    void setWhyItExists(const QString& value) { assign(draft_.whyItExists, value); }
    QString notes() const { return draft_.notes; }
    void setNotes(const QString& value) { assign(draft_.notes, value); }
    int difficulty() const { return draft_.difficulty; }
    void setDifficulty(int value);
    QString topicId() const { return draft_.topicId; }
    void setTopicId(const QString& value) { assign(draft_.topicId, value); }
    QVariantList examples() const { return draft_.examples; }
    void setExamples(const QVariantList& value) { assign(draft_.examples, value); }
    QVariantList miniProjects() const { return draft_.miniProjects; }
    void setMiniProjects(const QVariantList& value) { assign(draft_.miniProjects, value); }
    QVariantList references() const { return draft_.references; }
    void setReferences(const QVariantList& value) { assign(draft_.references, value); }

    double recall() const;
    bool pinned() const;
    void setPinned(bool pinned);
    QStringList difficultyNames() const;

    Q_INVOKABLE bool save();
    Q_INVOKABLE bool remove();
    Q_INVOKABLE void revert();

signals:
    void loaded();
    void edited();
    void errorOccurred(const QString& message);

private:
    struct Draft {
        QString title;
        QString definition;
        QString problemSolved;
        QString whyItExists;
        QString notes;
        int difficulty = 0;
        QString topicId;
        QVariantList examples;
        QVariantList miniProjects;
        QVariantList references;
    };

    template <typename T>
    void assign(T& field, const T& value) {
        if (field == value) return;
        field = value;
        dirty_ = true;
        emit edited();
    }

    void load();
    void onGraphChanged();
    bool fail(const QString& message);

    WorkspaceController* workspace_;
    MemoryController* memory_;
    PlacementController* placements_;
    QString conceptId_;
    bool exists_ = false;
    bool dirty_ = false;
    Draft draft_;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `concept_editor.cpp`**

```cpp
#include "atlas/viewmodels/concept_editor.hpp"

#include <QVariantMap>

#include <algorithm>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::Difficulty;
using atlas::core::Example;
using atlas::core::ItemRef;
using atlas::core::KnowledgeObject;
using atlas::core::KnowledgeObjectId;
using atlas::core::MiniProject;
using atlas::core::Reference;
using atlas::core::TopicId;

namespace {

constexpr int kHighestDifficulty = static_cast<int>(Difficulty::Expert);

QVariantMap listItem(const std::string& primary, const std::string& secondary) {
    return QVariantMap{{"primary", toQString(primary)}, {"secondary", toQString(secondary)}};
}

struct ListItem {
    std::string primary;
    std::optional<std::string> secondary;
};

std::vector<ListItem> readList(const QVariantList& list) {
    std::vector<ListItem> items;
    for (const auto& entry : list) {
        auto map = entry.toMap();
        QString primary = map.value("primary").toString();
        QString secondary = map.value("secondary").toString();
        if (primary.isEmpty() && secondary.isEmpty()) continue;
        items.push_back({toStdString(primary), secondary.isEmpty() ? std::nullopt : std::optional(toStdString(secondary))});
    }
    return items;
}

}  // namespace

ConceptEditor::ConceptEditor(WorkspaceController& workspace, MemoryController& memory,
                             PlacementController& placements, QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), placements_(&placements) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &ConceptEditor::onGraphChanged);
    connect(memory_, &MemoryController::memoryChanged, this, &ConceptEditor::loaded);
    connect(placements_, &PlacementController::placementsChanged, this, &ConceptEditor::loaded);
}

void ConceptEditor::setConceptId(const QString& conceptId) {
    if (conceptId == conceptId_ && exists_) return;
    conceptId_ = conceptId;
    load();
}

void ConceptEditor::load() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    auto object = id ? workspace_->findKnowledgeObject(*id) : std::nullopt;
    exists_ = object.has_value();
    draft_ = Draft{};
    if (object) {
        draft_.title = toQString(object->title());
        draft_.definition = toQString(object->definition());
        draft_.problemSolved = toQString(object->problemSolved());
        draft_.whyItExists = toQString(object->whyItExists());
        draft_.notes = toQString(object->notes());
        draft_.difficulty = static_cast<int>(object->difficulty());
        draft_.topicId = object->topicId() ? idString(*object->topicId()) : QString();
        for (const auto& example : object->examples()) {
            draft_.examples.append(listItem(example.description, example.snippet.value_or("")));
        }
        for (const auto& project : object->miniProjects()) {
            draft_.miniProjects.append(listItem(project.title, project.description));
        }
        for (const auto& reference : object->references()) {
            draft_.references.append(listItem(reference.title, reference.url.value_or("")));
        }
    }
    dirty_ = false;
    emit loaded();
    emit edited();
}

void ConceptEditor::onGraphChanged() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    bool stillThere = id && workspace_->findKnowledgeObject(*id).has_value();
    if (!stillThere) {
        if (exists_) load();
        return;
    }
    if (!dirty_) load();
}

void ConceptEditor::setDifficulty(int value) { assign(draft_.difficulty, std::clamp(value, 0, kHighestDifficulty)); }

double ConceptEditor::recall() const {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return -1.0;
    return memory_->recallChance(ItemRef::forConcept(*id)).value_or(-1.0);
}

bool ConceptEditor::pinned() const {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    return id && exists_ && placements_->isPinned(*id);
}

void ConceptEditor::setPinned(bool pinned) {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) {
        fail(tr("No concept is open"));
        return;
    }
    auto result = placements_->setPinned(*id, pinned);
    if (!result.hasValue()) fail(toQString(result.error().detail));
}

QStringList ConceptEditor::difficultyNames() const {
    QStringList names;
    for (int level = 0; level <= kHighestDifficulty; ++level) {
        names.append(QString::fromUtf8(atlas::core::toDisplayString(static_cast<Difficulty>(level))));
    }
    return names;
}

bool ConceptEditor::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

bool ConceptEditor::save() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return fail(tr("No concept is open"));
    auto topic = parseId<TopicId>(draft_.topicId);
    if (!topic) return fail(tr("Choose a topic"));

    KnowledgeObjectEdits edits;
    edits.title = toStdString(draft_.title);
    edits.definition = toStdString(draft_.definition);
    edits.problemSolved = toStdString(draft_.problemSolved);
    edits.whyItExists = toStdString(draft_.whyItExists);
    edits.notes = toStdString(draft_.notes);
    edits.difficulty = static_cast<Difficulty>(draft_.difficulty);
    edits.topicId = *topic;
    edits.examples.emplace();
    for (auto& entry : readList(draft_.examples)) edits.examples->push_back(Example{entry.primary, entry.secondary});
    edits.miniProjects.emplace();
    for (auto& entry : readList(draft_.miniProjects)) {
        edits.miniProjects->push_back(MiniProject{entry.primary, entry.secondary.value_or("")});
    }
    edits.references.emplace();
    for (auto& entry : readList(draft_.references)) edits.references->push_back(Reference{entry.primary, entry.secondary});

    auto updated = workspace_->updateKnowledgeObject(*id, std::move(edits));
    if (!updated.hasValue()) return fail(toQString(updated.error().detail));
    dirty_ = false;
    emit edited();
    return true;
}

bool ConceptEditor::remove() {
    auto id = parseId<KnowledgeObjectId>(conceptId_);
    if (!id || !exists_) return fail(tr("No concept is open"));
    auto removed = workspace_->removeKnowledgeObject(*id);
    return removed.hasValue() || fail(toQString(removed.error().detail));
}

void ConceptEditor::revert() { load(); }

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): edit concepts without exposing self-rated confidence"
```

---

### Task 10: ConceptLinksModel

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/concept_links_model.hpp`, `src/concept_links_model.cpp`
- Test: `modules/viewmodels/tests/test_concept_links_model.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: `WorkspaceController::allRelationships`, `createRelationship`, `removeRelationship`, `search`, `allTopics`, `graph()`, `graphChanged`; `MemoryController::recallChance`, `memoryChanged`; id helpers.
- Produces: QML singleton `ConceptLinks` (`ConceptLinksModel(WorkspaceController&, MemoryController&, QObject* parent = nullptr)`), a `QAbstractListModel` of the links touching `conceptId`, sorted by the other concept's title, roles `linkId`, `otherId`, `otherTitle`, `typeName`, `outgoing` (the concept is the source), `symmetric`, `note`, `recall` (link recall or -1). Properties `conceptId`, `count`, constant `typeNames` (readable names in `RelationshipType` order). `Q_INVOKABLE bool add(const QString& otherId, int typeIndex, bool outgoing, const QString& note)`, `bool remove(const QString& linkId)`, `QVariantList candidates(const QString& query) const` (up to 8 `{id, title, topic}` from every topic, never the concept itself). Signals `conceptIdChanged`, `countChanged`, `errorOccurred(QString)`.
- Free function `QString relationshipLabel(atlas::core::RelationshipType)`: "depends on", "uses", "implements", "solves", "contains", "part of", "related to", "alternative to", "opposite of", "causes".

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_concept_links_model.cpp`

```cpp
#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/concept_links_model.hpp"
#include "atlas/viewmodels/ids.hpp"
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
    MemoryController memory{db, workspace, systemClock()};
    ConceptLinksModel links{workspace, memory};
    KnowledgeObjectId btree = workspace.createKnowledgeObject("B-Tree").value();
    KnowledgeObjectId tree = workspace.createKnowledgeObject("Tree").value();
    TopicId databases = workspace.createTopic("Databases").value();
    KnowledgeObjectId index = workspace.createKnowledgeObject("Index", databases).value();
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(memory.load().hasValue());
        QObject::connect(&links, &ConceptLinksModel::errorOccurred, [this] { ++errors; });
        links.setConceptId(idString(btree));
    }

    QVariant at(int row, ConceptLinksModel::Role role) const { return links.data(links.index(row), role); }
};

}  // namespace

TEST_CASE("type names are readable and in enum order") {
    Fixture f;
    REQUIRE(f.links.typeNames().size() == 10);
    CHECK(f.links.typeNames()[0] == "depends on");
    CHECK(f.links.typeNames()[8] == "opposite of");
}

TEST_CASE("adding outgoing and incoming links lists both, sorted by the other title") {
    Fixture f;
    REQUIRE(f.links.add(idString(f.tree), 0, true, "needs a tree"));
    REQUIRE(f.links.add(idString(f.index), 1, false, ""));
    REQUIRE(f.links.count() == 2);
    CHECK(f.at(0, ConceptLinksModel::OtherTitleRole).toString() == "Index");
    CHECK_FALSE(f.at(0, ConceptLinksModel::OutgoingRole).toBool());
    CHECK(f.at(1, ConceptLinksModel::OtherTitleRole).toString() == "Tree");
    CHECK(f.at(1, ConceptLinksModel::OutgoingRole).toBool());
    CHECK(f.at(1, ConceptLinksModel::TypeNameRole).toString() == "depends on");
    CHECK(f.at(1, ConceptLinksModel::NoteRole).toString() == "needs a tree");
    CHECK(f.at(1, ConceptLinksModel::RecallRole).toDouble() < 0.0);
}

TEST_CASE("removing a link updates the list") {
    Fixture f;
    REQUIRE(f.links.add(idString(f.tree), 0, true, ""));
    QString linkId = f.at(0, ConceptLinksModel::LinkIdRole).toString();
    REQUIRE(f.links.remove(linkId));
    CHECK(f.links.count() == 0);
}

TEST_CASE("duplicates, bad types, and unknown ids are reported") {
    Fixture f;
    REQUIRE(f.links.add(idString(f.tree), 0, true, ""));
    CHECK_FALSE(f.links.add(idString(f.tree), 0, true, ""));
    CHECK_FALSE(f.links.add(idString(f.tree), 42, true, ""));
    CHECK_FALSE(f.links.add("garbage", 0, true, ""));
    CHECK_FALSE(f.links.remove("garbage"));
    CHECK(f.errors == 4);
}

TEST_CASE("candidates come from every topic but never the concept itself") {
    Fixture f;
    auto all = f.links.candidates("");
    CHECK(all.size() == 2);
    for (const auto& entry : all) CHECK(entry.toMap().value("id").toString() != idString(f.btree));
    auto index = f.links.candidates("Ind");
    REQUIRE(index.size() == 1);
    CHECK(index[0].toMap().value("topic").toString() == "Databases");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `concept_links_model.hpp` not found.

- [ ] **Step 3: Create `concept_links_model.hpp`**

```cpp
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <vector>

#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

QString relationshipLabel(atlas::core::RelationshipType type);

class ConceptLinksModel : public QAbstractListModel, public ProvidedSingleton<ConceptLinksModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(ConceptLinks)
    QML_SINGLETON
    Q_PROPERTY(QString conceptId READ conceptId WRITE setConceptId NOTIFY conceptIdChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList typeNames READ typeNames CONSTANT)

public:
    static constexpr int kCandidateLimit = 8;

    enum Role {
        LinkIdRole = Qt::UserRole + 1,
        OtherIdRole,
        OtherTitleRole,
        TypeNameRole,
        OutgoingRole,
        SymmetricRole,
        NoteRole,
        RecallRole
    };

    ConceptLinksModel(WorkspaceController& workspace, MemoryController& memory, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString conceptId() const { return conceptId_; }
    void setConceptId(const QString& conceptId);
    int count() const { return static_cast<int>(rows_.size()); }
    QStringList typeNames() const;

    Q_INVOKABLE bool add(const QString& otherId, int typeIndex, bool outgoing, const QString& note);
    Q_INVOKABLE bool remove(const QString& linkId);
    Q_INVOKABLE QVariantList candidates(const QString& query) const;

signals:
    void conceptIdChanged();
    void countChanged();
    void errorOccurred(const QString& message);

private:
    struct Row {
        QString linkId;
        QString otherId;
        QString otherTitle;
        QString typeName;
        bool outgoing = true;
        bool symmetric = false;
        QString note;
        double recall = -1.0;
    };

    void refresh();
    bool fail(const QString& message);

    WorkspaceController* workspace_;
    MemoryController* memory_;
    QString conceptId_;
    std::vector<Row> rows_;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `concept_links_model.cpp`**

```cpp
#include "atlas/viewmodels/concept_links_model.hpp"

#include <QVariantMap>

#include <algorithm>
#include <string>
#include <unordered_map>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::ItemRef;
using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;

namespace {
constexpr int kTypeCount = static_cast<int>(RelationshipType::Causes) + 1;
}  // namespace

QString relationshipLabel(RelationshipType type) {
    switch (type) {
        case RelationshipType::DependsOn: return QStringLiteral("depends on");
        case RelationshipType::Uses: return QStringLiteral("uses");
        case RelationshipType::Implements: return QStringLiteral("implements");
        case RelationshipType::Solves: return QStringLiteral("solves");
        case RelationshipType::Contains: return QStringLiteral("contains");
        case RelationshipType::PartOf: return QStringLiteral("part of");
        case RelationshipType::RelatedTo: return QStringLiteral("related to");
        case RelationshipType::AlternativeTo: return QStringLiteral("alternative to");
        case RelationshipType::OppositeOf: return QStringLiteral("opposite of");
        case RelationshipType::Causes: return QStringLiteral("causes");
    }
    return {};
}

ConceptLinksModel::ConceptLinksModel(WorkspaceController& workspace, MemoryController& memory, QObject* parent)
    : QAbstractListModel(parent), workspace_(&workspace), memory_(&memory) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &ConceptLinksModel::refresh);
    connect(memory_, &MemoryController::memoryChanged, this, &ConceptLinksModel::refresh);
}

void ConceptLinksModel::setConceptId(const QString& conceptId) {
    if (conceptId == conceptId_) return;
    conceptId_ = conceptId;
    emit conceptIdChanged();
    refresh();
}

void ConceptLinksModel::refresh() {
    beginResetModel();
    rows_.clear();
    auto self = parseId<KnowledgeObjectId>(conceptId_);
    if (self) {
        const auto& graph = workspace_->graph();
        for (const auto& link : workspace_->allRelationships()) {
            bool outgoing = link.sourceId() == *self;
            if (!outgoing && link.targetId() != *self) continue;
            const auto& otherId = outgoing ? link.targetId() : link.sourceId();
            const auto* other = graph.findNode(otherId);
            Row row;
            row.linkId = idString(link.id());
            row.otherId = idString(otherId);
            row.otherTitle = other ? toQString(other->title()) : QString();
            row.typeName = relationshipLabel(link.type());
            row.outgoing = outgoing;
            row.symmetric = atlas::core::isSymmetric(link.type());
            row.note = toQString(link.note().value_or(""));
            row.recall = memory_->recallChance(ItemRef::forLink(link.id())).value_or(-1.0);
            rows_.push_back(row);
        }
        std::sort(rows_.begin(), rows_.end(), [](const Row& a, const Row& b) {
            if (a.otherTitle != b.otherTitle) return a.otherTitle < b.otherTitle;
            return a.typeName < b.typeName;
        });
    }
    endResetModel();
    emit countChanged();
}

int ConceptLinksModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant ConceptLinksModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const Row& row = rows_[static_cast<size_t>(index.row())];
    switch (role) {
        case LinkIdRole: return row.linkId;
        case OtherIdRole: return row.otherId;
        case OtherTitleRole:
        case Qt::DisplayRole: return row.otherTitle;
        case TypeNameRole: return row.typeName;
        case OutgoingRole: return row.outgoing;
        case SymmetricRole: return row.symmetric;
        case NoteRole: return row.note;
        case RecallRole: return row.recall;
        default: return {};
    }
}

QHash<int, QByteArray> ConceptLinksModel::roleNames() const {
    return {{LinkIdRole, "linkId"},   {OtherIdRole, "otherId"},     {OtherTitleRole, "otherTitle"},
            {TypeNameRole, "typeName"}, {OutgoingRole, "outgoing"}, {SymmetricRole, "symmetric"},
            {NoteRole, "note"},       {RecallRole, "recall"}};
}

QStringList ConceptLinksModel::typeNames() const {
    QStringList names;
    for (int type = 0; type < kTypeCount; ++type) names.append(relationshipLabel(static_cast<RelationshipType>(type)));
    return names;
}

bool ConceptLinksModel::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

bool ConceptLinksModel::add(const QString& otherId, int typeIndex, bool outgoing, const QString& note) {
    auto self = parseId<KnowledgeObjectId>(conceptId_);
    auto other = parseId<KnowledgeObjectId>(otherId);
    if (!self || !other) return fail(tr("Unknown concept"));
    if (typeIndex < 0 || typeIndex >= kTypeCount) return fail(tr("Unknown link type"));
    auto type = static_cast<RelationshipType>(typeIndex);
    std::optional<std::string> text = note.isEmpty() ? std::nullopt : std::optional(toStdString(note));
    auto created = outgoing ? workspace_->createRelationship(*self, *other, type, text)
                            : workspace_->createRelationship(*other, *self, type, text);
    return created.hasValue() || fail(toQString(created.error().detail));
}

bool ConceptLinksModel::remove(const QString& linkId) {
    auto id = parseId<RelationshipId>(linkId);
    if (!id) return fail(tr("Unknown link"));
    auto removed = workspace_->removeRelationship(*id);
    return removed.hasValue() || fail(toQString(removed.error().detail));
}

QVariantList ConceptLinksModel::candidates(const QString& query) const {
    std::unordered_map<std::string, std::string> topicNames;
    for (const auto& topic : workspace_->allTopics()) topicNames.emplace(topic.id().toString(), topic.name());
    QVariantList results;
    for (const auto& object : workspace_->search(toStdString(query))) {
        QString id = idString(object.id());
        if (id == conceptId_) continue;
        QString topic;
        if (object.topicId()) {
            auto name = topicNames.find(object.topicId()->toString());
            if (name != topicNames.end()) topic = toQString(name->second);
        }
        results.append(QVariantMap{{"id", id}, {"title", toQString(object.title())}, {"topic", topic}});
        if (results.size() == kCandidateLimit) break;
    }
    return results;
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): list and edit a concept's links across topics"
```

---

### Task 11: TodayViewModel

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/today_view_model.hpp`, `src/today_view_model.cpp`
- Test: `modules/viewmodels/tests/test_today_view_model.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: `MemoryController::todayPlan`, `recallChance`, `memoryChanged`; `AppSettings::newPerDay`, `newPerDayChanged`; `WorkspaceController::allKnowledgeObjects`.
- Produces: QML singleton `Today` (`TodayViewModel(WorkspaceController&, MemoryController&, AppSettings&, QObject* parent = nullptr)`), read-only properties (notify `changed`): `dueCount` (focuses that are not new), `newCount`, `itemCount` (items across all focuses), `estimatedMinutes` (0 with no focuses, else at least 1, rounded up), `learnedCount` (concepts with a recall chance), `conceptCount`, `empty` (no concepts), `caughtUp` (concepts exist and nothing is planned); `Q_INVOKABLE void refresh()`.

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_today_view_model.cpp`

```cpp
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
        REQUIRE(LearningRepository(db).record({review}, {}).hasValue());
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
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `today_view_model.hpp` not found.

- [ ] **Step 3: Create `today_view_model.hpp`**

```cpp
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class TodayViewModel : public QObject, public ProvidedSingleton<TodayViewModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Today)
    QML_SINGLETON
    Q_PROPERTY(int dueCount READ dueCount NOTIFY changed)
    Q_PROPERTY(int newCount READ newCount NOTIFY changed)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY changed)
    Q_PROPERTY(int estimatedMinutes READ estimatedMinutes NOTIFY changed)
    Q_PROPERTY(int learnedCount READ learnedCount NOTIFY changed)
    Q_PROPERTY(int conceptCount READ conceptCount NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)
    Q_PROPERTY(bool caughtUp READ caughtUp NOTIFY changed)

public:
    TodayViewModel(WorkspaceController& workspace, MemoryController& memory, AppSettings& settings,
                   QObject* parent = nullptr);

    int dueCount() const { return dueCount_; }
    int newCount() const { return newCount_; }
    int itemCount() const { return itemCount_; }
    int estimatedMinutes() const { return estimatedMinutes_; }
    int learnedCount() const { return learnedCount_; }
    int conceptCount() const { return conceptCount_; }
    bool empty() const { return conceptCount_ == 0; }
    bool caughtUp() const { return conceptCount_ > 0 && dueCount_ + newCount_ == 0; }

    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    WorkspaceController* workspace_;
    MemoryController* memory_;
    AppSettings* settings_;
    int dueCount_ = 0;
    int newCount_ = 0;
    int itemCount_ = 0;
    int estimatedMinutes_ = 0;
    int learnedCount_ = 0;
    int conceptCount_ = 0;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `today_view_model.cpp`**

```cpp
#include "atlas/viewmodels/today_view_model.hpp"

#include <algorithm>

namespace atlas::viewmodels {

namespace {
constexpr long long kSecondsPerMinute = 60;
}  // namespace

TodayViewModel::TodayViewModel(WorkspaceController& workspace, MemoryController& memory, AppSettings& settings,
                               QObject* parent)
    : QObject(parent), workspace_(&workspace), memory_(&memory), settings_(&settings) {
    connect(memory_, &MemoryController::memoryChanged, this, &TodayViewModel::refresh);
    connect(settings_, &AppSettings::newPerDayChanged, this, &TodayViewModel::refresh);
    refresh();
}

void TodayViewModel::refresh() {
    atlas::learning::SessionLimits limits;
    limits.newPerDay = settings_->newPerDay();
    auto plan = memory_->todayPlan(limits);

    dueCount_ = 0;
    newCount_ = 0;
    itemCount_ = 0;
    for (const auto& focus : plan.focuses) {
        (focus.isNew ? newCount_ : dueCount_) += 1;
        itemCount_ += static_cast<int>(focus.items.size());
    }
    long long seconds = plan.estimatedDuration.count();
    estimatedMinutes_ = plan.focuses.empty()
                            ? 0
                            : static_cast<int>(std::max(1LL, (seconds + kSecondsPerMinute - 1) / kSecondsPerMinute));

    auto objects = workspace_->allKnowledgeObjects();
    conceptCount_ = static_cast<int>(objects.size());
    learnedCount_ = static_cast<int>(std::count_if(objects.begin(), objects.end(), [this](const auto& object) {
        return memory_->recallChance(atlas::core::ItemRef::forConcept(object.id())).has_value();
    }));
    emit changed();
}

}  // namespace atlas::viewmodels
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): summarize today's session"
```

---

### Task 12: AppContext and the QML exposure check

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/app_context.hpp`, `src/app_context.cpp`
- Test: `modules/viewmodels/tests/test_app_context.cpp`
- Modify: both viewmodels `CMakeLists.txt` files

**Interfaces:**
- Consumes: every class from Tasks 1 to 11.
- Produces: `class AppContext` with `AppContext(Database&, QSettings& store, Clock clock)`, `Result<void, ControllerFailure> load()` (workspace, then placements, then memory, then topics refresh), `void provideSingletons()` (calls `provide` on all eight QML singletons), and accessors `workspace()`, `memory()`, `placements()`, `settings()`, `palette()`, `topics()`, `map()`, `conceptEditor()`, `links()`, `today()`. Plan 3's `main` uses exactly this class.

- [ ] **Step 1: Write the failing test** `modules/viewmodels/tests/test_app_context.cpp`

```cpp
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>
#include <QTemporaryDir>

#include <memory>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/app_context.hpp"
#include "doctest.h"

using namespace atlas::persistence;
using namespace atlas::viewmodels;

TEST_CASE("the app context loads, and QML sees every singleton") {
    auto opened = Database::open(":memory:");
    REQUIRE(opened.hasValue());
    auto db = std::move(opened).value();
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);

    AppContext context(db, store, systemClock());
    REQUIRE(context.load().hasValue());
    context.provideSingletons();

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml\nimport Atlas.ViewModels\n"
                      "QtObject {\n"
                      "  property int topics: Topics.count\n"
                      "  property int concepts: MapView.conceptCount\n"
                      "  property bool empty: Today.empty\n"
                      "  property bool open: Concept.exists\n"
                      "  property int links: ConceptLinks.count\n"
                      "  property int perDay: AppSettings.newPerDay\n"
                      "  property color accent: Palette.accent\n"
                      "}",
                      QUrl());
    std::unique_ptr<QObject> object(component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(object != nullptr);
    CHECK(object->property("topics").toInt() == 1);
    CHECK(object->property("concepts").toInt() == 0);
    CHECK(object->property("empty").toBool());
    CHECK_FALSE(object->property("open").toBool());
    CHECK(object->property("links").toInt() == 0);
    CHECK(object->property("perDay").toInt() == 5);
    CHECK(object->property("accent").value<QColor>() == context.palette().accent());
}

TEST_CASE("view models react to changes made through the context") {
    auto opened = Database::open(":memory:");
    REQUIRE(opened.hasValue());
    auto db = std::move(opened).value();
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppContext context(db, store, systemClock());
    REQUIRE(context.load().hasValue());

    QString id = context.map().createConcept("Recursion");
    REQUIRE_FALSE(id.isEmpty());
    context.conceptEditor().setConceptId(id);
    CHECK(context.conceptEditor().exists());
    CHECK(context.today().newCount() == 1);
    CHECK(context.map().nodes().size() == 1);
    CHECK(context.placements().position(context.workspace().allKnowledgeObjects().front().id()).has_value());
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `app_context.hpp` not found.

- [ ] **Step 3: Create `app_context.hpp`**

```cpp
#pragma once

#include <QSettings>

#include "atlas/viewmodels/app_settings.hpp"
#include "atlas/viewmodels/clock.hpp"
#include "atlas/viewmodels/concept_editor.hpp"
#include "atlas/viewmodels/concept_links_model.hpp"
#include "atlas/viewmodels/map_view_model.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/today_view_model.hpp"
#include "atlas/viewmodels/topics_model.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class AppContext {
public:
    AppContext(atlas::persistence::Database& database, QSettings& store, Clock clock);

    Result<void, ControllerFailure> load();
    void provideSingletons();

    WorkspaceController& workspace() { return workspace_; }
    MemoryController& memory() { return memory_; }
    PlacementController& placements() { return placements_; }
    AppSettings& settings() { return settings_; }
    Palette& palette() { return palette_; }
    TopicsModel& topics() { return topics_; }
    MapViewModel& map() { return map_; }
    ConceptEditor& conceptEditor() { return conceptEditor_; }
    ConceptLinksModel& links() { return links_; }
    TodayViewModel& today() { return today_; }

private:
    WorkspaceController workspace_;
    MemoryController memory_;
    PlacementController placements_;
    AppSettings settings_;
    Palette palette_;
    TopicsModel topics_;
    MapViewModel map_;
    ConceptEditor conceptEditor_;
    ConceptLinksModel links_;
    TodayViewModel today_;
};

}  // namespace atlas::viewmodels
```

- [ ] **Step 4: Create `app_context.cpp`**

```cpp
#include "atlas/viewmodels/app_context.hpp"

namespace atlas::viewmodels {

AppContext::AppContext(atlas::persistence::Database& database, QSettings& store, Clock clock)
    : workspace_(database),
      memory_(database, workspace_, std::move(clock)),
      placements_(database, workspace_),
      settings_(store),
      palette_(settings_),
      topics_(workspace_),
      map_(workspace_, memory_, placements_, palette_),
      conceptEditor_(workspace_, memory_, placements_),
      links_(workspace_, memory_),
      today_(workspace_, memory_, settings_) {}

Result<void, ControllerFailure> AppContext::load() {
    if (auto loaded = workspace_.load(); !loaded.hasValue()) return loaded;
    if (auto placed = placements_.load(); !placed.hasValue()) return placed;
    if (auto remembered = memory_.load(); !remembered.hasValue()) return remembered;
    topics_.refresh();
    return Result<void, ControllerFailure>::ok();
}

void AppContext::provideSingletons() {
    AppSettings::provide(&settings_);
    Palette::provide(&palette_);
    TopicsModel::provide(&topics_);
    MapViewModel::provide(&map_);
    ConceptEditor::provide(&conceptEditor_);
    ConceptLinksModel::provide(&links_);
    TodayViewModel::provide(&today_);
}

}  // namespace atlas::viewmodels
```

Note: `MemoryController`'s constructor binds `NetworkRules` to `workspace_.graph()`, so `workspace_` must be declared (and constructed) first, as above.

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_viewmodels_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): wire every view model into one app context"
```

---

### Task 13: Record decisions and run the full suite

**Files:**
- Modify: `docs/DECISIONS.md`

- [ ] **Step 1: Append to `docs/DECISIONS.md`**

```markdown
## View models and canvas (plan 2)

- The C++ layer between the engine and QML lives in `atlas-viewmodels`, a static QML module (`Atlas.ViewModels`). QML reaches app objects as singletons provided from C++; those classes are never default-constructible, because QML builds its own copy of a default-constructible singleton.
- `WorkspaceController` moved to `atlas-viewmodels`; `atlas-ui` keeps forwarding headers until the Widgets UI is removed.
- `MemoryController` loads the review log at startup and rebuilds the memory cache when it is stale or the replay version changed.
- `PlacementController` follows the layout contract: saved positions never move on normal loads, physics only places new concepts, and tidy is an explicit action that keeps pinned concepts fixed.
- Canvas labels are laid out on the GUI thread when data changes and only drawn on the render thread.
- Below zoom 0.35 each topic collapses into one node with its mean recall ring; clicking it opens that topic.
- In a topic view, concepts from other topics that link to a member appear as faded ghost nodes.
- The self-rated Confidence field stays in the data model but no view model exposes it.
```

- [ ] **Step 2: Run everything**

Run: `cmake --build --preset dev && ctest --preset dev && ctest --test-dir build/dev -L scale --output-on-failure`
Expected: all pass, zero warnings.

Run: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
Expected: all pass, no sanitizer reports.

Run: `git diff PLAN2_BASE..HEAD | grep '^+' | grep -cP '\x{2014}'` where `PLAN2_BASE` is the commit before Task 1 (the commit that added this plan).
Expected: `0`.

- [ ] **Step 3: Commit**

```bash
git add docs/DECISIONS.md
git commit -m "docs: record view model and canvas decisions"
```
