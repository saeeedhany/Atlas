# Workspace and Identity Implementation Plan

**Goal:** Give Atlas its warm stone and clay identity and turn it into a pinboard workspace: one board with topic regions, sticky notes, concept cards beside their nodes, and a corner stack of panels that dock, fold, float, and expand.

**Architecture:** Design tokens live in the C++ `Theme`/`Palette` and the QML `Theme` singleton with Material role names; three OFL fonts ship in the QML module. The C++ canvas keeps drawing nodes and links and now also draws tinted topic regions; notes, cards, region labels, and panels are QML items that follow the board camera through new world/screen mapping functions. Notes persist in two new tables (migration 4) behind `NoteRepository` and a `Notes` QML singleton; panel layout persists in `settings.ini`.

**Tech Stack:** C++20, Qt 6.11 (Quick, QuickControls2 Basic, QuickShapes), QML, SQLite, doctest.

**Spec:** `docs/specs/2026-10-05-workspace-identity-design.md` (with `docs/specs/2026-10-04-atlas-learning-design.md` still in force outside its section 8.1)

## Global Constraints

- C++20, `CMAKE_CXX_EXTENSIONS OFF`. Every target builds with `-Wall -Wextra -Wpedantic -Werror` and passes under ASan + UBSan (`asan` preset).
- Requires Qt 6.7 or newer (developed on 6.11).
- Look: calm, matte, warm. No glass, no glow, no gradients, no blur. Surfaces separate by tone and a 1 px outline; only floating panels and dragged items get one soft shadow.
- Colors come only from `Theme` (QML) or `themeFor()` (C++), using the exact values in spec section 2.1.
- Fonts: Newsreader for titles and big numbers, Inter for text and controls, IBM Plex Mono for labels and numbers, all bundled. Scale: display 32, headline 24, title 17, body 13, label 11, caption 10.
- Corners: panels and cards 16, controls 12, chips 8, regions 28.
- Motion: existing `Motion` singleton only; movement `Behavior`s carry `enabled: !Motion.reduced`.
- Every QML component loads with zero QML warnings; tests fail on any warning.
- The self-rated Confidence field is never shown. Errors appear as a toast, never a modal dialog. A failed save never changes what the board shows.
- Code style: readable names, small units. Comments only when necessary, one line maximum.
- Never write the long dash character anywhere (code, QML, docs, commits). Use "-" only when needed.
- Code, docs, and commits describe only the product.
- Commit messages have no co-author or generated-by trailers.

## Review Focus

1. A floating panel saved at a position outside a smaller window is pulled back inside on restore. Test in Task 6.
2. Deleting a concept or topic that a note links to removes the link and keeps the note. Tests in Tasks 4 and 5.
3. Dragging a topic moves all its concepts, including pinned ones, and the move survives a restart. Test in Task 3.
4. A concept card near the right edge of the window opens on the left of its node and never leaves the window. Test in Task 8.
5. Zooming never changes a note's stored world position, and its screen box scales with the zoom. Test in Task 9.

## File Map

```
modules/qml/fonts/                      Inter.ttf, Newsreader.ttf, IBMPlexMono-Regular.ttf, OFL-*.txt
modules/qml/include/atlas/ui/fonts.hpp  loadBundledFonts()
modules/qml/src/fonts.cpp
modules/render/                         theme roles; node style; RenderRegion; region geometry; mapping and hit helpers
modules/viewmodels/                     Palette roles; MapViewModel regions + moveTopic; PlacementController::moveBy;
                                        AppSettings panel layout; NotesModel; AppContext wiring
modules/persistence/                    migration 4; NoteRepository
modules/qml/                            Theme.qml, Panel.qml, PanelLayer.qml, TopBar.qml, TodayPanel.qml,
                                        RegionLabels.qml, ConceptCard.qml, PinnedCard.qml, NotesLayer.qml,
                                        StickyNote.qml, Main.qml (rewritten), MapOverlay.qml, TopicBar.qml,
                                        SettingsScreen.qml; NavRail.qml and TodayOverlay.qml deleted
modules/app/src/main_qml.cpp            loads fonts
docs/DECISIONS.md
```

---

### Task 1: Tokens and fonts

**Files:**
- Create: `modules/qml/fonts/` (three fonts and their licenses), `modules/qml/include/atlas/ui/fonts.hpp`, `modules/qml/src/fonts.cpp`
- Modify: `modules/render/include/atlas/render/theme.hpp`, `modules/render/src/theme.cpp`
- Modify: `modules/viewmodels/include/atlas/viewmodels/palette.hpp`, `src/palette.cpp`
- Modify: `modules/qml/Theme.qml`, `modules/qml/CMakeLists.txt`, `modules/qml/Main.qml` (one line), `modules/qml/tests/test_main.cpp`, `modules/app/src/main_qml.cpp`
- Test: `modules/render/tests/test_canvas_geometry.cpp` (theme case), `modules/viewmodels/tests/test_appearance.cpp`, `modules/qml/tests/test_foundation.cpp`

**Interfaces:**
- Produces:
  - `atlas::render::Theme` gains `outlineStrong`, `textFaint`, `secondary`, `tertiary`, `onPrimary`, `link`, `std::array<QColor, 6> regionHues`, `std::array<QColor, 3> noteFills`, `std::array<QColor, 3> noteTexts`. Existing fields keep their names and take the new palette: `accent` = primary, `surfaceRaised` = surfaceHigh, `border` = outline, `text` = onSurface, `textMuted` = onSurfaceMuted, `edge` = link.
  - `Palette` Q_PROPERTYs `outlineStrong`, `textFaint`, `secondary`, `tertiary`, `onPrimary`, `link`; Q_INVOKABLEs `QColor regionTint(int hue) const`, `QColor noteFill(const QString& color) const`, `QColor noteText(const QString& color) const` (color is `clay`, `olive`, or `rose`; unknown falls back to clay).
  - QML `Theme` roles: `background`, `surface`, `surfaceHigh`, `outline`, `outlineStrong`, `onSurface`, `onSurfaceMuted`, `onSurfaceFaint`, `primary`, `onPrimary`, `secondary`, `tertiary`, `danger`, `link`, `nodeFill`, ring colors; aliases kept for existing components: `surfaceRaised`, `border`, `text`, `textMuted`, `accent`. Fonts `serif` ("Newsreader"), `sans` ("Inter"), `mono` ("IBM Plex Mono"). Sizes `fontCaption` 10, `fontSmall` 11, `fontBody` 13, `fontTitle` 17, `fontHeadline` 24, `fontHero` 32. Shape `radius` 12, `radiusPanel` 16, `radiusChip` 8, `radiusRegion` 28; `gap` 8. Functions `ringColor`, `regionTint(hue)`, `noteFill(name)`, `noteText(name)`.
  - `QStringList atlas::ui::loadBundledFonts()` registers the three fonts from the module resources and returns the family names it registered.

- [ ] **Step 1: Fetch the fonts**

```bash
mkdir -p modules/qml/fonts
base=https://github.com/google/fonts/raw/main/ofl
curl -fL -o modules/qml/fonts/Inter.ttf "$base/inter/Inter%5Bopsz,wght%5D.ttf"
curl -fL -o modules/qml/fonts/OFL-Inter.txt "$base/inter/OFL.txt"
curl -fL -o modules/qml/fonts/Newsreader.ttf "$base/newsreader/Newsreader%5Bopsz,wght%5D.ttf"
curl -fL -o modules/qml/fonts/OFL-Newsreader.txt "$base/newsreader/OFL.txt"
curl -fL -o modules/qml/fonts/IBMPlexMono-Regular.ttf "$base/ibmplexmono/IBMPlexMono-Regular.ttf"
curl -fL -o modules/qml/fonts/OFL-IBMPlexMono.txt "$base/ibmplexmono/OFL.txt"
file modules/qml/fonts/*.ttf
```

Expected: three `TrueType Font data` files.

- [ ] **Step 2: Write the failing tests**

Append to `modules/viewmodels/tests/test_appearance.cpp` (it already builds `AppSettings` and `Palette` from a temporary `QSettings`; reuse that setup):

```cpp
TEST_CASE("the palette carries warm stone and clay in both themes") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings settings(store);
    Palette palette(settings);
    CHECK(palette.background() == QColor("#1d1c1a"));
    CHECK(palette.accent() == QColor("#d29b6c"));
    CHECK(palette.secondary() == QColor("#a3b18a"));
    CHECK(palette.tertiary() == QColor("#c4806b"));
    CHECK(palette.link() == QColor("#57524a"));
    CHECK(palette.noteFill("olive") == QColor("#262b22"));
    CHECK(palette.noteFill("unknown") == palette.noteFill("clay"));
    CHECK(palette.regionTint(1) == QColor("#a3b18a"));
    settings.setDarkTheme(false);
    CHECK(palette.background() == QColor("#f6f1e7"));
    CHECK(palette.accent() == QColor("#b8784a"));
    CHECK(palette.noteText("rose") == QColor("#5a2f24"));
}
```

Append to `modules/qml/tests/test_foundation.cpp`:

```cpp
TEST_CASE("Theme exposes Material roles, aliases, and bundled fonts") {
    QmlFixture f;
    auto probe = f.createFromData("import QtQuick\nimport Atlas.Ui\n"
                                  "QtObject { property color primary: Theme.primary\n"
                                  "           property color accent: Theme.accent\n"
                                  "           property color outline: Theme.outline\n"
                                  "           property string serif: Theme.serif\n"
                                  "           property int hero: Theme.fontHero\n"
                                  "           property color note: Theme.noteFill(\"clay\") }");
    CHECK(probe->property("primary").value<QColor>() == f.context().palette().accent());
    CHECK(probe->property("accent").value<QColor>() == probe->property("primary").value<QColor>());
    CHECK(probe->property("outline").value<QColor>() == f.context().palette().border());
    CHECK(probe->property("serif").toString() == "Newsreader");
    CHECK(probe->property("hero").toInt() == 32);
    CHECK(probe->property("note").value<QColor>() == QColor("#2e2a23"));
}

TEST_CASE("the three bundled fonts are registered") {
    auto families = QFontDatabase::families();
    CHECK(families.contains("Inter"));
    CHECK(families.contains("Newsreader"));
    CHECK(families.contains("IBM Plex Mono"));
}
```

Add `#include <QFontDatabase>` to that file. In `modules/qml/tests/test_main.cpp`, include `"atlas/ui/fonts.hpp"` and call `atlas::ui::loadBundledFonts();` right after `registerGraphCanvasQmlType()`.

- [ ] **Step 3: Run to verify they fail**

Run: `cmake --build --preset dev`
Expected: FAIL, `secondary`, `noteFill`, and `atlas/ui/fonts.hpp` do not exist.

- [ ] **Step 4: Update the C++ theme**

In `theme.hpp` add `#include <array>` and these fields to `Theme` after `danger`:

```cpp
    QColor outlineStrong;
    QColor textFaint;
    QColor secondary;
    QColor tertiary;
    QColor onPrimary;
    QColor link;
    std::array<QColor, 6> regionHues{};
    std::array<QColor, 3> noteFills{};
    std::array<QColor, 3> noteTexts{};
```

Replace `makeDark()` and `makeLight()` in `theme.cpp`:

```cpp
Theme makeDark() {
    Theme theme;
    theme.background = QColor(0x1d1c1a);
    theme.dot = withAlpha(0xe2ddd3, 10);
    theme.edge = QColor(0x57524a);
    theme.edgeDimmed = withAlpha(0x57524a, 90);
    theme.selectedRing = QColor(0xe2ddd3);
    theme.neighborRing = withAlpha(0xa39c8f, 160);
    theme.hoverRing = withAlpha(0xe2ddd3, 110);
    theme.nodeBorder = QColor(0x1d1c1a);
    theme.accent = QColor(0xd29b6c);
    theme.surface = QColor(0x262420);
    theme.surfaceRaised = QColor(0x2e2b26);
    theme.border = QColor(0x34312b);
    theme.text = QColor(0xe2ddd3);
    theme.textMuted = QColor(0xa39c8f);
    theme.nodeFill = QColor(0xc9c2b4);
    theme.label = QColor(0xa39c8f);
    theme.ringStrong = QColor(0xa3b18a);
    theme.ringMedium = QColor(0xc9c2b4);
    theme.ringWeak = QColor(0xc4806b);
    theme.ringNew = QColor(0x57524a);
    theme.ringTrack = QColor(0x34312b);
    theme.danger = QColor(0xd0705f);
    theme.outlineStrong = QColor(0x4b463d);
    theme.textFaint = QColor(0x8c8578);
    theme.secondary = QColor(0xa3b18a);
    theme.tertiary = QColor(0xc4806b);
    theme.onPrimary = QColor(0x1d1c1a);
    theme.link = QColor(0x57524a);
    theme.regionHues = {QColor(0xd29b6c), QColor(0xa3b18a), QColor(0xc4806b),
                        QColor(0xcbb489), QColor(0x8fa89a), QColor(0xb394a0)};
    theme.noteFills = {QColor(0x2e2a23), QColor(0x262b22), QColor(0x2f2422)};
    theme.noteTexts = {QColor(0xead9bf), QColor(0xd3dcc0), QColor(0xecc9bf)};
    return theme;
}

Theme makeLight() {
    Theme theme;
    theme.background = QColor(0xf6f1e7);
    theme.dot = withAlpha(0x2b2722, 14);
    theme.edge = QColor(0xc9bda8);
    theme.edgeDimmed = withAlpha(0xc9bda8, 90);
    theme.selectedRing = QColor(0x2b2722);
    theme.neighborRing = withAlpha(0x5e574c, 160);
    theme.hoverRing = withAlpha(0x2b2722, 110);
    theme.nodeBorder = QColor(0xf6f1e7);
    theme.accent = QColor(0xb8784a);
    theme.surface = QColor(0xfffaf2);
    theme.surfaceRaised = QColor(0xefe7da);
    theme.border = QColor(0xe6dccb);
    theme.text = QColor(0x2b2722);
    theme.textMuted = QColor(0x5e574c);
    theme.nodeFill = QColor(0x6b6357);
    theme.label = QColor(0x5e574c);
    theme.ringStrong = QColor(0x7d8c62);
    theme.ringMedium = QColor(0x8a8070);
    theme.ringWeak = QColor(0xa8583f);
    theme.ringNew = QColor(0xc9bda8);
    theme.ringTrack = QColor(0xe6dccb);
    theme.danger = QColor(0xb3483a);
    theme.outlineStrong = QColor(0xd3c6b1);
    theme.textFaint = QColor(0x8a8070);
    theme.secondary = QColor(0x7d8c62);
    theme.tertiary = QColor(0xa8583f);
    theme.onPrimary = QColor(0xfffaf2);
    theme.link = QColor(0xc9bda8);
    theme.regionHues = {QColor(0xb8784a), QColor(0x7d8c62), QColor(0xa8583f),
                        QColor(0x9c8550), QColor(0x5f7a6b), QColor(0x86677a)};
    theme.noteFills = {QColor(0xf3e2cc), QColor(0xe4ead6), QColor(0xf1d9d0)};
    theme.noteTexts = {QColor(0x4a3622), QColor(0x3b4628), QColor(0x5a2f24)};
    return theme;
}
```

- [ ] **Step 5: Extend the palette**

`palette.hpp`, next to the existing properties:

```cpp
    Q_PROPERTY(QColor outlineStrong READ outlineStrong NOTIFY changed)
    Q_PROPERTY(QColor textFaint READ textFaint NOTIFY changed)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY changed)
    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY changed)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY changed)
    Q_PROPERTY(QColor link READ link NOTIFY changed)
```

and in the public section:

```cpp
    QColor outlineStrong() const { return theme().outlineStrong; }
    QColor textFaint() const { return theme().textFaint; }
    QColor secondary() const { return theme().secondary; }
    QColor tertiary() const { return theme().tertiary; }
    QColor onPrimary() const { return theme().onPrimary; }
    QColor link() const { return theme().link; }
    Q_INVOKABLE QColor regionTint(int hue) const;
    Q_INVOKABLE QColor noteFill(const QString& color) const;
    Q_INVOKABLE QColor noteText(const QString& color) const;
```

`palette.cpp`:

```cpp
namespace {

size_t noteIndex(const QString& color) {
    if (color == QLatin1String("olive")) return 1;
    if (color == QLatin1String("rose")) return 2;
    return 0;
}

}  // namespace

QColor Palette::regionTint(int hue) const {
    const auto& hues = theme().regionHues;
    return hues[static_cast<size_t>(((hue % 6) + 6) % 6)];
}

QColor Palette::noteFill(const QString& color) const { return theme().noteFills[noteIndex(color)]; }
QColor Palette::noteText(const QString& color) const { return theme().noteTexts[noteIndex(color)]; }
```

Place the anonymous namespace before the existing functions in that file and keep everything inside `namespace atlas::viewmodels`.

- [ ] **Step 6: Bundle and load the fonts**

`modules/qml/include/atlas/ui/fonts.hpp`:

```cpp
#pragma once

#include <QStringList>

namespace atlas::ui {

QStringList loadBundledFonts();

}  // namespace atlas::ui
```

`modules/qml/src/fonts.cpp`:

```cpp
#include "atlas/ui/fonts.hpp"

#include <QFontDatabase>

namespace atlas::ui {

QStringList loadBundledFonts() {
    QStringList families;
    for (const char* file : {"Inter.ttf", "Newsreader.ttf", "IBMPlexMono-Regular.ttf"}) {
        int id = QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Atlas/Ui/fonts/") + file);
        if (id >= 0) families.append(QFontDatabase::applicationFontFamilies(id));
    }
    return families;
}

}  // namespace atlas::ui
```

