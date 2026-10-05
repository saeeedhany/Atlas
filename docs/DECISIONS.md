# Engineering Decision Log

Detailed, chronological notes on specific design decisions and the bugs
found while building each milestone - the "why does this look this way"
forensics that don't belong in a top-level README but are worth keeping.
See `README.md` for the high-level picture; this file is the detail
underneath it.

Organized by module, roughly in the order each decision arose.

## Dark-mode contrast, app-wide theming, and animated graph transitions

- **The dark theme's actual bug: panel chrome and canvas background
  were nearly the same color.** `panelBackground` was `kBrown`
  (`0x1F150C`), barely distinguishable from the canvas's pure black -
  everything read as "almost-black-on-black" with no depth, which is
  what made dark mode look flat compared to light mode even with the
  same palette applied. Fixed by swapping `panelBackground` to
  `kCoffee` (a properly mid-dark brown, one full step up from black)
  and re-deriving `panelAlternateBackground`/`panelBorder` from that,
  entirely within the same four supplied colors - no new hues
  introduced, just a different assignment of the existing ones.
- **"The theme doesn't affect all windows" had one specific,
  identifiable cause: `setStyleSheet()` was called on `MainWindow`
  itself, not on `QApplication`.** Qt style sheets only cascade to
  descendants that belong to the *same* top-level window.
  `RelationshipsWindow` (explicitly `Qt::Window`) and every `QDialog`
  (`KnowledgeObjectEditDialog`, `RelationshipEditDialog`,
  `RoadmapDialog`, `ProjectSuggestionsDialog`, `TwoFieldItemDialog` -
  `QDialog` is inherently its own top-level window regardless of
  parent) were each invisible to a stylesheet set on `MainWindow`.
  Fixed by applying via `qApp->setStyleSheet()` instead, which is
  process-wide and ignores top-level-window boundaries entirely - the
  only mechanism that actually solves this, short of manually
  reapplying the same stylesheet string in every dialog's own
  constructor (which would drift out of sync the first time only one
  copy got edited).
- **Going process-wide meant actually finishing the theming pass this
  project had explicitly deferred earlier** (see the "UI enhancement
  pass" entry above: "those dialogs intentionally keep the OS default
  style until they get their own theming pass"). `QComboBox`,
  `QPlainTextEdit`, and `QTabWidget`/`QTabBar` - all used by the edit
  dialogs but never styled at all before this - got real rules for the
  first time, including `QComboBox`'s popup list
  (`QComboBox QAbstractItemView`), which is a separate top-level popup
  in Qt and needed its own explicit background/selection colors or it
  would have rendered as a stray white dropdown against an otherwise
  dark dialog.
- **`QLabel { color }` stayed an explicit, separate rule rather than
  relying on inheritance from the new blanket `QWidget` rule.** Qt
  style sheets don't reliably propagate the `color` property from an
  ancestor type-selector down to `QLabel` the way CSS inheritance on
  the web does; the project's own working stylesheet (verified by
  passing tests) had always set `QLabel` explicitly, so that pattern
  was kept rather than assumed to now work implicitly through a wider
  blanket rule.
