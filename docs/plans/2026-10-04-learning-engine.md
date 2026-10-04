# Learning Engine Implementation Plan (Plan 1 of 3)

**Goal:** Build and fully test the learning engine (memory model, review log, session planning, grading, calibration), its persistence, cross topic links, and warm started layout, with no visible UI change.

**Architecture:** A new Qt free module `atlas-learning` (depends on `atlas-core` and `atlas-graph` only) holds all learning logic as pure, clock free code. `atlas-core` gains the learning value types, `atlas-persistence` gains migration 3 plus two repositories, `atlas-render` gains warm started layout. Plans 2 and 3 build the Qt Quick UI on top.

**Tech Stack:** C++20, CMake 3.25+ (presets), SQLite 3, doctest (vendored), Qt 6 only in existing UI modules.

**Spec:** `docs/specs/2026-10-04-atlas-learning-design.md`

**Plan series:**
1. This plan: tooling, cross topic links, `atlas-learning`, persistence, warm started layout.
2. View models and the QML shell: Theme, Motion, Today and Map screens, rings, startup cache rebuild, placements wired into the map, Confidence field removed from UI.
3. Session screens (rebuild, explain, summary), parity check, delete `atlas-ui`, README update.

## Global Constraints

- C++20, `CMAKE_CXX_EXTENSIONS OFF`. Every target builds with `-Wall -Wextra -Wpedantic -Werror` and passes under ASan + UBSan.
- `atlas-learning` links only `atlas_core` and `atlas_graph`. No Qt, no SQLite, never reads the clock: `now` is always a parameter.
- Fallible boundaries return `atlas::core::Result<T, E>`. No exceptions.
- FSRS-6 with its published default parameters (21 values, w0 to w20). Target retention 0.90. Maximum interval 36500 days.
- Defaults: `new_per_day` 5, `max_focus` 12, schema `beta` 0.2, solid threshold 0.80, default focus time 40 s, `easy` needs at least 20 logged rebuild events.
- Code style: readable names, small units. Comments only when necessary, one line maximum.
- Never write the long dash character anywhere (code, docs, commits). Use "-" only when needed.
- Commit messages have no co-author or generated-by trailers.
- Database writes happen before in-memory updates; multi row writes run in one transaction.

## Review Focus

1. Device clock wrong: a review timestamp in the future, or earlier than the previous review, must not produce NaN or negative intervals. Tests in Task 4 and Task 5.
2. A dependency cycle (A depends on B, B depends on A) must not block both concepts from ever being introduced. Test in Task 6.
3. Stale memory states for concepts or links that no longer exist must be ignored by the planner, not crash it. Test in Task 8.
4. Naming the same hidden neighbor twice during a rebuild must not be reported as a confusion. Test in Task 7.
5. Empty data (no concepts, no events) must give an empty plan and zeroed calibration, never NaN. Tests in Task 8 and Task 9.

## File Map

```
CMakeLists.txt                                   modify: warning, sanitizer options, learning module
CMakePresets.json                                create: dev, asan, release
.github/workflows/ci.yml                         create
modules/graph/tests/CMakeLists.txt               modify: split scale suite
modules/graph/tests/test_graph_engine_scale.cpp  modify: tag scale suite
modules/render/tests/CMakeLists.txt              modify: split scale suite
modules/render/tests/test_force_directed_layout_scale.cpp  modify: tag scale suite

modules/ui/src/workspace_controller.cpp          modify: allow cross topic links
modules/ui/include/atlas/ui/workspace_controller.hpp  modify: comment
modules/ui/src/graph_window.cpp                  modify: draw only in-topic links
modules/ui/tests/test_workspace_controller.cpp   modify

modules/core/include/atlas/core/memory.hpp       create: ItemRef, Grade, Certainty, Exercise, Phase,
                                                         ReviewEvent, MemoryState, Placement
modules/core/src/memory.cpp                      create: storage string conversions
modules/core/tests/test_memory.cpp               create

modules/learning/CMakeLists.txt                  create
modules/learning/include/atlas/learning/fsrs.hpp
modules/learning/include/atlas/learning/memory_ledger.hpp
modules/learning/include/atlas/learning/network_rules.hpp
modules/learning/include/atlas/learning/rebuild_grader.hpp
modules/learning/include/atlas/learning/session_planner.hpp
modules/learning/include/atlas/learning/calibration.hpp
modules/learning/src/*.cpp                       one per header
modules/learning/tests/CMakeLists.txt
modules/learning/tests/test_main.cpp
modules/learning/tests/test_support.hpp
modules/learning/tests/test_*.cpp                one per unit + test_simulated_learner.cpp

modules/persistence/src/detail/statement.hpp/.cpp   modify: doubles, optional ints, transactions
modules/persistence/src/migrations.cpp           modify: migration 3
modules/persistence/include/atlas/persistence/database.hpp  modify: friends
modules/persistence/include/atlas/persistence/learning_repository.hpp   create
modules/persistence/src/learning_repository.cpp  create
modules/persistence/include/atlas/persistence/placement_repository.hpp  create
modules/persistence/src/placement_repository.cpp create
modules/persistence/tests/test_learning_repository.cpp   create
modules/persistence/tests/test_placement_repository.cpp  create
modules/persistence/tests/test_migration_three.cpp       create

modules/render/include/atlas/render/force_directed_layout.hpp  modify: hints
modules/render/src/force_directed_layout.cpp     rewrite: sorted ids, warm start, pins
modules/render/tests/test_force_directed_layout.cpp  modify

docs/DECISIONS.md                                modify: Plan 1 decisions
```

Refinements to the spec, recorded in Task 15:
- One `LearningRepository` instead of separate event and state repositories, since they always change in one transaction.
- Learning data of a deleted concept or link is removed by SQL triggers (atomic with the delete, also on cascades) instead of repository code.
- "Due" is `due_at <= now`, where `due_at` is the exact moment recall chance reaches the target. This is equivalent to the spec's `R(now) < target`.
- Prerequisites inside a dependency cycle with the concept do not block it.

---

### Task 1: Tooling (presets, warnings, sanitizers, scale split, CI)

**Files:**
- Modify: `CMakeLists.txt`
- Create: `CMakePresets.json`, `.github/workflows/ci.yml`
- Modify: `modules/graph/tests/CMakeLists.txt`, `modules/graph/tests/test_graph_engine_scale.cpp:33`
- Modify: `modules/render/tests/CMakeLists.txt`, `modules/render/tests/test_force_directed_layout_scale.cpp:27`
- Modify: `.gitignore`

**Interfaces:**
- Produces: presets `dev`, `asan`, `release`; ctest label `scale`; options `ATLAS_WERROR`, `ATLAS_SANITIZE`.

- [ ] **Step 1: Replace `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.21)
project(Atlas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ATLAS_BUILD_TESTS "Build unit tests" ON)
option(ATLAS_BUILD_UI "Build the Qt UI and app (requires Qt6)" ON)
option(ATLAS_WERROR "Treat compiler warnings as errors" OFF)
option(ATLAS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if (ATLAS_WERROR)
        add_compile_options(-Werror)
    endif()
    if (ATLAS_SANITIZE)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

enable_testing()

add_subdirectory(modules/core)
add_subdirectory(modules/persistence)
add_subdirectory(modules/graph)
add_subdirectory(modules/render)

if (ATLAS_BUILD_UI)
    add_subdirectory(modules/ui)
    add_subdirectory(modules/app)
endif()
```