`modules/qml/CMakeLists.txt`: change the library line to `qt_add_library(atlas_qml STATIC src/fonts.cpp)`, add after `qt_add_qml_module(...)`'s `QML_FILES` list a `RESOURCES` list with `fonts/Inter.ttf fonts/Newsreader.ttf fonts/IBMPlexMono-Regular.ttf`, and add:

```cmake
target_include_directories(atlas_qml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(atlas_qml PUBLIC Qt6::Gui)
```

`main_qml.cpp`: include `"atlas/ui/fonts.hpp"` and call `atlas::ui::loadBundledFonts();` after `registerGraphCanvasQmlType()`.

- [ ] **Step 7: Rewrite `Theme.qml`**

```qml
pragma Singleton
import QtQuick
import Atlas.ViewModels

QtObject {
    readonly property color background: Palette.background
    readonly property color surface: Palette.surface
    readonly property color surfaceHigh: Palette.surfaceRaised
    readonly property color outline: Palette.border
    readonly property color outlineStrong: Palette.outlineStrong
    readonly property color onSurface: Palette.text
    readonly property color onSurfaceMuted: Palette.textMuted
    readonly property color onSurfaceFaint: Palette.textFaint
    readonly property color primary: Palette.accent
    readonly property color onPrimary: Palette.onPrimary
    readonly property color secondary: Palette.secondary
    readonly property color tertiary: Palette.tertiary
    readonly property color danger: Palette.danger
    readonly property color link: Palette.link
    readonly property color nodeFill: Palette.nodeFill
    readonly property color ringStrong: Palette.ringStrong
    readonly property color ringMedium: Palette.ringMedium
    readonly property color ringWeak: Palette.ringWeak
    readonly property color ringNew: Palette.ringNew
    readonly property color ringTrack: Palette.ringTrack

    readonly property color surfaceRaised: surfaceHigh
    readonly property color border: outline
    readonly property color text: onSurface
    readonly property color textMuted: onSurfaceMuted
    readonly property color accent: primary

    readonly property string serif: "Newsreader"
    readonly property string sans: "Inter"
    readonly property string mono: "IBM Plex Mono"
    readonly property int fontCaption: 10
    readonly property int fontSmall: 11
    readonly property int fontBody: 13
    readonly property int fontTitle: 17
    readonly property int fontHeadline: 24
    readonly property int fontHero: 32

    readonly property int gap: 8
    readonly property int radius: 12
    readonly property int radiusPanel: 16
    readonly property int radiusChip: 8
    readonly property int radiusRegion: 28
    readonly property int railWidth: 72
    readonly property int panelWidth: 380

    function ringColor(recall: real): color {
        if (recall < 0)
            return ringNew
        if (recall >= 0.8)
            return ringStrong
        return recall >= 0.5 ? ringMedium : ringWeak
    }

    function regionTint(hue: int): color {
        return Palette.regionTint(hue)
    }

    function noteFill(name: string): color {
        return Palette.noteFill(name)
    }

    function noteText(name: string): color {
        return Palette.noteText(name)
    }
}
```

The color functions read `Palette` inside the call; components that bind to them also bind to `Palette.background` so they recolor on a theme switch (see Task 9).

In `Main.qml` add `font.family: Theme.sans` to the `ApplicationWindow`.

- [ ] **Step 8: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS. Existing color assertions that hard-coded the old Graphite values (search the tests for `0x18181b`, `0xa3e635`, `#18181b`) are updated to the new values in this step.

- [ ] **Step 9: Commit**

```bash
git add modules/qml modules/render modules/viewmodels modules/app
git commit -m "feat: warm stone and clay tokens with bundled fonts"
```

---

### Task 2: Canvas look, regions layer, and board mapping

**Files:**
- Create: `modules/render/include/atlas/render/region_geometry.hpp`, `modules/render/src/region_geometry.cpp`
- Modify: `modules/render/CMakeLists.txt`, `modules/render/include/atlas/render/graph_canvas_item.hpp`, `modules/render/src/graph_canvas_item.cpp`
- Test: `modules/render/tests/test_region_geometry.cpp` (new, in `atlas_render_canvas_tests`), `modules/render/tests/test_graph_canvas_item.cpp`

**Interfaces:**
- Consumes: Task 1 `Theme` fields.
- Produces:
  - `region_geometry.hpp` (namespace `atlas::render`): `QRectF regionBounds(const std::vector<QPointF>& points, double padding)` (empty rect for no points); `int regionHue(const QString& key)` (FNV-1a over the UTF-16 units, modulo 6, stable across runs); `std::vector<QPointF> roundedRectOutline(const QRectF& rect, double radius, int cornerSegments)` (closed convex polygon, clockwise, radius clamped to half the shorter side).
  - `struct RenderRegion { QString key; QRectF rect; int hue = 0; };` and `GraphCanvasItem::setRegions(std::vector<RenderRegion>)`, `const std::vector<RenderRegion>& regions() const`.
  - `RenderNode::degree` (int, default 0): the dot radius is `5.5 + min(degree, 5) * 0.5` world units.
  - Invokables: `QPointF mapToScreen(double worldX, double worldY) const`, `QPointF mapToWorld(double screenX, double screenY) const`, `QString nodeAt(double screenX, double screenY) const` (empty while collapsed), `void fitWorldRect(double x, double y, double width, double height)`, `void handleDoubleClick(double screenX, double screenY)`; signal `backgroundDoubleClicked(double worldX, double worldY)` (emitted by `handleDoubleClick` when no node is under the point; `mouseDoubleClickEvent` calls it).
- Drawing rules: regions draw first in world space under links and nodes: fill = hue color at 8 percent alpha, outline 1 screen pixel at 20 percent alpha, corner radius 28 world units; hidden while collapsed. Nodes: a background colored halo of radius dot + 1.5, the nodeFill dot, then the memory ring at radius 9.5 with thickness 1.6. Selection ring radius 13.5, neighbor and hover 12.5, hint 16. Labels use `QFont("Inter")` at pixel size 11.

- [ ] **Step 1: Write the failing tests**

`modules/render/tests/test_region_geometry.cpp`:

```cpp
#include "atlas/render/region_geometry.hpp"
#include "doctest.h"

using namespace atlas::render;

TEST_CASE("region bounds pad every member") {
    QRectF rect = regionBounds({{0, 0}, {100, 40}}, 48);
    CHECK(rect == QRectF(-48, -48, 196, 136));
    CHECK(regionBounds({{10, 10}}, 48) == QRectF(-38, -38, 96, 96));
    CHECK(regionBounds({}, 48).isEmpty());
}

TEST_CASE("region hues are stable and in range") {
    int hue = regionHue("1b9d6bcd-bbfd-4b2d-9b5d-ab8dfbbd4bed");
    CHECK(hue == regionHue("1b9d6bcd-bbfd-4b2d-9b5d-ab8dfbbd4bed"));
    for (const char* key : {"a", "b", "topic", "another topic", ""}) {
        int value = regionHue(key);
        CHECK(value >= 0);
        CHECK(value < 6);
    }
}

TEST_CASE("rounded outlines stay inside their rect and clamp the radius") {
    auto outline = roundedRectOutline(QRectF(0, 0, 100, 40), 28, 6);
    CHECK(outline.size() == 4 * 7);
    for (const auto& point : outline) {
        CHECK(point.x() >= -1e-9);
        CHECK(point.x() <= 100 + 1e-9);
        CHECK(point.y() >= -1e-9);
        CHECK(point.y() <= 40 + 1e-9);
    }
}
```

Append to `test_graph_canvas_item.cpp`:

```cpp
TEST_CASE("screen and world mapping are inverse and find nodes") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", 100, 50, "")}, {});
    QPointF screen = canvas.mapToScreen(100, 50);
    QPointF world = canvas.mapToWorld(screen.x(), screen.y());
    CHECK(world.x() == doctest::Approx(100.0));
    CHECK(world.y() == doctest::Approx(50.0));
    CHECK(canvas.nodeAt(screen.x(), screen.y()) == "a");
    CHECK(canvas.nodeAt(screen.x() + 200, screen.y()).isEmpty());
}

TEST_CASE("double clicking empty board reports the world point") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setGraphData({makeNode("a", 0, 0, "")}, {});
    QPointF reported(-1, -1);
    int calls = 0;
    QObject::connect(&canvas, &GraphCanvasItem::backgroundDoubleClicked, [&](double x, double y) {
        reported = QPointF(x, y);
        ++calls;
    });
    QPointF onNode = canvas.mapToScreen(0, 0);
    canvas.handleDoubleClick(onNode.x(), onNode.y());
    CHECK(calls == 0);
    QPointF empty = canvas.mapToScreen(300, 200);
    canvas.handleDoubleClick(empty.x(), empty.y());
    REQUIRE(calls == 1);
    CHECK(reported.x() == doctest::Approx(300.0));
    CHECK(reported.y() == doctest::Approx(200.0));
}

TEST_CASE("regions are kept and fitting a world rect shows it") {
    GraphCanvasItem canvas;
    canvas.setSize(QSizeF(800, 600));
    canvas.setProperty("animated", false);
    canvas.setRegions({RenderRegion{"t", QRectF(1000, 1000, 400, 200), 2}});
    REQUIRE(canvas.regions().size() == 1);
    CHECK(canvas.regions()[0].hue == 2);
    canvas.fitWorldRect(1000, 1000, 400, 200);
    QPointF topLeft = canvas.mapToScreen(1000, 1000);
    QPointF bottomRight = canvas.mapToScreen(1400, 1200);
    CHECK(topLeft.x() >= 0.0);
    CHECK(bottomRight.x() <= 800.0);
    CHECK(topLeft.y() >= 0.0);
    CHECK(bottomRight.y() <= 600.0);
}
```

Add `test_region_geometry.cpp` to `atlas_render_canvas_tests`.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev`
Expected: FAIL, `region_geometry.hpp` not found.

- [ ] **Step 3: Implement `region_geometry`**

`region_geometry.hpp`:

```cpp
#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>

#include <vector>

namespace atlas::render {

QRectF regionBounds(const std::vector<QPointF>& points, double padding);
int regionHue(const QString& key);
std::vector<QPointF> roundedRectOutline(const QRectF& rect, double radius, int cornerSegments);

}  // namespace atlas::render
```

`region_geometry.cpp`:

```cpp
#include "atlas/render/region_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace atlas::render {

QRectF regionBounds(const std::vector<QPointF>& points, double padding) {
    if (points.empty()) return {};
    double left = points.front().x();
    double right = left;
    double top = points.front().y();
    double bottom = top;
    for (const auto& point : points) {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return QRectF(left - padding, top - padding, right - left + 2 * padding, bottom - top + 2 * padding);
}

int regionHue(const QString& key) {
    std::uint32_t hash = 2166136261u;
    for (QChar unit : key) {
        hash ^= unit.unicode();
        hash *= 16777619u;
    }
    return static_cast<int>(hash % 6u);
}

std::vector<QPointF> roundedRectOutline(const QRectF& rect, double radius, int cornerSegments) {
    double r = std::clamp(radius, 0.0, std::min(rect.width(), rect.height()) / 2.0);
    const QPointF centers[4] = {{rect.right() - r, rect.top() + r},
                                {rect.right() - r, rect.bottom() - r},
                                {rect.left() + r, rect.bottom() - r},
                                {rect.left() + r, rect.top() + r}};
    constexpr double kQuarter = 1.5707963267948966;
    std::vector<QPointF> outline;
    outline.reserve(static_cast<size_t>(4 * (cornerSegments + 1)));
    for (int corner = 0; corner < 4; ++corner) {
        double start = -kQuarter + corner * kQuarter;
        for (int step = 0; step <= cornerSegments; ++step) {
            double angle = start + kQuarter * step / cornerSegments;
            outline.emplace_back(centers[corner].x() + r * std::cos(angle), centers[corner].y() + r * std::sin(angle));
        }
    }
    return outline;
}

}  // namespace atlas::render
```

Add `src/region_geometry.cpp` to `atlas_render_canvas` in `modules/render/CMakeLists.txt`.

- [ ] **Step 4: Extend `GraphCanvasItem`**

Header: include `"atlas/render/region_geometry.hpp"`; add `int degree = 0;` to `RenderNode`; add `struct RenderRegion { QString key; QRectF rect; int hue = 0; };` after `RenderEdge`; public:

```cpp
    void setRegions(std::vector<RenderRegion> regions);
    const std::vector<RenderRegion>& regions() const { return regions_; }
    Q_INVOKABLE QPointF mapToScreen(double worldX, double worldY) const;
    Q_INVOKABLE QPointF mapToWorld(double screenX, double screenY) const;
    Q_INVOKABLE QString nodeAt(double screenX, double screenY) const;
    Q_INVOKABLE void fitWorldRect(double x, double y, double width, double height);
    Q_INVOKABLE void handleDoubleClick(double screenX, double screenY);
```

signal `void backgroundDoubleClicked(double worldX, double worldY);`, protected `void mouseDoubleClickEvent(QMouseEvent* event) override;`, private `std::vector<RenderRegion> regions_;`, `bool regionsDirty_ = true;`, `void buildRegions(SceneVertices& out, const Theme& theme) const;`.

Implementation in `graph_canvas_item.cpp`:

```cpp
void GraphCanvasItem::setRegions(std::vector<RenderRegion> regions) {
    regions_ = std::move(regions);
    regionsDirty_ = true;
    update();
}

QPointF GraphCanvasItem::mapToScreen(double worldX, double worldY) const { return toScreen(QPointF(worldX, worldY)); }

QPointF GraphCanvasItem::mapToWorld(double screenX, double screenY) const {
    return QPointF((screenX - offsetX_) / scale_, (screenY - offsetY_) / scale_);
}

QString GraphCanvasItem::nodeAt(double screenX, double screenY) const {
    if (collapsed()) return {};
    QPointF world = mapToWorld(screenX, screenY);
    int hit = hitTest(world.x(), world.y());
    return hit >= 0 ? nodes_[static_cast<size_t>(hit)].id : QString();
}

void GraphCanvasItem::fitWorldRect(double x, double y, double width, double height) {
    if (this->width() <= 0.0 || this->height() <= 0.0 || width <= 0.0 || height <= 0.0) return;
    double scale = std::min((this->width() - 2.0 * kFitMarginPx) / width, (this->height() - 2.0 * kFitMarginPx) / height);
    scale = std::clamp(scale, kMinScale, kFitMaxScale);
    moveCamera(scale, this->width() / 2.0 - (x + width / 2.0) * scale, this->height() / 2.0 - (y + height / 2.0) * scale);
}

void GraphCanvasItem::handleDoubleClick(double screenX, double screenY) {
    if (!nodeAt(screenX, screenY).isEmpty()) return;
    QPointF world = mapToWorld(screenX, screenY);
    emit backgroundDoubleClicked(world.x(), world.y());
}

void GraphCanvasItem::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) handleDoubleClick(event->position().x(), event->position().y());
    event->accept();
}
```

Use the existing hit test helper and constants (`kFitMarginPx`, `kFitMaxScale`, `kMinScale`); if the hit test uses a different name, call that one.

Regions layer: add two layers before the highlight layer in the layer enum (`RegionFillLayer`, `RegionOutlineLayer`, both `DrawTriangles`). In `updatePaintNode`, when not collapsed and (`regionsDirty_` or `dataDirty_` or the zoom changed since the last build), rebuild them with `buildRegions`; clear them while collapsed. Set `regionsDirty_ = true` in `setTheme` and when `applyCamera` changes the scale.

```cpp
void GraphCanvasItem::buildRegions(SceneVertices& out, const Theme& theme) const {
    constexpr int kCornerSegments = 8;
    constexpr double kRegionRadius = 28.0;
    float outlineWidth = static_cast<float>(1.0 / scale_);
    for (const auto& region : regions_) {
        if (region.rect.isEmpty()) continue;
        QColor hue = theme.regionHues[static_cast<size_t>(((region.hue % 6) + 6) % 6)];
        QColor fill = hue;
        fill.setAlphaF(0.08f);
        QColor line = hue;
        line.setAlphaF(0.20f);
        auto outer = roundedRectOutline(region.rect, kRegionRadius, kCornerSegments);
        auto inner = roundedRectOutline(region.rect.adjusted(outlineWidth, outlineWidth, -outlineWidth, -outlineWidth),
                                        kRegionRadius - outlineWidth, kCornerSegments);
        QPointF center = region.rect.center();
        for (size_t i = 0; i < outer.size(); ++i) {
            const QPointF& a = outer[i];
            const QPointF& b = outer[(i + 1) % outer.size()];
            appendTriangle(out.regionFill, center, a, b, fill);
            const QPointF& c = inner[i];
            const QPointF& d = inner[(i + 1) % inner.size()];
            appendTriangle(out.regionOutline, a, b, d, line);
            appendTriangle(out.regionOutline, a, d, c, line);
        }
    }
}
```

Add `std::vector<ColoredVertex> regionFill;` and `regionOutline;` to `SceneVertices`, and a small `appendTriangle(std::vector<ColoredVertex>&, QPointF, QPointF, QPointF, QColor)` helper next to the existing geometry helpers that pushes three vertices with premultiplied color, the same way existing helpers do.

Node style: replace the node constants with `kNodeRadius = 5.5f`, `kNodeHalo = 1.5f`, `kMemoryRingRadius = 9.5f`, `kMemoryRingThickness = 1.6f`, `kSelectRingRadius = 13.5f`, `kNeighborRingRadius = 12.5f`, `kHoverRingRadius = 12.5f`, `kHintRingRadius = 16.0f`. In `buildNodes`, compute `float radius = kNodeRadius + std::min(node.degree, 5) * 0.5f;`, draw the halo disc (`theme.nodeBorder`, radius + kNodeHalo), then the fill disc (`theme.nodeFill`, radius), then the ring. Remove `kNodeBorderWidth` if unused. Set the label font to `QFont font("Inter"); font.setPixelSize(kLabelPixelSize);` where labels are laid out.

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add modules/render
git commit -m "feat(render): matte dot nodes, topic regions, and board mapping helpers"
```

