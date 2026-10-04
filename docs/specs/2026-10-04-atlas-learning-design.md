# Atlas Learning: Design Spec

Date: 2026-10-04
Status: Draft for review

## 1. Vision

Atlas becomes **a map you earn**. Today Atlas is a knowledge viewer: you write concepts and links, then look at them. Research shows that looking at a finished map teaches little, while rebuilding it from memory teaches a lot. Atlas will hide what you claim to know, ask you to rebuild it, measure what you actually recall, and show that measured memory on the map.

Three goals:

1. **Remember**: an evidence based recall loop (retrieval practice, spacing, prerequisite awareness).
2. **See**: the map shows real, measured memory strength, not self ratings.
3. **Learn how to learn**: calibration feedback, short research backed explanations, and a starter map about learning itself.

## 2. Audience and platforms

- Self learners first. The data model also supports shared or expert built maps later (import a map, then learn it).
- Desktop app (Qt, C++) for building, exploring, and recall sessions.
- A lightweight phone app for daily recall sessions comes later, synced. Everything in this spec keeps that possible.

## 3. Scope

**In this spec**

- New `atlas-learning` module: memory model, review log, session planning, grading, calibration.
- Persistence for review events, memory states, and node placements.
- Recall sessions: rebuild exercise and explain exercise.
- Visual layer: Graphite style, memory rings, stable layout, unified motion system.
- Full UI migration from Qt Widgets to Qt Quick (QML).
- Fixes from the review that the design depends on: cross topic links, stable layout.
- Tooling: CMake presets, CI, warnings as errors, sanitizers.

**Out of scope (future sub-projects, data must be ready for them)**

- Mind view: brain model plus insights dashboards (section 13).
- Starter map "How Learning Works".
- Phone review app and sync.
- Fitting personal FSRS parameters from a user's own log.

## 4. Research foundations

Every learning feature maps to a finding. Features without research support are not built.

| Finding | Source | Design decision |
|---|---|---|
| Retrieval practice beats restudy | Roediger & Karpicke 2006; Dunlosky et al. 2013 | Sessions are recall, never rereading |
| Building a concept map from memory beats studying one | Karpicke & Blunt 2011; Blunt & Karpicke 2014; Nesbit & Adesope 2006 | Core exercise is rebuilding hidden links |
| Spacing beats massing | Cepeda et al. 2006 | FSRS scheduling, short daily sessions |
| Forgetting follows a power curve; storage vs retrieval strength | Bjork & Bjork 1992; FSRS-6 | Item layer: stability, difficulty, recall chance |
| Harder successful retrieval strengthens more | Bjork 1994 | FSRS stability gain grows as recall chance falls |
| New knowledge consolidates faster when it fits a schema | Tse et al. 2007; van Kesteren et al. 2012 | Schema boost on initial stability |
| Learning works best just beyond what is known | Doignon & Falmagne 1985 (Knowledge Space Theory) | Frontier rule for new items |
| Similar memories interfere | Anderson, Bjork & Bjork 1994 | Contrast pairs reviewed together |
| Interleaving helps discrimination | Rohrer & Taylor 2007 | Topics interleaved inside a session |
| High confidence errors are corrected best with feedback | Butterfield & Metcalfe 2001 | Confidence prediction before each focus, immediate feedback, requeue |
| Learners misjudge their own knowledge | Koriat & Bjork 2005; Nelson & Dunlosky 1991 | Self rated Confidence retired, calibration measured instead |
| Self explanation and generation improve understanding | Chi et al. 1994; Slamecka & Graf 1978 | Explain exercise with typed answers |
| Depth of processing affects retention | Craik & Lockhart 1972 | Prompt to write a link note when missing |
| Experts chunk knowledge | Chase & Simon 1973 | Zoomed out map shows one ring per topic |
| Sleep supports consolidation | Diekelmann & Born 2010 | Reviews spread across days; same day repeats count less (FSRS-6 same day rule) |
| Decorative animation adds load; meaningful motion helps | Tversky, Morrison & Betrancourt 2002; Mayer 2009 | Every animation encodes a state change |
| Learning styles are not supported | Pashler et al. 2008 | No "learning style" modes |
| Spreading activation | Collins & Loftus 1975 | Mixed evidence: not modeled, measured later from the log |