- **Graph transitions animate by easing toward a newly-computed
  layout, not by running continuous force-directed physics.** True
  per-frame physics (what Obsidian's own graph view does) was
  considered and explicitly not built: it directly reopens an earlier,
  reasoned architecture decision (`ForceDirectedLayout` computes once
  per structural change specifically to avoid O(n²) work every single
  frame at the node counts this app targets). Instead,
  `GraphCanvasItem` now keeps two id-keyed position maps -
  `targetPositions_` (where the static layout says a node belongs) and
  `currentPositions_` (what's actually rendered) - and a `QTimer`
  exponentially eases the latter toward the former
  (`current += (target - current) * 0.2` per tick, ~60fps) until every
  node is within a small epsilon, then stops. This gets most of the
  "graph feels alive, watch it settle" effect Obsidian is known for,
  at a fraction of the ongoing computational cost, and the timer
  costs nothing once the graph is at rest - it doesn't run permanently.
- **`RenderEdge` changed from storing raw `x1/y1/x2/y2` coordinates to
  storing `sourceId`/`targetId` node ids**, resolved to
  `currentPositions_` fresh on every geometry rebuild. This was a
  forced consequence of the animation work, not a separate choice: an
  edge built from coordinates captured at `setGraphData()` time would
  visibly detach from its nodes the instant they started easing toward
  a new position, since the edge's own endpoints would stay frozen at
  the stale layout. Every other call site (there was exactly one,
  `GraphWindow::refreshGraph()`) was updated to pass ids instead of
  pre-resolved positions.
- **New nodes appear immediately at their target position; only
  already-known nodes (repositioned by a layout change) animate.**
  Guessing a plausible "was somewhere before" position for a brand-new
  node (e.g. the graph's centroid, or its first neighbor's position)
  was considered and deliberately left out of this pass - simpler, and
  a node popping in instantly still reads fine next to others gliding
  into their new spots, without extra logic to get subtly wrong.
- **Hit-testing and rendering both read from `currentPositions_`, not
  from the target coordinates baked into `nodes_`.** Without this, a
  click during an in-flight animation would be tested against where a
  node is *headed*, not where it visually *is* - a small but real
  correctness gap that would have made clicking a moving node feel
  broken during the ~1-second window most transitions take to settle.

## UI enhancement pass (canvas visuals, hover, topic management)

- **Nodes became circles (triangle-fan geometry), not textured
  sprites.** A texture-based approach (rendering a circle image via
  `QSGSimpleTextureNode`) would look smoother with less vertex data,
  but introduces asset generation/loading that a hand-built triangle
  fan avoids entirely - `appendCircleFan()` is one small function
  every circular shape on the canvas (node fills, node borders, and
  all three ring types) now goes through, so "make circles smoother"
  or "add anti-aliasing later" is a one-place change, not a rewrite.
  16 segments per circle was picked as a size/smoothness trade-off,
  not measured against a specific quality bar - worth revisiting if it
  ever looks faceted at high zoom.
- **Every node gets a permanent border, not just highlighted ones.** A
  flat-colored circle directly on a pure-black (or plain-beige)
  background reads as a blurry smudge with no defined edge - the
  border exists purely to give every node a crisp silhouette. A real
  bug was caught before shipping: the first attempt computed the dark
  theme's border as `QColor(kBlack).darker(140)`, which has no visible
  effect on pure black (there's nothing to darken) and would have been
  literally invisible. Fixed by using `kCoffee` (a distinct warm dark
  brown, not derived from the background color itself) for both themes.
- **Hover and selection are visually and architecturally separate
  states**, with their own ring color, sized smaller than the
  selection ring so priority reads correctly if a node is somehow both
  (selection ring wins - stronger and stickier as a user commitment).
  `GraphCanvasItem::nodeHovered(QString id)` is a dumb, stateless "here's
  what's under the cursor right now" signal - the canvas has no idea
  what a tooltip is; `GraphWindow` decides what showing "hovering this
  id" means (a `QToolTip` with title/difficulty/confidence/definition)
  and does all the domain-level interpretation.
- **Hit-testing switched from a square bounding-box check to a real
  circular distance check**, to match the new circular node shape -
  otherwise a click near a node's old bounding-box corner (now outside
  the visible circle) would register as a hit on empty-looking space,
  which would have read as broken precision rather than a deliberate
  design.
- **The dot-grid background is regenerated only on pan/zoom/resize
  (`backgroundDirty_`), completely independent of `dataDirty_`
  (nodes/edges/rings).** These two invalidation flags were kept
  strictly separate on purpose: panning the camera shouldn't force a
  full node/edge/ring vertex rebuild just to redraw dots, and adding a
  Knowledge Object shouldn't force every dot on screen to be
  recomputed. Dot spacing scales with zoom but is snapped to a pixel
  range (18-72px) via repeated halving/doubling - the standard
  level-of-detail trick applied to a *spacing value* rather than to
  actual rendered detail, so the grid never becomes so dense it's
  expensive to draw when zoomed out, or so sparse it stops reading as
  a grid when zoomed in.
- **Topic rename/delete were already fully implemented at the
  controller layer (with correct validation - can't delete
  Uncategorized, can't delete a non-empty topic) but had no UI at
  all.** `TopicSelectorWidget` gained a standard `Qt::CustomContextMenu`
  right-click handler; no new validation logic was needed anywhere,
  only wiring an existing, already-tested capability to an entry point
  a person could actually reach.
- **A second flaky-modal-test situation, recognized early because of
  the first one.** `TopicSelectorWidget`'s context menu is a
  `QMenu::exec()` call - the same class of blocking modal that made
  the `TwoFieldItemDialog` add-flow test flaky earlier in this project.
  Rather than repeat that mistake, the test suite here only verifies
  what's safely testable without driving the menu's own event loop
  (the context-menu policy is set; an empty-space right-click is a
  no-op) and leans on the controller-level `renameTopic`/`removeTopic`
  tests - already passing - for the actual business logic.
- **Adding a heading label to `KnowledgeObjectPanel`** (it had none -
  went straight from nothing to the search box) surfaced an ambiguous-
  lookup test bug: two existing tests called `findChild<QLabel*>()`
  with no name, which had silently relied on the empty-state label
  being the *only* `QLabel` in the panel. Adding a second `QLabel`
  (the heading) made both tests start matching the wrong one. Fixed by
  giving the empty-state label an explicit object name
  (`"emptyStateLabel"`) and updating the two lookups to target it by
  name - the same "don't rely on positional/ordering assumptions that
  happen to hold today" lesson as the deterministic tie-breaks added
  to `GraphEngine::search()`/`suggestProjects()` much earlier.
- **Explicit list-item selection styling was added specifically
  because its absence was very likely the real "why does this still
  look bad even with the palette applied" culprit.** Before this pass,
  `QListView`/`QListWidget` had a background/text/border color from
  the custom stylesheet, but no `::item:selected` rule - meaning
  selecting a row fell back to the OS's default selection highlight
  (typically a system blue), which clashes badly against a deliberately
  chosen four-color palette. A custom highlight is now
  applied uniformly for both list types, so no PC's OS theme choice
  can undermine the app's own palette on this one interaction.

## Examples / Mini Projects / References editing

- **`KnowledgeObject` gained `setExamples()`/`setMiniProjects()`/
  `setReferences()` - whole-list replacement, not incremental add/
  remove/edit mutators.** Matches a pattern already established one
  layer down: `atlas-persistence`'s `KnowledgeObjectRepository` already
  treats these three fields as "delete every child row for this
  object, then reinsert the current list" rather than diffing (a
  decision made back in M1, when these lists were expected to stay
  small). An editing UI collects a full edited list and hands it over
  in one call - consistent with how every other field in
  `KnowledgeObjectEdits` is already an optional full replacement, not
  an incremental patch.
- **One generic `TwoFieldItemDialog`, not three near-identical add/edit
  dialogs.** All three of `atlas-core`'s content types - `Example`
  (description, optional snippet), `MiniProject` (title, description),
  `Reference` (title, optional url) - are shaped the same way: one
  required string field, one second string field that's either
  required or optional. That's close enough to share one small
  parameterized dialog. The *list section* around each type
  (`buildExamplesSection()`/`buildMiniProjectsSection()`/
  `buildReferencesSection()`) stays three explicit, near-duplicated
  blocks rather than a generic "list-of-T" widget abstraction -
  mirroring `KnowledgeObjectRepository`'s own precedent of accepting
  duplication over genericizing these same three types, since each
  section's specific item-to-display-text formatting is different
  enough that a generic abstraction would need callback parameters for
  nearly everything anyway.
- **The edit dialog gained tabs.** Five text fields, two combo boxes,
  and three list-editing sections would not fit in one flat form
  without becoming an unusable wall of widgets - the existing "Details"
  form became one tab, Examples/Mini Projects/References each became
  their own.
- **List edits are staged locally in the dialog (a `std::vector<T>`
  member per section) and only committed via `examples()`/
  `miniProjects()`/`references()` if the dialog is accepted.**
  Cancelling the dialog discards any in-progress Add/Edit/Remove
  actions on these lists, the same as cancelling discards edits to
  every other field - nothing is written to the actual
  `KnowledgeObject` or the database until `WorkspaceController::
  updateKnowledgeObject()` is called with the whole edited state.
- **A flaky test was written, diagnosed, and deliberately removed
  rather than left in "usually passing."** An end-to-end test drove
  the "Add..." button's `TwoFieldItemDialog::exec()` via
  `QApplication::activeModalWidget()` inside a `QTimer::singleShot`
  callback - the standard Qt technique for testing a button that opens
  a modal. It failed intermittently: `activeModalWidget()` proved
  unreliable specifically under the `offscreen` Qt platform this whole
  test binary runs under, not a defect in the dialog itself. Every
  other dialog test in this codebase (`RoadmapDialog`,
  `RelationshipEditDialog`, `ProjectSuggestionsDialog`) already
  deliberately avoids driving a nested modal for what was, in
  retrospect, likely this same reason - a convention that had never
  been written down until this milestone made someone try to break it.
  Removed rather than kept "because it usually passes": a flaky test
  that intermittently fails CI erodes trust in every other test's
  result, which costs more than the coverage gap it leaves behind.

## Project suggestions

- **The ranking heuristic lives in `atlas-graph`, not `atlas-ui` or
  `atlas-core`.** It's a pure computation over graph structure -
  readiness needs `dependsOn()`, leverage needs the new
  `transitiveDependents()` - the same category as
  `topologicalOrder()`/`learningRoadmapFor()`, and for the same reason:
  keeping it testable with synthetic graphs, no Qt or SQLite involved.
- **Project suggestions use only data already in the graph.** The whole
  ranking (readiness x leverage, filtered to concepts with a MiniProject
  and not already Mastered) is computable from the graph itself.
  Relationship suggestions are not built.
- **`transitiveDependents()` is the mirror of `transitiveDependencies()`
  - added as a new public primitive, not inlined into
  `suggestProjects()`.** Both are BFS over `DependsOn` edges in
  opposite directions (dependsOn vs. usedBy), so the traversal itself
  was factored into one shared private `transitiveClosure()` helper
  rather than duplicating the loop - the same "don't duplicate a BFS
  that already exists" instinct that produced
  `learningRoadmapFor()` reusing `transitiveDependencies()` earlier.
  `transitiveDependents()` is useful on its own beyond this feature
  (e.g. "what would break if I forgot this concept"), so it's public,
  not a suggestProjects()-only implementation detail.
- **A hard filter, not just a scoring factor, for "has at least one
  MiniProject."** A highly-leveraged, fully-ready concept with zero
  MiniProjects would score arbitrarily high under readiness × leverage
  alone, but there's nothing to actually go *do* - suggesting it would
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
  `concept` had to be renamed to `knowledgeObject` - `concept` is a
  reserved keyword in C++20 (concepts, the language feature), and the
  compiler error surfaced as a bizarre "too many initializers"
  downstream of the real parse failure, not as an obvious "reserved
  word" message.

## Topics, unified window, and theming

- **A KnowledgeObject belongs to exactly one Topic; Topics don't
  nest.** Nesting was real future scope, not this pass's - a flat
  namespace was enough to make "OS," "Databases," "Distributed
  Systems" distinct maps without the added complexity of a tree
  (reparenting, cycle checks, depth limits) that nothing yet demands.
- **Relationships can't cross Topic boundaries - enforced in
  `WorkspaceController`, not on `Relationship` or `KnowledgeObject`
  themselves.** Same split as the duplicate-edge and self-loop checks
  from earlier: an app-level rule about how two objects may relate,
  not something intrinsic to either class. All the checks -
  self-loop, duplicate (including a symmetric type's reverse pair),
  and now cross-topic - run against the in-memory graph before
  anything is written, so a rejection is never discovered only after
  the database already accepted a row.
- **Migration 2 backfills a fixed "Uncategorized" topic using the nil
  UUID as its id**, not a freshly generated one. Every pre-existing
  KnowledgeObject needs a topic the moment this migration runs, and
  the fixed id means the app can reference "the Uncategorized topic"
  from C++ (`uncategorizedTopicId()`) without a database round trip -
  it's the same id on every machine's database, deterministically.
  `topic_id` stays nullable at the SQL level (SQLite can't cheaply add
  a `NOT NULL` column with no default to an existing table without a
  full rebuild); "every object has a topic" is enforced in code at the
  `WorkspaceController` boundary instead, the same way `KnowledgeObject`'s
  own invariants are.
- **`TopicRepository` has no in-memory cache the way
  `KnowledgeObject`/`Relationship` have `GraphEngine`.** Topics don't
  participate in graph traversal - they don't nest, don't have edges
  of their own - and at the scale a person actually has topics (dozens,
  not thousands), querying the repository directly on every call is
  simpler than a cache and can't go stale, with no real performance
  cost to weigh against that simplicity.
- **`topicsChanged` is a separate signal from `graphChanged`, not
  folded into it** - a deliberate exception to the earlier "collapse
  everything into one signal" pattern from the relationship-creation
  work. That collapse was about not missing a cascade; this is about
  not doing wasted work. `TopicSelectorWidget` cares about
  `topicsChanged` and not `graphChanged`; the workspace panel and
  canvas care about `graphChanged` and not `topicsChanged`. Folding
  them together would mean every topic rename triggers a full graph
  relayout in whichever topic happens to be open at the time - an
  unrelated observer paying a real cost, not a case where something
  could be silently missed.
- **The topic selector and the workspace view are two pages of one
  `QStackedWidget` inside `MainWindow`, not two windows.** Consistent
  with the earlier decision to unify the list and canvas into one
  window rather than several utility windows - adding a third
  top-level window for topic selection would have reintroduced the
  same problem in a new place. The page swap itself
  (`switchToPage`/`stack_->setCurrentWidget`) is synchronous and
  never gated behind animation completion - a fade-in runs on top of
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
  (often black) text - unreadable, and not something anyone asked for.
  Those dialogs intentionally keep the OS default style until they get
  their own theming pass; the four-color palette (`Theme`) only
  applies to the topic selector, the workspace panel, and the canvas.
- **`Theme` is a plain data struct plus a `themeFor(mode)` lookup, not
  a live-updating/observable theming system.** Two modes, both
  precomputed once as static locals - cheap to call repeatedly (e.g.
  once per `refreshGraph()`), no allocation, no signal/slot machinery
  for "theme changed" beyond `MainWindow` re-applying the stylesheet
  and calling `GraphWindow::setTheme()` directly on toggle. Difficulty
  colors are the one deliberate exception to "everything comes from
  the four-color palette" - they're a semantic mapping onto Beginner
  through Expert, so they get their own tuned hues per mode rather
  than being derived from Black/Brown/Coffee/Beige.

## Dependency visualization and learning roadmap generation

- **These turned out to be one feature, not two.** The original spec
  listed "dependency visualization" and "learning roadmap generation"
  as separate items. In practice, "what should I learn before X" and
  "show me X's dependency order" are the same question - building them
  as two separate UI flows would have meant building the same
  underlying primitive twice. Combined into one right-click action.
- **A real gap found while building the UI, not before:**
  `GraphEngine::topologicalOrder()` operates on the *entire* graph's
  `DependsOn` edges. A roadmap "to learn X" needs to be scoped to only
  X's prerequisite chain - without that restriction, asking for a
  roadmap to a beginner-level, dependency-free concept would still
  surface every other disconnected concept in the workspace that also
  happens to have no dependencies, since they'd all tie for "first" in
  a global topological sort. `learningRoadmapFor()` fixes this by
  intersecting the global topological order with
  `transitiveDependencies(id) ∪ {id}` - O(1) membership checks via an
  `unordered_set`, so the filter doesn't reintroduce the cost the
  global sort already paid. `GraphEngine`'s existing tests never
  caught this because they only ever asserted on the *global* ordering,
  never on "the order restricted to one node's prerequisites."
- **Right-click, not a third window or a toolbar button.** Canvas
  click-selection already existed from the relationship-highlighting
  work; right-click was the natural extension for a contextual action
  on a specific node, rather than introducing a new selection mechanism
  or a separate dialog flow to pick a target node from a list again.
- **`RoadmapDialog` is read-only by design** - a plain numbered list,
  no drag-to-reorder, no inline editing. The order is computed, not
  authored; a UI that implied it could be rearranged by hand would
  misrepresent what the feature actually does.
- **A cycle in the dependency graph is reported as a dialog message,
  not a crash or a silent empty list.** `GraphError::CycleDetected`
  threads all the way from `GraphEngine::learningRoadmapFor()` through
  `WorkspaceController::roadmapFor()`'s own `RoadmapErrorCode` (a
  separate UI-facing enum, not a re-export of `GraphError` - `atlas-ui`
  shouldn't need to know about `atlas-graph`'s internal error shape) up
  to a `QMessageBox` telling the person their `DependsOn` graph has a
  loop. This is a real, reachable state (it's trivial to accidentally
  create a `DependsOn` cycle through the existing relationship UI), so
  it needed a real, non-crashing answer.
- **A second occurrence of the same "edit tool drops the next
  `TEST_CASE` line" mistake from earlier sessions** happened twice
  while adding these tests - once in `test_graph_engine.cpp`, once in
  `test_workspace_controller.cpp`. Both times, an insertion edit
  silently swallowed the following test's `TEST_CASE(...)` declaration,
  leaving its body orphaned inside the previous test. Caught both times
  by a brace-balance + `grep -c "^TEST_CASE"` sanity check before
  trusting a clean compile - worth treating as a standing habit after
  any insertion-style edit to a test file, not just when something
  looks suspicious.

## Search

- **Matching logic lives in `atlas-core` as a pure, Qt-free scoring
  function (`matchScore`), not in `atlas-graph` or `atlas-ui`.**
  "Does this object's text match this query" doesn't need graph
  structure or Qt - it's a property of a single `KnowledgeObject`.
  `GraphEngine::search()` is a thin layer applying that function across
  every live node and ranking the results; `WorkspaceController::search()`
  resolves ids back to full objects, same pattern as
  `allKnowledgeObjects()`.
- **v1 searches Title/Definition/Problem Solved/Why It Exists/Notes -
  not Examples/MiniProjects/References.** Those are comparatively
  rarely where the differentiating text lives, and including them
  means iterating nested vectors-of-structs for a benefit with no
  evidence anyone needs it yet. Easy to extend later.
- **Search results rank by relevance, not alphabetically - a
  deliberate departure from `allKnowledgeObjects()`'s sort order.**
  Title matches outweigh body matches; matching in two fields outranks
  matching in only one. A deterministic tie-break (by id string) was
  added specifically because `GraphEngine`'s internal storage is a hash
  map - without it, two identical searches could return tied results in
  a different order purely from hash-map iteration order, which would
  make the list look like it's "jumping around" for no reason.
- **An empty query is handled differently at each layer, on purpose.**
  `matchScore()` returns `nullopt` for an empty query (an empty
  substring trivially "matches" everything via `std::string::find`,
  which isn't the behavior anyone wants). `GraphEngine::search("")`
  returns every live node, unranked. `WorkspaceController::search("")`
  explicitly delegates to `allKnowledgeObjects()` instead, so clearing
  the search box gives back the same familiar alphabetical order the
  plain list view uses - not `GraphEngine`'s hash-map order.
- **Verified fast at the scale that mattered, not assumed fast.** Linear
  scan, no indexing - and that's fine: ~200ms to rank all 10,000 nodes,
  measured under ASan/UBSan overhead (faster in Release). No debouncing
  added to the search box; there's no evidence it's needed at this cost
  per keystroke.

## Why some things look the way they do

- **IDs are UUIDs, not sequential integers** - so objects created in
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
  loading an existing one from storage have different invariants -
  `create()` generates a fresh id and timestamps; `reconstruct()`
  restores them exactly. Conflating the two either makes `create()`
  accept an id/timestamps it shouldn't, or makes loading silently
  overwrite `updatedAt` to "now" on every read. Same pattern on
  `Relationship`.
- **SQLite is used via the system dev package in this sandbox.** For
  shipping Atlas cross-platform (M9), vendoring the SQLite amalgamation
  is the better long-term choice - it guarantees the same SQLite
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
  This keeps the graph engine fully testable with synthetic data -
  which is what makes the 10,000-node smoke test possible without ever
  touching SQLite.
- **Graph storage uses dense vectors + an id-to-index map, not
  UUID-keyed containers everywhere.** Stable UUIDs stay the public
  identity; a `vector<size_t>`-based adjacency list is what actually
  gets walked during traversal, for cache locality at 10k+ scale.
  Deletions are tombstoned (slot cleared, not erased) rather than
  physically removed, since erasing from a dense vector's middle would
  invalidate every later index - worse at scale than a dead slot.
- **`addEdge` rejects a symmetric edge stored in the reverse pair
  order as a duplicate, even though the database's `UNIQUE(source_id,
  target_id, type)` constraint would not catch it.** "A RelatedTo B"
  and "B RelatedTo A" are the same fact for a symmetric type; expressing
  "unique unordered pair" in SQL isn't worth the complexity when this
  layer - the one that actually understands `isSymmetric()` - already
  guards every write path that will exist in practice. Documented as an
  accepted gap in the database's defense-in-depth, not an oversight.
- **`nodes_`/`edges_` are `std::deque`, not `std::vector` - found and
  fixed after the fact, not designed in from the start.** The original
  `vector`-backed version had a real heap-use-after-free:
  `findNode()`/`findEdge()` return raw pointers into the backing
  storage, and a `vector` reallocates on growth, invalidating every
  pointer into it. No test caught this - none held a pointer from
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
  strictly behind it - never ahead, never diverged - without any
  rollback-on-failure logic anywhere in this class. `updateKnowledgeObject`
  specifically copies the current object, mutates the copy, persists
  the copy, and only then calls `GraphEngine::updateNode()` - the live
  graph is never touched until the database write has already
  succeeded.
- **`Result<T,E>` was missing its non-const lvalue `value()`/`error()`
  overloads - caught by `-Wredundant-move`, not by a passing test
  suite.** Without a plain `T& value() &` overload, calling `.value()`
  on a named non-const `Result` had no matching non-const candidate
  and silently fell back to the `const&` one. The practical consequence:
  `WorkspaceController::load()`'s `for (auto& object : result.value())`
  loop was binding `object` as `const KnowledgeObject&`, making every
  `std::move(object)` inside it a silent copy instead of a move - every
  object loaded from the database at startup was being copied, not
  moved, into the graph. Fixed in `atlas-core`, with `Result<T,E>` now
  getting the dedicated test file it should have had from M0.
- **CMake's AUTOMOC didn't discover any of `atlas-ui`'s `Q_OBJECT`
  headers on the first attempt** - `add_library` only listed the `.cpp`
  files, and AUTOMOC's header-discovery heuristic across a separate
  `include/`/`src/` split didn't find them, producing an empty
  `mocs_compilation.cpp` and four "undefined reference to vtable"
  linker errors with no compile-time warning at all. Fixed by listing
  the `Q_OBJECT` headers explicitly as target sources - the robust,
  unambiguous way to guarantee AUTOMOC sees them, rather than relying
  on its include-graph discovery.
- **Qt widget tests run under the `offscreen` platform plugin**, set
  automatically for that one `ctest` entry via
  `set_tests_properties(... ENVIRONMENT ...)` - not exported globally,
  so running `atlas_app` directly still uses a real display. One
  `QApplication`, constructed once in a custom `main()`, shared by every
  test in `atlas_ui_tests` (including the plain-`QObject`
  controller/model tests) - only one `QApplication` may exist per
  process, so it's simplest for every UI test to share it rather than
  splitting Qt-widget tests into a separate binary.
- **Layout is computed once per structural graph change, never
  continuously simulated.** "Smooth at 10,000 nodes" is two different
  problems: GPU-batched pan/zoom (easy - the camera is a transform
  matrix update, not a geometry rebuild) versus continuous force-
  directed physics at that scale (genuinely hard, O(n²) per frame
  without spatial partitioning). The spec needs the first, not the
  second.
- **Naive O(n²) repulsion measured at 10.8 seconds for one layout pass
  at 10,000 nodes - far too slow even as a one-time computation.**
  Replaced with a uniform-grid (cell-list) approximation: repulsion
  decays as 1/distance², so contributions beyond a 3x3 neighborhood of
  grid cells are already negligible, the same principle Barnes-Hut
  generalizes with a quadtree. Brought it to ~550-900ms in Release.
  Built only after measuring the naive version was too slow - not
  speculative - same empirical bar as every other performance decision
  in this project. A first pass at the fix also repeated an earlier
  mistake: using `unordered_map<KnowledgeObjectId, Point2D>` in the hot
  loop, meaning every pairwise force check did several 16-byte-UUID
  hash lookups. All hashing is now confined to a one-time setup phase;
  the simulation loop itself touches only dense arrays - the same
  "stable id for identity, dense index for the hot path" lesson from
  `atlas-graph`'s `GraphEngine`, just not carried into this module the
  first time.
- **`GraphCanvasItem` draws all nodes in one `QSGGeometryNode`
  (`DrawTriangles`) and all edges in another (`DrawLines`)** - one GPU
  draw call per category regardless of node count. This is the actual
  payoff of choosing Qt Quick over `QGraphicsView` back when the
  rendering backend was decided: `QGraphicsView` does CPU-side
  per-item work that doesn't batch this way. Pan/zoom only updates a
  `QSGTransformNode`'s matrix; geometry is rebuilt only when
  `setGraphData()` is called (a structural change or a recomputed
  layout), never on every frame of a drag.
- **Ubuntu splits Qt6's QML *import* modules from the C++ runtime
  libraries into separate packages** (`libqt6qmlworkerscript6` vs
  `qml6-module-qtqml-workerscript`) - `apt install qt6-declarative-dev`
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
  `QQuickWidget::rootObject()` - a different object tree entirely, not
  reachable via the enclosing widget's child hierarchy. Fixed by adding
  a proper `GraphWindow::canvasItem()` accessor rather than working
  around it in the test.
- **A real usability gap, found only by actually running the app**: an
  empty list and a broken window looked identical (both just blank
  white), and a single new row was easy to miss against a blank
  background with no visual cue anything had changed. No test caught
  this - it's not the kind of thing a unit test checks. Fixed with an
  empty-state placeholder (swapped via `QStackedWidget`, checked with
  `currentWidget()` in tests rather than `isVisible()`, which depends
  on the window actually being shown via `show()` - something tests
  correctly never do) and alternating row colors.
- **Relationship creation reuses the list view's existing row
  selection, not canvas-click node picking.** Building this surfaced
  that the canvas has no node-picking at all - every mouse press
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
  remember to wire up correctly - and tracing it through surfaced a
  real gap: `removeKnowledgeObject`'s cascade (deleting relationships
  that touched the removed object) never fired any
  relationship-specific signal for those cascaded removals. A single
  signal that every dependent view treats as "go refresh yourself"
  can't miss a cascade, because there's nothing fine-grained to forget
  to wire up. Same "full reset over precise incremental tracking"
  trade as the list model's own refresh strategy, applied one level up.
- **`GraphEngine::hasDuplicateEdge` is public now, not an `addEdge()`-
  only implementation detail** - specifically so `createRelationship`
  can check it *before* writing anything to the database. The
  database's `UNIQUE(source_id, target_id, type)` constraint doesn't
  catch a symmetric type's reverse-pair duplicate (documented back in
  M2), but `GraphEngine` does; checking it first means that rejection
  happens before any database write, not as an inconsistency
  discovered after one already succeeded.

## Learning engine (plan 1)

- The review log is the source of truth. `memory_states` is a cache that `MemoryLedger::replay` can rebuild at any time.
- One `LearningRepository` covers events, states, and cache metadata, because they always change in one transaction.
- Learning data for a deleted concept or link is removed by SQL triggers. They run inside the delete itself, including relationship cascades, so no caller can forget them.
- `due_at` is the exact moment recall chance reaches the target retention. "Due" means `due_at <= now`, equivalent to `R(now) < target`.
- New items follow a simplified FSRS phase model: a first `again` enters learning, any other first grade enters review. There are no intra-day learning steps; the in-session requeue is the hypercorrection rule.
- A prerequisite that depends back on the concept (a cycle) does not block it from the frontier.
- Links between topics are allowed. The topic view draws only links with both ends in the topic until ghost nodes arrive with the new canvas.
- Layout processes nodes in id order, so results no longer depend on hash map order.
- Links that are ready but never reviewed join the due set with recall 0 and do not count toward the daily new limit, which counts frontier concepts only.
- `max_focus` is a hard cap: a contrast unit that does not fit is truncated to the remaining slots and planning stops.
- Topic interleaving looks at the last two placed focuses, not at whole units.
- Structs that are brace-initialized with fewer fields than they have give the remaining members a `{}` default, so `-Wextra -Werror` accepts them.
- Strongly connected components of DependsOn decide the cycle rule, so planning stays near linear at 10k concepts. New concepts are ordered by direct dependents; transitive leverage only breaks ties between due items, as the spec defines it.
- Same-day reviews floor the stability increase at 1 for hard, good and easy, matching the py-fsrs reference.
- Replay computes the schema boost from the current graph, so editing prerequisites changes rebuilt stabilities.
- `record` only refreshes the cache marker when the cache was fresh before the write, so a stale cache is never hidden.
- Plan 2 contract: saved placements are fixed during normal loads, physics runs only for unplaced nodes, and a full warm pass happens only on an explicit tidy action, so the map never drifts.
- Plan 2 contract: the links graded in a rebuild are exactly the link items in `FocusPlan::items` (hidden links are all introduced links of the focus); the caller sets `ReviewEvent::elapsedDays`; a `NetworkRules` must outlive any `BoostFn` it returns.

## View models and canvas (plan 2)

- The C++ layer between the engine and QML lives in `atlas-viewmodels`, a static QML module (`Atlas.ViewModels`). QML reaches app objects as singletons provided from C++; those classes are never default-constructible, because QML builds its own copy of a default-constructible singleton.
- `WorkspaceController` moved to `atlas-viewmodels`. The Widgets UI was removed in plan 4; the QML app is the only app.
- `MemoryController` loads the review log at startup and rebuilds the memory cache when it is stale or the replay version changed.
- `PlacementController` follows the layout contract: saved positions never move on normal loads, physics only places new concepts, and tidy is an explicit action that keeps pinned concepts fixed.
- Canvas labels are laid out on the GUI thread when data changes and only drawn on the render thread.
- Below zoom 0.35 each topic collapses into one node with its mean recall ring; clicking it opens that topic.
- In a topic view, concepts from other topics that link to a member appear as faded ghost nodes.
- The self-rated Confidence field stays in the data model but no view model exposes it.
- Atlas now requires Qt 6.7 or newer, because canvas labels use `QSGTextNode`.
- Canvas vertex colors are premultiplied by alpha before upload, as `QSGVertexColorMaterial` expects; faded colors would otherwise draw nearly opaque.
- Selection, neighbor, and hover highlights are thin rings drawn outside the memory ring, and links stop at the memory ring's outer edge, so neither hides the other.
- The canvas only collapses into topics when at least one node has a topic key, so views without topics never go blank when zoomed out.
- Tests that add reviews after the memory cache is loaded record the resulting memory states with the events, matching how the app records sessions.
- Map refreshes are coalesced into one queued refresh per event loop turn, and concepts appear on the map only once they have a saved position.
- A corrupt memory cache is rebuilt from the log at startup instead of blocking it.
- New concepts start near their linked neighbors, else near their topic's center, so topics form regions on the map.
- Ghost nodes are drawn at their own saved positions rather than at the edge of the view.
- Canvas labels are cached across scene updates; hover rebuilds only the highlight layer, while selection still rebuilds everything because it dims other nodes.

## QML shell (plan 3)

- The map canvas is one layer under every page; Today, Map, and Settings are overlays on top of it.
- Only `Main.qml` imports `Atlas.Render`; other components receive the canvas as `property Item canvas`.
- `Theme` forwards the C++ `Palette`; `Motion` holds the single curve and every duration, all zero when motion is reduced, and opacity changes fall back to 120 ms fades.
- Controls use the Basic style, recolored through the window palette.
- Concept fields write the draft on every edit. Switching concepts, closing the panel, or closing the window saves it. If that save fails, a toast says why and nothing moves on: the selection stays on the unsaved concept, focusing another concept keeps the topic scope, and the window stays open.
- View model errors appear as a toast for 4 s, never as a modal dialog.
- `AppStartup` turns a database that cannot be opened or loaded into a readable startup error window.
- The `atlas` app keeps the application name `atlas_app` so it shares the existing data folder until Plan 4 removes the old app.
- Camera moves ease on the canvas animation timer. Geometry is culled to a rect one viewport larger than the view and rebuilt only when the view leaves it or the topic collapse state flips.
- A topic change fits the view to the new scope; focusing a concept from search or creation centers on it instead.
- Search on the map finds concepts in the current topic; choose All topics to search everywhere.
- Single topic views keep topic labels on nodes but never collapse into topic bubbles.

## Sessions (plan 4)

- A focus is recorded when its explain step is graded. Rebuild grades wait in memory until then, so quitting drops only the unfinished focus.
- When a focus has no concept item, the explain step uses its weakest link, so that link gets a rebuild event and an explain event in one transaction; the second has 0 elapsed days.
- Concept prompts alternate between "What is A?" and "What problem does A solve?" by review count.
- A session shows all topics and restores the previous topic and selection when it ends. Clicking a topic bubble during a session does nothing.
- Starting a session saves an unsaved concept draft first; if that save fails, the session does not start and the toast says why.
- The explain prediction is fixed once the answer is revealed. The rebuild prediction must be picked before a hint and is fixed after the first one.
- If the written answer cannot be saved, the focus stays on the explain step with the text kept, so it can be retried.
- Clicking a node during a rebuild names it. The selection stays on the focus, and neighbors are never highlighted while a session runs.
- Rebuild suggestions match the start of a title, ignoring case, after three characters.
- Missed links turn rose; the one-time pulse from the spec is not drawn yet.
- Memory rings ease at a fixed rate on the canvas timer and snap when motion is reduced.
- Link notes are updated in place, so a link's review history is never deleted.
- Each install has a device id, a UUID kept in `settings.ini`.
- The Widgets app is gone. `atlas` keeps the application name `atlas_app`, so existing data stays where it was.
- A rebuild hides every introduced link of the focus, in either direction, but grades only its due links; this follows the spec over the earlier plan 2 contract.
- The learning roadmap moved into the concept panel ("Learn first") and project suggestions into the topic bar ("Ideas").
- The summary counts items, not events: "N of M items recalled". An item counts as recalled when its first event in the session was not Again.
- Ideas rank on measured memory, not the retired Confidence field: concepts in the topic with a mini project whose concept item is not solid. Readiness is the share of solid DependsOn prerequisites (1 with none), leverage is the number of transitive dependents, and ideas are ordered by readiness * (1 + leverage), ties by id.
- "Learn first" orders only the concept's own transitive DependsOn prerequisites, ties by id. A loop is reported only when it lies among them, and the path refreshes when the concept loads or the graph changes.
- Recorded review times are floored to whole milliseconds and each batch is applied in replay order, so live memory states equal replayed ones.
- The device id is read once per run and kept in memory.

## Workspace and identity (parts 1 and 2)

- Atlas looks warm stone and clay: matte surfaces separated by tone and a 1 px outline, one soft shadow only on floating panels and dragged items, no glass, glow, gradients, or blur.
- Color roles follow Material names in `Theme` (surface, surfaceHigh, outline, onSurface, primary, secondary, tertiary); older token names stay as aliases.
- Newsreader titles, Inter text, and IBM Plex Mono labels ship inside the app under the SIL Open Font License.
- Nodes are small dots with a thin memory ring; link count slightly enlarges a dot.
- The board is the only screen. Today sits in the corner stack, Settings opens as an expanded panel, and a session runs as a card on the board.
- Panels dock, fold, float, and expand; their layout is a view preference stored in `settings.ini`, and floating panels are pulled back inside the window when it shrinks.
- Topics are tinted regions fitted around their concepts; dragging a region's name moves every concept of the topic.
- One concept card is editable at a time; up to three more stay pinned as read only summaries beside their nodes.
- Sticky notes live in world coordinates in `board_notes`, link to concepts or topics through `note_links`, and lose a link automatically when its target is deleted.
- The on-demand panel flag is named `onDemand` because `transient` is reserved in QML.
- Floating panels get a soft shadow made of a few stacked translucent rounded rectangles (no blur); panels never animate in from the corner at start; dragged panels stay inside the window.
- Dots grow with link count up to three links, so the halo always stays inside the memory ring; links stop outside the selection ring.
- Notes and topics refresh links without resetting the board, so typing in a note is never interrupted.
- Note and region colors are rebuilt on theme change, because a binding that only reads Palette.background can be compiled away.
- Delegates that tests must find use Instantiator with an explicit parent.
- Pinned cards are laid out in one imperative pass so they never overlap and never form binding loops.
- NotesModel's factory method is createNote, because a method named create hides the singleton factory, as with createTopic.
- During a session the panel layer fades out and is disabled; starting a session closes an expanded panel.