---

### Task 3: Regions in the map view model and moving a topic

**Files:**
- Modify: `modules/viewmodels/include/atlas/viewmodels/map_view_model.hpp`, `src/map_view_model.cpp`
- Modify: `modules/viewmodels/include/atlas/viewmodels/placement_controller.hpp`, `src/placement_controller.cpp`
- Test: `modules/viewmodels/tests/test_map_view_model.cpp`, `test_placement_controller.cpp`

**Interfaces:**
- Consumes: Task 2 `RenderRegion`, `regionBounds`, `regionHue`, `setRegions`, `RenderNode::degree`.
- Produces:
  - `Result<void, ControllerFailure> PlacementController::moveBy(const std::vector<KnowledgeObjectId>& ids, double dx, double dy)`: moves each placed id by (dx, dy), keeps pinned flags, saves in one call, then updates memory and emits `placementsChanged`.
  - `MapViewModel`: `Q_PROPERTY(QVariantList regions READ regions NOTIFY sceneChanged)`, each `{topicId, name, caption, x, y, width, height, hue}` in world units; `Q_INVOKABLE bool moveTopic(const QString& topicId, double dx, double dy)` (moves every concept of the topic, shown or not; emits `errorOccurred` and returns false on failure); `conceptInfo` also returns `definition`.
  - Rules: a region exists for each topic other than Uncategorized that has at least one shown member (not ghost) with a position; its rect is `regionBounds(memberPositions, 48)`; caption is `"N concepts"` (or `"1 concept"`) followed by `"  ·  R%"` when any member is learned, where R is the rounded mean recall of learned members. Each node's `degree` is its link count. The canvas gets the same regions through `setRegions`.

- [ ] **Step 1: Write the failing tests**

Append to `test_placement_controller.cpp`:

```cpp
TEST_CASE("moving concepts shifts them, keeps pins, and persists") {
    Fixture f;
    PlacementController placements(f.db, f.workspace);
    REQUIRE(placements.load().hasValue());
    REQUIRE(placements.setPinned(f.a, true).hasValue());
    auto before = *placements.position(f.a);
    int changes = 0;
    QObject::connect(&placements, &PlacementController::placementsChanged, [&] { ++changes; });
    REQUIRE(placements.moveBy({f.a, f.b}, 30.0, -20.0).hasValue());
    CHECK(changes == 1);
    auto after = *placements.position(f.a);
    CHECK(after.x == doctest::Approx(before.x + 30.0));
    CHECK(after.y == doctest::Approx(before.y - 20.0));
    CHECK(placements.isPinned(f.a));

    PlacementController reloaded(f.db, f.workspace);
    REQUIRE(reloaded.load().hasValue());
    CHECK(reloaded.position(f.a)->x == doctest::Approx(after.x));
}
```

Append to `test_map_view_model.cpp`:

```cpp
TEST_CASE("topics become regions around their concepts") {
    Fixture f;
    auto os = f.topic("Operating Systems");
    auto paging = f.addConcept("Paging", os);
    auto tlb = f.addConcept("TLB", os);
    f.addConcept("Loose", uncategorizedTopicId());
    f.workspace.createRelationship(tlb, paging, RelationshipType::PartOf, std::nullopt).value();
    f.settle();

    auto regions = f.map.regions();
    REQUIRE(regions.size() == 1);
    auto region = regions[0].toMap();
    CHECK(region.value("name").toString() == "Operating Systems");
    CHECK(region.value("caption").toString() == "2 concepts");
    auto* pagingNode = f.node(paging);
    QRectF rect(region.value("x").toDouble(), region.value("y").toDouble(), region.value("width").toDouble(),
                region.value("height").toDouble());
    CHECK(rect.contains(QPointF(pagingNode->x, pagingNode->y)));
    CHECK(rect.left() <= pagingNode->x - 48.0 + 1e-6);
    CHECK(pagingNode->degree == 1);
    CHECK(f.map.conceptInfo(idString(paging)).contains("definition"));
}

TEST_CASE("moving a topic moves all its concepts") {
    Fixture f;
    auto os = f.topic("OS");
    auto paging = f.addConcept("Paging", os);
    f.settle();
    double x = f.node(paging)->x;
    REQUIRE(f.map.moveTopic(idString(os), 100.0, 0.0));
    f.settle();
    CHECK(f.node(paging)->x == doctest::Approx(x + 100.0));
    CHECK_FALSE(f.map.moveTopic("garbage", 1.0, 1.0));
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev`
Expected: FAIL, `moveBy`, `regions`, and `moveTopic` are not members.

- [ ] **Step 3: Implement**

`placement_controller.cpp`:

```cpp
Result<void, ControllerFailure> PlacementController::moveBy(const std::vector<KnowledgeObjectId>& ids, double dx,
                                                            double dy) {
    std::vector<Placement> moved;
    for (const auto& id : ids) {
        auto placed = placements_.find(id);
        if (placed == placements_.end()) continue;
        Placement next = placed->second;
        next.x += dx;
        next.y += dy;
        moved.push_back(next);
    }
    if (moved.empty()) return Result<void, ControllerFailure>::ok();
    return save(moved);
}
```

`save` already writes, updates `placements_`, and emits `placementsChanged`; if it does not emit, emit after a successful save here. Declare `moveBy` in the header.

`map_view_model.hpp`: `Q_PROPERTY(QVariantList regions READ regions NOTIFY sceneChanged)`, `QVariantList regions() const { return regions_; }`, `Q_INVOKABLE bool moveTopic(const QString& topicId, double dx, double dy);`, private `QVariantList regions_;` and `std::vector<atlas::render::RenderRegion> renderRegions_;`.

In `refresh()`, after nodes and edges are built: count each node's links into `degree` (loop over `edges_` and increment both ends by id), then build regions:

```cpp
    std::unordered_map<std::string, std::vector<QPointF>> members;
    std::unordered_map<std::string, std::vector<double>> recalls;
    for (const auto& object : objects) {
        auto topicId = object.topicId();
        if (!topicId || *topicId == atlas::core::uncategorizedTopicId()) continue;
        QString id = idString(object.id());
        auto node = std::find_if(nodes_.begin(), nodes_.end(), [&](const RenderNode& n) { return n.id == id; });
        if (node == nodes_.end() || node->ghost) continue;
        members[topicId->toString()].emplace_back(node->x, node->y);
        if (node->recall >= 0.0) recalls[topicId->toString()].push_back(node->recall);
    }
    regions_.clear();
    renderRegions_.clear();
    for (const auto& [topicKey, points] : members) {
        QString topicId = toQString(topicKey);
        QRectF rect = atlas::render::regionBounds(points, kRegionPadding);
        int hue = atlas::render::regionHue(topicId);
        QString caption = points.size() == 1 ? tr("1 concept") : tr("%1 concepts").arg(points.size());
        if (auto learned = recalls.find(topicKey); learned != recalls.end() && !learned->second.empty()) {
            double mean = std::accumulate(learned->second.begin(), learned->second.end(), 0.0) / learned->second.size();
            caption += QStringLiteral("  ·  %1%").arg(qRound(mean * 100.0));
        }
        auto name = topicNames_.find(topicKey);
        regions_.append(QVariantMap{{"topicId", topicId},
                                    {"name", name != topicNames_.end() ? toQString(name->second) : QString()},
                                    {"caption", caption},
                                    {"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()},
                                    {"hue", hue}});
        renderRegions_.push_back(atlas::render::RenderRegion{topicId, rect, hue});
    }
```

Add `static constexpr double kRegionPadding = 48.0;` to the class, includes `<algorithm>`, `<numeric>`, and `"atlas/render/region_geometry.hpp"`. In `pushToCanvas`, call `canvas_->setRegions(renderRegions_);` before `setGraphData`. Sort `regions_` by name so the order is stable.

```cpp
bool MapViewModel::moveTopic(const QString& topicId, double dx, double dy) {
    auto topic = parseId<TopicId>(topicId);
    if (!topic) return false;
    std::vector<KnowledgeObjectId> ids;
    for (const auto& object : workspace_->knowledgeObjectsInTopic(*topic)) ids.push_back(object.id());
    if (ids.empty()) return false;
    auto moved = placements_->moveBy(ids, dx, dy);
    if (!moved.hasValue()) {
        emit errorOccurred(toQString(moved.error().detail));
        return false;
    }
    return true;
}
```

In `conceptInfo`, add `{"definition", toQString(object->definition())}`.

- [ ] **Step 4: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): topic regions on the board and moving a whole topic"
```

---

### Task 4: Notes storage (migration 4)

**Files:**
- Create: `modules/persistence/include/atlas/persistence/note_repository.hpp`, `modules/persistence/src/note_repository.cpp`
- Modify: `modules/persistence/src/migrations.cpp`, `modules/persistence/CMakeLists.txt`, `modules/persistence/tests/CMakeLists.txt`
- Test: `modules/persistence/tests/test_note_repository.cpp`

**Interfaces:**
- Produces (namespace `atlas::persistence`):
  - `struct NoteLink { std::string kind; atlas::core::Uuid targetId; bool operator==(const NoteLink&) const = default; };` (`kind` is `concept` or `topic`).
  - `struct BoardNote { atlas::core::Uuid id; std::string body; std::string color; double x = 0.0; double y = 0.0; double width = 0.0; double height = 0.0; atlas::core::TimePoint createdAt; atlas::core::TimePoint updatedAt; std::vector<NoteLink> links{}; };`
  - `NoteRepository(Database&)` with `Result<void, PersistenceError> save(const BoardNote& note)` (upsert of the note row only), `remove(const Uuid& id)`, `addLink(const Uuid& noteId, const NoteLink& link)` (ignores duplicates), `removeLink(const Uuid& noteId, const NoteLink& link)`, `Result<std::vector<BoardNote>, PersistenceError> findAll()` (ordered by `created_at`, links included).
  - Migration 4 exactly as the spec's section 4, plus `CREATE INDEX idx_note_links_target ON note_links(target_kind, target_id);` and two triggers that delete `note_links` rows when a `knowledge_objects` row (kind `concept`) or a `topics` row (kind `topic`) is deleted.

- [ ] **Step 1: Write the failing tests**

`modules/persistence/tests/test_note_repository.cpp`:

```cpp
#include <chrono>

#include "atlas/persistence/database.hpp"
#include "atlas/persistence/knowledge_object_repository.hpp"
#include "atlas/persistence/note_repository.hpp"
#include "atlas/persistence/topic_repository.hpp"
#include "doctest.h"

using namespace atlas::core;
using namespace atlas::persistence;