## 5. Domain and data model

### 5.1 Items

A **memory item** is anything Atlas schedules:

- **Concept item**: one per Knowledge Object. Tested by the explain exercise.
- **Link item**: one per Relationship. Tested by the rebuild exercise. A symmetric relationship is one item.

`ItemRef { kind: Concept | Link, id: Uuid }` identifies an item.

### 5.2 Review log (source of truth)

Append only. Never updated or deleted, except when the concept or link it refers to is deleted.

```
review_events
  id               TEXT PRIMARY KEY   uuid
  item_kind        TEXT NOT NULL      concept | link
  item_id          TEXT NOT NULL
  session_id       TEXT NOT NULL
  device_id        TEXT NOT NULL
  reviewed_at      INTEGER NOT NULL   unix ms
  elapsed_days     REAL NOT NULL      since previous review of this item, 0 for first
  exercise         TEXT NOT NULL      rebuild | explain
  predicted        INTEGER NOT NULL   1 guess, 2 unsure, 3 fairly sure, 4 certain
  grade            INTEGER NOT NULL   1 again, 2 hard, 3 good, 4 easy
  hints_used       INTEGER NOT NULL
  wrong_target_id  TEXT               concept linked by mistake, if any
  response_ms      INTEGER NOT NULL
INDEX (item_kind, item_id, reviewed_at)
```

### 5.3 Memory states (cache)

Derived by replaying `review_events`. Can be dropped and rebuilt at any time.

```
memory_states
  item_kind         TEXT NOT NULL
  item_id           TEXT NOT NULL
  phase             TEXT NOT NULL   new | learning | review | relearning
  stability         REAL NOT NULL   days
  difficulty        REAL NOT NULL   1 to 10
  last_reviewed_at  INTEGER
  due_at            INTEGER
  review_count      INTEGER NOT NULL
  lapse_count       INTEGER NOT NULL
  PRIMARY KEY (item_kind, item_id)

memory_meta
  key TEXT PRIMARY KEY, value TEXT NOT NULL
  -- holds replay_version and last_event_id to detect a stale cache
```

### 5.4 Node placements

One global coordinate space for all concepts. Topics are regions inside it. Needed for stable spatial memory now and the meaning map later.

```
node_placements
  concept_id  TEXT PRIMARY KEY REFERENCES knowledge_objects(id) ON DELETE CASCADE
  x           REAL NOT NULL
  y           REAL NOT NULL
  pinned      INTEGER NOT NULL DEFAULT 0
```

### 5.5 Changes to existing data

