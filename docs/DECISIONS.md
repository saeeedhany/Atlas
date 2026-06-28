# Engineering Decision Log

Detailed, chronological notes on specific design decisions and the bugs
found while building each milestone — the "why does this look this way"
forensics that don't belong in a top-level README but are worth keeping.
See `README.md` for the high-level picture; this file is the detail
underneath it.

Organized by module, roughly in the order each decision arose.

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
- **Layout is computed once per structural graph change, never
  continuously simulated.** "Smooth at 10,000 nodes" is two different
  problems: GPU-batched pan/zoom (easy — the camera is a transform
  matrix update, not a geometry rebuild) versus continuous force-
  directed physics at that scale (genuinely hard, O(n²) per frame
  without spatial partitioning). The spec needs the first, not the
  second.
- **Naive O(n²) repulsion measured at 10.8 seconds for one layout pass
  at 10,000 nodes — far too slow even as a one-time computation.**
  Replaced with a uniform-grid (cell-list) approximation: repulsion
  decays as 1/distance², so contributions beyond a 3x3 neighborhood of
  grid cells are already negligible, the same principle Barnes-Hut
  generalizes with a quadtree. Brought it to ~550-900ms in Release.
  Built only after measuring the naive version was too slow — not
  speculative — same empirical bar as every other performance decision
  in this project. A first pass at the fix also repeated an earlier
  mistake: using `unordered_map<KnowledgeObjectId, Point2D>` in the hot
  loop, meaning every pairwise force check did several 16-byte-UUID
  hash lookups. All hashing is now confined to a one-time setup phase;
  the simulation loop itself touches only dense arrays — the same
  "stable id for identity, dense index for the hot path" lesson from
  `atlas-graph`'s `GraphEngine`, just not carried into this module the
  first time.
- **`GraphCanvasItem` draws all nodes in one `QSGGeometryNode`
  (`DrawTriangles`) and all edges in another (`DrawLines`)** — one GPU
  draw call per category regardless of node count. This is the actual
  payoff of choosing Qt Quick over `QGraphicsView` back when the
  rendering backend was decided: `QGraphicsView` does CPU-side
  per-item work that doesn't batch this way. Pan/zoom only updates a
  `QSGTransformNode`'s matrix; geometry is rebuilt only when
  `setGraphData()` is called (a structural change or a recomputed
  layout), never on every frame of a drag.
- **Ubuntu splits Qt6's QML *import* modules from the C++ runtime
  libraries into separate packages** (`libqt6qmlworkerscript6` vs
  `qml6-module-qtqml-workerscript`) — `apt install qt6-declarative-dev`
  alone wasn't enough; loading even a bare `import QtQuick` failed with
  "module is not installed" until `qml6-module-qtquick`,
  `qml6-module-qtquick-window`, and `qml6-module-qtqml-workerscript`
  were installed too. If you hit this on a different distro, look for
  an equivalent QML-module package split before assuming the C++ code
  is wrong.
- **A real test bug, not a product bug, briefly looked like a broken
  canvas**: the first version of the `GraphWindow` smoke test called
  `window.findChild<GraphCanvasItem*>(...)` on the `QWidget` itself.
  `GraphCanvasItem` lives in the QML scene graph rooted at
  `QQuickWidget::rootObject()` — a different object tree entirely, not
  reachable via the enclosing widget's child hierarchy. Fixed by adding
  a proper `GraphWindow::canvasItem()` accessor rather than working
  around it in the test.
- **A real usability gap, found only by actually running the app**: an
  empty list and a broken window looked identical (both just blank
  white), and a single new row was easy to miss against a blank
  background with no visual cue anything had changed. No test caught
  this — it's not the kind of thing a unit test checks. Fixed with an
  empty-state placeholder (swapped via `QStackedWidget`, checked with
  `currentWidget()` in tests rather than `isVisible()`, which depends
  on the window actually being shown via `show()` — something tests
  correctly never do) and alternating row colors.
- **Relationship creation reuses the list view's existing row
  selection, not canvas-click node picking.** Building this surfaced
  that the canvas has no node-picking at all — every mouse press
  starts a pan, regardless of where you click. Canvas-click selection
  is a real, separate feature (camera-transform-aware hit-testing)
  that was never built, just implicitly assumed. Building it now would
  have been scope creep beyond "make relationships creatable"; it
  remains a genuine future UX improvement, not a prerequisite.
- **`WorkspaceController` collapsed three signals
  (`knowledgeObjectAdded/Updated/Removed`) into one, `graphChanged()`,
  found necessary while designing where relationship signals should
  go.** Adding `relationshipAdded`/`relationshipRemoved` alongside the
  existing three would have made five signals for every view to
  remember to wire up correctly — and tracing it through surfaced a
  real gap: `removeKnowledgeObject`'s cascade (deleting relationships
  that touched the removed object) never fired any
  relationship-specific signal for those cascaded removals. A single
  signal that every dependent view treats as "go refresh yourself"
  can't miss a cascade, because there's nothing fine-grained to forget
  to wire up. Same "full reset over precise incremental tracking"
  trade as the list model's own refresh strategy, applied one level up.
- **`GraphEngine::hasDuplicateEdge` is public now, not an `addEdge()`-
  only implementation detail** — specifically so `createRelationship`
  can check it *before* writing anything to the database. The
  database's `UNIQUE(source_id, target_id, type)` constraint doesn't
  catch a symmetric type's reverse-pair duplicate (documented back in
  M2), but `GraphEngine` does; checking it first means that rejection
  happens before any database write, not as an inconsistency
  discovered after one already succeeded.