namespace {

Database openTestDatabase() {
    auto result = Database::open(":memory:");
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

BoardNote makeNote(const char* body) {
    BoardNote note;
    note.id = Uuid::generate();
    note.body = body;
    note.color = "clay";
    note.x = 10;
    note.y = 20;
    note.width = 180;
    note.height = 120;
    note.createdAt = std::chrono::system_clock::now();
    note.updatedAt = note.createdAt;
    return note;
}

}  // namespace

TEST_CASE("notes round trip with their links") {
    auto db = openTestDatabase();
    NoteRepository notes(db);
    auto note = makeNote("Why half full?");
    REQUIRE(notes.save(note).hasValue());
    NoteLink link{"concept", Uuid::generate()};
    REQUIRE(notes.addLink(note.id, link).hasValue());
    REQUIRE(notes.addLink(note.id, link).hasValue());

    auto all = notes.findAll().value();
    REQUIRE(all.size() == 1);
    CHECK(all[0].body == "Why half full?");
    CHECK(all[0].width == doctest::Approx(180.0));
    REQUIRE(all[0].links.size() == 1);
    CHECK(all[0].links[0] == link);

    note.body = "Edited";
    note.x = 99;
    REQUIRE(notes.save(note).hasValue());
    CHECK(notes.findAll().value()[0].body == "Edited");

    REQUIRE(notes.removeLink(note.id, link).hasValue());
    CHECK(notes.findAll().value()[0].links.empty());
    REQUIRE(notes.remove(note.id).hasValue());
    CHECK(notes.findAll().value().empty());
}

TEST_CASE("deleting a linked concept or topic removes the link but keeps the note") {
    auto db = openTestDatabase();
    KnowledgeObjectRepository objects(db);
    TopicRepository topics(db);
    auto topic = Topic::create("OS").value();
    REQUIRE(topics.save(topic).hasValue());
    auto object = KnowledgeObject::create("Paging").value();
    REQUIRE(objects.save(object).hasValue());

    NoteRepository notes(db);
    auto note = makeNote("Linked");
    REQUIRE(notes.save(note).hasValue());
    REQUIRE(notes.addLink(note.id, NoteLink{"concept", object.id().value()}).hasValue());
    REQUIRE(notes.addLink(note.id, NoteLink{"topic", topic.id().value()}).hasValue());

    REQUIRE(objects.remove(object.id()).hasValue());
    auto after = notes.findAll().value();
    REQUIRE(after.size() == 1);
    REQUIRE(after[0].links.size() == 1);
    CHECK(after[0].links[0].kind == "topic");

    REQUIRE(topics.remove(topic.id()).hasValue());
    CHECK(notes.findAll().value()[0].links.empty());
}
```

Use the creation and save calls the existing repository tests use for `Topic` and `KnowledgeObject` if they differ from `create(...).value()` and `save(...)`. Add the test file and `src/note_repository.cpp` to the CMake lists.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev`
Expected: FAIL, `note_repository.hpp` not found.

- [ ] **Step 3: Add migration 4**

In `migrations.cpp`, change the array size to 4 and append:

```cpp
    {4, "Board notes and their links", R"sql(
        CREATE TABLE board_notes (
            id TEXT PRIMARY KEY,
            body TEXT NOT NULL DEFAULT '',
            color TEXT NOT NULL,
            x REAL NOT NULL,
            y REAL NOT NULL,
            width REAL NOT NULL,
            height REAL NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );

        CREATE TABLE note_links (
            note_id TEXT NOT NULL REFERENCES board_notes(id) ON DELETE CASCADE,
            target_kind TEXT NOT NULL,
            target_id TEXT NOT NULL,
            PRIMARY KEY (note_id, target_kind, target_id)
        );
        CREATE INDEX idx_note_links_target ON note_links(target_kind, target_id);

        CREATE TRIGGER trg_knowledge_objects_forget_note_links AFTER DELETE ON knowledge_objects
        BEGIN
            DELETE FROM note_links WHERE target_kind = 'concept' AND target_id = OLD.id;
        END;

        CREATE TRIGGER trg_topics_forget_note_links AFTER DELETE ON topics
        BEGIN
            DELETE FROM note_links WHERE target_kind = 'topic' AND target_id = OLD.id;
        END;
    )sql"},
```

Check the topics table name in migration 2 and use it.

- [ ] **Step 4: Implement `NoteRepository`**

Header with the structs from Interfaces and the class. Implementation, following `placement_repository.cpp`:

```cpp
#include "atlas/persistence/note_repository.hpp"

#include <sqlite3.h>

#include <unordered_map>
#include <utility>

#include "detail/statement.hpp"

namespace atlas::persistence {

using atlas::core::TimePoint;
using atlas::core::Uuid;

namespace {

int64_t toMillis(TimePoint time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

TimePoint fromMillis(int64_t millis) { return TimePoint(std::chrono::milliseconds(millis)); }

Result<void, PersistenceError> run(sqlite3* db, const char* sql, const std::vector<std::string>& texts) {
    auto prepared = detail::Statement::prepare(db, sql);
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    for (size_t i = 0; i < texts.size(); ++i) statement.bindText(static_cast<int>(i) + 1, texts[i]);
    auto step = statement.step();
    if (!step.hasValue()) return Result<void, PersistenceError>::err(step.error());
    return Result<void, PersistenceError>::ok();
}

}  // namespace

NoteRepository::NoteRepository(Database& database) : database_(&database) {}

Result<void, PersistenceError> NoteRepository::save(const BoardNote& note) {
    auto prepared = detail::Statement::prepare(database_->handle(), R"sql(
        INSERT INTO board_notes (id, body, color, x, y, width, height, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET body = excluded.body, color = excluded.color, x = excluded.x,
            y = excluded.y, width = excluded.width, height = excluded.height, updated_at = excluded.updated_at;
    )sql");
    if (!prepared.hasValue()) return Result<void, PersistenceError>::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    statement.bindText(1, note.id.toString());
    statement.bindText(2, note.body);
    statement.bindText(3, note.color);
    statement.bindDouble(4, note.x);
    statement.bindDouble(5, note.y);
    statement.bindDouble(6, note.width);
    statement.bindDouble(7, note.height);
    statement.bindInt64(8, toMillis(note.createdAt));
    statement.bindInt64(9, toMillis(note.updatedAt));
    auto step = statement.step();
    if (!step.hasValue()) return Result<void, PersistenceError>::err(step.error());
    return Result<void, PersistenceError>::ok();
}

Result<void, PersistenceError> NoteRepository::remove(const Uuid& id) {
    return run(database_->handle(), "DELETE FROM board_notes WHERE id = ?;", {id.toString()});
}

Result<void, PersistenceError> NoteRepository::addLink(const Uuid& noteId, const NoteLink& link) {
    return run(database_->handle(),
               "INSERT OR IGNORE INTO note_links (note_id, target_kind, target_id) VALUES (?, ?, ?);",
               {noteId.toString(), link.kind, link.targetId.toString()});
}

Result<void, PersistenceError> NoteRepository::removeLink(const Uuid& noteId, const NoteLink& link) {
    return run(database_->handle(), "DELETE FROM note_links WHERE note_id = ? AND target_kind = ? AND target_id = ?;",
               {noteId.toString(), link.kind, link.targetId.toString()});
}

Result<std::vector<BoardNote>, PersistenceError> NoteRepository::findAll() {
    using Out = Result<std::vector<BoardNote>, PersistenceError>;
    sqlite3* db = database_->handle();
    auto prepared = detail::Statement::prepare(
        db, "SELECT id, body, color, x, y, width, height, created_at, updated_at FROM board_notes ORDER BY created_at, id;");
    if (!prepared.hasValue()) return Out::err(std::move(prepared).error());
    auto statement = std::move(prepared).value();
    std::vector<BoardNote> notes;
    std::unordered_map<std::string, size_t> indexById;
    while (true) {
        auto step = statement.step();
        if (!step.hasValue()) return Out::err(step.error());
        if (!step.value()) break;
        auto id = Uuid::parse(statement.columnText(0));
        if (!id) continue;
        BoardNote note;
        note.id = *id;
        note.body = statement.columnText(1);
        note.color = statement.columnText(2);
        note.x = statement.columnDouble(3);
        note.y = statement.columnDouble(4);
        note.width = statement.columnDouble(5);
        note.height = statement.columnDouble(6);
        note.createdAt = fromMillis(statement.columnInt64(7));
        note.updatedAt = fromMillis(statement.columnInt64(8));
        indexById.emplace(statement.columnText(0), notes.size());
        notes.push_back(std::move(note));
    }
    auto linkStatement = detail::Statement::prepare(
        db, "SELECT note_id, target_kind, target_id FROM note_links ORDER BY note_id, target_kind, target_id;");
    if (!linkStatement.hasValue()) return Out::err(std::move(linkStatement).error());
    auto links = std::move(linkStatement).value();
    while (true) {
        auto step = links.step();
        if (!step.hasValue()) return Out::err(step.error());
        if (!step.value()) break;
        auto owner = indexById.find(links.columnText(0));
        auto target = Uuid::parse(links.columnText(2));
        if (owner == indexById.end() || !target) continue;
        notes[owner->second].links.push_back(NoteLink{links.columnText(1), *target});
    }
    return Out::ok(std::move(notes));
}

}  // namespace atlas::persistence
```

- [ ] **Step 4b: Pin the migration on an old database**

Append to `modules/persistence/tests/test_migration_three.cpp` (or a new `test_migration_four.cpp` in the same executable), using that file's helper that builds a version 3 database:

```cpp
TEST_CASE("migration 4 upgrades a version 3 database and keeps its data") {
    QTemporaryDir dir;
    std::string path = dir.filePath("atlas.db").toStdString();
    {
        auto db = Database::open(path).value();
        KnowledgeObjectRepository objects(db);
        REQUIRE(objects.save(KnowledgeObject::create("Kept").value()).hasValue());
        REQUIRE(executeRawSql(path, "DROP TABLE note_links; DROP TABLE board_notes; "
                                    "DELETE FROM schema_migrations WHERE version = 4;"));
    }
    auto reopened = Database::open(path);
    REQUIRE(reopened.hasValue());
    auto db = std::move(reopened).value();
    CHECK(KnowledgeObjectRepository(db).findAll().value().size() == 1);
    CHECK(NoteRepository(db).findAll().value().empty());
}
```

Use the raw SQL helper the persistence tests already have (or add a small one built on `sqlite3_exec`, as `modules/viewmodels/tests/raw_sql.hpp` does), and `QTemporaryDir` only if the persistence tests link Qt Core; otherwise build a temporary path with `std::filesystem::temp_directory_path()` and remove it at the end.

- [ ] **Step 5: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS, including the existing migration tests (the version counter is now 4).

- [ ] **Step 6: Commit**

```bash
git add modules/persistence
git commit -m "feat(persistence): board notes and note links in migration 4"
```

---

### Task 5: The Notes model

**Files:**
- Create: `modules/viewmodels/include/atlas/viewmodels/notes_model.hpp`, `modules/viewmodels/src/notes_model.cpp`
- Modify: `modules/viewmodels/include/atlas/viewmodels/app_context.hpp`, `src/app_context.cpp`, `modules/viewmodels/CMakeLists.txt`, `modules/viewmodels/tests/CMakeLists.txt`
- Test: `modules/viewmodels/tests/test_notes_model.cpp`

**Interfaces:**
- Consumes: Task 4 `NoteRepository`, `BoardNote`, `NoteLink`.
- Produces: QML singleton `Notes` (`NotesModel(atlas::persistence::Database&, WorkspaceController&, Clock, QObject* parent = nullptr)`):
  - Roles `noteId`, `body`, `colorName`, `worldX`, `worldY`, `worldWidth`, `worldHeight`, `links` (list of `{kind, targetId}`); the world prefix keeps QML delegates from clashing with `Item.x` and friends. Property `count` (`NOTIFY countChanged`). `note(id)` returns a map with keys `noteId`, `body`, `color`, `x`, `y`, `width`, `height`, `links`.
  - `Result<void, ControllerFailure> load()`.
  - Invokables (all return bool except `create`): `QString create(double x, double y, const QString& body = QString())` (default size 180 by 120, color `clay`, returns the id), `setBody(id, body)`, `move(id, x, y)`, `resize(id, width, height)` (each side at least 96), `recolor(id, color)` (only `clay`, `olive`, `rose`), `link(id, kind, targetId)` (kind `concept` or `topic`; the target must exist), `unlink(id, kind, targetId)`, `remove(id)`; `QVariantMap note(const QString& id) const`.
  - On `WorkspaceController::graphChanged` and `topicsChanged` it reloads, so links removed by triggers disappear.
  - Every write goes to the database first; on failure `errorOccurred` fires and the model is unchanged.
  - `AppContext::notes()`; loaded in `AppContext::load()` and provided as a singleton.

- [ ] **Step 1: Write the failing tests**

`modules/viewmodels/tests/test_notes_model.cpp`:

```cpp
#include <QCoreApplication>
#include <QTemporaryDir>

#include "atlas/persistence/database.hpp"
#include "atlas/viewmodels/ids.hpp"
#include "atlas/viewmodels/notes_model.hpp"
#include "doctest.h"
#include "raw_sql.hpp"

using namespace atlas::core;
using namespace atlas::persistence;
using namespace atlas::viewmodels;

namespace {

Database openTestDatabase(const std::string& path) {
    auto result = Database::open(path);
    REQUIRE(result.hasValue());
    return std::move(result).value();
}

struct Fixture {
    QTemporaryDir dir;
    std::string path = dir.filePath("atlas.db").toStdString();
    Database db = openTestDatabase(path);
    WorkspaceController workspace{db};
    bool loaded = workspace.load().hasValue();
    NotesModel notes{db, workspace, systemClock()};
    int errors = 0;

    Fixture() {
        REQUIRE(loaded);
        REQUIRE(notes.load().hasValue());
        QObject::connect(&notes, &NotesModel::errorOccurred, [this] { ++errors; });
    }
};

}  // namespace

TEST_CASE("notes are created, edited, moved, resized, and recolored") {
    Fixture f;
    QString id = f.notes.create(10, 20, "Why half?");
    REQUIRE_FALSE(id.isEmpty());
    CHECK(f.notes.count() == 1);
    REQUIRE(f.notes.setBody(id, "Why half full?"));
    REQUIRE(f.notes.move(id, 300, 400));
    REQUIRE(f.notes.resize(id, 40, 300));
    REQUIRE(f.notes.recolor(id, "olive"));
    CHECK_FALSE(f.notes.recolor(id, "neon"));
    auto note = f.notes.note(id);
    CHECK(note.value("body").toString() == "Why half full?");
    CHECK(note.value("x").toDouble() == doctest::Approx(300.0));
    CHECK(note.value("width").toDouble() == doctest::Approx(96.0));
    CHECK(note.value("color").toString() == "olive");

    NotesModel reloaded(f.db, f.workspace, systemClock());
    REQUIRE(reloaded.load().hasValue());
    CHECK(reloaded.note(id).value("body").toString() == "Why half full?");
}

TEST_CASE("links follow the life of their targets") {
    Fixture f;
    auto concept_ = f.workspace.createKnowledgeObject("Paging").value();
    auto topic = f.workspace.createTopic("OS").value();
    QString id = f.notes.create(0, 0);
    REQUIRE(f.notes.link(id, "concept", idString(concept_)));
    REQUIRE(f.notes.link(id, "topic", idString(topic)));
    CHECK_FALSE(f.notes.link(id, "concept", "garbage"));
    CHECK(f.notes.note(id).value("links").toList().size() == 2);

    REQUIRE(f.workspace.removeKnowledgeObject(concept_).hasValue());
    QCoreApplication::processEvents();
    auto links = f.notes.note(id).value("links").toList();
    REQUIRE(links.size() == 1);
    CHECK(links[0].toMap().value("kind").toString() == "topic");

    REQUIRE(f.notes.unlink(id, "topic", idString(topic)));
    CHECK(f.notes.note(id).value("links").toList().isEmpty());
    REQUIRE(f.notes.remove(id));
    CHECK(f.notes.count() == 0);
}

TEST_CASE("a failed write leaves the board unchanged") {
    Fixture f;
    QString id = f.notes.create(5, 5, "Keep");
    REQUIRE(executeRawSql(f.path, "DROP TABLE note_links; DROP TABLE board_notes;"));
    CHECK_FALSE(f.notes.move(id, 900, 900));
    CHECK(f.errors == 1);
    CHECK(f.notes.note(id).value("x").toDouble() == doctest::Approx(5.0));
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev`
Expected: FAIL, `notes_model.hpp` not found.

- [ ] **Step 3: Implement `NotesModel`**

`notes_model.hpp`:

```cpp
#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <vector>

#include "atlas/persistence/note_repository.hpp"
#include "atlas/viewmodels/clock.hpp"
#include "atlas/viewmodels/controller_error.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class NotesModel : public QAbstractListModel, public ProvidedSingleton<NotesModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Notes)
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, BodyRole, ColorRole, XRole, YRole, WidthRole, HeightRole, LinksRole };
    static constexpr double kDefaultWidth = 180.0;
    static constexpr double kDefaultHeight = 120.0;
    static constexpr double kMinSize = 96.0;

    NotesModel(atlas::persistence::Database& database, WorkspaceController& workspace, Clock clock,
               QObject* parent = nullptr);

    Result<void, ControllerFailure> load();
    int count() const { return static_cast<int>(notes_.size()); }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString create(double x, double y, const QString& body = QString());
    Q_INVOKABLE bool setBody(const QString& id, const QString& body);
    Q_INVOKABLE bool move(const QString& id, double x, double y);
    Q_INVOKABLE bool resize(const QString& id, double width, double height);
    Q_INVOKABLE bool recolor(const QString& id, const QString& color);
    Q_INVOKABLE bool link(const QString& id, const QString& kind, const QString& targetId);
    Q_INVOKABLE bool unlink(const QString& id, const QString& kind, const QString& targetId);
    Q_INVOKABLE bool remove(const QString& id);
    Q_INVOKABLE QVariantMap note(const QString& id) const;

signals:
    void countChanged();
    void errorOccurred(const QString& message);

private:
    int indexOf(const QString& id) const;
    bool targetExists(const QString& kind, const QString& targetId) const;
    bool fail(const QString& message);
    bool update(const QString& id, const std::function<void(atlas::persistence::BoardNote&)>& change);
    QVariantMap toMap(const atlas::persistence::BoardNote& note) const;
    void reload();

    atlas::persistence::NoteRepository repository_;
    WorkspaceController* workspace_;
    Clock clock_;
    std::vector<atlas::persistence::BoardNote> notes_;
};

}  // namespace atlas::viewmodels
```

Include `<functional>`. `notes_model.cpp`:

```cpp
#include "atlas/viewmodels/notes_model.hpp"

#include <algorithm>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::Uuid;
using atlas::persistence::BoardNote;
using atlas::persistence::NoteLink;

namespace {

bool validColor(const QString& color) {
    return color == QLatin1String("clay") || color == QLatin1String("olive") || color == QLatin1String("rose");
}

bool validKind(const QString& kind) { return kind == QLatin1String("concept") || kind == QLatin1String("topic"); }

}  // namespace

NotesModel::NotesModel(atlas::persistence::Database& database, WorkspaceController& workspace, Clock clock,
                       QObject* parent)
    : QAbstractListModel(parent), repository_(database), workspace_(&workspace), clock_(std::move(clock)) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &NotesModel::reload);
    connect(workspace_, &WorkspaceController::topicsChanged, this, &NotesModel::reload);
}

Result<void, ControllerFailure> NotesModel::load() {
    auto all = repository_.findAll();
    if (!all.hasValue()) {
        return Result<void, ControllerFailure>::err({ControllerErrorCode::PersistenceFailed, all.error().detail});
    }
    beginResetModel();
    notes_ = std::move(all).value();
    endResetModel();
    emit countChanged();
    return Result<void, ControllerFailure>::ok();
}

void NotesModel::reload() {
    if (auto loaded = load(); !loaded.hasValue()) emit errorOccurred(toQString(loaded.error().detail));
}

int NotesModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }

QVariant NotesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const auto& note = notes_[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return toQString(note.id.toString());
        case BodyRole: return toQString(note.body);
        case ColorRole: return toQString(note.color);
        case XRole: return note.x;
        case YRole: return note.y;
        case WidthRole: return note.width;
        case HeightRole: return note.height;
        case LinksRole: return toMap(note).value("links");
        default: return {};
    }
}

QHash<int, QByteArray> NotesModel::roleNames() const {
    return {{IdRole, "noteId"},      {BodyRole, "body"},         {ColorRole, "colorName"},     {XRole, "worldX"},
            {YRole, "worldY"},       {WidthRole, "worldWidth"},  {HeightRole, "worldHeight"},  {LinksRole, "links"}};
}

QVariantMap NotesModel::toMap(const BoardNote& note) const {
    QVariantList links;
    for (const auto& link : note.links) {
        links.append(QVariantMap{{"kind", toQString(link.kind)}, {"targetId", toQString(link.targetId.toString())}});
    }
    return {{"noteId", toQString(note.id.toString())}, {"body", toQString(note.body)}, {"color", toQString(note.color)},
            {"x", note.x}, {"y", note.y}, {"width", note.width}, {"height", note.height}, {"links", links}};
}

int NotesModel::indexOf(const QString& id) const {
    auto it = std::find_if(notes_.begin(), notes_.end(), [&](const BoardNote& n) { return toQString(n.id.toString()) == id; });
    return it == notes_.end() ? -1 : static_cast<int>(it - notes_.begin());
}

bool NotesModel::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

QString NotesModel::create(double x, double y, const QString& body) {
    BoardNote note;
    note.id = Uuid::generate();
    note.body = toStdString(body);
    note.color = "clay";
    note.x = x;
    note.y = y;
    note.width = kDefaultWidth;
    note.height = kDefaultHeight;
    note.createdAt = clock_();
    note.updatedAt = note.createdAt;
    if (auto saved = repository_.save(note); !saved.hasValue()) {
        fail(toQString(saved.error().detail));
        return {};
    }
    beginInsertRows(QModelIndex(), count(), count());
    notes_.push_back(note);
    endInsertRows();
    emit countChanged();
    return toQString(note.id.toString());
}

bool NotesModel::update(const QString& id, const std::function<void(BoardNote&)>& change) {
    int row = indexOf(id);
    if (row < 0) return fail(tr("That note no longer exists"));
    BoardNote next = notes_[static_cast<size_t>(row)];
    change(next);
    next.updatedAt = clock_();
    if (auto saved = repository_.save(next); !saved.hasValue()) return fail(toQString(saved.error().detail));
    notes_[static_cast<size_t>(row)] = next;
    emit dataChanged(index(row), index(row));
    return true;
}

bool NotesModel::setBody(const QString& id, const QString& body) {
    return update(id, [&](BoardNote& note) { note.body = toStdString(body); });
}

bool NotesModel::move(const QString& id, double x, double y) {
    return update(id, [&](BoardNote& note) {
        note.x = x;
        note.y = y;
    });
}

bool NotesModel::resize(const QString& id, double width, double height) {
    return update(id, [&](BoardNote& note) {
        note.width = std::max(width, kMinSize);
        note.height = std::max(height, kMinSize);
    });
}

bool NotesModel::recolor(const QString& id, const QString& color) {
    if (!validColor(color)) return false;
    return update(id, [&](BoardNote& note) { note.color = toStdString(color); });
}

bool NotesModel::targetExists(const QString& kind, const QString& targetId) const {
    if (kind == QLatin1String("concept")) {
        auto conceptId = parseId<KnowledgeObjectId>(targetId);
        return conceptId && workspace_->graph().findNode(*conceptId) != nullptr;
    }
    auto topicId = parseId<TopicId>(targetId);
    return topicId && workspace_->findTopic(*topicId).has_value();
}

bool NotesModel::link(const QString& id, const QString& kind, const QString& targetId) {
    int row = indexOf(id);
    auto target = Uuid::parse(toStdString(targetId));
    if (row < 0 || !validKind(kind) || !target || !targetExists(kind, targetId)) return false;
    NoteLink link{toStdString(kind), *target};
    if (auto saved = repository_.addLink(notes_[static_cast<size_t>(row)].id, link); !saved.hasValue()) {
        return fail(toQString(saved.error().detail));
    }
    auto& links = notes_[static_cast<size_t>(row)].links;
    if (std::find(links.begin(), links.end(), link) == links.end()) links.push_back(link);
    emit dataChanged(index(row), index(row));
    return true;
}

bool NotesModel::unlink(const QString& id, const QString& kind, const QString& targetId) {
    int row = indexOf(id);
    auto target = Uuid::parse(toStdString(targetId));
    if (row < 0 || !validKind(kind) || !target) return false;
    NoteLink link{toStdString(kind), *target};
    if (auto saved = repository_.removeLink(notes_[static_cast<size_t>(row)].id, link); !saved.hasValue()) {
        return fail(toQString(saved.error().detail));
    }
    auto& links = notes_[static_cast<size_t>(row)].links;
    links.erase(std::remove(links.begin(), links.end(), link), links.end());
    emit dataChanged(index(row), index(row));
    return true;
}

bool NotesModel::remove(const QString& id) {
    int row = indexOf(id);
    if (row < 0) return false;
    if (auto removed = repository_.remove(notes_[static_cast<size_t>(row)].id); !removed.hasValue()) {
        return fail(toQString(removed.error().detail));
    }
    beginRemoveRows(QModelIndex(), row, row);
    notes_.erase(notes_.begin() + row);
    endRemoveRows();
    emit countChanged();
    return true;
}

QVariantMap NotesModel::note(const QString& id) const {
    int row = indexOf(id);
    return row < 0 ? QVariantMap() : toMap(notes_[static_cast<size_t>(row)]);
}

}  // namespace atlas::viewmodels
```

Use `workspace_->findTopic` as declared (it may not be const; if so, make `targetExists` non-const or look the topic up through `topics()`). Wire into `AppContext`: member `NotesModel notes_;` after `today_` and before `session_`, constructed with `(database, workspace_, [this] { return memory_.now(); })` (the constructor's clock was moved into `memory_`), loaded in `load()` after `memory_`, provided and reset like the others, accessor `notes()`. Add the files to CMake lists.

- [ ] **Step 4: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add modules/viewmodels
git commit -m "feat(viewmodels): sticky notes model with links"
```

---

### Task 6: Panels and the corner stack

**Files:**
- Create: `modules/qml/Panel.qml`, `modules/qml/PanelLayer.qml`, `modules/qml/IconButton.qml`
- Modify: `modules/viewmodels/include/atlas/viewmodels/app_settings.hpp`, `src/app_settings.cpp`, `modules/qml/CMakeLists.txt`, `modules/qml/tests/CMakeLists.txt`
- Test: `modules/viewmodels/tests/test_appearance.cpp`, `modules/qml/tests/test_panels.cpp`

**Interfaces:**
- Produces:
  - `AppSettings`: `Q_INVOKABLE QVariantMap panelLayout(const QString& id) const` (empty map when nothing is saved) and `Q_INVOKABLE void setPanelLayout(const QString& id, const QVariantMap& layout)`, stored under `panels/<id>/` with keys `mode`, `x`, `y`, `width`, `height`.
  - `Panel` (QML): `required property string panelId`, `title`, `transient` (hidden unless expanded), `dockedHeight` (default 240), `mode` (`docked`, `folded`, `floating`, `expanded`, `hidden`), `floatX`, `floatY`, `floatWidth`, `floatHeight`, `slot` (rect set by the layer), `expandedRect` (rect set by the layer), read-only `target` (the rect the panel should occupy), `titleHeight` 40, default property `content`; functions `fold()`, `unfold()`, `expand()`, `restore()`, `floatAt(x, y)`, `dock()`, `resizeTo(width, height)`, `savedLayout()`; signal `layoutChanged()`.
  - `PanelLayer` (QML): holds `Panel` children, stacks docked and folded panels at the top right (`stackWidth` 320, margin 16, spacing 10), gives every panel `expandedRect` = the layer inset by 48 horizontally and 24 vertically, saves a panel's layout on every `layoutChanged`, restores saved layouts at start, pulls floating panels back inside after restores and resizes, dims the board behind an expanded panel (click or Escape restores it); functions `layoutSlots()`, `closeExpanded()`, `panel(id)`.
  - `IconButton` (QML): `text` (a short glyph), `tip`, `signal clicked()`; 28 by 28, `radiusChip` corners, surfaceHigh on hover.
- Rules: only the title bar starts a drag; dragging a docked or folded panel more than 4 px floats it; floating panels show one soft shadow; geometry `Behavior`s are off while dragging and under reduced motion.

- [ ] **Step 1: Write the failing tests**

Append to `test_appearance.cpp`:

```cpp
TEST_CASE("panel layouts are stored per panel") {
    QTemporaryDir dir;
    QSettings store(dir.filePath("settings.ini"), QSettings::IniFormat);
    AppSettings settings(store);
    CHECK(settings.panelLayout("today").isEmpty());
    settings.setPanelLayout("today", {{"mode", "floating"}, {"x", 120.0}, {"y", 80.0}, {"width", 300.0}, {"height", 200.0}});
    AppSettings again(store);
    auto layout = again.panelLayout("today");
    CHECK(layout.value("mode").toString() == "floating");
    CHECK(layout.value("x").toDouble() == doctest::Approx(120.0));
    CHECK(again.panelLayout("brain").isEmpty());
}
```

`modules/qml/tests/test_panels.cpp`:

```cpp
#include <QRectF>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

const QByteArray kBoard = "import QtQuick\nimport Atlas.Ui\n"
                          "PanelLayer { width: 1200; height: 800\n"
                          "  Panel { objectName: \"today\"; panelId: \"today\"; title: \"Today\" }\n"
                          "  Panel { objectName: \"brain\"; panelId: \"brain\"; title: \"Brain\" }\n"
                          "  Panel { objectName: \"settings\"; panelId: \"settings\"; title: \"Settings\"; transient: true }\n"
                          "}";

QRectF targetOf(QObject* panel) { return panel->property("target").toRectF(); }

}  // namespace

TEST_CASE("docked panels stack at the top right and fold") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    auto layer = f.createFromData(kBoard);
    auto* today = QmlFixture::child(layer.get(), "today");
    auto* brain = QmlFixture::child(layer.get(), "brain");
    CHECK(targetOf(today) == QRectF(864, 16, 320, 240));
    CHECK(targetOf(brain).y() == doctest::Approx(266.0));
    REQUIRE(QMetaObject::invokeMethod(today, "fold"));
    CHECK(today->property("mode").toString() == "folded");
    CHECK(targetOf(today).height() == doctest::Approx(40.0));
    CHECK(targetOf(brain).y() == doctest::Approx(66.0));
}

