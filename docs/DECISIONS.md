# Engineering Decision Log

Detailed, chronological notes on specific design decisions and the bugs
found while building each milestone — the "why does this look this way"
forensics that don't belong in a top-level README but are worth keeping.
See `README.md` for the high-level picture; this file is the detail
underneath it.

Organized by module, roughly in the order each decision arose.

## Project suggestions

- **The ranking heuristic lives in `atlas-graph`, not `atlas-ui` or
  `atlas-core`.** It's a pure computation over graph structure —
  readiness needs `dependsOn()`, leverage needs the new
  `transitiveDependents()` — the same category as
  `topologicalOrder()`/`learningRoadmapFor()`, and for the same reason:
  keeping it testable with synthetic graphs, no Qt or SQLite involved.
- **No AI in v1, on purpose — not a placeholder for "AI later," a
  deliberate separate milestone.** The whole ranking (readiness ×
  leverage, filtered to concepts with a MiniProject and not already
  Mastered) is computable from data already in the graph. AI-assisted
  suggestions are explicitly the *next* roadmap item, not this one —
  conflating them would have meant either shipping nothing until an AI
  integration existed, or building a heuristic and quietly calling it
  "AI" when it isn't.
- **`transitiveDependents()` is the mirror of `transitiveDependencies()`
  — added as a new public primitive, not inlined into
  `suggestProjects()`.** Both are BFS over `DependsOn` edges in
  opposite directions (dependsOn vs. usedBy), so the traversal itself
  was factored into one shared private `transitiveClosure()` helper
  rather than duplicating the loop — the same "don't duplicate a BFS
  that already exists" instinct that produced
  `learningRoadmapFor()` reusing `transitiveDependencies()` earlier.
  `transitiveDependents()` is useful on its own beyond this feature
  (e.g. "what would break if I forgot this concept"), so it's public,
  not a suggestProjects()-only implementation detail.
- **A hard filter, not just a scoring factor, for "has at least one
  MiniProject."** A highly-leveraged, fully-ready concept with zero
  MiniProjects would score arbitrarily high under readiness × leverage
  alone, but there's nothing to actually go *do* — suggesting it would
  be pointing at an empty room. Filtered out before scoring, not scored
  low and hoping it sorts to the bottom.
- **Readiness and leverage are both exposed on `ProjectSuggestion`,
  not just the final rank.** A ranked list with no visible reasoning
  reads as arbitrary; showing "80% ready, unlocks 3 other concepts"
  lets the dialog explain *why* something was suggested, which matters
  more for a feature whose entire value proposition is guiding what to
  do next.
- **`WorkspaceController::ProjectSuggestion` is a distinct struct from
  `GraphEngine::ProjectSuggestion`, not a re-export.** Same
  resolve-and-wrap pattern as `roadmapFor()`: the graph-layer struct
  holds a bare `KnowledgeObjectId`; the UI-layer struct holds the
  resolved `KnowledgeObject` so a dialog can show its title and
  MiniProjects without a second lookup. The field originally named
  `concept` had to be renamed to `knowledgeObject` — `concept` is a
  reserved keyword in C++20 (concepts, the language feature), and the
  compiler error surfaced as a bizarre "too many initializers"
  downstream of the real parse failure, not as an obvious "reserved
  word" message.

## Topics, unified window, and theming

- **A KnowledgeObject belongs to exactly one Topic; Topics don't
  nest.** Nesting was real future scope, not this pass's — a flat
  namespace was enough to make "OS," "Databases," "Distributed
  Systems" distinct maps without the added complexity of a tree
  (reparenting, cycle checks, depth limits) that nothing yet demands.