- [ ] **Step 2: Create `CMakePresets.json`**

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "dev",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/dev",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "ATLAS_WERROR": "ON" }
    },
    {
      "name": "asan",
      "inherits": "dev",
      "binaryDir": "${sourceDir}/build/asan",
      "cacheVariables": { "ATLAS_SANITIZE": "ON" }
    },
    {
      "name": "release",
      "inherits": "dev",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "asan", "configurePreset": "asan" },
    { "name": "release", "configurePreset": "release" }
  ],
  "testPresets": [
    {
      "name": "dev",
      "configurePreset": "dev",
      "output": { "outputOnFailure": true },
      "filter": { "exclude": { "label": "scale" } }
    },
    {
      "name": "asan",
      "configurePreset": "asan",
      "output": { "outputOnFailure": true },
      "filter": { "exclude": { "label": "scale" } }
    },
    {
      "name": "release",
      "configurePreset": "release",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

- [ ] **Step 3: Tag the scale tests**

In `modules/graph/tests/test_graph_engine_scale.cpp` change the `TEST_CASE` line to:

```cpp
TEST_CASE("GraphEngine handles 10,000 nodes and ~30,000 edges without an algorithmic blowup" * doctest::test_suite("scale")) {
```

In `modules/render/tests/test_force_directed_layout_scale.cpp` change the `TEST_CASE` line to:

```cpp
TEST_CASE("layout timing at 10,000 nodes / ~30,000 edges, default 50 iterations" * doctest::test_suite("scale")) {
```

- [ ] **Step 4: Register split tests**

In `modules/graph/tests/CMakeLists.txt` replace the `add_test` line with:

```cmake
add_test(NAME atlas_graph_tests COMMAND atlas_graph_tests --test-suite-exclude=scale)
add_test(NAME atlas_graph_scale_tests COMMAND atlas_graph_tests --test-suite=scale)
set_tests_properties(atlas_graph_scale_tests PROPERTIES LABELS scale)
```

In `modules/render/tests/CMakeLists.txt` replace the `add_test` line with:

```cmake
add_test(NAME atlas_render_tests COMMAND atlas_render_tests --test-suite-exclude=scale)
add_test(NAME atlas_render_scale_tests COMMAND atlas_render_tests --test-suite=scale)
set_tests_properties(atlas_render_scale_tests PROPERTIES LABELS scale)
```

- [ ] **Step 5: Create `.github/workflows/ci.yml`**

```yaml
name: ci

on:
  push:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: archlinux:latest
      options: --cap-add SYS_PTRACE
    strategy:
      matrix:
        preset: [dev, asan]
    steps:
      - name: Install toolchain
        run: pacman -Syu --noconfirm base-devel cmake ninja git sqlite qt6-base qt6-declarative
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake --preset ${{ matrix.preset }}
      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}
      - name: Test
        run: ctest --preset ${{ matrix.preset }}
```

- [ ] **Step 6: Verify all presets build clean and pass**

Run: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
Expected: build has zero warnings, all tests pass, scale tests not run.

Run: `ctest --test-dir build/dev -L scale --output-on-failure`
Expected: `atlas_graph_scale_tests` and `atlas_render_scale_tests` pass.

Run: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
Expected: all pass with no sanitizer reports.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt CMakePresets.json .github/workflows/ci.yml .gitignore \
  modules/graph/tests/CMakeLists.txt modules/graph/tests/test_graph_engine_scale.cpp \
  modules/render/tests/CMakeLists.txt modules/render/tests/test_force_directed_layout_scale.cpp
git commit -m "build: add presets, strict warnings, sanitizers, scale test split, and CI"
```

---

### Task 2: Allow links between topics

**Files:**
- Modify: `modules/ui/src/workspace_controller.cpp` (inside `createRelationship`)
- Modify: `modules/ui/include/atlas/ui/workspace_controller.hpp` (comment above `createRelationship`)
- Modify: `modules/ui/src/graph_window.cpp` (`refreshGraph`)
- Test: `modules/ui/tests/test_workspace_controller.cpp:447`, `modules/ui/tests/test_graph_window.cpp`

**Interfaces:**
- Produces: `WorkspaceController::createRelationship` accepts endpoints in different topics.

- [ ] **Step 1: Replace the rejection test with acceptance tests**

In `modules/ui/tests/test_workspace_controller.cpp` replace the whole test case `"createRelationship rejects connecting KnowledgeObjects in different topics"` with:

```cpp
TEST_CASE("createRelationship connects KnowledgeObjects in different topics") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    auto paging = controller.createKnowledgeObject("Paging", os).value();
    auto indexing = controller.createKnowledgeObject("Indexing", databases).value();

    auto result =
        controller.createRelationship(paging, indexing, RelationshipType::RelatedTo, std::nullopt);
    REQUIRE(result.hasValue());
    CHECK(controller.allRelationships().size() == 1);
}

TEST_CASE("moving a concept to another topic keeps its relationships") {
    auto db = openTestDatabase();
    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto first = controller.createTopic("First").value();
    auto second = controller.createTopic("Second").value();
    auto a = controller.createKnowledgeObject("A", first).value();
    auto b = controller.createKnowledgeObject("B", first).value();
    REQUIRE(controller.createRelationship(a, b, RelationshipType::DependsOn, std::nullopt).hasValue());

    KnowledgeObjectEdits edits;
    edits.topicId = second;
    REQUIRE(controller.updateKnowledgeObject(a, edits).hasValue());

    CHECK(controller.allRelationships().size() == 1);
    RelationshipRepository repository(db);
    CHECK(repository.findAll().value().size() == 1);
}
```

Add to `modules/ui/tests/test_graph_window.cpp`:

```cpp
TEST_CASE("GraphWindow shows a topic that has links into another topic") {
    auto dbResult = Database::open(":memory:");
    REQUIRE(dbResult.hasValue());
    auto db = std::move(dbResult).value();

    WorkspaceController controller(db);
    REQUIRE(controller.load().hasValue());

    auto os = controller.createTopic("Operating Systems").value();
    auto databases = controller.createTopic("Databases").value();
    auto paging = controller.createKnowledgeObject("Paging", os).value();
    auto indexing = controller.createKnowledgeObject("Indexing", databases).value();
    REQUIRE(controller.createRelationship(indexing, paging, RelationshipType::Uses, std::nullopt)
                .hasValue());

    GraphWindow window(controller, nullptr, true, os);
    CHECK(window.canvasItem() != nullptr);
    window.setTopic(databases);
    CHECK(window.canvasItem() != nullptr);
}
```

- [ ] **Step 2: Run to verify the acceptance test fails**

Run: `cmake --build --preset dev && ./build/dev/modules/ui/tests/atlas_ui_tests -tc="createRelationship connects*"`
Expected: FAIL, `result.hasValue()` is false.

- [ ] **Step 3: Remove the topic check**

In `modules/ui/src/workspace_controller.cpp`, inside `createRelationship`, delete this block and the 4 comment lines directly above it:

```cpp
    if (sourceObject->topicId() != targetObject->topicId()) {
        return Result<RelationshipId, ControllerFailure>::err(
            {ControllerErrorCode::ValidationFailed,
             "Concepts in different topics can't be connected to each other"});
    }
```

In `modules/ui/include/atlas/ui/workspace_controller.hpp`, replace the 9 line comment directly above `createRelationship` with:

```cpp
    // Rejects self-loops and duplicates, including a symmetric pair stored in reverse.
```

- [ ] **Step 4: Draw only links whose two ends are in the topic**

In `modules/ui/src/graph_window.cpp`, `refreshGraph()`: replace the comment block at the top of the function with:

```cpp
    // Links into other topics are left out of a single topic's view.
```

and replace the relationship loop with:

```cpp
    for (auto& relationship : controller_->allRelationships()) {
        if (topicGraph.findNode(relationship.sourceId()) != nullptr &&
            topicGraph.findNode(relationship.targetId()) != nullptr) {
            topicGraph.addEdge(std::move(relationship));
        }
    }
```

- [ ] **Step 5: Run the UI tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_ui_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/ui/src/workspace_controller.cpp modules/ui/include/atlas/ui/workspace_controller.hpp \
  modules/ui/src/graph_window.cpp modules/ui/tests/test_workspace_controller.cpp \
  modules/ui/tests/test_graph_window.cpp
git commit -m "feat: allow relationships between concepts in different topics"
```

---

### Task 3: Learning value types in atlas-core

**Files:**
- Create: `modules/core/include/atlas/core/memory.hpp`, `modules/core/src/memory.cpp`
- Test: `modules/core/tests/test_memory.cpp`
- Modify: `modules/core/CMakeLists.txt` (add `src/memory.cpp`), `modules/core/tests/CMakeLists.txt` (add `test_memory.cpp`)

**Interfaces:**
- Produces (namespace `atlas::core`):
  - `using TimePoint = std::chrono::system_clock::time_point;`
  - `enum class ItemKind { Concept, Link };`
  - `enum class Grade { Again = 1, Hard = 2, Good = 3, Easy = 4 };`
  - `enum class Certainty { Guess = 1, Unsure = 2, FairlySure = 3, Certain = 4 };`
  - `enum class Exercise { Rebuild, Explain };`
  - `enum class Phase { New, Learning, Review, Relearning };`
  - `struct ItemRef { ItemKind kind; Uuid id; static ItemRef forConcept(const KnowledgeObjectId&); static ItemRef forLink(const RelationshipId&); }` with `==` and `std::hash`
  - `struct ReviewEvent`, `struct MemoryState` (with `==`), `struct Placement` as below
  - `toStorageString(ItemKind|Exercise|Phase)`, `itemKindFromString`, `exerciseFromString`, `phaseFromString`, `gradeFromInt`, `certaintyFromInt`

- [ ] **Step 1: Write the failing test** `modules/core/tests/test_memory.cpp`

```cpp
#include <unordered_set>

#include "atlas/core/memory.hpp"
#include "doctest.h"

using namespace atlas::core;

TEST_CASE("item kinds, exercises and phases round-trip through storage strings") {
    for (auto kind : {ItemKind::Concept, ItemKind::Link}) {
        CHECK(itemKindFromString(toStorageString(kind)) == kind);
    }
    for (auto exercise : {Exercise::Rebuild, Exercise::Explain}) {
        CHECK(exerciseFromString(toStorageString(exercise)) == exercise);
    }
    for (auto phase : {Phase::New, Phase::Learning, Phase::Review, Phase::Relearning}) {
        CHECK(phaseFromString(toStorageString(phase)) == phase);
    }
    CHECK_FALSE(itemKindFromString("node").has_value());
    CHECK_FALSE(phaseFromString("").has_value());
}

TEST_CASE("grades and certainty accept only 1 to 4") {
    CHECK(gradeFromInt(1) == Grade::Again);
    CHECK(gradeFromInt(4) == Grade::Easy);
    CHECK_FALSE(gradeFromInt(0).has_value());
    CHECK_FALSE(gradeFromInt(5).has_value());
    CHECK(certaintyFromInt(4) == Certainty::Certain);
    CHECK_FALSE(certaintyFromInt(0).has_value());
}

TEST_CASE("an item ref is identified by kind and id together") {
    auto id = Uuid::generate();
    ItemRef asConcept{ItemKind::Concept, id};
    ItemRef asLink{ItemKind::Link, id};
    CHECK(asConcept != asLink);

    std::unordered_set<ItemRef> set{asConcept, asLink, asConcept};
    CHECK(set.size() == 2);

    auto conceptId = KnowledgeObjectId::generate();
    CHECK(ItemRef::forConcept(conceptId) == ItemRef{ItemKind::Concept, conceptId.value()});
}
```

Add `test_memory.cpp` to `atlas_core_tests` in `modules/core/tests/CMakeLists.txt`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `atlas/core/memory.hpp` not found.

- [ ] **Step 3: Create `modules/core/include/atlas/core/memory.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "atlas/core/strong_id.hpp"
#include "atlas/core/uuid.hpp"

namespace atlas::core {

using TimePoint = std::chrono::system_clock::time_point;

enum class ItemKind { Concept, Link };
enum class Grade { Again = 1, Hard = 2, Good = 3, Easy = 4 };
enum class Certainty { Guess = 1, Unsure = 2, FairlySure = 3, Certain = 4 };
enum class Exercise { Rebuild, Explain };
enum class Phase { New, Learning, Review, Relearning };

struct ItemRef {
    ItemKind kind = ItemKind::Concept;
    Uuid id;

    static ItemRef forConcept(const KnowledgeObjectId& conceptId) {
        return ItemRef{ItemKind::Concept, conceptId.value()};
    }
    static ItemRef forLink(const RelationshipId& linkId) {
        return ItemRef{ItemKind::Link, linkId.value()};
    }

    bool operator==(const ItemRef&) const = default;
};

struct ReviewEvent {
    Uuid id;
    ItemRef item;
    Uuid sessionId;
    std::string deviceId;
    TimePoint reviewedAt;
    double elapsedDays = 0.0;
    Exercise exercise = Exercise::Rebuild;
    Certainty predicted = Certainty::Unsure;
    Grade grade = Grade::Good;
    int hintsUsed = 0;
    std::optional<KnowledgeObjectId> wrongTarget;
    std::chrono::milliseconds responseTime{0};
};

struct MemoryState {
    ItemRef item;
    Phase phase = Phase::New;
    double stability = 0.0;
    double difficulty = 0.0;
    std::optional<TimePoint> lastReviewedAt;
    std::optional<TimePoint> dueAt;
    int reviewCount = 0;
    int lapseCount = 0;

    bool operator==(const MemoryState&) const = default;
};

struct Placement {
    KnowledgeObjectId conceptId;
    double x = 0.0;
    double y = 0.0;
    bool pinned = false;
};

std::string_view toStorageString(ItemKind kind);
std::optional<ItemKind> itemKindFromString(std::string_view text);

std::string_view toStorageString(Exercise exercise);
std::optional<Exercise> exerciseFromString(std::string_view text);

std::string_view toStorageString(Phase phase);
std::optional<Phase> phaseFromString(std::string_view text);

std::optional<Grade> gradeFromInt(int64_t value);
std::optional<Certainty> certaintyFromInt(int64_t value);

}  // namespace atlas::core

namespace std {
template <>
struct hash<atlas::core::ItemRef> {
    size_t operator()(const atlas::core::ItemRef& item) const noexcept {
        return std::hash<atlas::core::Uuid>{}(item.id) ^ static_cast<size_t>(item.kind);
    }
};
}  // namespace std
```

- [ ] **Step 4: Create `modules/core/src/memory.cpp`**

```cpp
#include "atlas/core/memory.hpp"

namespace atlas::core {

std::string_view toStorageString(ItemKind kind) {
    return kind == ItemKind::Concept ? "concept" : "link";
}

std::optional<ItemKind> itemKindFromString(std::string_view text) {
    if (text == "concept") return ItemKind::Concept;
    if (text == "link") return ItemKind::Link;
    return std::nullopt;
}

std::string_view toStorageString(Exercise exercise) {
    return exercise == Exercise::Rebuild ? "rebuild" : "explain";
}

std::optional<Exercise> exerciseFromString(std::string_view text) {
    if (text == "rebuild") return Exercise::Rebuild;
    if (text == "explain") return Exercise::Explain;
    return std::nullopt;
}

std::string_view toStorageString(Phase phase) {
    switch (phase) {
        case Phase::New: return "new";
        case Phase::Learning: return "learning";
        case Phase::Review: return "review";
        case Phase::Relearning: return "relearning";
    }
    return "new";
}

std::optional<Phase> phaseFromString(std::string_view text) {
    if (text == "new") return Phase::New;
    if (text == "learning") return Phase::Learning;
    if (text == "review") return Phase::Review;
    if (text == "relearning") return Phase::Relearning;
    return std::nullopt;
}

std::optional<Grade> gradeFromInt(int64_t value) {
    if (value < 1 || value > 4) return std::nullopt;
    return static_cast<Grade>(value);
}

std::optional<Certainty> certaintyFromInt(int64_t value) {
    if (value < 1 || value > 4) return std::nullopt;
    return static_cast<Certainty>(value);
}

}  // namespace atlas::core
```

Add `src/memory.cpp` to `atlas_core` in `modules/core/CMakeLists.txt`.

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_core_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/core
git commit -m "feat(core): add learning value types"
```

---

### Task 4: atlas-learning module and FSRS-6

**Files:**
- Create: `modules/learning/CMakeLists.txt`, `modules/learning/tests/CMakeLists.txt`, `modules/learning/tests/test_main.cpp`, `modules/learning/tests/test_support.hpp`
- Create: `modules/learning/include/atlas/learning/fsrs.hpp`, `modules/learning/src/fsrs.cpp`
- Test: `modules/learning/tests/test_fsrs.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(modules/learning)` after `modules/graph`)

**Interfaces:**
- Consumes: Task 3 types.
- Produces (namespace `atlas::learning`):
  - `struct FsrsParameters { std::array<double, 21> w; double desiredRetention = 0.9; double maximumIntervalDays = 36500.0; };`
  - `class Fsrs` with `retrievability(double elapsedDays, double stability)`, `recallChance(const MemoryState&, TimePoint now)`, `intervalDays(double stability)`, `initialStability(Grade)`, `initialDifficulty(Grade)`, `nextDifficulty(double, Grade)`, `shortTermStability(double, Grade)`, `recallStability(double d, double s, double r, Grade)`, `forgetStability(double d, double s, double r)`, `review(const MemoryState&, Grade, TimePoint at, double firstReviewBoost = 1.0)`
  - `double elapsedDays(TimePoint from, TimePoint to)` (never negative)
  - Test helpers in `atlas::learning::testing`: `day(double)`, `addConcept(graph, title, topic)`, `addLink(graph, source, target, type)`, `event(item, at, grade, exercise, predicted, response)`

- [ ] **Step 1: Create the module build files**

`modules/learning/CMakeLists.txt`:

```cmake
add_library(atlas_learning STATIC
    src/fsrs.cpp
)

target_include_directories(atlas_learning PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(atlas_learning PUBLIC cxx_std_20)
target_link_libraries(atlas_learning PUBLIC atlas_core atlas_graph)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

`modules/learning/tests/CMakeLists.txt`:

```cmake
add_executable(atlas_learning_tests
    test_main.cpp
    test_fsrs.cpp
)

target_link_libraries(atlas_learning_tests PRIVATE atlas_learning)
target_include_directories(atlas_learning_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)

add_test(NAME atlas_learning_tests COMMAND atlas_learning_tests)
```

`modules/learning/tests/test_main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

`modules/learning/tests/test_support.hpp`:

```cpp
#pragma once

#include <chrono>
#include <optional>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/core/memory.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/graph/graph_engine.hpp"
#include "doctest.h"

namespace atlas::learning::testing {

using namespace atlas::core;
using atlas::graph::GraphEngine;

inline TimePoint day(double days) {
    auto offset = std::chrono::duration<double, std::ratio<86400>>(20000.0 + days);
    return TimePoint{} + std::chrono::duration_cast<std::chrono::system_clock::duration>(offset);
}

inline KnowledgeObjectId addConcept(GraphEngine& graph, const char* title,
                                    std::optional<TopicId> topic = std::nullopt) {
    auto object = KnowledgeObject::create(title).value();
    if (topic) object.assignToTopic(*topic);
    auto id = object.id();
    REQUIRE(graph.addNode(std::move(object)).hasValue());
    return id;
}

inline RelationshipId addLink(GraphEngine& graph, const KnowledgeObjectId& source,
                              const KnowledgeObjectId& target, RelationshipType type) {
    auto link = Relationship::create(source, target, type).value();
    auto id = link.id();
    REQUIRE(graph.addEdge(std::move(link)).hasValue());
    return id;
}

inline ReviewEvent event(const ItemRef& item, TimePoint at, Grade grade,
                         Exercise exercise = Exercise::Rebuild,
                         Certainty predicted = Certainty::Unsure,
                         std::chrono::milliseconds response = std::chrono::milliseconds(10000)) {
    ReviewEvent result;
    result.id = Uuid::generate();
    result.item = item;
    result.sessionId = Uuid::generate();
    result.deviceId = "test";
    result.reviewedAt = at;
    result.exercise = exercise;
    result.predicted = predicted;
    result.grade = grade;
    result.responseTime = response;
    return result;
}

}  // namespace atlas::learning::testing
```

Add `add_subdirectory(modules/learning)` to the root `CMakeLists.txt` directly after `add_subdirectory(modules/graph)`.

- [ ] **Step 2: Write the failing test** `modules/learning/tests/test_fsrs.cpp`

Expected values come from the FSRS-6 reference implementation (py-fsrs) formulas with default parameters.

```cpp
#include <cmath>

#include "atlas/learning/fsrs.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using atlas::learning::testing::day;

namespace {
constexpr double kEps = 1e-8;
}

TEST_CASE("retrievability follows the FSRS-6 power curve") {
    Fsrs fsrs;
    CHECK(fsrs.retrievability(10.0, 10.0) == doctest::Approx(0.9).epsilon(kEps));
    CHECK(fsrs.retrievability(5.0, 10.0) == doctest::Approx(0.9403442889).epsilon(kEps));
    CHECK(fsrs.retrievability(30.0, 10.0) == doctest::Approx(0.8093881036).epsilon(kEps));
    CHECK(fsrs.retrievability(0.0, 10.0) == doctest::Approx(1.0));
}

TEST_CASE("initial stability and difficulty match the reference values") {
    Fsrs fsrs;
    CHECK(fsrs.initialStability(Grade::Again) == doctest::Approx(0.212).epsilon(kEps));
    CHECK(fsrs.initialStability(Grade::Good) == doctest::Approx(2.3065).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Again) == doctest::Approx(6.4133).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Hard) == doctest::Approx(5.1121707056).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Good) == doctest::Approx(2.1181039705).epsilon(kEps));
    CHECK(fsrs.initialDifficulty(Grade::Easy) == doctest::Approx(1.0).epsilon(kEps));
}

TEST_CASE("difficulty update applies damping and mean reversion") {
    Fsrs fsrs;
    CHECK(fsrs.nextDifficulty(5.0, Grade::Again) == doctest::Approx(8.3417623693).epsilon(kEps));
    CHECK(fsrs.nextDifficulty(5.0, Grade::Good) == doctest::Approx(4.9902283693).epsilon(kEps));
    CHECK(fsrs.nextDifficulty(5.0, Grade::Easy) == doctest::Approx(3.3144613693).epsilon(kEps));
}

TEST_CASE("stability updates match the reference values") {
    Fsrs fsrs;
    CHECK(fsrs.recallStability(5.0, 10.0, 0.9, Grade::Good) == doctest::Approx(32.0267294820).epsilon(kEps));
    CHECK(fsrs.recallStability(5.0, 10.0, 0.9, Grade::Hard) == doctest::Approx(23.2468751105).epsilon(kEps));
    CHECK(fsrs.recallStability(5.0, 10.0, 0.9, Grade::Easy) == doctest::Approx(51.2538616468).epsilon(kEps));
    CHECK(fsrs.recallStability(5.0, 10.0, 0.7, Grade::Good) == doctest::Approx(81.7063939568).epsilon(kEps));
    CHECK(fsrs.forgetStability(5.0, 10.0, 0.9) == doctest::Approx(1.3919869730).epsilon(kEps));
    CHECK(fsrs.shortTermStability(2.3065, Grade::Good) == doctest::Approx(2.3065).epsilon(kEps));
    CHECK(fsrs.shortTermStability(2.3065, Grade::Again) == doctest::Approx(0.7750839829).epsilon(kEps));
}

TEST_CASE("the interval at 90 percent retention equals stability and is capped") {
    Fsrs fsrs;
    CHECK(fsrs.intervalDays(10.0) == doctest::Approx(10.0).epsilon(kEps));
    CHECK(fsrs.intervalDays(1e9) == doctest::Approx(36500.0));
}