TEST_CASE("a panel floats, expands, restores, and docks back") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    auto layer = f.createFromData(kBoard);
    auto* brain = QmlFixture::child(layer.get(), "brain");
    REQUIRE(QMetaObject::invokeMethod(brain, "floatAt", Q_ARG(double, 100.0), Q_ARG(double, 120.0)));
    CHECK(brain->property("mode").toString() == "floating");
    CHECK(targetOf(brain).topLeft() == QPointF(100, 120));
    REQUIRE(QMetaObject::invokeMethod(brain, "expand"));
    CHECK(targetOf(brain).width() == doctest::Approx(1104.0));
    REQUIRE(QMetaObject::invokeMethod(layer.get(), "closeExpanded"));
    CHECK(brain->property("mode").toString() == "floating");
    REQUIRE(QMetaObject::invokeMethod(brain, "dock"));
    CHECK(brain->property("mode").toString() == "docked");
}

TEST_CASE("transient panels only show while expanded") {
    QmlFixture f;
    auto layer = f.createFromData(kBoard);
    auto* settings = QmlFixture::child(layer.get(), "settings");
    CHECK(settings->property("mode").toString() == "hidden");
    CHECK_FALSE(settings->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(settings, "expand"));
    CHECK(settings->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(layer.get(), "closeExpanded"));
    CHECK(settings->property("mode").toString() == "hidden");
}

TEST_CASE("layouts survive a restart and come back inside the window") {
    QmlFixture f;
    f.context().settings().setReducedMotion(true);
    {
        auto layer = f.createFromData(kBoard);
        auto* today = QmlFixture::child(layer.get(), "today");
        REQUIRE(QMetaObject::invokeMethod(today, "floatAt", Q_ARG(double, 40.0), Q_ARG(double, 50.0)));
    }
    f.context().settings().setPanelLayout("brain", {{"mode", "floating"}, {"x", 5000.0}, {"y", 3000.0},
                                                    {"width", 300.0}, {"height", 200.0}});
    auto layer = f.createFromData(kBoard);
    auto* today = QmlFixture::child(layer.get(), "today");
    auto* brain = QmlFixture::child(layer.get(), "brain");
    CHECK(today->property("mode").toString() == "floating");
    CHECK(targetOf(today).topLeft() == QPointF(40, 50));
    QRectF placed = targetOf(brain);
    CHECK(placed.right() <= 1200.0);
    CHECK(placed.bottom() <= 800.0);
}
```

Add `test_panels.cpp` to `atlas_qml_tests`.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev && ctest --preset dev -R "atlas_qml_tests|atlas_viewmodels_tests"`
Expected: FAIL, `panelLayout` is unknown and `PanelLayer` is not a type.

- [ ] **Step 3: Implement the settings store**

`app_settings.cpp`:

```cpp
QVariantMap AppSettings::panelLayout(const QString& id) const {
    QString prefix = QStringLiteral("panels/%1/").arg(id);
    if (!store_->contains(prefix + "mode")) return {};
    QVariantMap layout;
    for (const char* key : {"mode", "x", "y", "width", "height"}) layout.insert(key, store_->value(prefix + key));
    return layout;
}

void AppSettings::setPanelLayout(const QString& id, const QVariantMap& layout) {
    QString prefix = QStringLiteral("panels/%1/").arg(id);
    for (auto it = layout.begin(); it != layout.end(); ++it) store_->setValue(prefix + it.key(), it.value());
}
```

Declare both as `Q_INVOKABLE` and include `<QVariantMap>`.

- [ ] **Step 4: Create `IconButton.qml`**

```qml
import QtQuick

Rectangle {
    id: button

    property string text
    property string tip

    signal clicked()

    implicitWidth: 28
    implicitHeight: 28
    radius: Theme.radiusChip
    color: hover.hovered ? Theme.surfaceHigh : "transparent"

    Behavior on color { ColorAnimation { duration: Motion.fast } }

    Text {
        anchors.centerIn: parent
        text: button.text
        color: Theme.onSurfaceMuted
        font.family: Theme.sans
        font.pixelSize: 14
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: button.clicked() }
}
```

- [ ] **Step 5: Create `Panel.qml`**

```qml
import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

Item {
    id: panel

    required property string panelId
    property string title
    property bool transient: false
    property int dockedHeight: 240
    property string mode: transient ? "hidden" : "docked"
    property string returnMode: "docked"
    property real floatX: 0
    property real floatY: 0
    property real floatWidth: 320
    property real floatHeight: dockedHeight
    property rect slot: Qt.rect(0, 0, 320, dockedHeight)
    property rect expandedRect: Qt.rect(0, 0, 640, 480)
    property bool dragging: false
    readonly property int titleHeight: 40
    readonly property rect target: mode === "expanded" ? expandedRect
                                 : mode === "floating" ? Qt.rect(floatX, floatY, floatWidth, floatHeight)
                                 : mode === "folded" ? Qt.rect(slot.x, slot.y, slot.width, titleHeight)
                                 : slot
    default property alias content: body.data

    signal layoutChanged()

    function fold() {
        if (mode !== "docked")
            return
        mode = "folded"
        layoutChanged()
    }

    function unfold() {
        if (mode !== "folded")
            return
        mode = "docked"
        layoutChanged()
    }

    function expand() {
        if (mode === "expanded")
            return
        returnMode = mode === "hidden" ? "docked" : mode
        mode = "expanded"
        layoutChanged()
    }

    function restore() {
        if (mode !== "expanded")
            return
        mode = transient ? "hidden" : returnMode
        layoutChanged()
    }

    function floatAt(px: real, py: real) {
        if (mode !== "floating") {
            floatWidth = Math.max(width, 220)
            floatHeight = Math.max(height, 160)
            mode = "floating"
        }
        floatX = px
        floatY = py
        layoutChanged()
    }

    function dock() {
        if (mode !== "floating")
            return
        mode = "docked"
        layoutChanged()
    }

    function resizeTo(w: real, h: real) {
        floatWidth = Math.max(220, w)
        floatHeight = Math.max(120, h)
        layoutChanged()
    }

    function savedLayout() {
        let kept = transient ? "hidden" : mode === "expanded" ? returnMode : mode
        return { mode: kept, x: floatX, y: floatY, width: floatWidth, height: floatHeight }
    }

    x: target.x
    y: target.y
    width: target.width
    height: target.height
    visible: mode !== "hidden"
    z: mode === "expanded" ? 30 : mode === "floating" ? 20 : 10

    Behavior on x { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }
    Behavior on y { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }
    Behavior on width { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }
    Behavior on height { enabled: !Motion.reduced && !panel.dragging; NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve } }

    Rectangle {
        anchors.fill: frame
        anchors.topMargin: 12
        anchors.leftMargin: 6
        anchors.rightMargin: -6
        anchors.bottomMargin: -12
        radius: Theme.radiusPanel
        color: "#000000"
        opacity: panel.mode === "floating" || panel.dragging ? (AppSettings.darkTheme ? 0.22 : 0.07) : 0
        visible: opacity > 0
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusPanel
        color: Theme.surface
        border.color: Theme.outline
        clip: true

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }

        Item {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: panel.titleHeight

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.OpenHandCursor
                property point pressAt
                property point panelAt
                onPressed: mouse => {
                    pressAt = mapToItem(panel.parent, mouse.x, mouse.y)
                    panelAt = Qt.point(panel.x, panel.y)
                }
                onPositionChanged: mouse => {
                    let now = mapToItem(panel.parent, mouse.x, mouse.y)
                    let dx = now.x - pressAt.x
                    let dy = now.y - pressAt.y
                    if (!panel.dragging && Math.abs(dx) + Math.abs(dy) < 4)
                        return
                    if (panel.mode === "expanded")
                        return
                    panel.dragging = true
                    panel.floatAt(panelAt.x + dx, panelAt.y + dy)
                }
                onReleased: panel.dragging = false
                onDoubleClicked: panel.mode === "expanded" ? panel.restore() : panel.expand()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: panel.title
                    color: Theme.onSurface
                    font.family: Theme.serif
                    font.pixelSize: 15
                    elide: Text.ElideRight
                }
                IconButton {
                    visible: panel.mode === "docked" || panel.mode === "folded"
                    text: panel.mode === "folded" ? "+" : "−"
                    tip: panel.mode === "folded" ? "Unfold" : "Fold"
                    onClicked: panel.mode === "folded" ? panel.unfold() : panel.fold()
                }
                IconButton {
                    visible: panel.mode === "floating"
                    text: "⇲"
                    tip: "Pin back"
                    onClicked: panel.dock()
                }
                IconButton {
                    text: panel.mode === "expanded" ? "↙" : "↗"
                    tip: panel.mode === "expanded" ? "Restore" : "Expand"
                    onClicked: panel.mode === "expanded" ? panel.restore() : panel.expand()
                }
            }
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.bottomMargin: 16
            visible: panel.mode !== "folded"
        }

        Rectangle {
            width: 14
            height: 14
            radius: 4
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 4
            color: Theme.outline
            visible: panel.mode === "floating"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeFDiagCursor
                property point pressAt
                property size sizeAt
                onPressed: mouse => {
                    pressAt = mapToItem(panel.parent, mouse.x, mouse.y)
                    sizeAt = Qt.size(panel.floatWidth, panel.floatHeight)
                    panel.dragging = true
                }
                onPositionChanged: mouse => {
                    let now = mapToItem(panel.parent, mouse.x, mouse.y)
                    panel.resizeTo(sizeAt.width + now.x - pressAt.x, sizeAt.height + now.y - pressAt.y)
                }
                onReleased: panel.dragging = false
            }
        }
    }
}
```

The glyphs are written as `\u` escapes: minus sign, down-left arrow to corner, north-east and south-west arrows. None of them is the long dash.

- [ ] **Step 6: Create `PanelLayer.qml`**

