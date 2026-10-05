# Atlas Workspace and Identity Design

Date: 2026-10-05. Status: approved in conversation, pending written review.

This spec is part 1 and 2 of five: the design system and the workspace shell. Parts 3 to 5 (brain view, insights panels, onboarding) get their own specs and plug into the panel system defined here. It replaces section 8.1 (Graphite style) of `2026-10-04-atlas-learning-design.md`; everything else in that spec stays in force.

## 1. Goals

- Atlas gets a recognizable identity: calm, matte, warm, premium. No glass, no glow, no neon, nothing shiny.
- The app behaves like a pinboard workspace: one board you work on, with panels that dock, float, fold, and grow like windows.
- Topics, concepts, links, and sticky notes all live on one board. The board is the main surface, never a page you leave.
- Brain and insight panels (later parts) have a home and a behavior defined now.

Success: a first-time viewer can name the look in a few words, every panel behaves the same way, and the board stays smooth at 10,000 concepts.

## 2. Design system

Material 3 structure (tonal surfaces, color roles, shape scale, one motion system), with Atlas's own palette and type.

### 2.1 Color: warm stone and clay

Roles follow Material naming. Dark is the default; light is warm paper.

| Role | Dark | Light |
|---|---|---|
| background | `#1d1c1a` | `#f6f1e7` |
| surface (panels, cards) | `#262420` | `#fffaf2` |
| surfaceHigh (hover, raised controls) | `#2e2b26` | `#efe7da` |
| outline | `#34312b` | `#e6dccb` |
| outlineStrong | `#4b463d` | `#d3c6b1` |
| onSurface (text) | `#e2ddd3` | `#2b2722` |
| onSurfaceMuted | `#a39c8f` | `#5e574c` |
| onSurfaceFaint | `#8c8578` | `#8a8070` |
| primary (clay) | `#d29b6c` | `#b8784a` |
| onPrimary | `#1d1c1a` | `#fffaf2` |
| secondary (olive) | `#a3b18a` | `#7d8c62` |
| tertiary (terracotta) | `#c4806b` | `#a8583f` |
| danger | `#d0705f` | `#b3483a` |
| link | `#57524a` | `#c9bda8` |
| nodeFill | `#c9c2b4` | `#6b6357` |

Memory rings: strong (recall 0.80 and above) = secondary, fading (0.50 to 0.80) = nodeFill, weak (below 0.50) = tertiary, not learned = dashed link color, track = outline.

Region tints: each topic gets one of six hues (clay, olive, terracotta, sand `#cbb489`/`#9c8550`, sage `#8fa89a`/`#5f7a6b`, mauve `#b394a0`/`#86677a`), drawn at 8 percent fill with a 20 percent outline. The hue is chosen from the topic id, so it is stable.

Sticky note colors: clay, olive, rose. Dark fills `#2e2a23`, `#262b22`, `#2f2422` with text `#ead9bf`, `#d3dcc0`, `#ecc9bf`; light fills `#f3e2cc`, `#e4ead6`, `#f1d9d0` with text `#4a3622`, `#3b4628`, `#5a2f24`.

### 2.2 Type

- Titles and big numbers: Newsreader (serif, weight 500).
- Text and controls: Inter (400, 500).
- Labels, counts, metadata: IBM Plex Mono (400).
- All three are SIL Open Font License fonts, bundled in the app resources and registered at startup, so the look never depends on installed fonts.
- Scale: display 32, headline 24, title 17, body 13, label 11, caption 10.

### 2.3 Shape and depth

- Corners: panels and cards 16, controls 12, chips 8, topic regions 28.
- Flat and matte: surfaces are separated by tone and a 1 px outline. Only floating panels and dragged items get one soft shadow (y 12, blur 28, black at 35 percent dark, 12 percent light). No gradients, glows, or blur.

### 2.4 Nodes and links

- A concept is a small filled dot (nodeFill) with a thin memory ring around it, like Obsidian's graph. Size grows slightly with the number of links.
- Links are 1 px in the link color; directed types keep a small arrowhead; contrast pairs are dashed.
- Labels use Inter caption size in onSurfaceMuted, hidden when zoomed out (existing rule).

### 2.5 Motion

The existing `Motion` system stays: one curve `cubic-bezier(0.22, 1, 0.36, 1)`, fast 140, normal 240, slow 420, decay 1200, stagger 40; reduced motion turns movement into 120 ms fades. Panels and cards grow from the element that opened them; folding and pinning back animate along the same curve.

## 3. Workspace

### 3.1 Layout

- The board fills the window.
- A slim top bar (height 44): the Atlas wordmark in Newsreader, a topic jump list, a search or add field, and the Recall button (primary) with the count of items waiting.
- The corner stack is pinned at the top right under the top bar. In this part it holds Today; the brain and insights panels join it in parts 3 and 4.
- The left rail goes away. Settings opens from a small menu next to the wordmark.