TEST_CASE("a full review history follows the reference scenario") {
    Fsrs fsrs;
    MemoryState state{ItemRef::forConcept(KnowledgeObjectId::generate())};

    state = fsrs.review(state, Grade::Good, day(0));
    CHECK(state.phase == Phase::Review);
    CHECK(state.stability == doctest::Approx(2.3065).epsilon(kEps));
    CHECK(state.difficulty == doctest::Approx(2.1181039705).epsilon(kEps));
    REQUIRE(state.dueAt.has_value());
    CHECK(elapsedDays(day(0), *state.dueAt) == doctest::Approx(2.3065).epsilon(1e-6));

    state = fsrs.review(state, Grade::Good, day(2));
    CHECK(state.stability == doctest::Approx(10.9643323358).epsilon(kEps));
    CHECK(state.difficulty == doctest::Approx(2.1112142358).epsilon(kEps));

    state = fsrs.review(state, Grade::Again, day(22));
    CHECK(state.phase == Phase::Relearning);
    CHECK(state.lapseCount == 1);
    CHECK(state.reviewCount == 3);
    CHECK(state.stability == doctest::Approx(1.6595898557).epsilon(kEps));
    CHECK(state.difficulty == doctest::Approx(7.3922381323).epsilon(kEps));
}

TEST_CASE("a first review rated again starts the learning phase") {
    Fsrs fsrs;
    auto state = fsrs.review(MemoryState{ItemRef::forConcept(KnowledgeObjectId::generate())},
                             Grade::Again, day(0));
    CHECK(state.phase == Phase::Learning);
    CHECK(state.lapseCount == 0);
}

TEST_CASE("the first review boost multiplies initial stability only") {
    Fsrs fsrs;
    MemoryState fresh{ItemRef::forConcept(KnowledgeObjectId::generate())};
    auto boosted = fsrs.review(fresh, Grade::Good, day(0), 1.2);
    CHECK(boosted.stability == doctest::Approx(2.3065 * 1.2).epsilon(kEps));

    auto second = fsrs.review(boosted, Grade::Good, day(3), 5.0);
    auto unboosted = fsrs.review(boosted, Grade::Good, day(3));
    CHECK(second == unboosted);
}

TEST_CASE("a review dated before the previous one counts as zero elapsed days") {
    Fsrs fsrs;
    auto state = fsrs.review(MemoryState{ItemRef::forConcept(KnowledgeObjectId::generate())},
                             Grade::Good, day(5));
    auto earlier = fsrs.review(state, Grade::Good, day(4));
    CHECK(std::isfinite(earlier.stability));
    CHECK(earlier.stability >= state.stability);
    CHECK(elapsedDays(day(5), day(4)) == 0.0);
}

TEST_CASE("an item never reviewed has zero recall chance") {
    Fsrs fsrs;
    MemoryState fresh{ItemRef::forConcept(KnowledgeObjectId::generate())};
    CHECK(fsrs.recallChance(fresh, day(0)) == 0.0);
}
```

- [ ] **Step 3: Run to verify it fails**

Run: `cmake --preset dev && cmake --build --preset dev`
Expected: FAIL, `atlas/learning/fsrs.hpp` not found.

- [ ] **Step 4: Create `modules/learning/include/atlas/learning/fsrs.hpp`**

```cpp
#pragma once

#include <array>

#include "atlas/core/memory.hpp"

namespace atlas::learning {

using atlas::core::Grade;
using atlas::core::MemoryState;
using atlas::core::TimePoint;

struct FsrsParameters {
    std::array<double, 21> w{0.212,  1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194,
                             0.001,  1.8722, 0.1666, 0.796,  1.4835, 0.0614, 0.2629,
                             1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, 0.1542};
    double desiredRetention = 0.9;
    double maximumIntervalDays = 36500.0;
};

double elapsedDays(TimePoint from, TimePoint to);

class Fsrs {
public:
    explicit Fsrs(FsrsParameters parameters = {});

    double retrievability(double elapsed, double stability) const;
    double recallChance(const MemoryState& state, TimePoint now) const;
    double intervalDays(double stability) const;

    double initialStability(Grade grade) const;
    double initialDifficulty(Grade grade) const;
    double nextDifficulty(double difficulty, Grade grade) const;
    double shortTermStability(double stability, Grade grade) const;
    double recallStability(double difficulty, double stability, double retrievability, Grade grade) const;
    double forgetStability(double difficulty, double stability, double retrievability) const;

    MemoryState review(const MemoryState& state, Grade grade, TimePoint at,
                       double firstReviewBoost = 1.0) const;

    const FsrsParameters& parameters() const { return parameters_; }

private:
    double rawInitialDifficulty(Grade grade) const;

    FsrsParameters parameters_;
    double decay_;
    double factor_;
};

}  // namespace atlas::learning
```

- [ ] **Step 5: Create `modules/learning/src/fsrs.cpp`**

```cpp
#include "atlas/learning/fsrs.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::learning {

using atlas::core::Phase;

namespace {

constexpr double kMinStability = 0.001;
constexpr double kMinDifficulty = 1.0;
constexpr double kMaxDifficulty = 10.0;

double clampStability(double stability) { return std::max(stability, kMinStability); }
double clampDifficulty(double difficulty) { return std::clamp(difficulty, kMinDifficulty, kMaxDifficulty); }
int rating(Grade grade) { return static_cast<int>(grade); }

std::chrono::system_clock::duration toDuration(double days) {
    return std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::duration<double, std::ratio<86400>>(days));
}

}  // namespace

double elapsedDays(TimePoint from, TimePoint to) {
    std::chrono::duration<double, std::ratio<86400>> days = to - from;
    return std::max(days.count(), 0.0);
}

Fsrs::Fsrs(FsrsParameters parameters)
    : parameters_(parameters),
      decay_(-parameters.w[20]),
      factor_(std::pow(0.9, 1.0 / decay_) - 1.0) {}

double Fsrs::retrievability(double elapsed, double stability) const {
    return std::pow(1.0 + factor_ * elapsed / stability, decay_);
}

double Fsrs::recallChance(const MemoryState& state, TimePoint now) const {
    if (state.phase == Phase::New || !state.lastReviewedAt) return 0.0;
    return retrievability(elapsedDays(*state.lastReviewedAt, now), state.stability);
}

double Fsrs::intervalDays(double stability) const {
    double raw = stability / factor_ * (std::pow(parameters_.desiredRetention, 1.0 / decay_) - 1.0);
    return std::min(raw, parameters_.maximumIntervalDays);
}

double Fsrs::initialStability(Grade grade) const {
    return clampStability(parameters_.w[rating(grade) - 1]);
}

double Fsrs::rawInitialDifficulty(Grade grade) const {
    const auto& w = parameters_.w;
    return w[4] - std::exp(w[5] * (rating(grade) - 1)) + 1.0;
}

double Fsrs::initialDifficulty(Grade grade) const {
    return clampDifficulty(rawInitialDifficulty(grade));
}

double Fsrs::nextDifficulty(double difficulty, Grade grade) const {
    const auto& w = parameters_.w;
    double delta = -w[6] * (rating(grade) - 3);
    double damped = difficulty + (10.0 - difficulty) * delta / 9.0;
    return clampDifficulty(w[7] * rawInitialDifficulty(Grade::Easy) + (1.0 - w[7]) * damped);
}

double Fsrs::shortTermStability(double stability, Grade grade) const {
    const auto& w = parameters_.w;
    double increase = std::exp(w[17] * (rating(grade) - 3 + w[18])) * std::pow(stability, -w[19]);
    if (grade != Grade::Again) increase = std::max(increase, 1.0);
    return clampStability(stability * increase);
}

double Fsrs::recallStability(double difficulty, double stability, double retrievability,
                             Grade grade) const {
    const auto& w = parameters_.w;
    double hardPenalty = grade == Grade::Hard ? w[15] : 1.0;
    double easyBonus = grade == Grade::Easy ? w[16] : 1.0;
    double growth = std::exp(w[8]) * (11.0 - difficulty) * std::pow(stability, -w[9]) *
                    (std::exp((1.0 - retrievability) * w[10]) - 1.0) * hardPenalty * easyBonus;
    return clampStability(stability * (1.0 + growth));
}

double Fsrs::forgetStability(double difficulty, double stability, double retrievability) const {
    const auto& w = parameters_.w;
    double longTerm = w[11] * std::pow(difficulty, -w[12]) * (std::pow(stability + 1.0, w[13]) - 1.0) *
                      std::exp((1.0 - retrievability) * w[14]);
    double shortTerm = stability / std::exp(w[17] * w[18]);
    return clampStability(std::min(longTerm, shortTerm));
}

MemoryState Fsrs::review(const MemoryState& state, Grade grade, TimePoint at,
                         double firstReviewBoost) const {
    MemoryState next = state;
    if (state.phase == Phase::New || !state.lastReviewedAt) {
        next.stability = clampStability(initialStability(grade) * firstReviewBoost);
        next.difficulty = initialDifficulty(grade);
        next.phase = grade == Grade::Again ? Phase::Learning : Phase::Review;
    } else {
        double elapsed = elapsedDays(*state.lastReviewedAt, at);
        double recall = retrievability(elapsed, state.stability);
        if (elapsed < 1.0) {
            next.stability = shortTermStability(state.stability, grade);
        } else if (grade == Grade::Again) {
            next.stability = forgetStability(state.difficulty, state.stability, recall);
        } else {
            next.stability = recallStability(state.difficulty, state.stability, recall, grade);
        }
        next.difficulty = nextDifficulty(state.difficulty, grade);
        if (grade == Grade::Again && state.phase == Phase::Review) {
            next.phase = Phase::Relearning;
            ++next.lapseCount;
        } else if (grade != Grade::Again) {
            next.phase = Phase::Review;
        }
    }
    ++next.reviewCount;
    next.lastReviewedAt = at;
    next.dueAt = at + toDuration(intervalDays(next.stability));
    return next;
}

}  // namespace atlas::learning
```

- [ ] **Step 6: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_learning_tests`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt modules/learning
git commit -m "feat(learning): add atlas-learning module with FSRS-6 memory model"
```

---

### Task 5: MemoryLedger (replay the log)

**Files:**
- Create: `modules/learning/include/atlas/learning/memory_ledger.hpp`, `modules/learning/src/memory_ledger.cpp`
- Test: `modules/learning/tests/test_memory_ledger.cpp`
- Modify: `modules/learning/CMakeLists.txt` (add `src/memory_ledger.cpp`), `modules/learning/tests/CMakeLists.txt` (add `test_memory_ledger.cpp`)

**Interfaces:**
- Consumes: `Fsrs::review`, `Fsrs::recallChance`.
- Produces:
  - `using StateMap = std::unordered_map<ItemRef, MemoryState>;`
  - `using BoostFn = std::function<double(const ItemRef&, TimePoint, const StateMap&)>;`
  - `using ForecastFn = std::function<void(const ReviewEvent&, double recallChance)>;`
  - `class MemoryLedger { explicit MemoryLedger(Fsrs fsrs = Fsrs{}); void apply(StateMap&, const ReviewEvent&, const BoostFn& = {}) const; StateMap replay(std::vector<ReviewEvent>, TimePoint now, const BoostFn& = {}, const ForecastFn& = {}) const; };`
  - `std::vector<ReviewEvent> inReplayOrder(std::vector<ReviewEvent>, TimePoint now);`

- [ ] **Step 1: Write the failing test** `modules/learning/tests/test_memory_ledger.cpp`

```cpp
#include <algorithm>

#include "atlas/learning/memory_ledger.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

TEST_CASE("apply gives the same state as a direct review") {
    MemoryLedger ledger;
    Fsrs fsrs;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());

    StateMap states;
    ledger.apply(states, event(item, day(0), Grade::Good));
    CHECK(states.at(item) == fsrs.review(MemoryState{item}, Grade::Good, day(0)));
}

TEST_CASE("replay does not depend on the order events arrive in") {
    MemoryLedger ledger;
    auto a = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto b = ItemRef::forLink(RelationshipId::generate());
    std::vector<ReviewEvent> events{event(a, day(0), Grade::Good), event(b, day(0.5), Grade::Hard),
                                    event(a, day(3), Grade::Again), event(b, day(4), Grade::Good),
                                    event(a, day(4), Grade::Good)};
    auto forward = ledger.replay(events, day(10));
    std::reverse(events.begin(), events.end());
    auto backward = ledger.replay(events, day(10));
    CHECK(forward.at(a) == backward.at(a));
    CHECK(forward.at(b) == backward.at(b));
    CHECK(forward.at(a).reviewCount == 3);
}

TEST_CASE("replay clamps review times in the future to now") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto states = ledger.replay({event(item, day(10), Grade::Good)}, day(5));
    CHECK(states.at(item).lastReviewedAt == day(5));
}

TEST_CASE("the boost applies to the first review only") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    BoostFn doubleIt = [](const ItemRef&, TimePoint, const StateMap&) { return 2.0; };

    StateMap states;
    ledger.apply(states, event(item, day(0), Grade::Good), doubleIt);
    CHECK(states.at(item).stability == doctest::Approx(2.3065 * 2.0));

    auto before = states.at(item);
    ledger.apply(states, event(item, day(5), Grade::Good), doubleIt);
    CHECK(states.at(item) == Fsrs{}.review(before, Grade::Good, day(5)));
}

TEST_CASE("a forecast is reported for every review except the first") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    std::vector<double> forecasts;
    ledger.replay({event(item, day(0), Grade::Good), event(item, day(2.3065), Grade::Again),
                   event(item, day(5), Grade::Good)},
                  day(10), {}, [&](const ReviewEvent&, double chance) { forecasts.push_back(chance); });
    REQUIRE(forecasts.size() == 2);
    CHECK(forecasts[0] == doctest::Approx(0.9).epsilon(1e-6));
}
```

Add `test_memory_ledger.cpp` to the test executable and `src/memory_ledger.cpp` to the library.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `memory_ledger.hpp` not found.

- [ ] **Step 3: Create `modules/learning/include/atlas/learning/memory_ledger.hpp`**

```cpp
#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/learning/fsrs.hpp"

namespace atlas::learning {

using atlas::core::ItemRef;
using atlas::core::ReviewEvent;

using StateMap = std::unordered_map<ItemRef, MemoryState>;
using BoostFn = std::function<double(const ItemRef&, TimePoint, const StateMap&)>;
using ForecastFn = std::function<void(const ReviewEvent&, double recallChance)>;

std::vector<ReviewEvent> inReplayOrder(std::vector<ReviewEvent> events, TimePoint now);

class MemoryLedger {
public:
    explicit MemoryLedger(Fsrs fsrs = Fsrs{});

    void apply(StateMap& states, const ReviewEvent& event, const BoostFn& boost = {}) const;

    StateMap replay(std::vector<ReviewEvent> events, TimePoint now, const BoostFn& boost = {},
                    const ForecastFn& forecast = {}) const;

private:
    Fsrs fsrs_;
};

}  // namespace atlas::learning
```

- [ ] **Step 4: Create `modules/learning/src/memory_ledger.cpp`**

```cpp
#include "atlas/learning/memory_ledger.hpp"

#include <algorithm>