- **Relationships can't cross Topic boundaries — enforced in
  `WorkspaceController`, not on `Relationship` or `KnowledgeObject`
  themselves.** Same split as the duplicate-edge and self-loop checks
  from earlier: an app-level rule about how two objects may relate,
  not something intrinsic to either class. All the checks —
  self-loop, duplicate (including a symmetric type's reverse pair),
  and now cross-topic — run against the in-memory graph before
  anything is written, so a rejection is never discovered only after
  the database already accepted a row.
- **Migration 2 backfills a fixed "Uncategorized" topic using the nil
  UUID as its id**, not a freshly generated one. Every pre-existing
  KnowledgeObject needs a topic the moment this migration runs, and
  the fixed id means the app can reference "the Uncategorized topic"
  from C++ (`uncategorizedTopicId()`) without a database round trip —
  it's the same id on every machine's database, deterministically.
  `topic_id` stays nullable at the SQL level (SQLite can't cheaply add
  a `NOT NULL` column with no default to an existing table without a
  full rebuild); "every object has a topic" is enforced in code at the
  `WorkspaceController` boundary instead, the same way `KnowledgeObject`'s
  own invariants are.
- **`TopicRepository` has no in-memory cache the way
  `KnowledgeObject`/`Relationship` have `GraphEngine`.** Topics don't
  participate in graph traversal — they don't nest, don't have edges
  of their own — and at the scale a person actually has topics (dozens,
  not thousands), querying the repository directly on every call is
  simpler than a cache and can't go stale, with no real performance
  cost to weigh against that simplicity.
- **`topicsChanged` is a separate signal from `graphChanged`, not
  folded into it** — a deliberate exception to the earlier "collapse
  everything into one signal" pattern from the relationship-creation
  work. That collapse was about not missing a cascade; this is about
  not doing wasted work. `TopicSelectorWidget` cares about
  `topicsChanged` and not `graphChanged`; the workspace panel and
  canvas care about `graphChanged` and not `topicsChanged`. Folding
  them together would mean every topic rename triggers a full graph
  relayout in whichever topic happens to be open at the time — an
  unrelated observer paying a real cost, not a case where something
  could be silently missed.
- **The topic selector and the workspace view are two pages of one
  `QStackedWidget` inside `MainWindow`, not two windows.** Consistent
  with the earlier decision to unify the list and canvas into one
  window rather than several utility windows — adding a third
  top-level window for topic selection would have reintroduced the
  same problem in a new place. The page swap itself
  (`switchToPage`/`stack_->setCurrentWidget`) is synchronous and
  never gated behind animation completion — a fade-in runs on top of
  it as a cosmetic layer, but `stack_->currentWidget()` is already the
  new page the instant the call returns, with no event-loop pumping
  required. That matters for tests: relying on an animation's
  `finished` signal to know when a page swap is "done" would need a
  running event loop to ever fire, the same timing trap the
  empty-state `currentWidget()` (not `isVisible()`) decision avoided
  earlier in this project.
- **The dark/light stylesheet is scoped to specific widget classes,
  not a blanket `QWidget` selector.** Qt stylesheets cascade to every
  descendant; the edit/relationship/roadmap dialogs are all parented
  under `MainWindow`, so a blanket rule would silently theme their
  backgrounds dark while their own `QLabel`s kept default-palette
  (often black) text — unreadable, and not something anyone asked for.
  Those dialogs intentionally keep the OS default style until they get
  their own theming pass; the four-color palette (`Theme`) only
  applies to the topic selector, the workspace panel, and the canvas.
- **`Theme` is a plain data struct plus a `themeFor(mode)` lookup, not
  a live-updating/observable theming system.** Two modes, both
  precomputed once as static locals — cheap to call repeatedly (e.g.
  once per `refreshGraph()`), no allocation, no signal/slot machinery
  for "theme changed" beyond `MainWindow` re-applying the stylesheet
  and calling `GraphWindow::setTheme()` directly on toggle. Difficulty
  colors are the one deliberate exception to "everything comes from
  the four-color palette" — they're a semantic mapping onto Beginner
  through Expert, so they get their own tuned hues per mode rather
  than being derived from Black/Brown/Coffee/Beige.

## Dependency visualization and learning roadmap generation

- **These turned out to be one feature, not two.** The original spec
  listed "dependency visualization" and "learning roadmap generation"
  as separate items. In practice, "what should I learn before X" and
  "show me X's dependency order" are the same question — building them
  as two separate UI flows would have meant building the same
  underlying primitive twice. Combined into one right-click action.
- **A real gap found while building the UI, not before:**
  `GraphEngine::topologicalOrder()` operates on the *entire* graph's
  `DependsOn` edges. A roadmap "to learn X" needs to be scoped to only
  X's prerequisite chain — without that restriction, asking for a
  roadmap to a beginner-level, dependency-free concept would still
  surface every other disconnected concept in the workspace that also
  happens to have no dependencies, since they'd all tie for "first" in
  a global topological sort. `learningRoadmapFor()` fixes this by
  intersecting the global topological order with
  `transitiveDependencies(id) ∪ {id}` — O(1) membership checks via an
  `unordered_set`, so the filter doesn't reintroduce the cost the
  global sort already paid. `GraphEngine`'s existing tests never
  caught this because they only ever asserted on the *global* ordering,
  never on "the order restricted to one node's prerequisites."
- **Right-click, not a third window or a toolbar button.** Canvas
  click-selection already existed from the relationship-highlighting
  work; right-click was the natural extension for a contextual action
  on a specific node, rather than introducing a new selection mechanism
  or a separate dialog flow to pick a target node from a list again.
- **`RoadmapDialog` is read-only by design** — a plain numbered list,
  no drag-to-reorder, no inline editing. The order is computed, not
  authored; a UI that implied it could be rearranged by hand would
  misrepresent what the feature actually does.
- **A cycle in the dependency graph is reported as a dialog message,
  not a crash or a silent empty list.** `GraphError::CycleDetected`
  threads all the way from `GraphEngine::learningRoadmapFor()` through
  `WorkspaceController::roadmapFor()`'s own `RoadmapErrorCode` (a
  separate UI-facing enum, not a re-export of `GraphError` — `atlas-ui`
  shouldn't need to know about `atlas-graph`'s internal error shape) up
  to a `QMessageBox` telling the person their `DependsOn` graph has a
  loop. This is a real, reachable state (it's trivial to accidentally
  create a `DependsOn` cycle through the existing relationship UI), so
  it needed a real, non-crashing answer.