- Migration 3 creates the tables above. `item_id` refers to either table, so it has no foreign key: the repositories delete a concept's or relationship's review events and memory state explicitly, in the same transaction as the delete itself (including the links removed by a concept's cascade).
- Cross topic relationships become allowed. `WorkspaceController::createRelationship` drops the same topic check. The topic scoped canvas shows neighbors from other topics as faded ghost nodes at the edge of the view.
- The `confidence` column stays in the schema for compatibility but is no longer shown or edited. Measured memory replaces it.
- `Difficulty` stays, since it describes the concept, not the learner.

## 6. Memory model

### 6.1 Item layer: FSRS-6

- Retrievability: `R(t, S) = (1 + factor * t / S) ^ (-w20)`, with `factor = 0.9 ^ (-1 / w20) - 1`, so `R(S, S) = 0.9`.
- Stability, difficulty, same day review, and post lapse updates follow the FSRS-6 reference implementation exactly.
- v1 uses the published FSRS-6 default parameters (21 values, w0 to w20). Personal fitting is a later sub-project.
- Target retention: 0.90. The next interval is computed from target retention and stability.
- `Fsrs` is a pure function: `(MemoryState, Grade, elapsed_days, Parameters) -> MemoryState`. No clock access, no I/O.

### 6.2 Network layer

Built on the relationship graph. `A DependsOn B` means B is a prerequisite of A.

- **Solid item**: phase is `review` and `R(now) >= 0.80`.
- **Frontier**: a concept that is still `new` and whose prerequisites are all solid concept items. Concepts with no prerequisites are always on the frontier.
- **Schema boost**: when a new concept item gets its first review, its initial stability is multiplied by `1 + beta * mean R(now)` of its prerequisites (no prerequisites means no boost). `beta` defaults to 0.2 and is a named parameter so it can be tuned from the log.
- **Link introduction**: a link item becomes reviewable once both endpoint concepts have been introduced.
- **Contrast pairs**: concepts joined by `AlternativeTo` or `OppositeOf`. When one is selected as a session focus, its introduced partner is added as a focus too.
- **Leverage**: number of transitive dependents of a concept. Used only to order due items.

### 6.3 Model self check

`Calibration` computes, over any window of the log:

- **Learner calibration**: for each predicted level, the observed success rate (success means grade is not `again`). Overconfidence is the mean predicted probability minus the observed rate, using the mapping guess 0.25, unsure 0.50, fairly sure 0.75, certain 0.95.
- **Model calibration**: the predicted `R` at review time vs the observed outcome, bucketed in tenths, plus log loss. This shows whether the model fits the user. It is the input for later parameter fitting and for testing the schema boost.

## 7. Recall sessions

### 7.1 Planning

`SessionPlanner::plan(graph, states, now, limits) -> SessionPlan`

1. Collect due items: `R(now) < target retention`, introduced items only.
2. Group them into **focus concepts**: a concept item belongs to its concept; a link item belongs to its source concept (symmetric links to the endpoint with the lower recall chance).
3. Order focus concepts by the lowest `R` among their due items, then by leverage descending.
4. Add contrast partners next to their focus.
5. Add up to `limits.new_per_day` (default 5) frontier concepts, minus those already introduced today.
6. Interleave topics: no more than two consecutive focus concepts from the same topic when an alternative exists.
7. Cap the session at `limits.max_focus` (default 12). Remaining due items wait for the next session.
8. The plan includes an estimated duration from the median `response_ms` in the log (default 40 s per focus with no history).

### 7.2 Rebuild exercise (per focus concept)

1. The map centers on the focus concept at its saved position. All its introduced links are hidden. Other nodes stay in place.
2. The learner picks **How sure are you?** (guess, unsure, fairly sure, certain). One prediction covers all items in this focus.
3. The learner names connected concepts by typing. Suggestions appear only after three typed characters and match by prefix only.
4. For each named concept the learner sets the relationship type and direction.
5. **Hint** shows a faint marker at the saved position of one missing neighbor. Each hint is counted.
6. On submit, `RebuildGrader` produces one grade per hidden link item:

| Outcome | Grade |
|---|---|
| Correct target, type, and direction, no hint | good |
| Correct target, type, and direction, recalled fast and predicted certain | easy |
| Correct target after a hint, or correct target with wrong type or direction | hard |
| Missed | again |
| A concept named that is not linked | recorded as `wrong_target_id` on one missed item, see below |

   A wrongly named concept is attached to the missed item whose target is its contrast partner; otherwise to the missed item whose target shares a neighbor with it; otherwise it is not recorded. It never changes a grade on its own.
   "Fast" means under the learner's median `response_ms` for rebuild exercises. With fewer than 20 logged rebuilds, `easy` is never given.
7. Feedback appears immediately on the map: lime recalled, soft white recalled with hint, rose missed, dashed rose confused.
8. **Hypercorrection**: if the prediction was certain and any item got `again`, the focus returns once at the end of the session. That second attempt is logged with its own event.

### 7.3 Explain exercise (once per focus)

1. Prompt is chosen from the focus's weakest due item:
   - Link item: "Why does A depend on B?" (wording per relationship type).
   - Concept item: "What is A?" or "What problem does A solve?", alternating.
2. Learner predicts confidence, types an answer, then reveals the stored note, definition, or problem field.
3. Learner self grades: again, hard, good, easy.
4. If the stored text is empty, Atlas asks the learner to write it now. The item is still graded.

### 7.4 Session end

One screen:

- Items recalled.
- This session's calibration ("Certain 6 times, right 4").
- One research tip matched to what happened (for example overconfidence, many hints, or contrast confusion). Tips are short and cite their source.

The map then animates rings to their new values.

### 7.5 Persistence of a session

Each submit (one focus) writes its review events and resulting memory states in one transaction. A session interrupted midway keeps every completed focus.

## 8. Visual layer

### 8.1 Style: Graphite

- Neutral grays, one sharp accent, monospace labels, system sans for text.
- Dark default, light twin. Colors are tokens in one `Theme` QML singleton.

### 8.2 Memory rings

- Ring length is the recall chance `R(now)`.
- Color reinforces it: lime for 0.80 and above, soft white for 0.50 to 0.80, rose below 0.50, dashed gray for not yet introduced.
- Rings are thin and quiet. Exact values appear on hover.
- Zoomed out below a threshold, each topic collapses into one node with one ring showing the mean `R` of its introduced items.

### 8.3 Map

- Positions come from `node_placements`. Layout runs warm started from saved positions and never moves pinned nodes. New concepts are placed near their neighbors.
- Labels are visible at normal zoom, hidden when zoomed out.
- Directed types get a small arrowhead, symmetric types none. Contrast pairs are dashed. Hovering a link shows its type and note.
- Concepts from other topics linked to the current topic appear as faded ghost nodes at the edge.

### 8.4 Screens

- **Today**: session card ("14 items, about 8 min") over the map.
- **Map**: explore and edit concepts, links, topics.
- **Session**: the map becomes the exercise.
- **Concept panel**: slides out of the clicked node.
- **Settings**: theme, reduced motion, daily new item limit.

### 8.5 Motion system

One `Motion` QML singleton used by every component:

- Easing `cubic-bezier(0.22, 1, 0.36, 1)`.
- Durations: fast 140 ms, normal 240 ms, slow 420 ms, memory decay 1200 ms. Stagger 40 ms.
- Every animation encodes a change: rings fill on strengthening and drain on decay, a missed link pulses once, panels grow from their source node, layout changes ease nodes to new positions.
- Reduced motion replaces movement with short fades.

## 9. Architecture

Dependencies point inward to `atlas-core`.

```
atlas-core          domain types + ReviewEvent, MemoryState, ItemRef, Placement, Grade
atlas-persistence   + migration 3, ReviewEventRepository, MemoryStateRepository,
                      PlacementRepository
atlas-graph         unchanged API, cross topic edges allowed
atlas-learning      new, depends on core + graph only, no Qt, no SQLite
  Fsrs, FsrsParameters
  MemoryLedger      replay events into states, apply one event
  NetworkRules      frontier, schema boost, contrast pairs, leverage
  SessionPlanner
  RebuildGrader
  Calibration
atlas-render        ForceDirectedLayout gains initial positions and pinned set;
                    GraphCanvasItem gains rings, labels, arrowheads, ghost nodes
atlas-viewmodels    new, QObject controllers and list models registered with QML_ELEMENT
  WorkspaceController (moved from atlas-ui), SessionController, PlacementController
atlas-qml           new, qt_add_qml_module: Theme, Motion, components, screens,
                    compiled into the binary (removes the source tree QML path)
atlas-app           QQmlApplicationEngine composition root
atlas-ui            Qt Widgets, deleted once atlas-qml reaches feature parity
```

Rules:

- `atlas-learning` never reads the clock. `now` is always a parameter.
- The database is written first, in-memory state is updated only after a successful commit (existing invariant).
- Requires Qt 6.7 or newer (developed on 6.11).

### Data flow for one focus submit

1. QML calls `SessionController::submitRebuild(answer)`.
2. `RebuildGrader` returns grades.
3. `MemoryLedger` computes new states from current states and the new events.
4. One transaction appends events and upserts states.
5. On commit, in-memory states update and `memoryChanged` is emitted.
6. List models update, rings animate.

## 10. Error handling

- `Result<T, E>` at every fallible boundary, as today.
- Errors appear as a quiet inline message in the current screen, no modal dialogs.
- On startup, if `memory_meta.replay_version` differs from the code's version or `last_event_id` does not match the newest event, memory states are rebuilt from the log.
- Review timestamps later than now (wrong clock) are clamped to now. Negative elapsed days are clamped to 0.
- Event ids are UUIDs and every event has a `device_id`, so later sync only merges sets of events.

## 11. Testing

- **atlas-learning**: unit tests with a fixed clock. FSRS-6 outputs checked against reference values from the reference implementation. Grader, planner, network rules, and calibration covered case by case. Replaying the same log always yields identical states.
- **Simulated learner**: a learner with a known forgetting curve runs 90 simulated days. The test asserts retention stays near the target and the daily review count stays bounded.
- **atlas-persistence**: repository tests, migration 3 applied to a real version 2 database file, cascade deletes of events and states.
- **atlas-viewmodels**: headless C++ tests, replacing the current Widgets tests.
- **atlas-qml**: Qt Quick Test for the ring, canvas interactions, and the session flow.
- **Tooling**: `CMakePresets.json` with `dev`, `asan`, `release`; warnings as errors; CI runs build and `ctest` for `dev` and `asan`. Scale tests are labeled so quick runs can skip them.

## 12. Code rules

- Readable names, small units, almost no comments. A comment is used only when necessary and is one line at most.
- Design reasoning goes in `docs/DECISIONS.md` and commit messages, not in code.

## 13. Future sub-projects and the data they rely on

**Mind view** (brain model plus insights)

- Brain stages driven by memory phase and stability: new and learning items in the hippocampus, consolidating items flowing toward the cortex, stable items in the cortex. Calibration drives the prefrontal region. Labeled as a research based model, not a brain scan.
- Cortex meaning map: the global placement layout projected onto the cortex surface, related concepts near each other, strength shown by glow (after Huth et al. 2016).
- Insights: forgetting curves, predicted vs actual retention, calibration chart, consistency heatmap.
- Diagnoses from the log:

| Signal | Diagnosis | Suggestion |
|---|---|---|
| Failures where prerequisites are also weak | Prerequisite gap | Strengthen the prerequisite first |
| `wrong_target_id` set repeatedly for a pair | Interference | Practice the pair together |
| Failures concentrated after long overdue gaps | Broken spacing | Shorter, more frequent sessions |
| `again` with predicted certain | Overconfidence | Predict before every reveal |
| Many reviews in one day, then none | Cramming | Spread reviews out |
| Repeated failures on concepts with few links or empty notes | Shallow encoding | Write the why, connect it to more concepts |

All required fields exist in `review_events` and `node_placements` from this spec.

**Starter map**: a "How Learning Works" topic shipped as an importable map, learned with Atlas itself.

**Phone app and sync**: exchanges review events and placements by id; memory states are rebuilt locally.

**Personal parameters**: fit FSRS-6 parameters and `beta` from the user's log once enough reviews exist, using the model calibration from 6.3 to confirm the fit improves predictions.

## 14. Rollout order

1. Tooling: presets, CI, warnings as errors.
2. Fixes: cross topic links, warm started layout with saved placements.
3. `atlas-learning` with full tests and the simulated learner.
4. Persistence: migration 3 and repositories.
5. `atlas-viewmodels`: move `WorkspaceController`, add `SessionController`, `PlacementController`.
6. `atlas-qml` shell: Theme, Motion, Today and Map screens, canvas rings and labels.
7. Session screens: rebuild, explain, summary.
8. Parity check, delete `atlas-ui`, update README and roadmap to match this spec.

## 15. References

- Anderson, M. C., Bjork, R. A., & Bjork, E. L. (1994). Remembering can cause forgetting. *JEP: LMC*, 20(5).
- Bjork, R. A. (1994). Memory and metamemory considerations in the training of human beings. In *Metacognition: Knowing about Knowing*. MIT Press.
- Bjork, R. A., & Bjork, E. L. (1992). A new theory of disuse and an old theory of stimulus fluctuation. In *From Learning Processes to Cognitive Processes*.
- Blunt, J. R., & Karpicke, J. D. (2014). Learning with retrieval-based concept mapping. *Journal of Educational Psychology*, 106(3).
- Butterfield, B., & Metcalfe, J. (2001). Errors committed with high confidence are hypercorrected. *JEP: LMC*, 27(6).
- Cepeda, N. J., et al. (2006). Distributed practice in verbal recall tasks. *Psychological Bulletin*, 132(3).
- Chase, W. G., & Simon, H. A. (1973). Perception in chess. *Cognitive Psychology*, 4(1).
- Chi, M. T. H., et al. (1994). Eliciting self-explanations improves understanding. *Cognitive Science*, 18(3).
- Collins, A. M., & Loftus, E. F. (1975). A spreading-activation theory of semantic processing. *Psychological Review*, 82(6).
- Craik, F. I. M., & Lockhart, R. S. (1972). Levels of processing. *Journal of Verbal Learning and Verbal Behavior*, 11(6).
- Diekelmann, S., & Born, J. (2010). The memory function of sleep. *Nature Reviews Neuroscience*, 11(2).
- Doignon, J.-P., & Falmagne, J.-C. (1985). Spaces for the assessment of knowledge. *International Journal of Man-Machine Studies*, 23(2).
- Dunlosky, J., et al. (2013). Improving students' learning with effective learning techniques. *Psychological Science in the Public Interest*, 14(1).
- Fleming, S. M., et al. (2010). Relating introspective accuracy to individual differences in brain structure. *Science*, 329(5998).
- Huth, A. G., et al. (2016). Natural speech reveals the semantic maps that tile human cerebral cortex. *Nature*, 532(7600).
- Karpicke, J. D., & Blunt, J. R. (2011). Retrieval practice produces more learning than elaborative studying with concept mapping. *Science*, 331(6018).
- Koriat, A., & Bjork, R. A. (2005). Illusions of competence in monitoring one's knowledge during study. *JEP: LMC*, 31(2).
- Mayer, R. E. (2009). *Multimedia Learning* (2nd ed.). Cambridge University Press.
- McClelland, J. L., McNaughton, B. L., & O'Reilly, R. C. (1995). Why there are complementary learning systems in the hippocampus and neocortex. *Psychological Review*, 102(3).
- Nelson, T. O., & Dunlosky, J. (1991). When people's judgments of learning are extremely accurate at predicting subsequent recall. *Psychological Science*, 2(4).
- Nesbit, J. C., & Adesope, O. O. (2006). Learning with concept and knowledge maps: A meta-analysis. *Review of Educational Research*, 76(3).
- Pashler, H., et al. (2008). Learning styles: Concepts and evidence. *Psychological Science in the Public Interest*, 9(3).
- Roediger, H. L., & Karpicke, J. D. (2006). Test-enhanced learning. *Psychological Science*, 17(3).
- Rohrer, D., & Taylor, K. (2007). The shuffling of mathematics problems improves learning. *Instructional Science*, 35(6).
- Slamecka, N. J., & Graf, P. (1978). The generation effect. *JEP: Human Learning and Memory*, 4(6).
- Tse, D., et al. (2007). Schemas and memory consolidation. *Science*, 316(5821).
- Tversky, B., Morrison, J. B., & Betrancourt, M. (2002). Animation: Can it facilitate? *International Journal of Human-Computer Studies*, 57(4).
- van Kesteren, M. T. R., et al. (2012). How schema and novelty augment memory formation. *Trends in Neurosciences*, 35(4).
- FSRS-6 algorithm: open-spaced-repetition project, "The Algorithm" wiki and reference implementations.