namespace atlas::learning {

using atlas::core::Phase;

std::vector<ReviewEvent> inReplayOrder(std::vector<ReviewEvent> events, TimePoint now) {
    for (auto& event : events) event.reviewedAt = std::min(event.reviewedAt, now);
    std::sort(events.begin(), events.end(), [](const ReviewEvent& a, const ReviewEvent& b) {
        if (a.reviewedAt != b.reviewedAt) return a.reviewedAt < b.reviewedAt;
        return a.id.toString() < b.id.toString();
    });
    return events;
}

MemoryLedger::MemoryLedger(Fsrs fsrs) : fsrs_(std::move(fsrs)) {}

void MemoryLedger::apply(StateMap& states, const ReviewEvent& event, const BoostFn& boost) const {
    auto current = states.try_emplace(event.item, MemoryState{event.item}).first->second;
    bool first = current.phase == Phase::New;
    double factor = first && boost ? boost(event.item, event.reviewedAt, states) : 1.0;
    states[event.item] = fsrs_.review(current, event.grade, event.reviewedAt, factor);
}

StateMap MemoryLedger::replay(std::vector<ReviewEvent> events, TimePoint now, const BoostFn& boost,
                              const ForecastFn& forecast) const {
    StateMap states;
    for (const auto& event : inReplayOrder(std::move(events), now)) {
        auto known = states.find(event.item);
        if (forecast && known != states.end() && known->second.phase != Phase::New) {
            forecast(event, fsrs_.recallChance(known->second, event.reviewedAt));
        }
        apply(states, event, boost);
    }
    return states;
}

}  // namespace atlas::learning
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_learning_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/learning
git commit -m "feat(learning): replay the review log into memory states"
```

---

### Task 6: NetworkRules (frontier, schema boost, contrast pairs, leverage)

**Files:**
- Create: `modules/learning/include/atlas/learning/network_rules.hpp`, `modules/learning/src/network_rules.cpp`
- Test: `modules/learning/tests/test_network_rules.cpp`
- Modify: both learning `CMakeLists.txt` files

**Interfaces:**
- Consumes: `GraphEngine::dependsOn`, `transitiveDependencies`, `transitiveDependents`, `neighbors`, `findEdge`, `allNodeIds`; `Fsrs::recallChance`; `StateMap`, `BoostFn`.
- Produces:
  - `struct NetworkConfig { double solidThreshold = 0.80; double schemaBeta = 0.2; };`
  - `std::vector<KnowledgeObjectId> contrastPartners(const GraphEngine&, const KnowledgeObjectId&);` sorted by id text
  - `class NetworkRules { NetworkRules(const GraphEngine&, Fsrs = Fsrs{}, NetworkConfig = {}); bool isIntroduced(const ItemRef&, const StateMap&) const; bool isSolid(const KnowledgeObjectId&, TimePoint, const StateMap&) const; bool isOnFrontier(const KnowledgeObjectId&, TimePoint, const StateMap&) const; std::vector<KnowledgeObjectId> frontier(TimePoint, const StateMap&) const; double schemaBoost(const ItemRef&, TimePoint, const StateMap&) const; BoostFn boostFn() const; bool isLinkReady(const RelationshipId&, const StateMap&) const; int leverage(const KnowledgeObjectId&) const; };`
  - The graph passed in must outlive the `NetworkRules`.

- [ ] **Step 1: Write the failing test** `modules/learning/tests/test_network_rules.cpp`

```cpp
#include "atlas/learning/network_rules.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

namespace {

struct Fixture {
    GraphEngine graph;
    KnowledgeObjectId tree = addConcept(graph, "Tree");
    KnowledgeObjectId btree = addConcept(graph, "B-Tree");
    KnowledgeObjectId index = addConcept(graph, "Index");
    KnowledgeObjectId hash = addConcept(graph, "Hash Table");
    RelationshipId btreeNeedsTree = addLink(graph, btree, tree, RelationshipType::DependsOn);
    RelationshipId indexNeedsBtree = addLink(graph, index, btree, RelationshipType::DependsOn);
    RelationshipId hashVsBtree = addLink(graph, hash, btree, RelationshipType::AlternativeTo);
    Fsrs fsrs;
    NetworkRules rules{graph, fsrs};
    StateMap states;

    void review(const KnowledgeObjectId& conceptId, Grade grade, TimePoint at) {
        auto item = ItemRef::forConcept(conceptId);
        auto it = states.try_emplace(item, MemoryState{item}).first;
        it->second = fsrs.review(it->second, grade, at);
    }
};

}  // namespace

TEST_CASE("the frontier starts with concepts that have no prerequisites, highest leverage first") {
    Fixture f;
    CHECK(f.rules.frontier(day(0), f.states) == std::vector<KnowledgeObjectId>{f.tree, f.hash});
    CHECK(f.rules.leverage(f.tree) == 2);
    CHECK(f.rules.leverage(f.hash) == 0);
}

TEST_CASE("a solid prerequisite opens the frontier to the concept that needs it") {
    Fixture f;
    f.review(f.tree, Grade::Good, day(0));
    CHECK(f.rules.isSolid(f.tree, day(3), f.states));
    CHECK(f.rules.frontier(day(3), f.states) == std::vector<KnowledgeObjectId>{f.btree, f.hash});
}

TEST_CASE("a failed or faded prerequisite keeps the frontier closed") {
    Fixture failed;
    failed.review(failed.tree, Grade::Again, day(0));
    CHECK_FALSE(failed.rules.isOnFrontier(failed.btree, day(0), failed.states));

    Fixture faded;
    faded.review(faded.tree, Grade::Good, day(0));
    CHECK_FALSE(faded.rules.isSolid(faded.tree, day(10), faded.states));
    CHECK_FALSE(faded.rules.isOnFrontier(faded.btree, day(10), faded.states));
}

TEST_CASE("concepts in a dependency cycle do not block each other") {
    GraphEngine graph;
    auto a = addConcept(graph, "A");
    auto b = addConcept(graph, "B");
    addLink(graph, a, b, RelationshipType::DependsOn);
    addLink(graph, b, a, RelationshipType::DependsOn);
    NetworkRules rules(graph);
    CHECK(rules.isOnFrontier(a, day(0), {}));
    CHECK(rules.isOnFrontier(b, day(0), {}));
}

TEST_CASE("the schema boost grows with the recall chance of prerequisites") {
    Fixture f;
    CHECK(f.rules.schemaBoost(ItemRef::forConcept(f.tree), day(0), f.states) == 1.0);
    CHECK(f.rules.schemaBoost(ItemRef::forConcept(f.btree), day(0), f.states) == 1.0);

    f.review(f.tree, Grade::Good, day(0));
    CHECK(f.rules.schemaBoost(ItemRef::forConcept(f.btree), day(0), f.states) == doctest::Approx(1.2));
    CHECK(f.rules.schemaBoost(ItemRef::forLink(f.btreeNeedsTree), day(0), f.states) == 1.0);
    CHECK(f.rules.boostFn()(ItemRef::forConcept(f.btree), day(0), f.states) == doctest::Approx(1.2));
}

TEST_CASE("a link is ready once both of its concepts are introduced") {
    Fixture f;
    f.review(f.tree, Grade::Good, day(0));
    CHECK_FALSE(f.rules.isLinkReady(f.btreeNeedsTree, f.states));
    f.review(f.btree, Grade::Good, day(1));
    CHECK(f.rules.isLinkReady(f.btreeNeedsTree, f.states));
    CHECK_FALSE(f.rules.isLinkReady(RelationshipId::generate(), f.states));
}

