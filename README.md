# Atlas

A visual knowledge navigation system. See project notes for the full
rationale; this README covers only how to build what currently exists.

## Status

**M0 + M1 + M2 + M3 complete:**
- `atlas-core` — the domain model. Zero dependencies on Qt, SQLite, or OpenGL.
- `atlas-persistence` — SQLite-backed storage with schema migrations and a repository per aggregate.
- `atlas-graph` — in-memory index over the domain model, verified at 10,000 nodes / ~30,000 edges.
- `atlas-ui` — the first Qt-dependent module. `WorkspaceController` bridges persistence and the graph (database write first, in-memory graph updated only after that succeeds — no rollback logic needed anywhere). List view, create/edit/delete for KnowledgeObjects. No relationship UI yet — deferred to the graph canvas (M4), where creating an edge has an obvious visual gesture instead of picking two ids from a list.
- `atlas-app` — the composition root. Opens a real database file under the OS's standard app-data directory, wires everything together, runs the Qt event loop. Verified to actually launch and persist data, not just compile.

All four fully unit tested, clean under `-Wall -Wextra -Wpedantic -Werror` and under ASan/UBSan. `ctest` runs everything — including the Qt widget tests, via the `offscreen` platform plugin — with no manual environment setup required.

Nothing else exists yet — no graph canvas, no relationship UI, no search. That is deliberate; see the module roadmap below.

## Building

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
```

Requires a C++20 compiler, CMake 3.21+, and Qt6 (Widgets component —
on Ubuntu/Debian: `apt install qt6-base-dev`; on Arch:
`pacman -S qt6-base`). No other dependencies are fetched or required
to build `atlas-core`/`atlas-persistence`/`atlas-graph` — `doctest.h` is
vendored directly in `third_party/doctest/` rather than pulled at build
time, in keeping with Atlas's offline-first philosophy extending to its
own build process.

If Qt6 isn't available, configure with `-DATLAS_BUILD_UI=OFF` to build
and test the lower layers (`atlas-core`, `atlas-persistence`,
`atlas-graph`) on their own.

Run the actual application with `./build/modules/app/atlas_app` — it
opens (and creates, on first run) a SQLite database under the OS's
standard application-data directory (e.g.
`~/.local/share/atlas_app/atlas.db` on Linux).

## Module layout (target architecture)

```
atlas-core/        domain model — no Qt, no SQLite, no I/O      [DONE]
atlas-persistence/ SQLite repository layer                      [DONE]
atlas-graph/        in-memory graph engine: traversal, search,
                     roadmap algorithms                          [DONE]
atlas-ui/            Qt shell: controller, list CRUD             [DONE — list only, no graph canvas yet]
atlas-app/           composition root, owns main()                [DONE]
atlas-render/        QML/Qt Quick scene-graph canvas, layout,
                     camera/picking                               [planned]