```qml
import QtQuick
import Atlas.ViewModels

Item {
    id: layer

    property int stackWidth: 320
    property int margin: 16
    property int spacing: 10
    property bool restoring: false
    readonly property var panels: {
        let list = []
        for (let i = 0; i < children.length; ++i) {
            if (children[i].panelId !== undefined)
                list.push(children[i])
        }
        return list
    }
    readonly property bool anyExpanded: panels.some(p => p.mode === "expanded")

    function panel(id: string) {
        return panels.find(p => p.panelId === id) || null
    }

    function layoutSlots() {
        let y = margin
        for (const p of panels) {
            p.expandedRect = Qt.rect(48, 24, Math.max(320, width - 96), Math.max(240, height - 48))
            if (p.mode !== "docked" && p.mode !== "folded")
                continue
            p.slot = Qt.rect(width - stackWidth - margin, y, stackWidth, p.dockedHeight)
            y += (p.mode === "folded" ? p.titleHeight : p.dockedHeight) + spacing
        }
    }

    function clampFloating() {
        if (width <= 0 || height <= 0)
            return
        for (const p of panels) {
            if (p.mode !== "floating")
                continue
            p.floatWidth = Math.min(p.floatWidth, width)
            p.floatHeight = Math.min(p.floatHeight, height)
            p.floatX = Math.max(0, Math.min(p.floatX, width - p.floatWidth))
            p.floatY = Math.max(0, Math.min(p.floatY, height - p.floatHeight))
        }
    }

    function save(p) {
        if (!restoring)
            AppSettings.setPanelLayout(p.panelId, p.savedLayout())
    }

    function restoreAll() {
        restoring = true
        for (const p of panels) {
            let saved = AppSettings.panelLayout(p.panelId)
            if (saved.mode === undefined || p.transient)
                continue
            p.floatX = Number(saved.x)
            p.floatY = Number(saved.y)
            p.floatWidth = Number(saved.width)
            p.floatHeight = Number(saved.height)
            p.mode = ["docked", "folded", "floating"].includes(saved.mode) ? saved.mode : "docked"
        }
        clampFloating()
        layoutSlots()
        restoring = false
    }

    function closeExpanded() {
        for (const p of panels)
            p.restore()
    }

    onWidthChanged: { clampFloating(); layoutSlots() }
    onHeightChanged: { clampFloating(); layoutSlots() }

    Component.onCompleted: {
        for (const p of panels) {
            let item = p
            item.layoutChanged.connect(() => {
                layer.layoutSlots()
                layer.save(item)
            })
        }
        restoreAll()
    }

    Rectangle {
        anchors.fill: parent
        z: 25
        color: Theme.background
        opacity: layer.anyExpanded ? 0.72 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: Motion.appear } }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: layer.closeExpanded()
            onWheel: wheel => wheel.accepted = true
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: layer.anyExpanded
        onActivated: layer.closeExpanded()
    }
}
```

Add `Panel.qml`, `PanelLayer.qml`, `IconButton.qml` to `QML_FILES`.

- [ ] **Step 7: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add modules/viewmodels modules/qml
git commit -m "feat(qml): panels that dock, fold, float, and expand, with saved layout"
```

---

### Task 7: The new shell

**Files:**
- Create: `modules/qml/TopBar.qml`, `modules/qml/TodayPanel.qml`, `modules/qml/RegionLabels.qml`
- Modify: `modules/qml/Main.qml` (rewrite), `modules/qml/MapOverlay.qml`, `modules/qml/TopicBar.qml`, `modules/qml/SettingsScreen.qml`, `modules/qml/CMakeLists.txt`
- Delete: `modules/qml/NavRail.qml`, `modules/qml/TodayOverlay.qml`
- Test: `modules/qml/tests/test_shell.cpp` (rewrite), `test_session_screen.cpp`, `test_settings.cpp`, `test_map.cpp` (updates)

**Interfaces:**
- Consumes: Task 3 `MapView.regions`, `MapView.moveTopic`; Task 2 `canvas.mapToScreen`, `canvas.fitWorldRect`, `canvas.zoom`, `canvas.collapsed`; Task 6 `Panel`, `PanelLayer`.
- Produces:
  - `Main`: `readonly property string mode` (`board` when `Session.stage` is idle, else `session`); `function startSession(): bool` (saves a dirty concept first; returns whether a session started); `function openSettings()`; children `graphCanvas`, `regionLabels`, `mapOverlay`, `topBar`, `panelLayer`, `todayPanel` (the content inside the Panel with id `today`), `settingsPanel` (transient Panel `settings`), `settingsScreen`, `sessionScreen`, `toast`.
  - `TopBar`: height 52; wordmark "Atlas" (Newsreader 22) that opens a menu with Settings, Fit the board, Tidy layout; the embedded `TopicBar` (object name `topicBar`); a primary `recallButton` reading "Recall N" (N = `Today.itemCount`, disabled at 0); signals `recallRequested()`, `settingsRequested()`, `focusRequested(string id)`, `fitRequested()`; function `focusNewConcept()`.
  - `TopicBar`: new `property bool embedded` (transparent, no border, hides its own Fit and Tidy buttons).
  - `TodayPanel`: `headline`, `action` (`add`, `map`, `session`), `actionText` ("Add your first concept", "Explore the map", "Start session"), `signal actionTriggered(string action)`, `function act()`.
  - `RegionLabels`: one label per region at the region's top left (name in Newsreader 16 tinted with the region hue, caption in Plex Mono 10 faint); click zooms to the region; drag moves the topic on release; hidden while the canvas is collapsed.
  - `SettingsScreen`: content only (no `shown`, no full-screen background); fits inside a panel.
  - `MapOverlay`: no longer owns the topic bar; `focusNewConcept` removed (Main calls `topBar.focusNewConcept()`); the concept panel sits at the left edge until Task 8.
- Rules: the left rail is gone; Today is the first docked panel; Settings opens as an expanded transient panel; the top bar is disabled during a session.

- [ ] **Step 1: Update the tests**

Rewrite `modules/qml/tests/test_shell.cpp`:

```cpp
#include <QColor>

#include "doctest.h"
#include "qml_fixture.hpp"

TEST_CASE("first run with an empty map says so") {
    QmlFixture f;
    auto window = f.create("Main");
    CHECK(window->property("mode").toString() == "board");
    auto* today = QmlFixture::child(window.get(), "todayPanel");
    CHECK(today->property("headline").toString() == "Your map is empty");
    CHECK(today->property("actionText").toString() == "Add your first concept");
    REQUIRE(QMetaObject::invokeMethod(today, "act"));
    CHECK(window->property("mode").toString() == "board");
}

TEST_CASE("Today and the recall button count the work waiting") {
    QmlFixture f;
    f.context().map().createConcept("Recursion");
    f.context().map().createConcept("Stack");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* today = QmlFixture::child(window.get(), "todayPanel");
    CHECK(today->property("headline").toString().startsWith("2 items"));
    CHECK(today->property("actionText").toString() == "Start session");
    CHECK(QmlFixture::child(window.get(), "recallButton")->property("text").toString() == "Recall 2");
}

TEST_CASE("the recall button starts a session and locks the top bar") {
    QmlFixture f;
    f.context().map().createConcept("Recursion");
    auto window = f.create("Main");
    QmlFixture::settle();
    auto* bar = QmlFixture::child(window.get(), "topBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "recallRequested"));
    CHECK(window->property("mode").toString() == "session");
    CHECK_FALSE(bar->property("enabled").toBool());
}

TEST_CASE("settings open as an expanded panel") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* settings = QmlFixture::child(window.get(), "settingsPanel");
    CHECK(settings->property("mode").toString() == "hidden");
    REQUIRE(QMetaObject::invokeMethod(window.get(), "openSettings"));
    CHECK(settings->property("mode").toString() == "expanded");
    REQUIRE(QMetaObject::invokeMethod(QmlFixture::child(window.get(), "panelLayer"), "closeExpanded"));
    CHECK(settings->property("mode").toString() == "hidden");
}

TEST_CASE("topics show as labeled regions on the board") {
    QmlFixture f;
    QString os = f.context().topics().createTopic("Operating Systems");
    f.context().map().setTopicId(os);
    f.context().map().createConcept("Paging");
    f.context().map().setTopicId(QString());
    auto window = f.create("Main");
    QmlFixture::settle();
    auto labels = QmlFixture::child(window.get(), "regionLabels")->findChildren<QObject*>("regionLabel");
    REQUIRE(labels.size() == 1);
    CHECK(labels[0]->property("topicName").toString() == "Operating Systems");
}

TEST_CASE("switching the theme recolors the window at once") {
    QmlFixture f;
    auto window = f.create("Main");
    QColor dark = window->property("color").value<QColor>();
    f.context().settings().setDarkTheme(false);
    QColor light = window->property("color").value<QColor>();
    CHECK(light != dark);
    CHECK(light == f.context().palette().background());
}

TEST_CASE("reduced motion turns off canvas camera animation") {
    QmlFixture f;
    auto window = f.create("Main");
    auto* canvas = QmlFixture::child(window.get(), "graphCanvas");
    CHECK(canvas->property("animated").toBool());
    f.context().settings().setReducedMotion(true);
    CHECK_FALSE(canvas->property("animated").toBool());
}

TEST_CASE("view model errors reach the toast") {
    QmlFixture f;
    auto window = f.create("Main");
    emit f.context().topics().errorOccurred("Could not rename the topic");
    auto* toast = QmlFixture::child(window.get(), "toast");
    CHECK(toast->property("shown").toBool());
    CHECK(toast->property("text").toString() == "Could not rename the topic");
}
```

In `test_session_screen.cpp`: replace every `QmlFixture::child(window.get(), "todayOverlay")` used only to start a session with `REQUIRE(QMetaObject::invokeMethod(window.get(), "startSession"));` (keep the cases that check Today's `actionText`, renaming the child to `todayPanel`); replace `property("page")` checks: `"session"` stays `"session"` but reads `property("mode")`, and `"map"` or `"today"` become `"board"`; replace the `navRail` enabled check with `topBar`. In `test_settings.cpp`, create `SettingsScreen` without the `shown` property. In `test_map.cpp`, replace any use of `mapOverlay.focusNewConcept` with `topBar.focusNewConcept`.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_qml_tests`
Expected: FAIL, `mode`, `todayPanel`, `topBar`, and `regionLabels` do not exist.

- [ ] **Step 3: Create `TodayPanel.qml`**

```qml
import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

ColumnLayout {
    id: today

    readonly property string headline: Today.empty ? "Your map is empty"
                                     : Today.caughtUp ? "All caught up"
                                     : Today.itemCount + (Today.itemCount === 1 ? " item" : " items")
                                       + ", about " + Today.estimatedMinutes + " min"
    readonly property string action: Today.empty ? "add" : Today.caughtUp ? "map" : "session"
    readonly property string actionText: action === "add" ? "Add your first concept"
                                       : action === "map" ? "Explore the map"
                                       : "Start session"

    signal actionTriggered(string action)

    function act() {
        actionTriggered(action)
    }

    spacing: 8

    Text {
        Layout.fillWidth: true
        text: today.headline
        color: Theme.onSurface
        font.family: Theme.serif
        font.pixelSize: Theme.fontHeadline
        wrapMode: Text.Wrap
    }
    Text {
        Layout.fillWidth: true
        visible: !Today.empty
        text: Today.dueCount + " due, " + Today.newCount + " new  ·  " + Today.learnedCount + " of "
              + Today.conceptCount + " learned"
        color: Theme.onSurfaceFaint
        font.family: Theme.mono
        font.pixelSize: Theme.fontSmall
    }
    Text {
        Layout.fillWidth: true
        text: Today.empty ? "Start with one idea you want to keep, then link it to what you know."
                          : "Short daily recall beats one long session."
        color: Theme.onSurfaceMuted
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }
    Item { Layout.fillHeight: true }
    AppButton {
        primary: true
        text: today.actionText
        onClicked: today.act()
    }
}
```

- [ ] **Step 4: Create `TopBar.qml` and embed `TopicBar`**

```qml
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: bar

    signal recallRequested()
    signal settingsRequested()
    signal focusRequested(string id)
    signal fitRequested()

    function focusNewConcept() {
        topics.focusNewConcept()
    }

    implicitHeight: 52
    color: Theme.background
    opacity: enabled ? 1 : 0.55

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.outline
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 16
        spacing: 12

        Text {
            id: wordmark
            text: "Atlas"
            color: Theme.onSurface
            font.family: Theme.serif
            font.pixelSize: 22

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: menu.popup(wordmark, 0, wordmark.height + 8) }
        }

        TopicBar {
            id: topics
            objectName: "topicBar"
            Layout.fillWidth: true
            embedded: true
            onFocusRequested: id => bar.focusRequested(id)
            onFitRequested: bar.fitRequested()
        }

        AppButton {
            objectName: "recallButton"
            primary: true
            enabled: Today.itemCount > 0
            text: Today.itemCount > 0 ? "Recall " + Today.itemCount : "Recall"
            onClicked: bar.recallRequested()
        }
    }

    Menu {
        id: menu

        MenuItem { text: "Settings"; onTriggered: bar.settingsRequested() }
        MenuItem { text: "Fit the board"; onTriggered: bar.fitRequested() }
        MenuItem { text: "Tidy layout"; onTriggered: MapView.tidy() }
    }
}
```

In `TopicBar.qml`: add `property bool embedded: false`; set `color: embedded ? "transparent" : Theme.surface`, `border.width: embedded ? 0 : 1`; add `visible: !bar.embedded` to the Fit and Tidy `AppButton`s; keep every function and object name.

- [ ] **Step 5: Create `RegionLabels.qml`**

```qml
import QtQuick
import Atlas.ViewModels

Item {
    id: labels

    property Item canvas
    property bool interactive: true
    property int viewTick: 0

    visible: canvas !== null && !canvas.collapsed

    Connections {
        target: labels.canvas
        function onViewChanged() { labels.viewTick++ }
        function onZoomChanged() { labels.viewTick++ }
    }
    Connections {
        target: MapView
        function onSceneChanged() { labels.viewTick++ }
    }

    Repeater {
        model: MapView.regions

        delegate: Item {
            id: label
            objectName: "regionLabel"

            required property var modelData
            readonly property string topicName: modelData.name
            readonly property point anchor: {
                labels.viewTick
                return labels.canvas ? labels.canvas.mapToScreen(modelData.x, modelData.y) : Qt.point(0, 0)
            }
            property real dragX: 0
            property real dragY: 0

            x: anchor.x + 20 + dragX
            y: anchor.y + 14 + dragY
            width: column.implicitWidth
            height: column.implicitHeight

            Column {
                id: column
                spacing: 2

                Text {
                    text: label.modelData.name
                    color: { Palette.background; return Theme.regionTint(label.modelData.hue) }
                    font.family: Theme.serif
                    font.pixelSize: 16
                }
                Text {
                    text: label.modelData.caption
                    color: Theme.onSurfaceFaint
                    font.family: Theme.mono
                    font.pixelSize: Theme.fontCaption
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: labels.interactive
                cursorShape: Qt.PointingHandCursor
                property point pressAt
                property bool moved: false
                onPressed: mouse => {
                    pressAt = mapToItem(labels, mouse.x, mouse.y)
                    moved = false
                }
                onPositionChanged: mouse => {
                    let now = mapToItem(labels, mouse.x, mouse.y)
                    label.dragX = now.x - pressAt.x
                    label.dragY = now.y - pressAt.y
                    moved = moved || Math.abs(label.dragX) + Math.abs(label.dragY) > 4
                }
                onReleased: {
                    let scale = labels.canvas.zoom
                    if (moved)
                        MapView.moveTopic(label.modelData.topicId, label.dragX / scale, label.dragY / scale)
                    else
                        labels.canvas.fitWorldRect(label.modelData.x, label.modelData.y,
                                                   label.modelData.width, label.modelData.height)
                    label.dragX = 0
                    label.dragY = 0
                }
            }
        }
    }
}
```

- [ ] **Step 6: Make `SettingsScreen` panel content**

Change its root from a full-screen `Rectangle` to `Item { id: screen; implicitWidth: column.implicitWidth; implicitHeight: column.implicitHeight }`, remove `shown`, the opacity, its `Behavior`, and the full-area `MouseArea`; anchor its `ColumnLayout` (give it `id: column`) with `anchors.left/right/top: parent.*` and `width` following the parent instead of the fixed 560 cap. Keep every object name and the switch and spin box behavior. Make the "Settings" title `Theme.serif` at `Theme.fontHeadline`; the Panel title bar already says Settings, so drop the in-content title if both read the same.

- [ ] **Step 7: Rewrite `Main.qml`**