TEST_CASE("contrast partners are found from either side") {
    Fixture f;
    CHECK(contrastPartners(f.graph, f.btree) == std::vector<KnowledgeObjectId>{f.hash});
    CHECK(contrastPartners(f.graph, f.hash) == std::vector<KnowledgeObjectId>{f.btree});
    CHECK(contrastPartners(f.graph, f.tree).empty());
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `network_rules.hpp` not found.

- [ ] **Step 3: Create `modules/learning/include/atlas/learning/network_rules.hpp`**

```cpp
#pragma once

#include <vector>

#include "atlas/graph/graph_engine.hpp"
#include "atlas/learning/fsrs.hpp"
#include "atlas/learning/memory_ledger.hpp"

namespace atlas::learning {

using atlas::core::KnowledgeObjectId;
using atlas::core::RelationshipId;
using atlas::graph::GraphEngine;

struct NetworkConfig {
    double solidThreshold = 0.80;
    double schemaBeta = 0.2;
};

std::vector<KnowledgeObjectId> contrastPartners(const GraphEngine& graph,
                                                const KnowledgeObjectId& conceptId);

class NetworkRules {
public:
    NetworkRules(const GraphEngine& graph, Fsrs fsrs = Fsrs{}, NetworkConfig config = {});

    bool isIntroduced(const ItemRef& item, const StateMap& states) const;
    bool isSolid(const KnowledgeObjectId& conceptId, TimePoint now, const StateMap& states) const;
    bool isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now, const StateMap& states) const;
    std::vector<KnowledgeObjectId> frontier(TimePoint now, const StateMap& states) const;

    double schemaBoost(const ItemRef& item, TimePoint at, const StateMap& states) const;
    BoostFn boostFn() const;

    bool isLinkReady(const RelationshipId& linkId, const StateMap& states) const;
    int leverage(const KnowledgeObjectId& conceptId) const;

private:
    const GraphEngine* graph_;
    Fsrs fsrs_;
    NetworkConfig config_;
};

}  // namespace atlas::learning
```

- [ ] **Step 4: Create `modules/learning/src/network_rules.cpp`**

```cpp
#include "atlas/learning/network_rules.hpp"

#include <algorithm>
#include <utility>

namespace atlas::learning {

using atlas::core::ItemKind;
using atlas::core::Phase;
using atlas::core::RelationshipType;

namespace {

bool byIdText(const KnowledgeObjectId& a, const KnowledgeObjectId& b) {
    return a.toString() < b.toString();
}

bool contains(const std::vector<KnowledgeObjectId>& ids, const KnowledgeObjectId& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}  // namespace

std::vector<KnowledgeObjectId> contrastPartners(const GraphEngine& graph,
                                                const KnowledgeObjectId& conceptId) {
    std::vector<KnowledgeObjectId> partners;
    for (auto type : {RelationshipType::AlternativeTo, RelationshipType::OppositeOf}) {
        auto found = graph.neighbors(conceptId, type, GraphEngine::Direction::Both);
        partners.insert(partners.end(), found.begin(), found.end());
    }
    std::sort(partners.begin(), partners.end(), byIdText);
    partners.erase(std::unique(partners.begin(), partners.end()), partners.end());
    return partners;
}

NetworkRules::NetworkRules(const GraphEngine& graph, Fsrs fsrs, NetworkConfig config)
    : graph_(&graph), fsrs_(std::move(fsrs)), config_(config) {}

bool NetworkRules::isIntroduced(const ItemRef& item, const StateMap& states) const {
    auto it = states.find(item);
    return it != states.end() && it->second.phase != Phase::New;
}

bool NetworkRules::isSolid(const KnowledgeObjectId& conceptId, TimePoint now,
                           const StateMap& states) const {
    auto it = states.find(ItemRef::forConcept(conceptId));
    if (it == states.end() || it->second.phase != Phase::Review) return false;
    return fsrs_.recallChance(it->second, now) >= config_.solidThreshold;
}

bool NetworkRules::isOnFrontier(const KnowledgeObjectId& conceptId, TimePoint now,
                                const StateMap& states) const {
    if (isIntroduced(ItemRef::forConcept(conceptId), states)) return false;
    for (const auto& prerequisite : graph_->dependsOn(conceptId)) {
        if (isSolid(prerequisite, now, states)) continue;
        if (!contains(graph_->transitiveDependencies(prerequisite), conceptId)) return false;
    }
    return true;
}

std::vector<KnowledgeObjectId> NetworkRules::frontier(TimePoint now, const StateMap& states) const {
    std::vector<std::pair<int, KnowledgeObjectId>> ranked;
    for (const auto& id : graph_->allNodeIds()) {
        if (isOnFrontier(id, now, states)) ranked.emplace_back(leverage(id), id);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return byIdText(a.second, b.second);
    });
    std::vector<KnowledgeObjectId> result;
    for (const auto& entry : ranked) result.push_back(entry.second);
    return result;
}

double NetworkRules::schemaBoost(const ItemRef& item, TimePoint at, const StateMap& states) const {
    if (item.kind != ItemKind::Concept) return 1.0;
    auto prerequisites = graph_->dependsOn(KnowledgeObjectId(item.id));
    if (prerequisites.empty()) return 1.0;
    double total = 0.0;
    for (const auto& prerequisite : prerequisites) {
        auto it = states.find(ItemRef::forConcept(prerequisite));
        if (it != states.end()) total += fsrs_.recallChance(it->second, at);
    }
    return 1.0 + config_.schemaBeta * total / static_cast<double>(prerequisites.size());
}

BoostFn NetworkRules::boostFn() const {
    return [this](const ItemRef& item, TimePoint at, const StateMap& states) {
        return schemaBoost(item, at, states);
    };
}

bool NetworkRules::isLinkReady(const RelationshipId& linkId, const StateMap& states) const {
    const auto* link = graph_->findEdge(linkId);
    return link != nullptr && isIntroduced(ItemRef::forConcept(link->sourceId()), states) &&
           isIntroduced(ItemRef::forConcept(link->targetId()), states);
}

int NetworkRules::leverage(const KnowledgeObjectId& conceptId) const {
    return static_cast<int>(graph_->transitiveDependents(conceptId).size());
}

}  // namespace atlas::learning
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_learning_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/learning
git commit -m "feat(learning): add frontier, schema boost, and contrast pair rules"
```

---

### Task 7: RebuildGrader

**Files:**
- Create: `modules/learning/include/atlas/learning/rebuild_grader.hpp`, `modules/learning/src/rebuild_grader.cpp`
- Test: `modules/learning/tests/test_rebuild_grader.cpp`
- Modify: both learning `CMakeLists.txt` files

**Interfaces:**
- Consumes: `contrastPartners` (Task 6), `GraphEngine::findEdge`, `neighbors`, `atlas::core::isSymmetric`.
- Produces:
  - `struct RecalledLink { KnowledgeObjectId other; RelationshipType type; bool focusIsSource = true; };`
  - `struct RebuildAnswer { KnowledgeObjectId focus; Certainty predicted = Certainty::Unsure; std::vector<RecalledLink> recalled; std::vector<KnowledgeObjectId> hinted; std::chrono::milliseconds responseTime{0}; };`
  - `struct GradedLink { RelationshipId link; Grade grade; int hintsUsed = 0; std::optional<KnowledgeObjectId> wrongTarget; };`
  - `class RebuildGrader { explicit RebuildGrader(const GraphEngine&); std::vector<GradedLink> grade(const RebuildAnswer&, const std::vector<RelationshipId>& hidden, std::optional<std::chrono::milliseconds> medianResponse) const; };`
  - `std::optional<std::chrono::milliseconds> medianRebuildResponse(const std::vector<ReviewEvent>&);` nullopt below 20 rebuild events

- [ ] **Step 1: Write the failing test** `modules/learning/tests/test_rebuild_grader.cpp`

```cpp
#include "atlas/learning/rebuild_grader.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;
using std::chrono::milliseconds;

namespace {

struct Fixture {
    GraphEngine graph;
    KnowledgeObjectId tree = addConcept(graph, "Tree");
    KnowledgeObjectId btree = addConcept(graph, "B-Tree");
    KnowledgeObjectId index = addConcept(graph, "Index");
    KnowledgeObjectId hash = addConcept(graph, "Hash Table");
    KnowledgeObjectId skiplist = addConcept(graph, "Skip List");
    KnowledgeObjectId array = addConcept(graph, "Array");
    KnowledgeObjectId heap = addConcept(graph, "Heap");
    RelationshipId needsTree = addLink(graph, btree, tree, RelationshipType::DependsOn);
    RelationshipId indexNeeds = addLink(graph, index, btree, RelationshipType::DependsOn);
    RelationshipId versusHash = addLink(graph, btree, hash, RelationshipType::AlternativeTo);
    RelationshipId skipVersusTree = addLink(graph, skiplist, tree, RelationshipType::AlternativeTo);
    RelationshipId heapNeedsArray = addLink(graph, heap, array, RelationshipType::DependsOn);
    RelationshipId treeNeedsArray = addLink(graph, tree, array, RelationshipType::DependsOn);
    RebuildGrader grader{graph};
    std::vector<RelationshipId> hidden{needsTree, indexNeeds, versusHash};

    RebuildAnswer answer(std::vector<RecalledLink> recalled) const {
        RebuildAnswer result{btree};
        result.recalled = std::move(recalled);
        return result;
    }
};

}  // namespace

TEST_CASE("every link recalled correctly without hints is good") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.tree, RelationshipType::DependsOn, true},
                                           {f.index, RelationshipType::DependsOn, false},
                                           {f.hash, RelationshipType::AlternativeTo, false}}),
                                 f.hidden, std::nullopt);
    REQUIRE(graded.size() == 3);
    for (const auto& link : graded) {
        CHECK(link.grade == Grade::Good);
        CHECK(link.hintsUsed == 0);
        CHECK_FALSE(link.wrongTarget.has_value());
    }
}

TEST_CASE("a wrong direction or type is hard, but symmetric links accept either direction") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.tree, RelationshipType::DependsOn, false},
                                           {f.index, RelationshipType::Uses, false},
                                           {f.hash, RelationshipType::AlternativeTo, true}}),
                                 f.hidden, std::nullopt);
    CHECK(graded[0].grade == Grade::Hard);
    CHECK(graded[1].grade == Grade::Hard);
    CHECK(graded[2].grade == Grade::Good);
}

TEST_CASE("a hinted link is hard when recalled and again when missed") {
    Fixture f;
    auto answer = f.answer({{f.tree, RelationshipType::DependsOn, true}});
    answer.hinted = {f.tree, f.index};
    auto graded = f.grader.grade(answer, f.hidden, std::nullopt);
    CHECK(graded[0].grade == Grade::Hard);
    CHECK(graded[0].hintsUsed == 1);
    CHECK(graded[1].grade == Grade::Again);
    CHECK(graded[1].hintsUsed == 1);
    CHECK(graded[2].grade == Grade::Again);
    CHECK(graded[2].hintsUsed == 0);
}

TEST_CASE("easy needs a certain prediction, a fast answer, and enough history") {
    Fixture f;
    auto answer = f.answer({{f.tree, RelationshipType::DependsOn, true}});
    answer.predicted = Certainty::Certain;
    answer.responseTime = milliseconds(5000);
    std::vector<RelationshipId> onlyTree{f.needsTree};

    CHECK(f.grader.grade(answer, onlyTree, milliseconds(10000))[0].grade == Grade::Easy);
    CHECK(f.grader.grade(answer, onlyTree, std::nullopt)[0].grade == Grade::Good);

    answer.responseTime = milliseconds(15000);
    CHECK(f.grader.grade(answer, onlyTree, milliseconds(10000))[0].grade == Grade::Good);

    answer.responseTime = milliseconds(5000);
    answer.predicted = Certainty::FairlySure;
    CHECK(f.grader.grade(answer, onlyTree, milliseconds(10000))[0].grade == Grade::Good);
}

TEST_CASE("a missed link records the contrast partner named instead of it") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.skiplist, RelationshipType::DependsOn, true}}),
                                 f.hidden, std::nullopt);
    CHECK(graded[0].grade == Grade::Again);
    CHECK(graded[0].wrongTarget == f.skiplist);
    CHECK_FALSE(graded[1].wrongTarget.has_value());
    CHECK_FALSE(graded[2].wrongTarget.has_value());
}

TEST_CASE("a missed link records a named concept that shares a neighbor with its target") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.heap, RelationshipType::DependsOn, true}}),
                                 f.hidden, std::nullopt);
    CHECK(graded[0].wrongTarget == f.heap);
    CHECK_FALSE(graded[1].wrongTarget.has_value());
}

TEST_CASE("naming a hidden neighbor twice is not reported as a confusion") {
    Fixture f;
    auto graded = f.grader.grade(f.answer({{f.tree, RelationshipType::DependsOn, true},
                                           {f.tree, RelationshipType::DependsOn, true}}),
                                 f.hidden, std::nullopt);
    for (const auto& link : graded) CHECK_FALSE(link.wrongTarget.has_value());
}

TEST_CASE("two links to the same concept are matched by type first") {
    GraphEngine graph;
    auto a = addConcept(graph, "A");
    auto b = addConcept(graph, "B");
    auto uses = addLink(graph, a, b, RelationshipType::Uses);
    auto needs = addLink(graph, a, b, RelationshipType::DependsOn);
    RebuildGrader grader(graph);
    RebuildAnswer answer{a};
    answer.recalled = {{b, RelationshipType::DependsOn, true}, {b, RelationshipType::Uses, true}};
    auto graded = grader.grade(answer, {uses, needs}, std::nullopt);
    CHECK(graded[0].grade == Grade::Good);
    CHECK(graded[1].grade == Grade::Good);
}

TEST_CASE("the median response needs at least 20 rebuild events") {
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    std::vector<ReviewEvent> events;
    for (int i = 1; i <= 30; ++i) {
        events.push_back(event(item, day(i), Grade::Good, Exercise::Explain, Certainty::Unsure, milliseconds(1)));
    }
    for (int i = 1; i <= 19; ++i) {
        events.push_back(event(item, day(i), Grade::Good, Exercise::Rebuild, Certainty::Unsure, milliseconds(i * 1000)));
    }
    CHECK_FALSE(medianRebuildResponse(events).has_value());

    events.push_back(event(item, day(20), Grade::Good, Exercise::Rebuild, Certainty::Unsure, milliseconds(20000)));
    CHECK(medianRebuildResponse(events) == milliseconds(11000));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `rebuild_grader.hpp` not found.

- [ ] **Step 3: Create `modules/learning/include/atlas/learning/rebuild_grader.hpp`**

```cpp
#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/core/relationship.hpp"
#include "atlas/graph/graph_engine.hpp"

namespace atlas::learning {

using atlas::core::Certainty;
using atlas::core::Grade;
using atlas::core::KnowledgeObjectId;
using atlas::core::Relationship;
using atlas::core::RelationshipId;
using atlas::core::RelationshipType;
using atlas::core::ReviewEvent;
using atlas::graph::GraphEngine;

struct RecalledLink {
    KnowledgeObjectId other;
    RelationshipType type;
    bool focusIsSource = true;
};

struct RebuildAnswer {
    KnowledgeObjectId focus;
    Certainty predicted = Certainty::Unsure;
    std::vector<RecalledLink> recalled;
    std::vector<KnowledgeObjectId> hinted;
    std::chrono::milliseconds responseTime{0};
};

struct GradedLink {
    RelationshipId link;
    Grade grade;
    int hintsUsed = 0;
    std::optional<KnowledgeObjectId> wrongTarget;
};

std::optional<std::chrono::milliseconds> medianRebuildResponse(const std::vector<ReviewEvent>& events);

class RebuildGrader {
public:
    explicit RebuildGrader(const GraphEngine& graph);

    std::vector<GradedLink> grade(const RebuildAnswer& answer, const std::vector<RelationshipId>& hidden,
                                  std::optional<std::chrono::milliseconds> medianResponse) const;

private:
    void attachWrongTarget(std::vector<GradedLink>& graded, const std::vector<KnowledgeObjectId>& targets,
                           const KnowledgeObjectId& wrong, const KnowledgeObjectId& focus) const;

    const GraphEngine* graph_;
};

}  // namespace atlas::learning
```

- [ ] **Step 4: Create `modules/learning/src/rebuild_grader.cpp`**

```cpp
#include "atlas/learning/rebuild_grader.hpp"

#include <algorithm>

#include "atlas/learning/network_rules.hpp"

namespace atlas::learning {

using atlas::core::Exercise;

namespace {

constexpr size_t kMinRebuildsForEasy = 20;

KnowledgeObjectId otherEnd(const Relationship& link, const KnowledgeObjectId& focus) {
    return link.sourceId() == focus ? link.targetId() : link.sourceId();
}

bool contains(const std::vector<KnowledgeObjectId>& ids, const KnowledgeObjectId& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool sharesNeighbor(const GraphEngine& graph, const KnowledgeObjectId& a, const KnowledgeObjectId& b,
                    const KnowledgeObjectId& excluded) {
    auto neighborsOfB = graph.neighbors(b, std::nullopt, GraphEngine::Direction::Both);
    for (const auto& neighbor : graph.neighbors(a, std::nullopt, GraphEngine::Direction::Both)) {
        if (neighbor != excluded && contains(neighborsOfB, neighbor)) return true;
    }
    return false;
}

Grade gradeMatch(const RebuildAnswer& answer, const Relationship& link, const RecalledLink& recalled,
                 bool hinted, std::optional<std::chrono::milliseconds> medianResponse) {
    bool typeRight = recalled.type == link.type();
    bool directionRight = atlas::core::isSymmetric(link.type()) ||
                          recalled.focusIsSource == (link.sourceId() == answer.focus);
    if (hinted || !typeRight || !directionRight) return Grade::Hard;
    bool fast = medianResponse.has_value() && answer.responseTime < *medianResponse;
    return answer.predicted == Certainty::Certain && fast ? Grade::Easy : Grade::Good;
}

}  // namespace

std::optional<std::chrono::milliseconds> medianRebuildResponse(const std::vector<ReviewEvent>& events) {
    std::vector<std::chrono::milliseconds> times;
    for (const auto& event : events) {
        if (event.exercise == Exercise::Rebuild) times.push_back(event.responseTime);
    }
    if (times.size() < kMinRebuildsForEasy) return std::nullopt;
    auto middle = times.begin() + static_cast<long>(times.size() / 2);
    std::nth_element(times.begin(), middle, times.end());
    return *middle;
}

RebuildGrader::RebuildGrader(const GraphEngine& graph) : graph_(&graph) {}

std::vector<GradedLink> RebuildGrader::grade(const RebuildAnswer& answer,
                                             const std::vector<RelationshipId>& hidden,
                                             std::optional<std::chrono::milliseconds> medianResponse) const {
    std::vector<const Relationship*> links;
    std::vector<KnowledgeObjectId> targets;
    for (const auto& id : hidden) {
        const Relationship* link = graph_->findEdge(id);
        if (link == nullptr) continue;
        links.push_back(link);
        targets.push_back(otherEnd(*link, answer.focus));
    }

    std::vector<std::optional<size_t>> matches(links.size());
    std::vector<bool> used(answer.recalled.size(), false);
    for (bool requireType : {true, false}) {
        for (size_t h = 0; h < links.size(); ++h) {
            if (matches[h]) continue;
            for (size_t i = 0; i < answer.recalled.size(); ++i) {
                const auto& recalled = answer.recalled[i];
                if (used[i] || recalled.other != targets[h]) continue;
                if (requireType && recalled.type != links[h]->type()) continue;
                matches[h] = i;
                used[i] = true;
                break;
            }
        }
    }

    std::vector<GradedLink> graded;
    for (size_t h = 0; h < links.size(); ++h) {
        bool hinted = contains(answer.hinted, targets[h]);
        GradedLink result{links[h]->id(), Grade::Again, hinted ? 1 : 0, std::nullopt};
        if (matches[h]) {
            result.grade = gradeMatch(answer, *links[h], answer.recalled[*matches[h]], hinted, medianResponse);
        }
        graded.push_back(result);
    }

    for (size_t i = 0; i < answer.recalled.size(); ++i) {
        const auto& named = answer.recalled[i].other;
        if (!used[i] && !contains(targets, named)) attachWrongTarget(graded, targets, named, answer.focus);
    }
    return graded;
}

void RebuildGrader::attachWrongTarget(std::vector<GradedLink>& graded,
                                      const std::vector<KnowledgeObjectId>& targets,
                                      const KnowledgeObjectId& wrong, const KnowledgeObjectId& focus) const {
    auto partners = contrastPartners(*graph_, wrong);
    GradedLink* byNeighbor = nullptr;
    for (size_t h = 0; h < graded.size(); ++h) {
        auto& candidate = graded[h];
        if (candidate.grade != Grade::Again || candidate.wrongTarget) continue;
        if (contains(partners, targets[h])) {
            candidate.wrongTarget = wrong;
            return;
        }
        if (byNeighbor == nullptr && sharesNeighbor(*graph_, targets[h], wrong, focus)) {
            byNeighbor = &candidate;
        }
    }
    if (byNeighbor != nullptr) byNeighbor->wrongTarget = wrong;
}

}  // namespace atlas::learning
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_learning_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/learning
git commit -m "feat(learning): grade map rebuilds and detect confused concepts"
```

---

### Task 8: SessionPlanner

**Files:**
- Create: `modules/learning/include/atlas/learning/session_planner.hpp`, `modules/learning/src/session_planner.cpp`
- Test: `modules/learning/tests/test_session_planner.cpp`
- Modify: both learning `CMakeLists.txt` files

**Interfaces:**
- Consumes: `NetworkRules` (Task 6), `contrastPartners`, `Fsrs::recallChance`, `GraphEngine::allNodeIds`, `allEdgeIds`, `findEdge`, `findNode`.
- Produces:
  - `struct SessionLimits { int newPerDay = 5; int maxFocus = 12; std::chrono::seconds defaultFocusTime{40}; };`
  - `struct FocusPlan { KnowledgeObjectId conceptId; std::vector<ItemRef> items; bool isNew = false; };`
  - `struct SessionPlan { std::vector<FocusPlan> focuses; std::chrono::seconds estimatedDuration{0}; };`
  - `class SessionPlanner { SessionPlanner(const GraphEngine&, const NetworkRules&, Fsrs = Fsrs{}); SessionPlan plan(const StateMap&, TimePoint now, int introducedToday, const SessionLimits&, std::optional<std::chrono::milliseconds> medianFocusTime = std::nullopt) const; };`

- [ ] **Step 1: Write the failing test** `modules/learning/tests/test_session_planner.cpp`

```cpp
#include "atlas/learning/session_planner.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

namespace {

struct World {
    GraphEngine graph;
    Fsrs fsrs;
    NetworkRules rules{graph, fsrs};
    SessionPlanner planner{graph, rules, fsrs};
    StateMap states;

    void review(const ItemRef& item, Grade grade, TimePoint at) {
        auto it = states.try_emplace(item, MemoryState{item}).first;
        it->second = fsrs.review(it->second, grade, at);
    }
    void review(const KnowledgeObjectId& id, Grade grade, TimePoint at) {
        review(ItemRef::forConcept(id), grade, at);
    }
    std::vector<KnowledgeObjectId> order(const SessionPlan& plan) const {
        std::vector<KnowledgeObjectId> ids;
        for (const auto& focus : plan.focuses) ids.push_back(focus.conceptId);
        return ids;
    }
};

}  // namespace

TEST_CASE("a first session introduces frontier concepts up to the daily limit") {
    World w;
    auto tree = addConcept(w.graph, "Tree");
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    addLink(w.graph, btree, tree, RelationshipType::DependsOn);

    auto plan = w.planner.plan(w.states, day(0), 0, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{tree, hash});
    CHECK(plan.focuses[0].isNew);
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forConcept(tree)});

    SessionLimits oneNew;
    oneNew.newPerDay = 1;
    CHECK(w.order(w.planner.plan(w.states, day(0), 0, oneNew)) == std::vector<KnowledgeObjectId>{tree});
    CHECK(w.planner.plan(w.states, day(0), 5, SessionLimits{}).focuses.empty());
}

TEST_CASE("due items come first, weakest recall first, then new concepts") {
    World w;
    auto tree = addConcept(w.graph, "Tree");
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    addLink(w.graph, btree, tree, RelationshipType::DependsOn);
    w.review(tree, Grade::Good, day(0));
    w.review(hash, Grade::Hard, day(0));

    auto plan = w.planner.plan(w.states, day(3), 0, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{hash, tree, btree});
    CHECK_FALSE(plan.focuses[0].isNew);
    CHECK(plan.focuses[2].isNew);
}

TEST_CASE("a ready link is grouped under its source concept") {
    World w;
    auto tree = addConcept(w.graph, "Tree");
    auto btree = addConcept(w.graph, "B-Tree");
    auto link = addLink(w.graph, btree, tree, RelationshipType::DependsOn);
    w.review(tree, Grade::Good, day(0));
    w.review(btree, Grade::Good, day(0));

    auto plan = w.planner.plan(w.states, day(0), 2, SessionLimits{});
    REQUIRE(plan.focuses.size() == 1);
    CHECK(plan.focuses[0].conceptId == btree);
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forLink(link)});
}

