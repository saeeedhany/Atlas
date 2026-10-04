# Atlas

Atlas helps you keep what you learn. You build a map of concepts and the links between them, then Atlas asks you to recall that map at the right moments, so knowledge sticks instead of fading.

## How it works

- **A map of meaning.** Every concept has a place on one shared map, with topics as regions. Links are typed (depends on, uses, part of, alternative to, and more). Related ideas sit near each other, so the map itself becomes something you remember.
- **Recall, not rereading.** A session hides a concept's links and asks you to rebuild them, then to explain the concept in your own words. Retrieval strengthens memory far more than review (Roediger & Karpicke 2006).
- **Spacing.** Each concept and each link has its own memory model (FSRS-6). Atlas schedules a review when the chance of recall drops to 90 percent, so effort goes where forgetting is about to happen (Cepeda et al. 2006).
- **Prerequisites first.** New concepts are introduced only after what they depend on is solid, and well known prerequisites make new concepts stick faster.
- **Calibration.** Before each answer you say how sure you are. The summary shows how often you were right at each level, and confident mistakes come back at the end of the session (Butterfield & Metcalfe 2001).
- **Rings.** Every concept wears a ring whose length is its current recall chance: lime when strong, soft white when fading, rose when weak, dashed when not learned yet.

The full design, with the research behind each rule, is in `docs/specs/2026-10-04-atlas-learning-design.md`. Design decisions are logged in `docs/DECISIONS.md`.

## Using it

- **Today** shows how many items are waiting and about how long they take. Start a session from there.
- **Map** is where you add concepts, links, and topics, search, and open a concept's panel to edit it. The panel also lists what to learn first; the topic bar offers project ideas.
- **Settings** has the theme, reduced motion, and how many new concepts to introduce per day.

## Building

Requirements: CMake 3.21+, a C++20 compiler, Qt 6.7 or newer (Quick and Quick Controls), SQLite 3.

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/modules/app/atlas
```

Presets: `dev` (warnings as errors), `asan` (AddressSanitizer and UndefinedBehaviorSanitizer), `release`. Scale tests carry the `scale` label: `ctest --preset dev -LE scale` skips them.

Your data lives in `~/.local/share/atlas_app/atlas.db` (the platform's app data folder elsewhere). To try Atlas without touching it, run with a temporary data folder: `XDG_DATA_HOME=$(mktemp -d) ./build/dev/modules/app/atlas`.

## Project layout

| Module | Role |
|---|---|
| `atlas-core` | Domain types: concepts, links, topics, review events, memory states |
| `atlas-persistence` | SQLite storage and migrations |
| `atlas-graph` | The concept graph and its queries |
| `atlas-learning` | FSRS-6, replaying the review log, network rules, session planning, grading, calibration. No Qt, no clock access |
| `atlas-render` | Layout and the map canvas |
| `atlas-viewmodels` | Controllers and models that QML uses |
| `atlas-qml` | The Qt Quick interface |
| `atlas-app` | The `atlas` executable |

## Coming later

A mind view (memory stages and a meaning map, with insight dashboards), a starter map about how learning works, a phone app for daily recall with sync, and memory parameters fitted to your own history.
