# Atlas

A visual knowledge navigation system — not a note-taking app.

## The problem this exists to solve

People don't forget concepts because they're difficult. They forget
because they can't see how concepts relate to each other. Notes and
wikis store knowledge as isolated documents; Atlas stores it as a
graph, because that's closer to how understanding actually works —
you don't really know recursion until you can see how it relates to
stack frames, to mathematical induction, to the call stack, to tail
calls. Atlas is built to make those relationships first-class,
navigable, and impossible to lose track of.

## What a Knowledge Object is

Every concept in Atlas is a **Knowledge Object** with:

| Field | Purpose |
|---|---|
| Title | Required — the only field with a real invariant |
| Definition | What it is |
| Problem it solves | Why anyone would reach for it |
| Why it exists | The historical/design motivation |
| Examples | Concrete illustrations |
| Mini Projects | Hands-on ways to internalize it |
| References | Where to read more |
| Notes | Free-form, personal |
| Difficulty | Intrinsic to the concept (Beginner -> Expert) |
| Confidence | Your own mastery of it (Unknown -> Mastered) — a separate axis from Difficulty on purpose |

Concepts never store their own relationships as fields. "Depends On"
and "Used By" look like attributes but are actually graph edges — see
[Architecture](#architecture) for why that distinction is load-bearing.

## What a Relationship is

A directed, typed edge between two Knowledge Objects:

`DependsOn` - `Uses` - `Implements` - `Solves` - `Contains` - `PartOf` -
`RelatedTo` - `AlternativeTo` - `OppositeOf` - `Causes`

The first seven are directional (A depends on B is not the same claim
as B depends on A). `RelatedTo`, `AlternativeTo`, and `OppositeOf` are
symmetric — A RelatedTo B is the same fact as B RelatedTo A, and the
system actually enforces that (see `isSymmetric()` in `atlas-core` and
the duplicate-detection notes in `docs/DECISIONS.md`).

## The full feature set — built vs. planned

| Feature | Status |
|---|---|
| Knowledge Object CRUD | **Built** |
| Relationship CRUD | **Built** (via list selection, not canvas clicks — see below) |
| SQLite storage, offline-first | **Built** |
| Interactive graph canvas, pan/zoom | **Built** |
| Large graph support (10,000+ nodes) | **Built and load-tested** — see Current State below |
| Dependency visualization (topological ordering) | Algorithm **built** (`GraphEngine::topologicalOrder`); no UI yet |
| Fast search | Not built |
| Relationship highlighting | Not built |
| Learning roadmap generation | Not built (the topological-sort primitive it needs already exists) |
| Project suggestions | Not built |
| AI-assisted relationship suggestions | Not built |
| Plugin architecture | Not built |
| Cross-platform packaging | Not built — Linux-developed only so far, no Windows/macOS verification |
| Canvas-click node selection | Not built — relationships are created via list selection instead |

## Architecture

Six modules, each with a single, deliberately narrow responsibility.
The dependency rule: every arrow points inward toward `atlas-core`.
`atlas-core` depends on nothing else in this tree, which is what keeps
it testable in isolation forever, no matter what happens to the UI
framework, the rendering backend, or the storage engine layered on top
of it.

```
atlas-core         domain model: KnowledgeObject, Relationship, value types
                    - zero dependencies on Qt, SQLite, or OpenGL

atlas-persistence   SQLite storage: schema migrations, one repository
                    per aggregate (KnowledgeObjectRepository,
                    RelationshipRepository)
                    - depends on atlas-core + SQLite only

atlas-graph         in-memory index over the domain model: O(1) id
                    lookup, adjacency respecting relationship symmetry,
                    BFS dependency traversal, topological sort
                    - depends on atlas-core only (never atlas-persistence
                      - this is what makes it testable with 10,000
                      synthetic nodes without touching a database)

atlas-render        force-directed layout (Qt-free) + the Qt Quick
                    rendering canvas (hand-built scene graph nodes -
                    one GPU draw call for all nodes, one for all edges)
                    - depends on atlas-core + atlas-graph

atlas-ui            WorkspaceController (the only class that touches
                    both persistence and the graph), and every Qt
                    Widgets window/dialog
                    - depends on everything above

atlas-app           the composition root: opens the real database file,
                    wires everything together, owns main()
```

`WorkspaceController` carries one invariant without exception: **every
mutation writes to the database first, and only touches the in-memory
graph after that write has already succeeded.** That ordering is what
makes the graph always either consistent with the database or strictly
behind it — never ahead, never diverged — without any
rollback-on-failure logic anywhere in the codebase.

## Current state

Five modules built, all independently unit tested, all passing under
`-Wall -Wextra -Wpedantic -Werror` and under AddressSanitizer +
UndefinedBehaviorSanitizer. As of this writing:

| Module | Test cases | Assertions |
|---|---|---|
| `atlas-core` | 31 | 118 |
| `atlas-persistence` | 15 | 116 |
| `atlas-graph` | 19 | 20,620 |
| `atlas-render` | 7 | 20,030 |
| `atlas-ui` | 30 | 169 |
| **Total** | **102** | **~41,000** |

(`atlas-graph` and `atlas-render`'s assertion counts are dominated by
their 10,000-node scale tests, which assert per-node correctness, not
102 hand-written checks.)

**Load-tested, not just unit-tested, at the scale the spec actually
asks for:**
- `GraphEngine` holds 10,000 nodes and ~30,000 edges and runs a full
  BFS traversal + topological sort over them in well under a second.
- `ForceDirectedLayout` computes a full 50-iteration layout pass over
  the same graph in ~525ms in a Release build — after a real
  algorithmic correction (naive O(n^2) repulsion measured at **10.8
  seconds** before being replaced with a grid-based approximation; see
  `docs/DECISIONS.md`).
- `GraphCanvasItem` renders the result as two GPU draw calls total
  (one for all nodes, one for all edges) regardless of node count, so
  pan/zoom stays smooth independent of graph size.

**What actually works end-to-end right now, if you build and run it:**
create a Knowledge Object (title-only, then flesh it out via Edit),
connect two of them with a typed relationship and an optional note,
open the Graph View to see the result laid out and navigable (drag to
pan, scroll to zoom), delete things, restart the app and have it all
still be there.

**Known, deliberate gaps — not oversights:**
- The list view and the graph canvas are two separate windows, not one
  unified layout.
- Relationships are created by selecting a row in the list and picking
  a target from a dialog, not by clicking two nodes on the canvas — the
  canvas has no click-to-select node picking built yet (every click
  currently just pans).
- No search, no relationship highlighting, no roadmap generation UI,
  no AI suggestions, no plugin system. The graph engine has the
  primitives several of these need (topological sort, transitive
  dependency traversal) already built and tested; the UI for them
  doesn't exist yet.
- Cross-platform packaging hasn't been attempted — built and tested on
  Linux only so far.
- No threading: all database/graph operations run synchronously on the
  UI thread. Fine at current data volumes (sub-second even at 10k
  nodes); will need revisiting before anything long-running (AI calls,
  large imports) lands.

## Engineering principles actually followed, not just stated

- **SOLID, composition over inheritance.** Every class has one reason
  to change; cross-cutting concerns (validation, persistence, graph
  indexing, rendering) live in separate, separately-testable layers
  rather than being mixed into one God class.
- **`Result<T,E>`, not exceptions, at every fallible boundary** — a
  deliberate choice made in the first milestone specifically because a
  future plugin system will eventually cross a C ABI where C++
  exceptions aren't safe to propagate.
- **Every feature has unit tests, written against real behavior, not
  against the implementation.** Several real bugs (a dangling-pointer
  use-after-free, a silent copy-instead-of-move, a usability gap where
  an empty list and a broken window were indistinguishable) were only
  caught because of this discipline — and a few were only caught
  *despite* it, by actually running the app, which is exactly why that
  step never got skipped. See `docs/DECISIONS.md` for specifics.
- **Verify empirically, not by assumption** — every performance claim
  in this README (the 10k-node numbers, the layout timing, the
  before/after of the O(n^2) fix) was measured in this repository, not
  estimated.

## Building

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
```

Requires a C++20 compiler, CMake 3.21+, and Qt6 — Widgets, Quick, and
QuickWidgets components.

**Ubuntu/Debian:**
```sh
apt install qt6-base-dev qt6-declarative-dev
# Ubuntu also splits QML *import* modules from their C++ runtime
# libraries into separate packages. If "module ... is not installed"
# errors show up at runtime even with the above installed:
apt install qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtqml-workerscript
```

**Arch:** `pacman -S qt6-base qt6-declarative` (Arch doesn't split QML
modules the way Ubuntu does, but if anything QML-related fails to
load, that package split is the first thing to rule out).

No other dependencies are fetched or required to build
`atlas-core`/`atlas-persistence`/`atlas-graph`/`atlas-render`'s Qt-free
layout math on their own — `doctest.h` is vendored directly in
`third_party/doctest/` rather than pulled at build time, in keeping
with Atlas's own offline-first philosophy extending to its build
process. If Qt6 isn't available at all, configure with
`-DATLAS_BUILD_UI=OFF` to build and test everything below the UI layer.

Run the actual application with `./build/modules/app/atlas_app` — it
creates a SQLite database under the OS's standard application-data
directory on first run (e.g. `~/.local/share/atlas_app/atlas.db` on
Linux).

## Roadmap

Originally planned as M0-M9; M0-M4 plus a relationship-creation
addendum are done. Next, in the order they were last agreed on:

1. **Search** — fast lookup across Knowledge Objects by title/content.
2. **Relationship highlighting** — select a node, see its dependency
   neighborhood lit up on the canvas.
3. **Dependency visualization** — surface `GraphEngine::topologicalOrder`
   and `transitiveDependencies` in the UI, not just in tests.
4. **Learning roadmap generation** — the actual point of the
   topological sort primitive: turn "what should I learn, in what
   order" into a real feature.
5. Canvas-click node picking, then unifying the list and graph windows.
6. Project suggestions, AI-assisted relationship suggestions, plugin
   architecture, cross-platform packaging — in roughly that order, per
   the original milestone plan.

For the detailed "why does this specific thing look this way" history
— every bug found, every algorithmic correction, every cross-module
gap discovered while building the next layer — see
[`docs/DECISIONS.md`](docs/DECISIONS.md).