TEST_CASE("a symmetric link goes to the weaker concept and its contrast partner follows") {
    World w;
    auto btree = addConcept(w.graph, "B-Tree");
    auto hash = addConcept(w.graph, "Hash Table");
    auto link = addLink(w.graph, hash, btree, RelationshipType::AlternativeTo);
    w.review(btree, Grade::Good, day(0));
    w.review(hash, Grade::Hard, day(0));

    auto plan = w.planner.plan(w.states, day(0.5), 2, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{hash, btree});
    CHECK(plan.focuses[0].items == std::vector<ItemRef>{ItemRef::forLink(link)});
    CHECK(plan.focuses[1].items == std::vector<ItemRef>{ItemRef::forConcept(btree)});
}

TEST_CASE("topics interleave so one topic never runs three in a row when another is waiting") {
    World w;
    auto a = TopicId::generate();
    auto b = TopicId::generate();
    auto weakest = addConcept(w.graph, "Weakest", a);
    auto weak = addConcept(w.graph, "Weak", a);
    auto fair = addConcept(w.graph, "Fair", a);
    auto other = addConcept(w.graph, "Other", b);
    w.review(weakest, Grade::Again, day(0));
    w.review(weak, Grade::Hard, day(0));
    w.review(fair, Grade::Good, day(0));
    w.review(other, Grade::Easy, day(0));

    auto plan = w.planner.plan(w.states, day(10), 0, SessionLimits{});
    CHECK(w.order(plan) == std::vector<KnowledgeObjectId>{weakest, weak, other, fair});

    SessionLimits two;
    two.maxFocus = 2;
    CHECK(w.order(w.planner.plan(w.states, day(10), 0, two)) == std::vector<KnowledgeObjectId>{weakest, weak});
}

TEST_CASE("the estimate uses the median focus time when it is known") {
    World w;
    for (const char* title : {"A", "B", "C"}) addConcept(w.graph, title);
    auto plan = w.planner.plan(w.states, day(0), 0, SessionLimits{});
    REQUIRE(plan.focuses.size() == 3);
    CHECK(plan.estimatedDuration == std::chrono::seconds(120));
    auto measured = w.planner.plan(w.states, day(0), 0, SessionLimits{}, std::chrono::milliseconds(20000));
    CHECK(measured.estimatedDuration == std::chrono::seconds(60));
}