```qml
import QtQuick
import QtQuick.Controls.Basic
import Atlas.Render
import Atlas.ViewModels

ApplicationWindow {
    id: window

    readonly property string mode: Session.stage === "idle" ? "board" : "session"

    function startSession(): bool {
        if (Concept.dirty && !Concept.save())
            return false
        return Session.start()
    }

    function openSettings() {
        settingsPanel.expand()
    }

    width: 1280
    height: 820
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: "Atlas"
    color: Theme.background
    font.family: Theme.sans

    palette.window: Theme.surfaceHigh
    palette.windowText: Theme.onSurface
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceHigh
    palette.text: Theme.onSurface
    palette.button: Theme.surface
    palette.buttonText: Theme.onSurface
    palette.highlight: Theme.primary
    palette.highlightedText: Theme.onPrimary
    palette.light: Theme.surfaceHigh
    palette.midlight: Theme.surfaceHigh
    palette.mid: Theme.outline
    palette.dark: Theme.outline
    palette.placeholderText: Theme.onSurfaceFaint

    onActiveChanged: {
        if (active) {
            Today.refresh()
            MapView.refresh()
        }
    }

    onClosing: close => {
        if (Concept.dirty && !Concept.save())
            close.accepted = false
    }

    TopBar {
        id: topBar
        objectName: "topBar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 5
        enabled: window.mode === "board"
        onRecallRequested: window.startSession()
        onSettingsRequested: window.openSettings()
        onFocusRequested: id => mapOverlay.focusConcept(id)
        onFitRequested: canvas.fitToContent()
    }

    Item {
        id: board
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        clip: true

        GraphCanvas {
            id: canvas
            objectName: "graphCanvas"
            anchors.fill: parent
            animated: !AppSettings.reducedMotion
            Component.onCompleted: MapView.attach(canvas)
        }

        RegionLabels {
            objectName: "regionLabels"
            anchors.fill: parent
            canvas: canvas
            interactive: window.mode === "board"
        }

        MapOverlay {
            id: mapOverlay
            objectName: "mapOverlay"
            anchors.fill: parent
            canvas: canvas
            active: window.mode === "board"
        }

        SessionScreen {
            objectName: "sessionScreen"
            anchors.fill: parent
            canvas: canvas
            shown: window.mode === "session"
        }

        PanelLayer {
            id: panelLayer
            objectName: "panelLayer"
            anchors.fill: parent

            Panel {
                panelId: "today"
                title: "Today"
                dockedHeight: 230

                TodayPanel {
                    objectName: "todayPanel"
                    anchors.fill: parent
                    onActionTriggered: action => {
                        if (action === "session")
                            window.startSession()
                        else if (action === "add")
                            topBar.focusNewConcept()
                    }
                }
            }

            Panel {
                id: settingsPanel
                objectName: "settingsPanel"
                panelId: "settings"
                title: "Settings"
                transient: true

                SettingsScreen {
                    objectName: "settingsScreen"
                    anchors.fill: parent
                }
            }
        }

        Toast {
            id: toast
            objectName: "toast"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            z: 40
        }
    }

    Connections {
        target: MapView
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: Topics
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: Concept
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: ConceptLinks
        function onErrorOccurred(message) { toast.show(message) }
    }
    Connections {
        target: Session
        function onErrorOccurred(message) { toast.show(message) }
    }
}
```

`SessionScreen.onDone` is no longer needed (the mode follows `Session.stage`); remove the handler, keep the signal.

- [ ] **Step 8: Adjust `MapOverlay.qml`**

Remove the `TopicBar` block and `focusNewConcept()`. Anchor the `ConceptPanel` to the left edge (`anchors.left: parent.left`, `anchors.top: parent.top`, `anchors.bottom: parent.bottom`, margins `Theme.gap * 2`) with its slide `Behavior` on `anchors.leftMargin` instead of the right; Task 8 replaces this placement. The empty-map hint now reads "Type a concept name in the bar above and press Enter."

Delete `NavRail.qml` and `TodayOverlay.qml`; update `QML_FILES` (remove those two, add `TopBar.qml`, `TodayPanel.qml`, `RegionLabels.qml`).

- [ ] **Step 9: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS with zero QML warnings.

- [ ] **Step 10: Commit**

```bash
git add -A modules/qml
git commit -m "feat(qml): pinboard shell with a top bar, corner stack, and topic regions"
```

---

### Task 8: Concept cards beside their nodes

**Files:**
- Create: `modules/qml/ConceptCard.qml`, `modules/qml/PinnedCard.qml`
- Modify: `modules/qml/MapOverlay.qml`, `modules/qml/CMakeLists.txt`
- Test: `modules/qml/tests/test_concept_cards.cpp`

**Interfaces:**
- Consumes: Task 2 `canvas.screenPositionOf`, `viewChanged`; Task 3 `MapView.conceptInfo(id).definition`; existing `ConceptPanel` (object name `conceptPanel`, unchanged API).
- Produces:
  - `MapOverlay`: `function besideRect(point, cardWidth, cardHeight, areaWidth, areaHeight)` returning `Qt.rect` (to the right of the point by 28, or to the left when that would leave the area; clamped 16 from every edge); `property var pinnedIds` (at most 3); `function pinCurrent(): bool`; `function unpin(id: string)`; children `conceptCard` and `pinnedCard` delegates.
  - `ConceptCard`: wraps `ConceptPanel`; `detached`, `expanded`, `detachedX`, `detachedY`; functions `detachTo(x, y)`, `attach()`, `expand()`, `restore()`; a 32 px header strip with a grip (drag detaches), Pin, and Expand buttons; attached cards follow their node on every view change; selecting a different concept attaches and restores the card (`Concept.loaded` also fires on memory changes, so the card compares ids before resetting).
  - `PinnedCard`: `required property string conceptId`, `canvas`; a compact 240 px summary (title in Newsreader, topic and recall in Plex Mono, ring, three lines of definition), beside its own node; buttons Edit (selects it) and Unpin.
- Rules: one editable card (the selected concept) plus up to three pinned summaries; the selected concept never also shows a pinned summary; pinned ids that no longer exist are dropped on scene changes.

- [ ] **Step 1: Write the failing tests**

`modules/qml/tests/test_concept_cards.cpp`:

```cpp
#include <QRectF>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

struct CardFixture {
    QmlFixture f;
    QString a = f.context().map().createConcept("Alpha");
    QString b = f.context().map().createConcept("Beta");
    QString c = f.context().map().createConcept("Gamma");
    QString d = f.context().map().createConcept("Delta");
    QString e = f.context().map().createConcept("Epsilon");
    std::unique_ptr<QObject> window;
    QObject* overlay = nullptr;
    QObject* card = nullptr;

    CardFixture() {
        f.context().settings().setReducedMotion(true);
        window = f.create("Main");
        QmlFixture::settle();
        overlay = QmlFixture::child(window.get(), "mapOverlay");
        card = QmlFixture::child(window.get(), "conceptCard");
    }

    void select(const QString& id) {
        f.context().map().setSelectedId(id);
        QmlFixture::settle();
    }

    QRectF beside(double x, double y) {
        QVariant rect;
        REQUIRE(QMetaObject::invokeMethod(overlay, "besideRect", Q_RETURN_ARG(QVariant, rect),
                                          Q_ARG(QVariant, QVariant::fromValue(QPointF(x, y))), Q_ARG(QVariant, 380),
                                          Q_ARG(QVariant, 500), Q_ARG(QVariant, 1200), Q_ARG(QVariant, 800)));
        return rect.toRectF();
    }
};

}  // namespace

TEST_CASE("cards open beside their node and flip near the right edge") {
    CardFixture p;
    QRectF right = p.beside(300, 300);
    CHECK(right.left() == doctest::Approx(328.0));
    QRectF flipped = p.beside(1000, 300);
    CHECK(flipped.right() <= 1000.0 - 28.0 + 1e-6);
    CHECK(flipped.left() >= 16.0);
    QRectF low = p.beside(300, 790);
    CHECK(low.bottom() <= 800.0 - 16.0 + 1e-6);
}

TEST_CASE("the editable card follows the selection and can detach and expand") {
    CardFixture p;
    p.select(p.a);
    CHECK(p.card->property("visible").toBool());
    REQUIRE(QMetaObject::invokeMethod(p.card, "detachTo", Q_ARG(double, 60.0), Q_ARG(double, 70.0)));
    CHECK(p.card->property("detached").toBool());
    CHECK(p.card->property("x").toDouble() == doctest::Approx(60.0));
    REQUIRE(QMetaObject::invokeMethod(p.card, "expand"));
    CHECK(p.card->property("width").toDouble() > 900.0);
    p.select(p.b);
    CHECK_FALSE(p.card->property("detached").toBool());
    CHECK_FALSE(p.card->property("expanded").toBool());
}

TEST_CASE("up to three concepts stay pinned as summaries") {
    CardFixture p;
    for (const QString& id : {p.a, p.b, p.c, p.d}) {
        p.select(id);
        bool pinned = false;
        REQUIRE(QMetaObject::invokeMethod(p.overlay, "pinCurrent", Q_RETURN_ARG(bool, pinned)));
        CHECK(pinned == (id != p.d));
    }
    p.select(p.e);
    auto summaries = p.window->findChildren<QObject*>("pinnedCard");
    CHECK(summaries.size() == 3);
    REQUIRE(QMetaObject::invokeMethod(summaries[0], "edit"));
    QmlFixture::settle();
    CHECK(p.f.context().map().selectedId() == summaries[0]->property("conceptId").toString());
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_qml_tests`
Expected: FAIL, no `conceptCard` child and no `besideRect`.

- [ ] **Step 3: Create `ConceptCard.qml`**

```qml
import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

Item {
    id: card

    property Item canvas
    property Item area
    property bool detached: false
    property bool expanded: false
    property real detachedX: 0
    property real detachedY: 0
    property int viewTick: 0
    readonly property real cardWidth: Theme.panelWidth
    readonly property real cardHeight: Math.min(area ? area.height - 32 : 600, 620)
    readonly property var nodePoint: {
        viewTick
        return canvas && Concept.exists ? canvas.screenPositionOf(Concept.conceptId) : undefined
    }
    readonly property rect besideRect: area && nodePoint !== undefined
        ? area.besideRect(nodePoint, cardWidth, cardHeight, area.width, area.height)
        : Qt.rect(16, 16, cardWidth, cardHeight)

    signal focusRequested(string id)
    signal pinRequested()

    function detachTo(px: real, py: real) {
        detachedX = px
        detachedY = py
        detached = true
    }

    function attach() {
        detached = false
    }

    function expand() {
        expanded = true
    }

    function restore() {
        expanded = false
    }

    x: expanded ? 48 : detached ? detachedX : besideRect.x
    y: expanded ? 24 : detached ? detachedY : besideRect.y
    width: expanded && area ? area.width - 96 : cardWidth
    height: expanded && area ? area.height - 48 : cardHeight
    visible: editor.shown
    z: expanded ? 30 : 15

    Connections {
        target: card.canvas
        function onViewChanged() { card.viewTick++ }
    }
    Connections {
        target: MapView
        function onSceneChanged() { card.viewTick++ }
    }
    property string shownId

    Connections {
        target: Concept
        function onLoaded() {
            card.viewTick++
            if (Concept.conceptId === card.shownId)
                return
            card.shownId = Concept.conceptId
            card.detached = false
            card.expanded = false
        }
    }

    Rectangle {
        id: strip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 32
        radius: Theme.radiusPanel
        color: Theme.surface
        border.color: Theme.outline

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.OpenHandCursor
            property point pressAt
            property point cardAt
            onPressed: mouse => {
                pressAt = mapToItem(card.parent, mouse.x, mouse.y)
                cardAt = Qt.point(card.x, card.y)
            }
            onPositionChanged: mouse => {
                let now = mapToItem(card.parent, mouse.x, mouse.y)
                if (Math.abs(now.x - pressAt.x) + Math.abs(now.y - pressAt.y) > 4 && !card.expanded)
                    card.detachTo(cardAt.x + now.x - pressAt.x, cardAt.y + now.y - pressAt.y)
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 4
            spacing: 2

            Rectangle {
                width: 28
                height: 4
                radius: 2
                color: Theme.outlineStrong
            }
            Item { Layout.fillWidth: true }
            IconButton {
                visible: card.detached
                text: "⌖"
                tip: "Back to its node"
                onClicked: card.attach()
            }
            IconButton {
                text: "†"
                tip: "Pin"
                onClicked: card.pinRequested()
            }
            IconButton {
                text: card.expanded ? "↙" : "↗"
                tip: card.expanded ? "Restore" : "Expand"
                onClicked: card.expanded ? card.restore() : card.expand()
            }
        }
    }

    ConceptPanel {
        id: editor
        objectName: "conceptPanel"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: strip.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        onFocusRequested: id => card.focusRequested(id)
    }
}
```

The glyphs are `\u` escapes (position indicator, dagger used as a pin mark, arrows), none of them the long dash.

- [ ] **Step 4: Create `PinnedCard.qml`**

```qml
import QtQuick
import QtQuick.Layouts
import Atlas.ViewModels

Rectangle {
    id: pinned
    objectName: "pinnedCard"

    required property string conceptId
    property Item canvas
    property Item area
    property int viewTick: 0
    property var info: MapView.conceptInfo(conceptId)
    readonly property var nodePoint: {
        viewTick
        return canvas ? canvas.screenPositionOf(conceptId) : undefined
    }
    readonly property rect place: area && nodePoint !== undefined
        ? area.besideRect(nodePoint, 240, height, area.width, area.height)
        : Qt.rect(16, 16, 240, height)

    signal unpinRequested(string id)

    function edit() {
        MapView.selectedId = conceptId
    }

    x: place.x
    y: place.y
    width: 240
    height: column.implicitHeight + 28
    radius: Theme.radiusPanel
    color: Theme.surface
    border.color: Theme.outline
    z: 14

    Connections {
        target: pinned.canvas
        function onViewChanged() { pinned.viewTick++ }
    }
    Connections {
        target: MapView
        function onSceneChanged() {
            pinned.info = MapView.conceptInfo(pinned.conceptId)
            pinned.viewTick++
        }
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Ring {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                thickness: 2
                recall: pinned.info.recall !== undefined ? pinned.info.recall : -1
            }
            Text {
                Layout.fillWidth: true
                text: pinned.info.title || ""
                color: Theme.onSurface
                font.family: Theme.serif
                font.pixelSize: Theme.fontTitle
                elide: Text.ElideRight
            }
        }
        Text {
            Layout.fillWidth: true
            text: (pinned.info.topic || "") + (pinned.info.recall >= 0 ? "  ·  " + Math.round(pinned.info.recall * 100) + "%" : "")
            color: Theme.onSurfaceFaint
            font.family: Theme.mono
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: pinned.info.definition || ""
            color: Theme.onSurfaceMuted
            font.pixelSize: Theme.fontSmall + 1
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }
            AppButton {
                text: "Unpin"
                onClicked: pinned.unpinRequested(pinned.conceptId)
            }
            AppButton {
                text: "Edit"
                primary: true
                onClicked: pinned.edit()
            }
        }
    }
}
```

- [ ] **Step 5: Wire cards into `MapOverlay.qml`**

Add:

```qml
    property var pinnedIds: []

    function besideRect(point, cardWidth, cardHeight, areaWidth, areaHeight) {
        const gapToNode = 28
        const edge = 16
        let x = point.x + gapToNode
        if (x + cardWidth > areaWidth - edge)
            x = point.x - gapToNode - cardWidth
        x = Math.max(edge, Math.min(x, areaWidth - cardWidth - edge))
        let y = Math.max(edge, Math.min(point.y - 80, areaHeight - cardHeight - edge))
        return Qt.rect(x, y, cardWidth, cardHeight)
    }

    function pinCurrent(): bool {
        if (!Concept.exists || pinnedIds.includes(Concept.conceptId) || pinnedIds.length >= 3)
            return false
        pinnedIds = pinnedIds.concat([Concept.conceptId])
        return true
    }

    function unpin(id: string) {
        pinnedIds = pinnedIds.filter(pinnedId => pinnedId !== id)
    }
```

Replace the `ConceptPanel` block with:

```qml
    ConceptCard {
        id: conceptCard
        objectName: "conceptCard"
        canvas: overlay.canvas
        area: overlay
        onFocusRequested: id => overlay.focusConcept(id)
        onPinRequested: overlay.pinCurrent()
    }

    Repeater {
        model: overlay.pinnedIds.filter(id => id !== Concept.conceptId)

        delegate: PinnedCard {
            required property string modelData
            conceptId: modelData
            canvas: overlay.canvas
            area: overlay
            onUnpinRequested: id => overlay.unpin(id)
        }
    }
```

In the existing `MapView` `Connections`, extend `onSceneChanged` to drop pinned ids whose `MapView.conceptInfo(id)` is empty. Add `ConceptCard.qml` and `PinnedCard.qml` to `QML_FILES`.

- [ ] **Step 6: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS, including the existing concept panel tests (the editor kept its object name).

- [ ] **Step 7: Commit**

```bash
git add modules/qml
git commit -m "feat(qml): concept cards beside their nodes with pinned summaries"
```

---

### Task 9: Sticky notes on the board

**Files:**
- Create: `modules/qml/NotesLayer.qml`, `modules/qml/StickyNote.qml`
- Modify: `modules/qml/Main.qml`, `modules/qml/TopicBar.qml`, `modules/qml/TopBar.qml`, `modules/qml/CMakeLists.txt`
- Test: `modules/qml/tests/test_notes.cpp`

**Interfaces:**
- Consumes: Task 5 `Notes` (roles `noteId`, `body`, `colorName`, `worldX`, `worldY`, `worldWidth`, `worldHeight`, `links`); Task 2 `mapToScreen`, `mapToWorld`, `nodeAt`, `backgroundDoubleClicked`, `zoom`; Task 3 `MapView.regions`.
- Produces:
  - `NotesLayer` (object name `notesLayer`): `canvas`, `interactive`; functions `createAtScreen(x, y, body)` (returns id), `moveNoteTo(id, x, y)`, `linkNoteAt(id, x, y)` (concept under the point, else the topic region containing it; returns bool), `screenRectOf(worldX, worldY, worldWidth, worldHeight)`, `beginDraft(id, point)`, `moveDraft(point)`, `endDraft(point)`; draws link lines as dashed 1.5 px lines in the note's text color from the note's edge to the concept's screen position or the region's center.
  - `StickyNote` (object name `stickyNote`): positioned at its world rect mapped to screen, size scaled by `canvas.zoom`; header (16 px) drag moves; text area edits the body (saved on `editingFinished`); three color dots; a close button removes; a link handle on the right edge drags a line and links on release; text hidden below zoom 0.45.
  - `TopicBar`: `signal noteRequested(string body)`; `addConcept` treats input starting with `note:` (any case) as a note.
  - `TopBar`: re-emits `noteRequested(string body)`.
  - `Main`: double click on empty board (board mode) creates a note there; `noteRequested` creates one at the board center.