- **A second occurrence of the same "edit tool drops the next
  `TEST_CASE` line" mistake from earlier sessions** happened twice
  while adding these tests — once in `test_graph_engine.cpp`, once in
  `test_workspace_controller.cpp`. Both times, an insertion edit
  silently swallowed the following test's `TEST_CASE(...)` declaration,
  leaving its body orphaned inside the previous test. Caught both times
  by a brace-balance + `grep -c "^TEST_CASE"` sanity check before
  trusting a clean compile — worth treating as a standing habit after
  any insertion-style edit to a test file, not just when something
  looks suspicious.

## Search

- **Matching logic lives in `atlas-core` as a pure, Qt-free scoring
  function (`matchScore`), not in `atlas-graph` or `atlas-ui`.**
  "Does this object's text match this query" doesn't need graph
  structure or Qt — it's a property of a single `KnowledgeObject`.
  `GraphEngine::search()` is a thin layer applying that function across
  every live node and ranking the results; `WorkspaceController::search()`
  resolves ids back to full objects, same pattern as
  `allKnowledgeObjects()`.
- **v1 searches Title/Definition/Problem Solved/Why It Exists/Notes —
  not Examples/MiniProjects/References.** Those are comparatively
  rarely where the differentiating text lives, and including them
  means iterating nested vectors-of-structs for a benefit with no
  evidence anyone needs it yet. Easy to extend later.
- **Search results rank by relevance, not alphabetically — a
  deliberate departure from `allKnowledgeObjects()`'s sort order.**
  Title matches outweigh body matches; matching in two fields outranks
  matching in only one. A deterministic tie-break (by id string) was
  added specifically because `GraphEngine`'s internal storage is a hash
  map — without it, two identical searches could return tied results in
  a different order purely from hash-map iteration order, which would
  make the list look like it's "jumping around" for no reason.
- **An empty query is handled differently at each layer, on purpose.**
  `matchScore()` returns `nullopt` for an empty query (an empty
  substring trivially "matches" everything via `std::string::find`,
  which isn't the behavior anyone wants). `GraphEngine::search("")`
  returns every live node, unranked. `WorkspaceController::search("")`
  explicitly delegates to `allKnowledgeObjects()` instead, so clearing
  the search box gives back the same familiar alphabetical order the
  plain list view uses — not `GraphEngine`'s hash-map order.
- **Verified fast at the scale that mattered, not assumed fast.** Linear
  scan, no indexing — and that's fine: ~200ms to rank all 10,000 nodes,
  measured under ASan/UBSan overhead (faster in Release). No debouncing
  added to the search box; there's no evidence it's needed at this cost
  per keystroke.

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