TEST_CASE("states for concepts and links no longer in the graph are ignored") {
    World w;
    w.review(ItemRef::forConcept(KnowledgeObjectId::generate()), Grade::Good, day(0));
    w.review(ItemRef::forLink(RelationshipId::generate()), Grade::Good, day(0));
    auto plan = w.planner.plan(w.states, day(30), 0, SessionLimits{});
    CHECK(plan.focuses.empty());
    CHECK(plan.estimatedDuration == std::chrono::seconds(0));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `session_planner.hpp` not found.

- [ ] **Step 3: Create `modules/learning/include/atlas/learning/session_planner.hpp`**

```cpp
#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "atlas/learning/network_rules.hpp"

namespace atlas::learning {

using atlas::core::Relationship;
using atlas::core::TopicId;

struct SessionLimits {
    int newPerDay = 5;
    int maxFocus = 12;
    std::chrono::seconds defaultFocusTime{40};
};

struct FocusPlan {
    KnowledgeObjectId conceptId;
    std::vector<ItemRef> items;
    bool isNew = false;
};

struct SessionPlan {
    std::vector<FocusPlan> focuses;
    std::chrono::seconds estimatedDuration{0};
};

class SessionPlanner {
public:
    SessionPlanner(const GraphEngine& graph, const NetworkRules& rules, Fsrs fsrs = Fsrs{});

    SessionPlan plan(const StateMap& states, TimePoint now, int introducedToday,
                     const SessionLimits& limits,
                     std::optional<std::chrono::milliseconds> medianFocusTime = std::nullopt) const;

private:
    using FocusUnit = std::vector<FocusPlan>;

    double conceptRecall(const KnowledgeObjectId& conceptId, const StateMap& states, TimePoint now) const;
    KnowledgeObjectId linkOwner(const Relationship& link, const StateMap& states, TimePoint now) const;
    std::optional<TopicId> topicOf(const KnowledgeObjectId& conceptId) const;
    std::vector<FocusUnit> interleaveByTopic(std::vector<FocusUnit> units) const;

    const GraphEngine* graph_;
    const NetworkRules* rules_;
    Fsrs fsrs_;
};

}  // namespace atlas::learning
```

- [ ] **Step 4: Create `modules/learning/src/session_planner.cpp`**

```cpp
#include "atlas/learning/session_planner.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace atlas::learning {

using atlas::core::Phase;

namespace {

struct DueGroup {
    KnowledgeObjectId conceptId;
    std::vector<ItemRef> items;
    double lowestRecall = 1.0;
    int leverage = 0;
};

bool isDue(const MemoryState& state, TimePoint now) {
    return state.phase != Phase::New && state.dueAt.has_value() && *state.dueAt <= now;
}

}  // namespace

SessionPlanner::SessionPlanner(const GraphEngine& graph, const NetworkRules& rules, Fsrs fsrs)
    : graph_(&graph), rules_(&rules), fsrs_(std::move(fsrs)) {}

double SessionPlanner::conceptRecall(const KnowledgeObjectId& conceptId, const StateMap& states,
                                     TimePoint now) const {
    auto it = states.find(ItemRef::forConcept(conceptId));
    return it == states.end() ? 0.0 : fsrs_.recallChance(it->second, now);
}

KnowledgeObjectId SessionPlanner::linkOwner(const Relationship& link, const StateMap& states,
                                            TimePoint now) const {
    if (!atlas::core::isSymmetric(link.type())) return link.sourceId();
    bool targetWeaker = conceptRecall(link.targetId(), states, now) < conceptRecall(link.sourceId(), states, now);
    return targetWeaker ? link.targetId() : link.sourceId();
}

std::optional<TopicId> SessionPlanner::topicOf(const KnowledgeObjectId& conceptId) const {
    const auto* object = graph_->findNode(conceptId);
    if (object == nullptr) return std::nullopt;
    return object->topicId();
}

std::vector<SessionPlanner::FocusUnit> SessionPlanner::interleaveByTopic(std::vector<FocusUnit> units) const {
    std::vector<FocusUnit> result;
    while (!units.empty()) {
        size_t pick = 0;
        if (result.size() >= 2) {
            auto last = topicOf(result.back().front().conceptId);
            if (last == topicOf(result[result.size() - 2].front().conceptId)) {
                for (size_t i = 0; i < units.size(); ++i) {
                    if (topicOf(units[i].front().conceptId) != last) {
                        pick = i;
                        break;
                    }
                }
            }
        }
        result.push_back(std::move(units[pick]));
        units.erase(units.begin() + static_cast<long>(pick));
    }
    return result;
}

SessionPlan SessionPlanner::plan(const StateMap& states, TimePoint now, int introducedToday,
                                 const SessionLimits& limits,
                                 std::optional<std::chrono::milliseconds> medianFocusTime) const {
    std::unordered_map<KnowledgeObjectId, DueGroup> groups;
    auto addDue = [&](const KnowledgeObjectId& owner, const ItemRef& item, double recall) {
        auto& group = groups.try_emplace(owner, DueGroup{owner}).first->second;
        group.items.push_back(item);
        group.lowestRecall = std::min(group.lowestRecall, recall);
    };

    for (const auto& conceptId : graph_->allNodeIds()) {
        auto it = states.find(ItemRef::forConcept(conceptId));
        if (it != states.end() && isDue(it->second, now)) {
            addDue(conceptId, it->first, fsrs_.recallChance(it->second, now));
        }
    }
    for (const auto& linkId : graph_->allEdgeIds()) {
        if (!rules_->isLinkReady(linkId, states)) continue;
        auto item = ItemRef::forLink(linkId);
        auto it = states.find(item);
        bool fresh = it == states.end() || it->second.phase == Phase::New;
        if (!fresh && !isDue(it->second, now)) continue;
        double recall = fresh ? 0.0 : fsrs_.recallChance(it->second, now);
        addDue(linkOwner(*graph_->findEdge(linkId), states, now), item, recall);
    }

    std::vector<DueGroup*> ordered;
    for (auto& [conceptId, group] : groups) {
        group.leverage = rules_->leverage(conceptId);
        ordered.push_back(&group);
    }
    std::sort(ordered.begin(), ordered.end(), [](const DueGroup* a, const DueGroup* b) {
        if (a->lowestRecall != b->lowestRecall) return a->lowestRecall < b->lowestRecall;
        if (a->leverage != b->leverage) return a->leverage > b->leverage;
        return a->conceptId.toString() < b->conceptId.toString();
    });

    std::vector<FocusUnit> units;
    std::unordered_set<KnowledgeObjectId> placed;
    for (const DueGroup* group : ordered) {
        if (placed.contains(group->conceptId)) continue;
        FocusUnit unit{FocusPlan{group->conceptId, group->items, false}};
        placed.insert(group->conceptId);
        for (const auto& partner : contrastPartners(*graph_, group->conceptId)) {
            if (placed.contains(partner) || !rules_->isIntroduced(ItemRef::forConcept(partner), states)) continue;
            auto found = groups.find(partner);
            auto items = found != groups.end() ? found->second.items
                                               : std::vector<ItemRef>{ItemRef::forConcept(partner)};
            unit.push_back(FocusPlan{partner, std::move(items), false});
            placed.insert(partner);
        }
        units.push_back(std::move(unit));
    }

    int newSlots = std::max(0, limits.newPerDay - introducedToday);
    for (const auto& conceptId : rules_->frontier(now, states)) {
        if (newSlots == 0) break;
        if (placed.contains(conceptId)) continue;
        units.push_back(FocusUnit{FocusPlan{conceptId, {ItemRef::forConcept(conceptId)}, true}});
        placed.insert(conceptId);
        --newSlots;
    }

    SessionPlan plan;
    for (auto& unit : interleaveByTopic(std::move(units))) {
        bool full = plan.focuses.size() + unit.size() > static_cast<size_t>(limits.maxFocus);
        if (!plan.focuses.empty() && full) break;
        for (auto& focus : unit) plan.focuses.push_back(std::move(focus));
    }
    auto perFocus = medianFocusTime ? std::chrono::duration_cast<std::chrono::seconds>(*medianFocusTime)
                                    : limits.defaultFocusTime;
    plan.estimatedDuration = perFocus * static_cast<long>(plan.focuses.size());
    return plan;
}

}  // namespace atlas::learning
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_learning_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/learning
git commit -m "feat(learning): plan daily sessions from due items and the frontier"
```

---

### Task 9: Calibration (learner and model)

**Files:**
- Create: `modules/learning/include/atlas/learning/calibration.hpp`, `modules/learning/src/calibration.cpp`
- Test: `modules/learning/tests/test_calibration.cpp`
- Modify: both learning `CMakeLists.txt` files

**Interfaces:**
- Consumes: `MemoryLedger::replay` with `ForecastFn`.
- Produces:
  - `double expectedSuccess(Certainty);` guess 0.25, unsure 0.50, fairly sure 0.75, certain 0.95
  - `struct LevelStats { int attempts = 0; int successes = 0; double observedRate() const; };`
  - `struct LearnerCalibration { std::array<LevelStats, 4> levels{}; double overconfidence = 0.0; int total = 0; };` index is `Certainty - 1`
  - `LearnerCalibration learnerCalibration(const std::vector<ReviewEvent>&);`
  - `struct Forecast { double recallChance; bool recalled; };`
  - `struct ForecastBucket { int count = 0; double meanPredicted = 0.0; double observedRate = 0.0; };`
  - `struct ModelCalibration { std::array<ForecastBucket, 10> buckets{}; double logLoss = 0.0; int total = 0; };`
  - `ModelCalibration modelCalibration(const std::vector<Forecast>&);`
  - `std::vector<Forecast> collectForecasts(const MemoryLedger&, std::vector<ReviewEvent>, TimePoint now, const BoostFn& = {});`

- [ ] **Step 1: Write the failing test** `modules/learning/tests/test_calibration.cpp`

```cpp
#include "atlas/learning/calibration.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

TEST_CASE("learner calibration compares predictions with results") {
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    std::vector<ReviewEvent> events{
        event(item, day(0), Grade::Good, Exercise::Rebuild, Certainty::Certain),
        event(item, day(1), Grade::Again, Exercise::Rebuild, Certainty::Certain),
        event(item, day(2), Grade::Hard, Exercise::Explain, Certainty::Certain),
        event(item, day(3), Grade::Again, Exercise::Explain, Certainty::Certain),
    };
    auto calibration = learnerCalibration(events);
    CHECK(calibration.total == 4);
    CHECK(calibration.levels[3].attempts == 4);
    CHECK(calibration.levels[3].successes == 2);
    CHECK(calibration.levels[3].observedRate() == doctest::Approx(0.5));
    CHECK(calibration.overconfidence == doctest::Approx(0.45));
}

TEST_CASE("calibration of no events is all zero") {
    auto learner = learnerCalibration({});
    CHECK(learner.total == 0);
    CHECK(learner.overconfidence == 0.0);
    CHECK(learner.levels[0].observedRate() == 0.0);

    auto model = modelCalibration({});
    CHECK(model.total == 0);
    CHECK(model.logLoss == 0.0);
}

TEST_CASE("model calibration buckets forecasts and computes log loss") {
    auto model = modelCalibration({{0.95, true}, {0.95, false}, {0.15, false}});
    CHECK(model.total == 3);
    CHECK(model.buckets[9].count == 2);
    CHECK(model.buckets[9].meanPredicted == doctest::Approx(0.95));
    CHECK(model.buckets[9].observedRate == doctest::Approx(0.5));
    CHECK(model.buckets[1].count == 1);
    CHECK(model.logLoss == doctest::Approx(1.069848).epsilon(1e-5));
}

TEST_CASE("forecasts come from replaying the log") {
    MemoryLedger ledger;
    auto item = ItemRef::forConcept(KnowledgeObjectId::generate());
    auto forecasts = collectForecasts(
        ledger, {event(item, day(0), Grade::Good), event(item, day(2.3065), Grade::Again)}, day(5));
    REQUIRE(forecasts.size() == 1);
    CHECK(forecasts[0].recallChance == doctest::Approx(0.9).epsilon(1e-6));
    CHECK_FALSE(forecasts[0].recalled);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `calibration.hpp` not found.

- [ ] **Step 3: Create `modules/learning/include/atlas/learning/calibration.hpp`**

```cpp
#pragma once

#include <array>
#include <vector>

#include "atlas/learning/memory_ledger.hpp"

namespace atlas::learning {

using atlas::core::Certainty;

double expectedSuccess(Certainty certainty);

struct LevelStats {
    int attempts = 0;
    int successes = 0;
    double observedRate() const;
};

struct LearnerCalibration {
    std::array<LevelStats, 4> levels{};
    double overconfidence = 0.0;
    int total = 0;
};

LearnerCalibration learnerCalibration(const std::vector<ReviewEvent>& events);

struct Forecast {
    double recallChance;
    bool recalled;
};

struct ForecastBucket {
    int count = 0;
    double meanPredicted = 0.0;
    double observedRate = 0.0;
};

struct ModelCalibration {
    std::array<ForecastBucket, 10> buckets{};
    double logLoss = 0.0;
    int total = 0;
};

ModelCalibration modelCalibration(const std::vector<Forecast>& forecasts);

std::vector<Forecast> collectForecasts(const MemoryLedger& ledger, std::vector<ReviewEvent> events,
                                       TimePoint now, const BoostFn& boost = {});

}  // namespace atlas::learning
```

- [ ] **Step 4: Create `modules/learning/src/calibration.cpp`**

```cpp
#include "atlas/learning/calibration.hpp"

#include <algorithm>
#include <cmath>

namespace atlas::learning {

namespace {
constexpr double kProbabilityFloor = 1e-6;
}

double expectedSuccess(Certainty certainty) {
    switch (certainty) {
        case Certainty::Guess: return 0.25;
        case Certainty::Unsure: return 0.50;
        case Certainty::FairlySure: return 0.75;
        case Certainty::Certain: return 0.95;
    }
    return 0.50;
}

double LevelStats::observedRate() const {
    return attempts == 0 ? 0.0 : static_cast<double>(successes) / attempts;
}

LearnerCalibration learnerCalibration(const std::vector<ReviewEvent>& events) {
    LearnerCalibration result;
    double expected = 0.0;
    int successes = 0;
    for (const auto& event : events) {
        auto& level = result.levels[static_cast<size_t>(event.predicted) - 1];
        bool success = event.grade != Grade::Again;
        ++level.attempts;
        if (success) {
            ++level.successes;
            ++successes;
        }
        expected += expectedSuccess(event.predicted);
        ++result.total;
    }
    if (result.total > 0) result.overconfidence = (expected - successes) / result.total;
    return result;
}

ModelCalibration modelCalibration(const std::vector<Forecast>& forecasts) {
    ModelCalibration result;
    double loss = 0.0;
    for (const auto& forecast : forecasts) {
        auto index = std::min<size_t>(static_cast<size_t>(forecast.recallChance * 10.0), 9);
        auto& bucket = result.buckets[index];
        ++bucket.count;
        bucket.meanPredicted += forecast.recallChance;
        bucket.observedRate += forecast.recalled ? 1.0 : 0.0;
        double p = std::clamp(forecast.recallChance, kProbabilityFloor, 1.0 - kProbabilityFloor);
        loss -= forecast.recalled ? std::log(p) : std::log(1.0 - p);
        ++result.total;
    }
    for (auto& bucket : result.buckets) {
        if (bucket.count == 0) continue;
        bucket.meanPredicted /= bucket.count;
        bucket.observedRate /= bucket.count;
    }
    if (result.total > 0) result.logLoss = loss / result.total;
    return result;
}

std::vector<Forecast> collectForecasts(const MemoryLedger& ledger, std::vector<ReviewEvent> events,
                                       TimePoint now, const BoostFn& boost) {
    std::vector<Forecast> forecasts;
    ledger.replay(std::move(events), now, boost, [&](const ReviewEvent& event, double chance) {
        forecasts.push_back(Forecast{chance, event.grade != Grade::Again});
    });
    return forecasts;
}

}  // namespace atlas::learning
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_learning_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/learning
git commit -m "feat(learning): measure learner and model calibration"
```

---

### Task 10: Simulated learner

**Files:**
- Test: `modules/learning/tests/test_simulated_learner.cpp`
- Modify: `modules/learning/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SessionPlanner`, `NetworkRules`, `MemoryLedger`, `Fsrs` from Tasks 4 to 8. No production code changes expected.

- [ ] **Step 1: Write the test**

```cpp
#include <random>
#include <string>

#include "atlas/learning/memory_ledger.hpp"
#include "atlas/learning/network_rules.hpp"
#include "atlas/learning/session_planner.hpp"
#include "doctest.h"
#include "test_support.hpp"

using namespace atlas::core;
using namespace atlas::learning;
using namespace atlas::learning::testing;

TEST_CASE("a simulated learner keeps retention near the target over 90 days") {
    GraphEngine graph;
    std::vector<std::vector<KnowledgeObjectId>> chains(10);
    for (size_t chain = 0; chain < chains.size(); ++chain) {
        for (int level = 0; level < 4; ++level) {
            auto title = "C" + std::to_string(chain) + "L" + std::to_string(level);
            auto id = addConcept(graph, title.c_str());
            if (level > 0) addLink(graph, id, chains[chain].back(), RelationshipType::DependsOn);
            chains[chain].push_back(id);
        }
    }
    for (size_t chain = 0; chain + 1 < chains.size(); chain += 2) {
        addLink(graph, chains[chain][1], chains[chain + 1][1], RelationshipType::AlternativeTo);
    }

    Fsrs fsrs;
    NetworkRules rules(graph, fsrs);
    SessionPlanner planner(graph, rules, fsrs);
    MemoryLedger ledger(fsrs);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> coin(0.0, 1.0);

    StateMap states;
    int reviewed = 0;
    int recalled = 0;
    for (int d = 0; d < 90; ++d) {
        TimePoint now = day(d + 0.375);
        for (const auto& focus : planner.plan(states, now, 0, SessionLimits{}).focuses) {
            for (const auto& item : focus.items) {
                auto it = states.find(item);
                bool first = it == states.end() || it->second.phase == Phase::New;
                Grade grade = Grade::Good;
                if (!first) {
                    bool remembered = coin(rng) < fsrs.recallChance(it->second, now);
                    grade = remembered ? Grade::Good : Grade::Again;
                    if (d >= 60) {
                        ++reviewed;
                        if (remembered) ++recalled;
                    }
                }
                ledger.apply(states, event(item, now, grade), rules.boostFn());
            }
        }
    }

    int introduced = 0;
    for (const auto& id : graph.allNodeIds()) {
        if (rules.isIntroduced(ItemRef::forConcept(id), states)) ++introduced;
    }
    CHECK(introduced == 40);
    REQUIRE(reviewed > 0);
    double retention = static_cast<double>(recalled) / reviewed;
    CHECK(retention >= 0.80);
    CHECK(retention <= 0.98);
}
```

Add `test_simulated_learner.cpp` to `atlas_learning_tests`.

- [ ] **Step 2: Run it**

Run: `cmake --build --preset dev && ./build/dev/modules/learning/tests/atlas_learning_tests -tc="a simulated learner*"`
Expected: PASS. If it fails, print `introduced`, `reviewed`, `retention` with `MESSAGE` and report the numbers. Do not loosen the thresholds without asking.

- [ ] **Step 3: Commit**

```bash
git add modules/learning/tests
git commit -m "test(learning): simulate 90 days of a learner against the planner"
```

---

### Task 11: Statement helpers and migration 3

**Files:**
- Modify: `modules/persistence/src/detail/statement.hpp`, `modules/persistence/src/detail/statement.cpp`
- Modify: `modules/persistence/src/migrations.cpp`
- Test: `modules/persistence/tests/test_migration_three.cpp`
- Modify: `modules/persistence/tests/CMakeLists.txt`

**Interfaces:**
- Produces on `detail::Statement`: `void bindDouble(int, double)`, `void bindOptionalInt64(int, std::optional<int64_t>)`, `double columnDouble(int) const`, `std::optional<int64_t> columnOptionalInt64(int) const`.
- Produces `detail::inTransaction(sqlite3*, Body)`: runs `body()` (returns `Result<void, PersistenceError>`) inside BEGIN/COMMIT, rolls back on error.
- Produces tables `review_events`, `memory_states`, `memory_meta`, `node_placements` and triggers `trg_knowledge_objects_forget_learning`, `trg_relationships_forget_learning`.

- [ ] **Step 1: Write the failing test** `modules/persistence/tests/test_migration_three.cpp`

```cpp
#include <sqlite3.h>

#include <filesystem>
#include <string>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

void removeDatabaseFiles(const std::string& path) {
    std::error_code ignored;
    for (const char* suffix : {"", "-wal", "-shm"}) std::filesystem::remove(path + suffix, ignored);
}

int tableCount(const std::string& path, const char* table) {
    sqlite3* raw = nullptr;
    REQUIRE(sqlite3_open(path.c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM sqlite_master WHERE name = ?;", -1, &statement, nullptr);
    sqlite3_bind_text(statement, 1, table, -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    sqlite3_close(raw);
    return count;
}

}  // namespace

TEST_CASE("migration 3 upgrades a version 2 database and keeps its data") {
    auto path = (std::filesystem::temp_directory_path() / ("atlas_v2_" + Uuid::generate().toString() + ".db")).string();
    {
        auto db = Database::open(path).value();
        KnowledgeObjectRepository objects(db);
        REQUIRE(objects.save(KnowledgeObject::create("Paging").value()).hasValue());
    }
    {
        sqlite3* raw = nullptr;
        REQUIRE(sqlite3_open(path.c_str(), &raw) == SQLITE_OK);
        REQUIRE(sqlite3_exec(raw,
                             "DROP TRIGGER trg_knowledge_objects_forget_learning;"
                             "DROP TRIGGER trg_relationships_forget_learning;"
                             "DROP TABLE review_events; DROP TABLE memory_states;"
                             "DROP TABLE memory_meta; DROP TABLE node_placements;"
                             "DELETE FROM schema_migrations WHERE version = 3;",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(raw);
    }
    REQUIRE(tableCount(path, "review_events") == 0);

    {
        auto reopened = Database::open(path);
        REQUIRE(reopened.hasValue());
        auto db = std::move(reopened).value();
        KnowledgeObjectRepository objects(db);
        CHECK(objects.findAll().value().size() == 1);
    }
    for (const char* table : {"review_events", "memory_states", "memory_meta", "node_placements",
                              "trg_knowledge_objects_forget_learning", "trg_relationships_forget_learning"}) {
        CHECK(tableCount(path, table) == 1);
    }
    removeDatabaseFiles(path);
}
```

Add `test_migration_three.cpp` to `atlas_persistence_tests`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_persistence_tests`
Expected: FAIL, `DROP TRIGGER` fails because the triggers do not exist.

- [ ] **Step 3: Add Statement helpers**

In `statement.hpp`, add to the public section of `Statement`:

```cpp
    void bindDouble(int index, double value);
    void bindOptionalInt64(int index, std::optional<int64_t> value);
    double columnDouble(int index) const;
    std::optional<int64_t> columnOptionalInt64(int index) const;
```

and after the class, still in `atlas::persistence::detail`:

```cpp
template <typename Body>
Result<void, PersistenceError> inTransaction(sqlite3* db, Body&& body) {
    auto begin = execute(db, "BEGIN;");
    if (!begin.hasValue()) return begin;
    auto result = body();
    if (!result.hasValue()) {
        execute(db, "ROLLBACK;");
        return result;
    }
    return execute(db, "COMMIT;");
}
```

In `statement.cpp`:

```cpp
void Statement::bindDouble(int index, double value) { sqlite3_bind_double(stmt_, index, value); }

void Statement::bindOptionalInt64(int index, std::optional<int64_t> value) {
    if (value.has_value()) {
        bindInt64(index, *value);
    } else {
        sqlite3_bind_null(stmt_, index);
    }
}

double Statement::columnDouble(int index) const { return sqlite3_column_double(stmt_, index); }

std::optional<int64_t> Statement::columnOptionalInt64(int index) const {
    if (sqlite3_column_type(stmt_, index) == SQLITE_NULL) return std::nullopt;
    return columnInt64(index);
}
```

- [ ] **Step 4: Add migration 3**

In `migrations.cpp` change `std::array<Migration, 2>` to `std::array<Migration, 3>` and append this entry after migration 2:

```cpp
    {3, "Learning: review log, memory state cache, node placements", R"sql(
        CREATE TABLE review_events (
            id TEXT PRIMARY KEY,
            item_kind TEXT NOT NULL,
            item_id TEXT NOT NULL,
            session_id TEXT NOT NULL,
            device_id TEXT NOT NULL,
            reviewed_at INTEGER NOT NULL,
            elapsed_days REAL NOT NULL,
            exercise TEXT NOT NULL,
            predicted INTEGER NOT NULL,
            grade INTEGER NOT NULL,
            hints_used INTEGER NOT NULL,
            wrong_target_id TEXT,
            response_ms INTEGER NOT NULL
        );
        CREATE INDEX idx_review_events_item ON review_events(item_kind, item_id, reviewed_at);

        CREATE TABLE memory_states (
            item_kind TEXT NOT NULL,
            item_id TEXT NOT NULL,
            phase TEXT NOT NULL,
            stability REAL NOT NULL,
            difficulty REAL NOT NULL,
            last_reviewed_at INTEGER,
            due_at INTEGER,
            review_count INTEGER NOT NULL,
            lapse_count INTEGER NOT NULL,
            PRIMARY KEY (item_kind, item_id)
        );

        CREATE TABLE memory_meta (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );

        CREATE TABLE node_placements (
            concept_id TEXT PRIMARY KEY REFERENCES knowledge_objects(id) ON DELETE CASCADE,
            x REAL NOT NULL,
            y REAL NOT NULL,
            pinned INTEGER NOT NULL DEFAULT 0
        );

        CREATE TRIGGER trg_knowledge_objects_forget_learning AFTER DELETE ON knowledge_objects
        BEGIN
            DELETE FROM review_events WHERE item_kind = 'concept' AND item_id = OLD.id;
            DELETE FROM memory_states WHERE item_kind = 'concept' AND item_id = OLD.id;
            UPDATE review_events SET wrong_target_id = NULL WHERE wrong_target_id = OLD.id;
        END;

        CREATE TRIGGER trg_relationships_forget_learning AFTER DELETE ON relationships
        BEGIN
            DELETE FROM review_events WHERE item_kind = 'link' AND item_id = OLD.id;
            DELETE FROM memory_states WHERE item_kind = 'link' AND item_id = OLD.id;
        END;
    )sql"},
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_persistence_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/persistence
git commit -m "feat(persistence): add migration 3 for the review log, memory states, and placements"
```

---

### Task 12: LearningRepository

**Files:**
- Create: `modules/persistence/include/atlas/persistence/learning_repository.hpp`, `modules/persistence/src/learning_repository.cpp`
- Modify: `modules/persistence/include/atlas/persistence/database.hpp` (add `friend class LearningRepository;`), `modules/persistence/CMakeLists.txt`
- Test: `modules/persistence/tests/test_learning_repository.cpp`, add to `modules/persistence/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 types, Task 11 helpers.
- Produces `class LearningRepository`:
  - `explicit LearningRepository(Database&);`
  - `Result<void, PersistenceError> record(const std::vector<ReviewEvent>&, const std::vector<MemoryState>&);` one transaction
  - `Result<std::vector<ReviewEvent>, PersistenceError> allEvents();` ordered by time, then id
  - `Result<std::vector<MemoryState>, PersistenceError> allStates();`
  - `Result<void, PersistenceError> replaceStates(const std::vector<MemoryState>&, int replayVersion);`
  - `Result<bool, PersistenceError> isCacheFresh(int replayVersion);`

- [ ] **Step 1: Write the failing test** `modules/persistence/tests/test_learning_repository.cpp`

```cpp
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
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `learning_repository.hpp` not found.

- [ ] **Step 3: Create `modules/persistence/include/atlas/persistence/learning_repository.hpp`**

```cpp
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

}  // namespace atlas::persistence
```

Add `friend class LearningRepository;` next to the other friends in `database.hpp`, and `src/learning_repository.cpp` to `atlas_persistence` in `modules/persistence/CMakeLists.txt`.

- [ ] **Step 4: Create `modules/persistence/src/learning_repository.cpp`**

```cpp
#include "atlas/persistence/learning_repository.hpp"

#include <sqlite3.h>

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

Result<void, PersistenceError> syncLastEventId(sqlite3* db) {
    auto newest = newestEventId(db);
    if (!newest.hasValue()) return Result<void, PersistenceError>::err(newest.error());
    if (!newest.value()) return Result<void, PersistenceError>::ok();
    return setMeta(db, kLastEventKey, *newest.value());
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

}  // namespace

LearningRepository::LearningRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> LearningRepository::record(const std::vector<ReviewEvent>& events,
                                                          const std::vector<MemoryState>& states) {
    sqlite3* db = database_->handle();
    return detail::inTransaction(db, [&]() -> Result<void, PersistenceError> {
        for (const auto& event : events) {
            if (auto inserted = insertEvent(db, event); !inserted.hasValue()) return inserted;
        }
        for (const auto& state : states) {
            if (auto upserted = upsertState(db, state); !upserted.hasValue()) return upserted;
        }
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

}  // namespace atlas::persistence
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_persistence_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/persistence
git commit -m "feat(persistence): store the review log and memory state cache"
```

---

### Task 13: PlacementRepository

**Files:**
- Create: `modules/persistence/include/atlas/persistence/placement_repository.hpp`, `modules/persistence/src/placement_repository.cpp`
- Modify: `database.hpp` (add `friend class PlacementRepository;`), `modules/persistence/CMakeLists.txt`
- Test: `modules/persistence/tests/test_placement_repository.cpp`, add to tests `CMakeLists.txt`

**Interfaces:**
- Produces `class PlacementRepository { explicit PlacementRepository(Database&); Result<void, PersistenceError> saveAll(const std::vector<Placement>&); Result<std::vector<Placement>, PersistenceError> findAll(); };` `saveAll` upserts in one transaction.

- [ ] **Step 1: Write the failing test** `modules/persistence/tests/test_placement_repository.cpp`

```cpp
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/placement_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

KnowledgeObjectId saveConcept(KnowledgeObjectRepository& objects, const char* title) {
    auto object = KnowledgeObject::create(title).value();
    REQUIRE(objects.save(object).hasValue());
    return object.id();
}

}  // namespace

TEST_CASE("placements round-trip and saving again updates them") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    PlacementRepository placements(db);
    auto a = saveConcept(objects, "A");

    REQUIRE(placements.saveAll({Placement{a, 10.5, -3.25, false}}).hasValue());
    REQUIRE(placements.saveAll({Placement{a, 11.0, 4.0, true}}).hasValue());

    auto all = placements.findAll().value();
    REQUIRE(all.size() == 1);
    CHECK(all[0].conceptId == a);
    CHECK(all[0].x == 11.0);
    CHECK(all[0].y == 4.0);
    CHECK(all[0].pinned);
}

TEST_CASE("a placement for an unknown concept fails the whole batch") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    PlacementRepository placements(db);
    auto a = saveConcept(objects, "A");

    auto result = placements.saveAll({Placement{a, 1.0, 1.0, false}, Placement{KnowledgeObjectId::generate(), 0.0, 0.0, false}});
    CHECK_FALSE(result.hasValue());
    CHECK(placements.findAll().value().empty());
}

TEST_CASE("deleting a concept deletes its placement") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    PlacementRepository placements(db);
    auto a = saveConcept(objects, "A");
    REQUIRE(placements.saveAll({Placement{a, 1.0, 2.0, false}}).hasValue());
    REQUIRE(objects.remove(a).hasValue());
    CHECK(placements.findAll().value().empty());
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset dev`
Expected: FAIL, `placement_repository.hpp` not found.

- [ ] **Step 3: Create `modules/persistence/include/atlas/persistence/placement_repository.hpp`**

```cpp
#pragma once

#include <vector>

#include "atlas/core/memory.hpp"
#include "atlas/core/result.hpp"
#include "atlas/persistence/database.hpp"
#include "atlas/persistence/persistence_error.hpp"

namespace atlas::persistence {

using atlas::core::Placement;

class PlacementRepository {
public:
    explicit PlacementRepository(Database& database);

    Result<void, PersistenceError> saveAll(const std::vector<Placement>& placements);
    Result<std::vector<Placement>, PersistenceError> findAll();

private:
    Database* database_;
};

}  // namespace atlas::persistence
```

- [ ] **Step 4: Create `modules/persistence/src/placement_repository.cpp`**

```cpp
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

}  // namespace atlas::persistence
```

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_persistence_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/persistence
git commit -m "feat(persistence): store node placements"
```

---

### Task 14: Warm started, pinned, deterministic layout

**Files:**
- Modify: `modules/render/include/atlas/render/force_directed_layout.hpp`
- Rewrite: `modules/render/src/force_directed_layout.cpp`
- Test: `modules/render/tests/test_force_directed_layout.cpp`

**Interfaces:**
- Produces:
  - `LayoutConfig` gains `double warmStartDisplacementPerIteration = 8.0;`
  - `struct LayoutHints { std::unordered_map<KnowledgeObjectId, Point2D> initial; std::unordered_set<KnowledgeObjectId> pinned; };`
  - `ForceDirectedLayout::compute(const GraphEngine&, LayoutConfig = {}, const LayoutHints& = {})`
- Behavior: ids processed in id text order (independent of hash order); hinted nodes start at their hint; unhinted nodes with hinted neighbors start near the mean of those neighbors; pinned nodes never move; with any hints, per iteration movement starts at `warmStartDisplacementPerIteration`.

- [ ] **Step 1: Add the failing tests** to `modules/render/tests/test_force_directed_layout.cpp`

```cpp
TEST_CASE("pinned nodes keep their exact position") {
    GraphEngine graph;
    auto a = makeNode("A");
    auto b = makeNode("B");
    auto aId = a.id();
    auto bId = b.id();
    REQUIRE(graph.addNode(std::move(a)).hasValue());
    REQUIRE(graph.addNode(std::move(b)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(aId, bId, RelationshipType::DependsOn).value()).hasValue());

    LayoutHints hints;
    hints.initial[aId] = Point2D{500.0, -200.0};
    hints.pinned.insert(aId);
    auto positions = ForceDirectedLayout::compute(graph, {}, hints);
    CHECK(positions.at(aId).x == 500.0);
    CHECK(positions.at(aId).y == -200.0);
}

TEST_CASE("with saved positions, no node moves further than the warm start allows") {
    GraphEngine graph;
    std::vector<KnowledgeObjectId> ids;
    for (const char* title : {"A", "B", "C", "D", "E", "F"}) {
        auto node = makeNode(title);
        ids.push_back(node.id());
        REQUIRE(graph.addNode(std::move(node)).hasValue());
    }
    for (size_t i = 1; i < ids.size(); ++i) {
        REQUIRE(graph.addEdge(Relationship::create(ids[i], ids[i - 1], RelationshipType::DependsOn).value()).hasValue());
    }
    auto cold = ForceDirectedLayout::compute(graph);

    LayoutHints hints;
    for (const auto& [id, point] : cold) hints.initial[id] = point;
    auto warm = ForceDirectedLayout::compute(graph, {}, hints);

    for (const auto& [id, point] : cold) {
        CHECK(distance(point, warm.at(id)) < 53.4);
    }
}

TEST_CASE("a new node starts next to the saved neighbors it links to") {
    GraphEngine graph;
    auto anchor = makeNode("Anchor");
    auto fresh = makeNode("Fresh");
    auto anchorId = anchor.id();
    auto freshId = fresh.id();
    REQUIRE(graph.addNode(std::move(anchor)).hasValue());
    REQUIRE(graph.addNode(std::move(fresh)).hasValue());
    REQUIRE(graph.addEdge(Relationship::create(freshId, anchorId, RelationshipType::DependsOn).value()).hasValue());

    LayoutHints hints;
    hints.initial[anchorId] = Point2D{1000.0, 1000.0};
    hints.pinned.insert(anchorId);
    auto positions = ForceDirectedLayout::compute(graph, {}, hints);
    CHECK(distance(positions.at(freshId), Point2D{1000.0, 1000.0}) < 120.0);
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev`
Expected: FAIL, `LayoutHints` is not declared.

- [ ] **Step 3: Replace `modules/render/include/atlas/render/force_directed_layout.hpp`**

```cpp
#pragma once

#include <unordered_map>
#include <unordered_set>

#include "atlas/core/knowledge_object.hpp"
#include "atlas/graph/graph_engine.hpp"

namespace atlas::render {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct LayoutConfig {
    int iterations = 50;
    double idealEdgeLength = 80.0;
    double repulsionStrength = 4000.0;
    double dampening = 0.85;
    double maxDisplacementPerIteration = 50.0;
    double warmStartDisplacementPerIteration = 8.0;
    unsigned seed = 42;
};

struct LayoutHints {
    std::unordered_map<atlas::core::KnowledgeObjectId, Point2D> initial;
    std::unordered_set<atlas::core::KnowledgeObjectId> pinned;
};

class ForceDirectedLayout {
public:
    static std::unordered_map<atlas::core::KnowledgeObjectId, Point2D> compute(
        const atlas::graph::GraphEngine& graph, LayoutConfig config = {}, const LayoutHints& hints = {});
};

}  // namespace atlas::render
```

- [ ] **Step 4: Replace `modules/render/src/force_directed_layout.cpp`**

The repulsion grid and attraction math are unchanged; only id order, starting positions, pins, and starting temperature are new.

```cpp
#include "atlas/render/force_directed_layout.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace atlas::render {

namespace {

using atlas::core::KnowledgeObjectId;

constexpr double kMinDistance = 0.01;

struct Edge {
    size_t a;
    size_t b;
};

struct CellKey {
    long long x;
    long long y;
    bool operator==(const CellKey& other) const { return x == other.x && y == other.y; }
};

struct CellKeyHash {
    size_t operator()(const CellKey& key) const noexcept {
        return std::hash<long long>{}(key.x) ^ (std::hash<long long>{}(key.y) << 1);
    }
};

CellKey cellOf(const Point2D& point, double cellSize) {
    return CellKey{static_cast<long long>(std::floor(point.x / cellSize)),
                   static_cast<long long>(std::floor(point.y / cellSize))};
}

std::vector<KnowledgeObjectId> sortedIds(const atlas::graph::GraphEngine& graph) {
    auto ids = graph.allNodeIds();
    std::sort(ids.begin(), ids.end(), [](const auto& a, const auto& b) { return a.toString() < b.toString(); });
    return ids;
}

std::vector<Point2D> startingPositions(const std::vector<KnowledgeObjectId>& ids, const std::vector<Edge>& edges,
                                       const LayoutHints& hints, const LayoutConfig& config) {
    const size_t n = ids.size();
    double side = std::sqrt(static_cast<double>(n)) * config.idealEdgeLength;
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<double> anywhere(-side / 2.0, side / 2.0);
    std::uniform_real_distribution<double> nearby(-config.idealEdgeLength / 2.0, config.idealEdgeLength / 2.0);

    std::vector<Point2D> positions(n);
    std::vector<bool> known(n, false);
    for (size_t i = 0; i < n; ++i) {
        Point2D random{anywhere(rng), anywhere(rng)};
        auto hint = hints.initial.find(ids[i]);
        known[i] = hint != hints.initial.end();
        positions[i] = known[i] ? hint->second : random;
    }

    for (size_t i = 0; i < n; ++i) {
        if (known[i]) continue;
        Point2D sum;
        int count = 0;
        for (const auto& edge : edges) {
            size_t other = edge.a == i ? edge.b : (edge.b == i ? edge.a : n);
            if (other == n || !known[other]) continue;
            sum.x += positions[other].x;
            sum.y += positions[other].y;
            ++count;
        }
        if (count > 0) positions[i] = Point2D{sum.x / count + nearby(rng), sum.y / count + nearby(rng)};
    }
    return positions;
}

}  // namespace

std::unordered_map<KnowledgeObjectId, Point2D> ForceDirectedLayout::compute(
    const atlas::graph::GraphEngine& graph, LayoutConfig config, const LayoutHints& hints) {
    auto ids = sortedIds(graph);
    std::unordered_map<KnowledgeObjectId, Point2D> result;
    if (ids.empty()) return result;
    const size_t n = ids.size();

    std::unordered_map<KnowledgeObjectId, size_t> indexOf;
    indexOf.reserve(n);
    for (size_t i = 0; i < n; ++i) indexOf[ids[i]] = i;

    std::vector<Edge> edges;
    for (size_t i = 0; i < n; ++i) {
        for (const auto& neighborId :
             graph.neighbors(ids[i], std::nullopt, atlas::graph::GraphEngine::Direction::Outgoing)) {
            edges.push_back(Edge{i, indexOf.at(neighborId)});
        }
    }

    auto positions = startingPositions(ids, edges, hints, config);
    std::vector<bool> pinned(n, false);
    for (size_t i = 0; i < n; ++i) pinned[i] = hints.pinned.contains(ids[i]);

    std::vector<Point2D> displacement(n);
    double temperature = hints.initial.empty() ? config.maxDisplacementPerIteration
                                               : config.warmStartDisplacementPerIteration;
    double cellSize = std::max(config.idealEdgeLength * 2.0, kMinDistance);

    for (int iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(displacement.begin(), displacement.end(), Point2D{0.0, 0.0});

        std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> grid;
        for (size_t i = 0; i < n; ++i) grid[cellOf(positions[i], cellSize)].push_back(i);

        for (size_t i = 0; i < n; ++i) {
            CellKey home = cellOf(positions[i], cellSize);
            for (long long dx = -1; dx <= 1; ++dx) {
                for (long long dy = -1; dy <= 1; ++dy) {
                    auto it = grid.find(CellKey{home.x + dx, home.y + dy});
                    if (it == grid.end()) continue;
                    for (size_t j : it->second) {
                        if (j == i) continue;
                        double ddx = positions[i].x - positions[j].x;
                        double ddy = positions[i].y - positions[j].y;
                        double d = std::max(std::sqrt(ddx * ddx + ddy * ddy), kMinDistance);
                        double force = config.repulsionStrength / (d * d);
                        displacement[i].x += (ddx / d) * force;
                        displacement[i].y += (ddy / d) * force;
                    }
                }
            }
        }

        for (const auto& edge : edges) {
            double dx = positions[edge.a].x - positions[edge.b].x;
            double dy = positions[edge.a].y - positions[edge.b].y;
            double d = std::max(std::sqrt(dx * dx + dy * dy), kMinDistance);
            double force = (d * d) / config.idealEdgeLength;
            displacement[edge.a].x -= (dx / d) * force;
            displacement[edge.a].y -= (dy / d) * force;
        }

        for (size_t i = 0; i < n; ++i) {
            if (pinned[i]) continue;
            double magnitude = std::max(
                std::sqrt(displacement[i].x * displacement[i].x + displacement[i].y * displacement[i].y),
                kMinDistance);
            double step = std::min(magnitude, temperature);
            positions[i].x += (displacement[i].x / magnitude) * step;
            positions[i].y += (displacement[i].y / magnitude) * step;
        }

        temperature *= config.dampening;
    }

    result.reserve(n);
    for (size_t i = 0; i < n; ++i) result[ids[i]] = positions[i];
    return result;
}

}  // namespace atlas::render
```

- [ ] **Step 5: Run render tests, including scale**

Run: `cmake --build --preset dev && ctest --test-dir build/dev -R atlas_render --output-on-failure`
Expected: PASS for `atlas_render_tests` and `atlas_render_scale_tests`.

- [ ] **Step 6: Commit**

```bash
git add modules/render
git commit -m "feat(render): warm start layout from saved positions and honor pinned nodes"
```

---

### Task 15: Record decisions and run the full suite

**Files:**
- Modify: `docs/DECISIONS.md` (append a section at the end)

- [ ] **Step 1: Append to `docs/DECISIONS.md`**

```markdown
## Learning engine (plan 1)

- The review log is the source of truth. `memory_states` is a cache that `MemoryLedger::replay` can rebuild at any time.
- One `LearningRepository` covers events, states, and cache metadata, because they always change in one transaction.
- Learning data for a deleted concept or link is removed by SQL triggers. They run inside the delete itself, including relationship cascades, so no caller can forget them.
- `due_at` is the exact moment recall chance reaches the target retention. "Due" means `due_at <= now`, equivalent to `R(now) < target`.
- New items follow a simplified FSRS phase model: a first `again` enters learning, any other first grade enters review. There are no intra-day learning steps; the in-session requeue is the hypercorrection rule.
- A prerequisite that depends back on the concept (a cycle) does not block it from the frontier.
- Links between topics are allowed. The topic view draws only links with both ends in the topic until ghost nodes arrive with the new canvas.
- Layout processes nodes in id order, so results no longer depend on hash map order.
```

- [ ] **Step 2: Run everything**

Run: `cmake --build --preset dev && ctest --preset dev && ctest --test-dir build/dev -L scale --output-on-failure`
Expected: all pass, zero warnings.

Run: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
Expected: all pass, no sanitizer reports.

Run: `git diff eafcbec..HEAD | grep '^+' | grep -cP '\x{2014}'`
Expected: `0` (no long dash added anywhere by this plan).

- [ ] **Step 3: Commit**

```bash
git add docs/DECISIONS.md
git commit -m "docs: record learning engine decisions"
```