atlas-ai/            suggestion engine (local + optional remote)  [planned]
atlas-plugins/       plugin host + SDK                            [planned]
```

Dependency rule: every arrow points inward toward `atlas-core`.
`atlas-core` depends on nothing else in this tree, so it stays
testable in isolation forever, regardless of what happens to the UI
framework, the rendering backend, or the storage engine above it.

## Why some things look the way they do

- **IDs are UUIDs, not sequential integers** — so objects created in
  different sessions, imports, or (eventually) plugins never collide
  when merged.
- **`KnowledgeObject` has no "Depends On" / "Used By" fields.** Those
  are graph edges (`Relationship`), owned by `atlas-graph`, not
  intrinsic content. Storing the same fact in two places invites them
  to go out of sync.
- **`Result<T,E>`, not exceptions, at the domain boundary.** Plugins
  will eventually cross a C ABI where C++ exceptions aren't safe to
  propagate. Deciding this now is cheap; retrofitting it later is not.
- **Enums are stored as text, not integer ordinals.** Reordering an
  enum looks like a harmless refactor but silently changes the meaning
  of every row already on disk if stored as an ordinal. Storing the
  name costs a few bytes and a small mapping function; see
  `atlas::core::toDisplayString` / `*FromString` in `enums.hpp`.
- **`KnowledgeObject::create()` and `KnowledgeObject::reconstruct()`
  are two different factories on purpose.** Authoring a new concept and
  loading an existing one from storage have different invariants —
  `create()` generates a fresh id and timestamps; `reconstruct()`
  restores them exactly. Conflating the two either makes `create()`
  accept an id/timestamps it shouldn't, or makes loading silently
  overwrite `updatedAt` to "now" on every read. Same pattern on
  `Relationship`.
- **SQLite is used via the system dev package in this sandbox.** For
  shipping Atlas cross-platform (M9), vendoring the SQLite amalgamation
  is the better long-term choice — it guarantees the same SQLite
  version everywhere and removes a dependency on a system package that
  may not exist on a fresh Windows/macOS machine. That swap is isolated
  to `modules/persistence/CMakeLists.txt`.
- **Repositories are CRUD only.** Graph traversal, dependency-chain
  queries, and anything that needs to reason about the graph as a whole
  belongs to `atlas-graph` (next), which loads everything via
  `findAll()` once at startup and builds its own in-memory index.
- **`atlas-graph` depends on `atlas-core` only, never on
  `atlas-persistence`.** It takes domain objects directly via
  `addNode`/`addEdge`; populating it from the database is the
  composition root's job (eventually `atlas-app`), not this module's.
  This keeps the graph engine fully testable with synthetic data —
  which is what makes the 10,000-node smoke test possible without ever
  touching SQLite.
- **Graph storage uses dense vectors + an id-to-index map, not
  UUID-keyed containers everywhere.** Stable UUIDs stay the public
  identity; a `vector<size_t>`-based adjacency list is what actually
  gets walked during traversal, for cache locality at 10k+ scale.
  Deletions are tombstoned (slot cleared, not erased) rather than
  physically removed, since erasing from a dense vector's middle would
  invalidate every later index — worse at scale than a dead slot.
- **`addEdge` rejects a symmetric edge stored in the reverse pair
  order as a duplicate, even though the database's `UNIQUE(source_id,
  target_id, type)` constraint would not catch it.** "A RelatedTo B"
  and "B RelatedTo A" are the same fact for a symmetric type; expressing
  "unique unordered pair" in SQL isn't worth the complexity when this
  layer — the one that actually understands `isSymmetric()` — already
  guards every write path that will exist in practice. Documented as an
  accepted gap in the database's defense-in-depth, not an oversight.
- **`nodes_`/`edges_` are `std::deque`, not `std::vector` — found and
  fixed after the fact, not designed in from the start.** The original
  `vector`-backed version had a real heap-use-after-free:
  `findNode()`/`findEdge()` return raw pointers into the backing
  storage, and a `vector` reallocates on growth, invalidating every
  pointer into it. No test caught this — none held a pointer from
  `findNode()` across a later `addNode()` call, which is exactly the
  pattern a UI naturally does (e.g. a cached "selected node" pointer
  while the user keeps adding concepts). Confirmed with
  AddressSanitizer against a reverted copy before fixing it for real;
  see `test_graph_engine.cpp`'s pointer-stability regression test.
  `deque` gives the needed guarantee (no invalidation of existing
  elements on append) while keeping O(1) `operator[]`.
- **`WorkspaceController`'s one rule: write to the database first,
  touch the in-memory graph only after that succeeds.** Every mutating
  method (`createKnowledgeObject`, `updateKnowledgeObject`,
  `removeKnowledgeObject`) follows this without exception. It's what
  makes the graph always either consistent with the database or
  strictly behind it — never ahead, never diverged — without any
  rollback-on-failure logic anywhere in this class. `updateKnowledgeObject`
  specifically copies the current object, mutates the copy, persists
  the copy, and only then calls `GraphEngine::updateNode()` — the live
  graph is never touched until the database write has already
  succeeded.
- **`Result<T,E>` was missing its non-const lvalue `value()`/`error()`
  overloads — caught by `-Wredundant-move`, not by a passing test
  suite.** Without a plain `T& value() &` overload, calling `.value()`
  on a named non-const `Result` had no matching non-const candidate
  and silently fell back to the `const&` one. The practical consequence:
  `WorkspaceController::load()`'s `for (auto& object : result.value())`
  loop was binding `object` as `const KnowledgeObject&`, making every
  `std::move(object)` inside it a silent copy instead of a move — every
  object loaded from the database at startup was being copied, not
  moved, into the graph. Fixed in `atlas-core`, with `Result<T,E>` now
  getting the dedicated test file it should have had from M0.
- **CMake's AUTOMOC didn't discover any of `atlas-ui`'s `Q_OBJECT`
  headers on the first attempt** — `add_library` only listed the `.cpp`
  files, and AUTOMOC's header-discovery heuristic across a separate
  `include/`/`src/` split didn't find them, producing an empty
  `mocs_compilation.cpp` and four "undefined reference to vtable"
  linker errors with no compile-time warning at all. Fixed by listing
  the `Q_OBJECT` headers explicitly as target sources — the robust,
  unambiguous way to guarantee AUTOMOC sees them, rather than relying
  on its include-graph discovery.
- **Qt widget tests run under the `offscreen` platform plugin**, set
  automatically for that one `ctest` entry via
  `set_tests_properties(... ENVIRONMENT ...)` — not exported globally,
  so running `atlas_app` directly still uses a real display. One
  `QApplication`, constructed once in a custom `main()`, shared by every
  test in `atlas_ui_tests` (including the plain-`QObject`
  controller/model tests) — only one `QApplication` may exist per
  process, so it's simplest for every UI test to share it rather than
  splitting Qt-widget tests into a separate binary.