### 3.2 Panels

One `Panel` component gives every panel the same behavior:

| State | Behavior |
|---|---|
| docked | Sits in the corner stack at its slot. |
| folded | Shows only its title bar; one click unfolds it. |
| floating | Dragged out of the stack by its title bar; free position and size on top of the board; one soft shadow. |
| expanded | Grows into a large view over the board (board dimmed behind), then returns to where it came from. |

- Title bar: title in Newsreader, then fold, expand, and (when floating) pin back.
- Floating panels stay in screen space (they do not pan or zoom with the board).
- Panel state and floating geometry are saved per panel id in the settings file and restored at start; a geometry outside the current window is moved back inside.
- Escape closes an expanded panel.

### 3.3 Topics as regions

- Every topic is a tinted rounded region drawn around its concepts with 48 px padding (world units), so it follows its concepts.
- The topic name (Newsreader) and a caption (`12 concepts · 78%`, mono) sit at the top left of the region.
- Clicking the name zooms the camera to fit the region. Dragging the name moves every concept of the topic together and saves their placements.
- Below the collapse zoom the existing topic bubbles take over.
- Uncategorized concepts get no region.

### 3.4 Concept cards

- Clicking a concept unfolds a card beside it (the concept panel content in the new style). The card follows the node as the board pans and zooms.
- A card can be dragged off its node (it then stays in screen space), expanded to a full view, or pinned so that opening another concept opens a second card instead of replacing it. At most four cards are open.
- Unsaved edits follow the existing rules: switching or closing saves; a failed save keeps the card open with a toast.

### 3.5 Sticky notes

- Created from the add field ("note: ...") or a double click on empty board space.
- Free text, three colors, positioned and sized in world units, so they pan and zoom with the board.
- A note can be linked to concepts or topics by dragging from its edge handle onto them; links draw as thin dashed lines in the note's color. Notes are never quizzed and never affect memory.
- Deleting a concept or topic removes links to it; deleting a note removes its links.

### 3.6 Sessions, Today, Settings

Behavior is unchanged from the learning design; only the look moves to this system. Today becomes the first docked panel. The session card keeps its place at the bottom of the board.

## 4. Data model

Migration 4:

```
board_notes
  id          TEXT PRIMARY KEY   uuid
  body        TEXT NOT NULL
  color       TEXT NOT NULL      clay | olive | rose
  x, y, width, height  REAL NOT NULL   world units
  created_at  INTEGER NOT NULL
  updated_at  INTEGER NOT NULL

note_links
  note_id     TEXT NOT NULL REFERENCES board_notes(id) ON DELETE CASCADE
  target_kind TEXT NOT NULL      concept | topic
  target_id   TEXT NOT NULL
  PRIMARY KEY (note_id, target_kind, target_id)
```

Triggers remove `note_links` rows when the target concept or topic is deleted. Panel layout lives in `settings.ini` (it is a view preference, not knowledge).

## 5. Architecture

- `atlas-render`: the canvas also draws topic regions (rounded hull per topic, tinted) beneath links and nodes; `Theme` gains the new roles. Region shapes are computed by a pure function from member positions, testable without Qt Quick.
- `atlas-persistence`: migration 4, `NoteRepository`.
- `atlas-viewmodels`: `NotesModel` (list model of notes with create, edit, move, resize, recolor, link, unlink, remove); `Palette` exposes the new roles; `MapViewModel` exposes topic regions (id, name, caption, world rect) for the QML labels and moves a topic's concepts when its name is dragged.
- `atlas-qml`: `Panel`, `CornerStack`, `TopBar`, `ConceptCard`, `StickyNote`, `RegionLabel`, a `BoardLayer` that maps world to screen for notes and cards, and the restyled Today, session, and settings content. Fonts load in `main_qml.cpp` before the engine.
- Panels are registered by id in the corner stack, so parts 3 and 4 add `brain` and `insights` panels without changing the shell.

## 6. Errors

- All failures show in the existing toast. A failed save never changes what the board shows.
- A corrupt or missing panel layout falls back to the default (everything docked).
- A note linked to something that no longer exists simply has no line; the trigger removes the row.

## 7. Testing

- Migration 4 on a fresh database and on a copy of a version 3 database; cascade and trigger behavior.
- `NoteRepository` and `NotesModel`: create, edit, move, resize, recolor, link, unlink, remove, and links vanishing with a deleted concept or topic.
- Region shape function: padding, single member, overlapping topics, empty topic.
- Panel: every state transition, saved and restored layout, off-screen geometry pulled back, Escape closes expanded.
- Concept cards: open, follow node, detach, pin (max four), save on switch.
- Zero QML warnings in every test; screenshots of the board, an expanded panel, cards, notes, and a session in both themes.

## 8. Out of scope here

Brain view (part 3), insights panels and diagnoses (part 4), onboarding (part 5), region resizing by edge, note formatting beyond plain text, phone app and sync.