- [ ] **Step 1: Write the failing tests**

`modules/qml/tests/test_notes.cpp`:

```cpp
#include <QColor>
#include <QPointF>

#include "doctest.h"
#include "qml_fixture.hpp"

namespace {

struct NoteFixture {
    QmlFixture f;
    QString concept_ = f.context().map().createConcept("Paging");
    std::unique_ptr<QObject> window;
    QObject* canvas = nullptr;
    QObject* layer = nullptr;

    NoteFixture() {
        f.context().settings().setReducedMotion(true);
        window = f.create("Main");
        QmlFixture::settle();
        canvas = QmlFixture::child(window.get(), "graphCanvas");
        layer = QmlFixture::child(window.get(), "notesLayer");
    }

    QPointF toScreen(double x, double y) {
        QPointF point;
        REQUIRE(QMetaObject::invokeMethod(canvas, "mapToScreen", Q_RETURN_ARG(QPointF, point), Q_ARG(double, x),
                                          Q_ARG(double, y)));
        return point;
    }
};

}  // namespace

TEST_CASE("double clicking empty board creates a note there") {
    NoteFixture n;
    QPointF empty = n.toScreen(2000, 2000);
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "handleDoubleClick", Q_ARG(double, empty.x()), Q_ARG(double, empty.y())));
    QmlFixture::settle();
    REQUIRE(n.f.context().notes().count() == 1);
    CHECK(n.window->findChildren<QObject*>("stickyNote").size() == 1);
}

TEST_CASE("typing note: in the bar creates a note with that text") {
    NoteFixture n;
    auto* bar = QmlFixture::child(n.window.get(), "topicBar");
    REQUIRE(QMetaObject::invokeMethod(bar, "addConcept", Q_ARG(QString, "Note: Why half full?")));
    QmlFixture::settle();
    REQUIRE(n.f.context().notes().count() == 1);
    auto id = n.f.context().notes().data(n.f.context().notes().index(0), atlas::viewmodels::NotesModel::IdRole).toString();
    CHECK(n.f.context().notes().note(id).value("body").toString() == "Why half full?");
    CHECK(n.f.context().map().conceptCount() == 1);
}

TEST_CASE("notes keep their world place while the board zooms") {
    NoteFixture n;
    QString id = n.f.context().notes().create(100, 100, "Stay");
    QmlFixture::settle();
    auto* note = QmlFixture::child(n.window.get(), "stickyNote");
    double width = note->property("width").toDouble();
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "zoomAt", Q_ARG(double, 2.0), Q_ARG(double, 400.0), Q_ARG(double, 300.0)));
    QmlFixture::settle();
    CHECK(n.f.context().notes().note(id).value("x").toDouble() == doctest::Approx(100.0));
    CHECK(note->property("width").toDouble() == doctest::Approx(width * 2.0).epsilon(0.02));
}

TEST_CASE("dragging and linking a note use board positions") {
    NoteFixture n;
    QString id = n.f.context().notes().create(0, 0, "Link me");
    QmlFixture::settle();
    QPointF target = n.toScreen(500, 500);
    REQUIRE(QMetaObject::invokeMethod(n.layer, "moveNoteTo", Q_ARG(QString, id), Q_ARG(double, target.x()),
                                      Q_ARG(double, target.y())));
    CHECK(n.f.context().notes().note(id).value("x").toDouble() == doctest::Approx(500.0));

    QVariant point;
    REQUIRE(QMetaObject::invokeMethod(n.canvas, "screenPositionOf", Q_RETURN_ARG(QVariant, point),
                                      Q_ARG(QString, n.concept_)));
    QPointF node = point.toPointF();
    bool linked = false;
    REQUIRE(QMetaObject::invokeMethod(n.layer, "linkNoteAt", Q_RETURN_ARG(bool, linked), Q_ARG(QString, id),
                                      Q_ARG(double, node.x()), Q_ARG(double, node.y())));
    CHECK(linked);
    auto links = n.f.context().notes().note(id).value("links").toList();
    REQUIRE(links.size() == 1);
    CHECK(links[0].toMap().value("targetId").toString() == n.concept_);
}

TEST_CASE("notes recolor with the theme") {
    NoteFixture n;
    n.f.context().notes().create(0, 0, "Color");
    QmlFixture::settle();
    auto* note = QmlFixture::child(n.window.get(), "stickyNote");
    QColor dark = note->property("color").value<QColor>();
    n.f.context().settings().setDarkTheme(false);
    CHECK(note->property("color").value<QColor>() != dark);
}
```

Include `"atlas/viewmodels/notes_model.hpp"`; `AppContext::notes()` exists from Task 5.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build --preset dev && ctest --preset dev -R atlas_qml_tests`
Expected: FAIL, no `notesLayer` child.

- [ ] **Step 3: Create `StickyNote.qml`**

```qml
import QtQuick
import QtQuick.Controls.Basic
import Atlas.ViewModels

Rectangle {
    id: note
    objectName: "stickyNote"

    required property string noteId
    required property string body
    required property string colorName
    required property real worldX
    required property real worldY
    required property real worldWidth
    required property real worldHeight
    required property var links
    property Item layer
    property bool dragging: false
    property real dragDX: 0
    property real dragDY: 0
    readonly property rect screenRect: {
        layer.viewTick
        return layer.screenRectOf(worldX, worldY, worldWidth, worldHeight)
    }

    x: screenRect.x + dragDX
    y: screenRect.y + dragDY
    width: screenRect.width
    height: screenRect.height
    radius: Theme.radius
    color: { Palette.background; return Theme.noteFill(note.colorName) }
    border.color: { Palette.background; return Qt.darker(Theme.noteFill(note.colorName), 1.15) }
    z: dragging ? 3 : 2

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.leftMargin: 4
        anchors.rightMargin: -4
        anchors.bottomMargin: -10
        z: -1
        radius: parent.radius
        color: "#000000"
        opacity: note.dragging ? (AppSettings.darkTheme ? 0.22 : 0.07) : 0
        visible: opacity > 0
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 18

        MouseArea {
            anchors.fill: parent
            enabled: note.layer.interactive
            cursorShape: Qt.OpenHandCursor
            property point pressAt
            onPressed: mouse => {
                pressAt = mapToItem(note.layer, mouse.x, mouse.y)
                note.dragging = true
            }
            onPositionChanged: mouse => {
                let now = mapToItem(note.layer, mouse.x, mouse.y)
                note.dragDX = now.x - pressAt.x
                note.dragDY = now.y - pressAt.y
            }
            onReleased: {
                note.layer.moveNoteTo(note.noteId, note.screenRect.x + note.dragDX, note.screenRect.y + note.dragDY)
                note.dragDX = 0
                note.dragDY = 0
                note.dragging = false
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            visible: note.height > 70

            Repeater {
                model: ["clay", "olive", "rose"]

                delegate: Rectangle {
                    required property string modelData
                    width: 8
                    height: 8
                    radius: 4
                    color: { Palette.background; return Theme.noteText(modelData) }
                    opacity: note.colorName === modelData ? 1 : 0.45

                    TapHandler { onTapped: Notes.recolor(note.noteId, parent.modelData) }
                }
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "×"
            color: { Palette.background; return Theme.noteText(note.colorName) }
            font.pixelSize: 13
            visible: note.height > 70

            TapHandler { onTapped: Notes.remove(note.noteId) }
        }
    }

    TextArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        leftPadding: 10
        rightPadding: 10
        topPadding: 2
        bottomPadding: 10
        visible: note.layer.canvas.zoom >= 0.45
        enabled: note.layer.interactive
        text: note.body
        wrapMode: TextArea.Wrap
        color: { Palette.background; return Theme.noteText(note.colorName) }
        font.family: Theme.sans
        font.pixelSize: 12
        background: null
        onEditingFinished: if (text !== note.body) Notes.setBody(note.noteId, text)
    }

    Rectangle {
        width: 10
        height: 10
        radius: 5
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -5
        color: { Palette.background; return Theme.noteText(note.colorName) }
        visible: note.layer.interactive

        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.CrossCursor
            onPressed: mouse => note.layer.beginDraft(note.noteId, mapToItem(note.layer, mouse.x, mouse.y))
            onPositionChanged: mouse => note.layer.moveDraft(mapToItem(note.layer, mouse.x, mouse.y))
            onReleased: mouse => note.layer.endDraft(mapToItem(note.layer, mouse.x, mouse.y))
        }
    }
}
```

- [ ] **Step 4: Create `NotesLayer.qml`**

```qml
import QtQuick
import Atlas.ViewModels

Item {
    id: layer

    property Item canvas
    property bool interactive: true
    property int viewTick: 0
    property string draftNoteId
    property point draftFrom
    property point draftTo

    function screenRectOf(worldX: real, worldY: real, worldWidth: real, worldHeight: real): rect {
        if (!canvas)
            return Qt.rect(0, 0, 0, 0)
        let topLeft = canvas.mapToScreen(worldX, worldY)
        return Qt.rect(topLeft.x, topLeft.y, worldWidth * canvas.zoom, worldHeight * canvas.zoom)
    }

    function createAtScreen(sx: real, sy: real, body: string): string {
        let world = canvas.mapToWorld(sx, sy)
        return Notes.create(world.x, world.y, body)
    }

    function moveNoteTo(noteId: string, sx: real, sy: real): bool {
        let world = canvas.mapToWorld(sx, sy)
        return Notes.move(noteId, world.x, world.y)
    }

    function linkNoteAt(noteId: string, sx: real, sy: real): bool {
        let node = canvas.nodeAt(sx, sy)
        if (node !== "")
            return Notes.link(noteId, "concept", node)
        let world = canvas.mapToWorld(sx, sy)
        for (const region of MapView.regions) {
            if (world.x >= region.x && world.x <= region.x + region.width
                    && world.y >= region.y && world.y <= region.y + region.height)
                return Notes.link(noteId, "topic", region.topicId)
        }
        return false
    }

    function targetPoint(link) {
        if (link.kind === "concept")
            return canvas.screenPositionOf(link.targetId)
        let region = MapView.regions.find(r => r.topicId === link.targetId)
        return region ? canvas.mapToScreen(region.x + region.width / 2, region.y + region.height / 2) : undefined
    }

    function beginDraft(noteId: string, at: point) {
        draftNoteId = noteId
        draftFrom = at
        draftTo = at
        lines.requestPaint()
    }

    function moveDraft(at: point) {
        draftTo = at
        lines.requestPaint()
    }

    function endDraft(at: point) {
        if (draftNoteId !== "")
            linkNoteAt(draftNoteId, at.x, at.y)
        draftNoteId = ""
        lines.requestPaint()
    }

    Connections {
        target: layer.canvas
        function onViewChanged() { layer.viewTick++; lines.requestPaint() }
        function onZoomChanged() { layer.viewTick++; lines.requestPaint() }
    }
    Connections {
        target: MapView
        function onSceneChanged() { lines.requestPaint() }
    }
    Connections {
        target: Notes
        function onDataChanged(topLeft, bottomRight, roles) { lines.requestPaint() }
        function onCountChanged() { lines.requestPaint() }
    }
    Connections {
        target: Palette
        function onChanged() { lines.requestPaint() }
    }

    Canvas {
        id: lines
        anchors.fill: parent

        onPaint: {
            let ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = 1.5
            ctx.setLineDash([4, 4])
            for (let i = 0; i < notes.count; ++i) {
                let item = notes.itemAt(i)
                if (!item)
                    continue
                ctx.strokeStyle = Theme.noteText(item.colorName)
                for (const link of item.links) {
                    let to = layer.targetPoint(link)
                    if (to === undefined)
                        continue
                    ctx.beginPath()
                    ctx.moveTo(item.x + item.width, item.y + item.height / 2)
                    ctx.lineTo(to.x, to.y)
                    ctx.stroke()
                }
            }
            if (layer.draftNoteId !== "") {
                ctx.strokeStyle = Theme.primary
                ctx.beginPath()
                ctx.moveTo(layer.draftFrom.x, layer.draftFrom.y)
                ctx.lineTo(layer.draftTo.x, layer.draftTo.y)
                ctx.stroke()
            }
        }
    }

    Repeater {
        id: notes
        model: Notes

        delegate: StickyNote {
            layer: layer
        }
    }
}
```

`Notes` is a `QAbstractListModel`; write its handler as `function onDataChanged(topLeft, bottomRight, roles) { lines.requestPaint() }` so the arguments are declared.

- [ ] **Step 5: Wire notes into the shell**

`TopicBar.qml`: add `signal noteRequested(string body)`; at the top of `addConcept`:

```qml
        let trimmed = title.trim()
        if (trimmed.toLowerCase().startsWith("note:")) {
            noteRequested(trimmed.slice(5).trim())
            newField.clear()
            return
        }
```

and use `trimmed` for the concept creation below it. `TopBar.qml`: add `signal noteRequested(string body)` and `onNoteRequested: body => bar.noteRequested(body)` on the embedded `TopicBar`.

`Main.qml`: add after `RegionLabels`:

```qml
        NotesLayer {
            id: notesLayer
            objectName: "notesLayer"
            anchors.fill: parent
            canvas: canvas
            interactive: window.mode === "board"
        }
```

on `TopBar`: `onNoteRequested: body => notesLayer.createAtScreen(board.width / 2, board.height / 2, body)`, and on the `GraphCanvas`: `onBackgroundDoubleClicked: (x, y) => { if (window.mode === "board") Notes.create(x, y, "") }`. Add `NotesLayer.qml` and `StickyNote.qml` to `QML_FILES` and `test_notes.cpp` to the test executable.

- [ ] **Step 6: Run tests**

Run: `cmake --build --preset dev && ctest --preset dev`
Expected: PASS with zero QML warnings.

- [ ] **Step 7: Commit**

```bash
git add modules/qml
git commit -m "feat(qml): sticky notes on the board with links to concepts and topics"
```

---

### Task 10: Restyle pass, decisions, and verification

**Files:**
- Modify: `modules/qml/SectionLabel.qml`, `AppButton.qml`, `AppTextField.qml`, `AppTextArea.qml`, `ConceptPanel.qml`, `SessionScreen.qml`, `HoverCard.qml`, `Toast.qml`, `Theme.qml` (drop `railWidth`), `docs/DECISIONS.md`
- Test: existing QML tests (no new behavior)

**Interfaces:**
- Produces: the type and shape rules of spec section 2 applied to every remaining component; a "Workspace and identity" section in `docs/DECISIONS.md`.

- [ ] **Step 1: Apply the type and shape system**

- `SectionLabel.qml`: `font.family: Theme.mono`, `font.pixelSize: Theme.fontCaption`, `font.letterSpacing: 0.6`, `color: Theme.onSurfaceFaint`, keep uppercase.
- `AppButton.qml`: `font.family: Theme.sans`, `font.pixelSize: Theme.fontBody`, `font.weight: Font.Medium`; background `radius: Theme.radius`, height 34; primary uses `Theme.primary` with `Theme.onPrimary` text; the rest use `Theme.surface`, `Theme.surfaceHigh` on hover, `Theme.outline` border; danger text `Theme.danger`.
- `AppTextField.qml` and `AppTextArea.qml`: `radius: Theme.radius`, background `Theme.surface`, border `Theme.outline` (focused `Theme.primary`), placeholder `Theme.onSurfaceFaint`, `font.family: Theme.sans`.
- `ConceptPanel.qml`: the title field uses `Theme.serif` at `Theme.fontHeadline`; recall and topic captions use `Theme.mono`; card corners `Theme.radiusPanel`.
- `SessionScreen.qml`: focus title `Theme.serif` at `Theme.fontHeadline`; summary count `Theme.serif` at `Theme.fontHero`; prompts `Theme.serif` at `Theme.fontTitle`; card corners `Theme.radiusPanel`.
- `HoverCard.qml`: title `Theme.serif` 15; details `Theme.mono` at `Theme.fontCaption`.
- `Toast.qml`: corners `Theme.radius`, `Theme.surfaceHigh` fill, `Theme.outline` border, `Theme.danger` left accent of 3 px instead of a full danger border.
- `Theme.qml`: remove `railWidth`.

- [ ] **Step 2: Record the decisions**

Append to `docs/DECISIONS.md`:

```markdown
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
```

- [ ] **Step 3: Full verification**

Run:

```bash
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
git grep -nP "\x{2014}" -- . ':!third_party' || echo "no long dashes"
git grep -nwi "A[I]" -- . ':!third_party' || echo "clean"
```

Expected: every test passes in both presets; the greps print "no long dashes" and "clean".

- [ ] **Step 4: Visual check (controller)**

With a temporary data folder, build a small board (two topics, six concepts, links, two notes, one linked), open a concept card, pin one, float the Today panel, expand it, open Settings, run one recall session, and switch themes. Take screenshots in both themes and view them. Fix anything that looks wrong before committing.

- [ ] **Step 5: Commit**

```bash
git add modules/qml docs/DECISIONS.md
git commit -m "feat(qml): apply the type and shape system everywhere and record the decisions"
```
