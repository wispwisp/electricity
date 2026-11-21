# F05 Panel Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new project shows a supply block and a 12-module enclosure with N/PE bars. The user drags presets from the palette onto the DIN rails, moves and deletes devices with undo/redo, sees position-based labels (QF1, QD1, QFD1), zooms and pans, and edits rating, poles and enclosure size in a properties panel.

**Architecture:**
- `panelcore` gets `project.{h,cpp}`: a `Project` QObject holding the enclosure id and the placed devices, the placement rules (rows, rail ends, overlap), position-derived designations and one `changed()` signal. The catalog gains a `designation` prefix per device type.
- The app gets undo commands (`editcommands`), an `EditorScene` that re-syncs its items from the model on every `changed()`, scene items that paint the SVGs (`sceneitems`), a `CanvasView` for zoom, pan and fit, palette drag-and-drop, and a `PropertiesPanel` dock. `MainWindow` owns the `Project` and the `QUndoStack`.
- Commands call model methods and never touch items; callers check validity before pushing, so a refused edit leaves no undo entry.

**Tech Stack:** C++20, Qt 6.12.0 static (Core, Widgets, Svg; `QUndoStack` from Gui), Qt Test (core and offscreen GUI), Xvfb + xdotool for the smoke run.

**Spec:** `docs/superpowers/specs/2026-11-20-f05-panel-editor-design.md`

## Global Constraints

- No new Qt modules or plugins. `qsvg`/`qsvgicon` stay excluded: pictures and icons are rendered with `QSvgRenderer`.
- Scene units are picture units: 1 module = 18 × 90. The enclosure item sits at scene (0, 0), so scene coordinates equal enclosure-picture coordinates.
- Rows and modules are 0-based: row 0 is the top rail, module 0 is the leftmost module. The test scenarios refer to row 0 module 0 and module 2.
- Designation prefixes come from the catalog: `breaker` `QF`, `rcd` `QD`, `rcbo` `QFD`. Numbers are per prefix, ordered by row, then module.
- Rating text on a device: curve + rated current when the device has a `curve` (`C16`), otherwise `<ratedCurrent> A` (`40 A`). Rating drop-down entries read `16 A`. Technical notation stays Latin in every language (`2P`, `1P+N`, `C`, `30 mA`, `A`).
- Scene item ids (`data(0)`): `supply`, `enclosure`, `nbar` and `pebar` (on every bar cell), `preview` (the drag preview), and the device id (`d1`, `d2`, …) on device items.
- Object names: `canvasView` (now a `CanvasView`), `editMenu`, `viewMenu`, `editToolBar`, `undoAction`, `redoAction`, `deleteAction`, `fitAction`, `deviceContextMenu`, `propertiesDock`, `propertiesPanel`, `propertiesHint`, `designationLabel`, `typeNameLabel`, `ratingCombo`, `polesCombo`, `polesValueLabel`, `curveValueLabel`, `residualValueLabel`, `enclosureSizeCombo`. Existing names stay unchanged.
- Shortcuts: Undo `QKeySequence::Undo`; Redo `Ctrl+Shift+Z` and `Ctrl+Y` on every platform; Delete `QKeySequence::Delete` with `Qt::WidgetShortcut`, active only while the canvas has focus; Fit to window `Ctrl+0`.
- Status bar hints: `statusBar()->showMessage(text, 5000)`. When the message becomes empty, "Ready" is shown again.
- Editor colours live in `src/app/theme.{h,cpp}` (ADR 0013) and are the same in both themes: `Theme::selectionColor()` (accent `#2a82da`), `Theme::refusedColor()` (red), `Theme::enclosureTextColor()` (light grey, for designations on the enclosure's dark interior). The "Supply" text uses the current palette's `Text` colour at paint time, so it follows a theme switch.
- New SVGs (`supply-1p.svg`, `ui/undo.svg`, `ui/redo.svg`) follow ADR 0013 and the F04 art rules. They are SVG Tiny 1.2 with no `<text>`, CSS, `<image>`, `<filter>` or `<script>`, and each starts with `<!-- SPDX-License-Identifier: GPL-3.0-or-later -->`.
- Strings (English source; translations to enter exactly):

  | Context | Source | ru | kk |
  |---|---|---|---|
  | MainWindow | Edit | Правка | Өңдеу |
  | MainWindow | Undo | Отменить | Болдырмау |
  | MainWindow | Redo | Повторить | Қайталау |
  | MainWindow | Delete | Удалить | Жою |
  | MainWindow | View | Вид | Көрініс |
  | MainWindow | Fit to window | По размеру окна | Терезе өлшеміне сыйғызу |
  | MainWindow | Properties | Свойства | Қасиеттер |
  | EditorScene | Supply | Ввод | Кіріс |
  | EditorScene | No room here: another device is in the way. | Здесь нет места: мешает другой аппарат. | Мұнда орын жоқ: басқа аппарат кедергі жасайды. |
  | EditorScene | Devices go on a DIN rail inside the enclosure. | Аппараты ставятся на DIN-рейку внутри щита. | Аппараттар қалқан ішіндегі DIN-рейкаға орнатылады. |
  | PropertiesPanel | Select a device on the panel to see its properties. | Выберите аппарат на щите, чтобы увидеть его свойства. | Қасиеттерін көру үшін қалқандағы аппаратты таңдаңыз. |
  | PropertiesPanel | Enclosure | Корпус щита | Қалқан корпусы |
  | PropertiesPanel | Rating | Номинал | Номинал |
  | PropertiesPanel | Poles | Полюсы | Полюстер |
  | PropertiesPanel | Curve | Характеристика | Сипаттама |
  | PropertiesPanel | Residual current | Ток утечки | Ағу тогы |
  | PropertiesPanel | Size, modules | Размер, модулей | Өлшемі, модуль |
  | PropertiesPanel | Not enough room next to the device for more poles. | Рядом с аппаратом не хватает места для дополнительных полюсов. | Аппараттың жанында қосымша полюстерге орын жетпейді. |
  | PropertiesPanel | Move or delete the devices on the rows that would be removed first. | Сначала переместите или удалите аппараты с рядов, которые будут убраны. | Алдымен алынып тасталатын қатарлардағы аппараттарды жылжытыңыз немесе жойыңыз. |

  The toolbar's window title reuses `tr("Edit")`. Each task that adds strings runs `cmake --build build --target update_translations` and enters its rows, so `translations_complete` passes after every task.
- Screenshots: `build/screenshots/f05-<step>.png`.
- Every new source file has an SPDX `GPL-3.0-or-later` header and is clang-format clean (Qt style, 100 columns). GUI tests call `useTemporarySettings(); Settings::saveLanguage(Language::English); setupApplication(*qApp);` in `initTestCase`.
- Never change an existing test to make it pass. Adding coverage to an existing test is allowed where a step says so.
- Never `git push`. Commit only as the steps say, and end each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- To wait for a long command, run it with `run_in_background: true` and wait for its completion notice. Never poll with `pgrep -f`. Before reporting, stop every background task you started.

## Review Focus

- **Undoing or deleting the device shown in the properties panel:** the panel switches to the hint, with no stale values and no crash. Task 6 `panelFollowsUndoOfSelectedDevice`.
- **Dropping a 2P device with the cursor over the first or last module of the rail:** it lands on the two end modules instead of being refused, because the slot is clamped to the rail. Task 4 `snapsToRailEnds`.
- **Pressing Delete while a properties drop-down has focus:** the device stays. Task 6 `deleteKeyInComboKeepsDevice`.
- **Undo after a refused edit:** the refusal added no undo entry, so Ctrl+Z undoes the previous real edit. Task 5 `refusedEditLeavesNoUndoEntry`.
- **Releasing a dragged device on its own spot, or a plain click:** nothing changes and no undo entry is added. Task 4 `movesDeviceByMouse` and `clickSelectsWithoutMoving`.

---

### Task 1: Project model, placement rules and designations (core)

**Files:**
- Create: `src/core/project.h`, `src/core/project.cpp`, `tests/core/tst_project.cpp`, `docs/adr/0024-placed-devices-and-designations.md`
- Modify: `src/core/catalog.h`, `src/core/catalog.cpp`, `resources/catalog/devices.json`, `tests/core/tst_catalog.cpp`, `src/core/CMakeLists.txt` (sources), `tests/CMakeLists.txt` (`add_core_test(tst_project core/tst_project.cpp)`), `docs/adr/README.md`, `docs/roadmap.md` (F05 ☐ → ◐)

**Interfaces:**
- Produces, `catalog.h`: `DeviceType` gains `QString designation;`, read from the JSON key `"designation"`. Missing or empty fails with `<type id>: missing designation`. Data: `breaker` `"QF"`, `rcd` `"QD"`, `rcbo` `"QFD"`.
- Produces, `project.h`:

```cpp
struct PlacedDevice {
    QString id;             // stable "d<n>"; empty until added
    QString type;           // catalog device type id
    QString poles;          // variant label: "1P", "2P", "1P+N"
    QJsonObject parameters; // full rating parameters, e.g. {"curve":"C","ratedCurrent":16}
    int row = 0;
    int module = 0;
    friend bool operator==(const PlacedDevice &, const PlacedDevice &) = default;
};

enum class Placement : std::uint8_t {
    Fits, Overlaps, PastRailEnd, NoRail,
    Invalid // unknown type or variant, or parameters that aren't exactly the type's allowed keys and values
};

PlacedDevice deviceFromPreset(const Preset &preset, int row, int module);
QString ratingText(const PlacedDevice &device); // "C16", "40 A"

class Project : public QObject {
    Q_OBJECT
public:
    static constexpr char DefaultEnclosureId[] = "enclosure-12";
    explicit Project(const Catalog &catalog, QObject *parent = nullptr);
    const Catalog &catalog() const;
    const EnclosureType &enclosure() const;
    int modulesPerRow() const;                          // enclosure modules / rows
    const QList<PlacedDevice> &devices() const;         // insertion order
    const PlacedDevice *device(QStringView id) const;   // nullptr if absent
    int moduleWidth(const PlacedDevice &device) const;  // variant modules, 0 if unknown
    Placement placement(const PlacedDevice &device) const; // a device with the same id is ignored
    QString designation(QStringView id) const;          // "" if absent
    bool fitsEnclosure(QStringView enclosureId) const;  // false if unknown or a device would fall off
    QString addDevice(PlacedDevice device);   // keeps a non-empty id; returns the id, "" if refused
    bool removeDevice(QStringView id);
    bool updateDevice(const PlacedDevice &device); // replaces the device with the same id
    bool setEnclosure(const QString &enclosureId);
signals:
    void changed();          // after every successful mutation
    void enclosureChanged(); // after changed(), when the enclosure changed
};
```

- Rules:
  - `addDevice` refuses when `placement` isn't `Fits` or when the id is already in use.
  - New ids are `d1`, `d2`, … from a counter that only grows, so an id is never reused in a session.
  - `updateDevice` refuses an unknown id or a placement other than `Fits`.
  - A refused mutation changes nothing and emits nothing.

- [ ] **Step 1: Write the failing tests.**
  - `tst_catalog.cpp`: add `"designation": "QF"` to the fixture breaker, add a `rejectsInvalidCatalog` row `missing designation` (designation `""` → `breaker: missing designation`), and check the three prefixes in `deviceTypesMatchSpec`.
  - `tests/core/tst_project.cpp` (class `TestProject`, `QTEST_APPLESS_MAIN`) uses `embeddedCatalog()`, with a helper `PlacedDevice preset(const char *id, int row, int module)` built on `deviceFromPreset`:
    - `startsWithDefaultEnclosure`: `enclosure().id == "enclosure-12"`, `modulesPerRow() == 12`, no devices.
    - `deviceFromPresetCopiesTypePolesAndParameters`: `breaker-2p-c40` at (0, 3) gives type `breaker`, poles `2P`, parameters `{"curve":"C","ratedCurrent":40}`, row 0, module 3, empty id.
    - `ratingText`: `breaker-1p-c16` → `C16`; `rcd-2p-40-30` → `40 A`; `rcbo-c16-30` → `C16`.
    - `placement_data` / `placement`: with `breaker-2p-c40` added at (0, 2) as `d1`:

      | Row | Device | Expected |
      |---|---|---|
      | left of it | 1P (0,0), (0,1) | Fits |
      | on it | 1P (0,2), (0,3) | Overlaps |
      | right after | 1P (0,4) | Fits |
      | 2P half on | 2P (0,1) | Overlaps |
      | last module | 1P (0,11) | Fits |
      | past end | 2P (0,11); 1P (0,-1) | PastRailEnd |
      | no such row | 1P (1,0); 1P (-1,0) | NoRail |
      | itself moved | `d1` at (0,3) | Fits |
      | unknown variant | poles `3P` | Invalid |
      | unknown type | type `fuse` | Invalid |
      | rating not allowed | `ratedCurrent` 17 | Invalid |
      | missing parameter | no `curve` | Invalid |

    - `addAssignsStableIdsAndSignals`: two adds return `d1`, `d2`, and a `QSignalSpy` on `changed` counts 2. An overlapping add returns `""`, the spy stays at 2, and there are still 2 devices.
    - `restoresGivenIdAndNeverReusesIds`: remove `d1`, add the saved copy back, get `d1` and an equal `*device(u"d1")`. Remove `d2`, add a new device, get `d3`.
    - `designationsFollowPosition`: add `breaker-1p-c10` (0,4), `rcd-2p-40-30` (0,2), `breaker-2p-c40` (0,0), `rcbo-c16-30` (0,8). They read `QF2`, `QD1`, `QF1`, `QFD1`. Move the (0,0) breaker to (0,5) with `updateDevice`: the (0,4) one becomes `QF1` and the moved one `QF2`. Remove the (0,4) one: the moved one becomes `QF1`. In `enclosure-24`, a breaker on row 1 module 0 numbers after a row 0 breaker at module 11.
    - `updateDeviceRules`:
      - Move into an overlap: refused, the device is unchanged and no signal fires.
      - Rating 25: accepted. Rating 17: refused.
      - Poles 1P→2P at (0,4) with (0,5) free: accepted, width 2.
      - The same change with a device at (0,5): placement `Overlaps`, refused.
      - At (0,11): placement `PastRailEnd`, refused.
    - `enclosureRules`:
      - `setEnclosure("enclosure-24")` succeeds, with a spy order of `changed` then `enclosureChanged`.
      - With a device on row 1, `fitsEnclosure(u"enclosure-12")` is false and `setEnclosure` refuses without signals.
      - With devices only on row 0, the shrink succeeds.
      - An unknown id is refused.
    - `removeUnknownIsRefused`.
- [ ] **Step 2: Verify red.** Run `cmake --preset dev && cmake --build build`. Expected: compile error, because `core/project.h` doesn't exist.
- [ ] **Step 3: Implement** the catalog field and data, and `project.{h,cpp}` per Interfaces.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build -R "tst_(project|catalog)" --output-on-failure`. Expected: PASS.
- [ ] **Step 5: Write ADR 0024** "Placed devices and designations" from the template and add its index row. It records:
  - A placed device stores type, poles, its full rating parameters and (row, module). This answers ADR 0023's open baseline question and refines the "stores only overrides" wording of 0009 and 0010 for ratings. Later engineering constants (τ etc.) remain type defaults with overrides.
  - Ids are `d<n>`, never reused, and restored by undo.
  - Designations are derived from position per prefix and never stored. The prefixes are catalog data (`designation`, new in schema v1).
  - The model emits one `changed()` and the scene re-syncs all items.
  - The scene places rows on the enclosure picture's `rail-<n>` elements and bars on `nbar-mount`/`pebar-mount` (ADR 0023 ids).
- [ ] **Step 6: Commit** the files above with the message `F05: project model, placement rules and designations (ADR 0024)`.

### Task 2: Undo commands

**Files:**
- Create: `src/app/editcommands.h`, `src/app/editcommands.cpp`, `tests/gui/tst_editcommands.cpp`
- Modify: `src/app/CMakeLists.txt`, `tests/CMakeLists.txt` (`add_gui_test(tst_editcommands gui/tst_editcommands.cpp)`)

**Interfaces:**
- Consumes: `Project`, `PlacedDevice`.
- Produces, `editcommands.h` (all derive from `QUndoCommand`, no command text):
  - `AddDeviceCommand(Project &project, PlacedDevice device)` with `QString deviceId() const`. The first `redo` takes the id the model assigns, and later redos re-add with that id.
  - `RemoveDeviceCommand(Project &project, const QString &id)`, which keeps a copy for undo.
  - `UpdateDeviceCommand(Project &project, PlacedDevice after)`, which reads "before" from the model at construction.
  - `SetEnclosureCommand(Project &project, QString enclosureId)`.
- Contract: callers check `placement(...) == Placement::Fits` or `fitsEnclosure(...)` before pushing. Each command `Q_ASSERT`s that its model call succeeded.

- [ ] **Step 1: Write the failing tests** (class `TestEditCommands`, `QTEST_MAIN`; one `Project` on `embeddedCatalog()` and one `QUndoStack` per test). After each push, check the model. After `undo()`, `devices()` and `enclosure().id` must equal a snapshot taken before the push (`QCOMPARE` on `QList<PlacedDevice>`). After `redo()`, they must equal the state after the push.
  - `addUndoRedoKeepsId`: push Add `breaker-2p-c40` (0,0) → `deviceId() == "d1"`; undo → empty; redo → `device(u"d1")` back.
  - `removeRestoresDeviceAndDesignations`: three breakers at (0,0), (0,1) and (0,2). Remove the middle one (`QF2`) and the third becomes `QF2`. Undo, and the removed device is back, equal to its copy, as `QF2`.
  - `updateUndoRestoresPositionAndParameters`: a move to (0,6), then rating 25, then poles 2P. Each undo restores the previous `PlacedDevice` exactly.
  - `setEnclosureUndo`: 12 → 24, undo → `enclosure-12`, redo → `enclosure-24`.
- [ ] **Step 2: Verify red.** Run `cmake --build build`. Expected: compile error, because `app/editcommands.h` doesn't exist.
- [ ] **Step 3: Implement** the four commands.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build -R tst_editcommands --output-on-failure`. Expected: PASS.
- [ ] **Step 5: Commit** with the message `F05: undo commands for device and enclosure edits`.

### Task 3: Canvas: enclosure, bars, supply, devices, zoom and pan

**Files:**
- Create: `src/app/sceneitems.{h,cpp}`, `src/app/editorscene.{h,cpp}`, `src/app/canvasview.{h,cpp}`, `resources/svg/devices/supply-1p.svg`, `tests/gui/editorhelpers.{h,cpp}`, `tests/gui/tst_canvas.cpp`
- Modify: `src/app/mainwindow.{h,cpp}`, `src/app/svgpixmap.{h,cpp}`, `src/app/theme.{h,cpp}`, `src/app/CMakeLists.txt` (sources, plus `supply-1p.svg` in the `pictures` resource block), `tests/CMakeLists.txt` (add `gui/editorhelpers.cpp` to `add_gui_test`; `add_gui_test(tst_canvas gui/tst_canvas.cpp)`), `tests/gui/tst_palette.cpp` (use the moved `presetItem`), `tests/gui/tst_catalogart.cpp`, translations

**Interfaces:**
- Consumes: `Project`, `ratingText`, `deviceFromPreset`, `embeddedCatalog()`, `picturePath()`.
- Produces, `svgpixmap.h`: `QSvgRenderer &svgRenderer(const QString &path);`, one renderer per path cached for the process.
- Produces, `theme.h`: `QColor selectionColor(); QColor refusedColor(); QColor enclosureTextColor();` (see Global Constraints).
- Produces, `sceneitems.h`:
  - `class PictureItem : public QGraphicsItem`, constructed as `PictureItem(QString path, const QString &id, QGraphicsItem *parent = nullptr)`. It paints the SVG at its view-box size and sets `data(0) = id`. When selected, it draws a `selectionColor()` outline.
  - `class DeviceItem : public QGraphicsItem`, with `DeviceItem(const Catalog &catalog, const QString &id)` and these members:
    - `void setDevice(const PlacedDevice &device, const QString &designation);`
    - `QString designation() const;` and `QString rating() const;`
    - `void setRefused(bool refused);` and `bool isRefused() const;`
  - DeviceItem layout and drawing:
    - The item origin is the device's top-left, and `boundingRect()` also covers the label above.
    - It paints the variant picture and the rating text centred in the picture's `label` element.
    - It paints the designation centred above the device, in `enclosureTextColor()`, small enough that `QF10` fits over one module.
    - When selected, it draws a `selectionColor()` outline; when refused, a `refusedColor()` translucent tint.
- Produces, `editorscene.h`:

```cpp
struct RailSlot { int row = 0; int module = 0; friend bool operator==(const RailSlot &, const RailSlot &) = default; };
class EditorScene : public QGraphicsScene {
    Q_OBJECT
public:
    static constexpr int IdKey = 0;
    EditorScene(Project &project, QUndoStack &undoStack, QObject *parent = nullptr);
    QRectF slotRect(int row, int module, int modules) const; // scene rect the device body would occupy
    std::optional<RailSlot> slotAt(QPointF scenePos, int modules) const; // device centred on scenePos
    QRectF panelRect() const;           // supply, its text and the enclosure
    QString selectedDeviceId() const;   // "" if none
    bool isEnclosureSelected() const;
    void retranslate();                 // the "Supply" text
signals:
    void hint(const QString &text);     // emitted from Task 4 on
};
```

  - Geometry:
    - For row `r`, take the bounds of the enclosure picture's `rail-<r+1>` element, mapped with `transformForElement`.
    - `originX = rail.center().x() - modulesPerRow * 18 / 2` and `top = rail.center().y() - 45`.
    - `slotRect(r, m, w) = QRectF(originX + 18 * m, top, 18 * w, 90)`.
  - `slotAt` (the clamp is a plan decision: a drop never hangs off the rail):

```cpp
// nullopt unless scenePos lies inside the enclosure picture's "interior" element
row    = index of the rail whose centre y is nearest to scenePos.y()
module = std::clamp(qFloor((scenePos.x() - originX) / 18.0 - modules / 2.0 + 0.5),
                    0, modulesPerRow - modules)
```

  - Bars: cell `i` of `nBarTerminals` / `peBarTerminals` sits at `(mount.left() + 9 * i, mount.top())` inside `nbar-mount` / `pebar-mount`. Bar cells are children of the enclosure item and aren't selectable, so a click on them selects the enclosure.
  - Supply: a non-selectable `PictureItem("supply")` whose bottom edge is 12 units above the enclosure's top and whose left edge lines up with `slotRect(0, 0, 1).left()`. The `tr("Supply")` text sits to its right.
  - Sync:
    - On `Project::changed`, the scene creates, updates (with `setDevice` and `setPos(slotRect(...).topLeft())`) or deletes device items, so items match `devices()`.
    - On an enclosure change, it swaps the enclosure picture and bars in place, so a selected enclosure stays selected.
    - `sceneRect` is `panelRect()` grown by 1000 units on each side, so the view can pan.
  - Selection: one item at a time. A device or the enclosure is selectable; a click on empty canvas clears the selection. Ctrl- or Shift-click never adds a second item.
- Produces, `canvasview.h`: `class CanvasView : public QGraphicsView` with `explicit CanvasView(EditorScene *scene, QWidget *parent = nullptr); void fitToPanel();`.
  - `fitToPanel()` fits `panelRect()` plus a 16-unit margin, keeping the aspect ratio.
  - It fits once when first shown with a real size.
  - The wheel zooms by ×1.15 per 120 angle-delta around the cursor, keeping `transform().m11()` within [0.5, 12].
  - Middle-button drag pans. Space held plus a left-button drag pans and doesn't move devices.
  - Views and viewports accept drops (used from Task 4 on).
- Produces, `mainwindow.h`: `Project &project(); QUndoStack &undoStack();`. The window owns `Project` (constructed from `embeddedCatalog()`) and the `QUndoStack`. The central widget is a `CanvasView` named `canvasView`. `project.enclosureChanged` → `canvas->fitToPanel()`, connected after the scene's own connection. `retranslateUi()` calls `scene->retranslate()`.
- Produces, `editorhelpers.h` (tests):
  - `QTreeWidgetItem *presetItem(QTreeWidget *tree, const QString &id);`, moved from `tst_palette.cpp`.
  - `const Preset &preset(const QString &id);`, looked up in `embeddedCatalog()`.
  - `QGraphicsItem *sceneItem(QGraphicsScene *scene, const QString &id);` returns the first item with that `data(0)`, and `int countItems(QGraphicsScene *scene, const QString &id);` counts them.
  - `QPoint viewportPoint(QGraphicsView *view, QPointF scenePos);`
  - `void mouseDrag(QWidget *viewport, QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton, Qt::KeyboardModifiers modifiers = {});` sends a press, five intermediate moves and a release.
  - `void wheel(QWidget *viewport, QPoint pos, int angleDeltaY);`
  - `bool fullyVisible(QGraphicsView *view, const QRectF &sceneRect);`

- [ ] **Step 1: Move `presetItem`** into `editorhelpers` and run `ctest --test-dir build -R tst_palette --output-on-failure`. Expected: PASS (pure move).
- [ ] **Step 2: Write the failing tests** in `tests/gui/tst_canvas.cpp` (class `TestCanvas`). Find the scene through `qobject_cast<EditorScene *>(window.findChild<CanvasView *>("canvasView")->scene())`.
  - `startsWithSupplyAndEmptyEnclosure`:
    - One `supply` item and one `enclosure` item; `countItems` gives 8 for `nbar` and 8 for `pebar`; there are no devices.
    - `fullyVisible(canvas, panelRect())`.
    - The enclosure's mapped width is at least 40 % of the viewport width, or its height at least 60 % of the viewport height.
    - Screenshot `start`.
  - `showsDevicesAtTheirSlots`:
    - Add, via `window.project().addDevice`, `breaker-2p-c40` (0,0), `rcd-2p-40-30` (0,2) and `breaker-1p-c10` (0,4).
    - Each device item's `pos() == slotRect(...).topLeft()`.
    - Designations are `QF1`, `QD1`, `QF2`, and ratings `C40`, `40 A`, `C10`.
    - `slotRect(0, 0, 1)` is `QRectF(32, 48, 18, 90)`, matching the comment in `enclosure-12.svg`.
    - Screenshots: the window as `devices`; also `scene->render` of `panelRect()` at 4× into a pixmap, saved as `devices-closeup`.
  - `enclosureSizeChangeRelayoutsAndRefits`: zoom in with three wheel notches, then `project().setEnclosure("enclosure-24")`. There are now 12 `nbar` cells, `slotRect(1, 0, 1).top() > slotRect(0, 0, 1).bottom()`, and `fullyVisible(canvas, panelRect())` is true again. Screenshot `enclosure-24`.
  - `zoomsAndPans`:
    - One wheel notch up multiplies `m11` by 1.15 (fuzzy compare).
    - Forty notches leave `m11 <= 12`, and forty down leave it `>= 0.5`.
    - A middle-button `mouseDrag` by (+100, 0) px moves `mapToScene(viewport centre)` by −100/`m11` in x (within 1 unit).
    - Space pressed (`QTest::keyPress(canvas, Qt::Key_Space)`) plus a left `mouseDrag` does the same, then `keyRelease`.
    - `fitToPanel()` makes the panel fully visible again.
  - `selectsOneThingAtATime`:
    - Click a device: `selectedDeviceId()` is that id.
    - Click the enclosure interior away from devices: `isEnclosureSelected()`.
    - Ctrl-click a second device: `scene->selectedItems().size() == 1` and it is that device.
    - Click outside the enclosure: nothing is selected.
    - Click the supply block: nothing is selected.
  - `followsLightTheme`: `Theme::apply(Theme::Mode::Light)`, screenshot `light`, then restore dark.
  - In `tst_catalogart.cpp`, extend `everyPictureExistsAndFollowsArtRules` to also check `:/svg/devices/supply-1p.svg` (added coverage). The supply picture must have elements `terminal-L`, `terminal-N`, `terminal-PE`.
- [ ] **Step 3: Verify red.** Run `cmake --build build`. Expected: compile error, because `app/editorscene.h` doesn't exist.
- [ ] **Step 4: Implement** per Interfaces. The supply picture is a three-way terminal block with brown, blue and yellow-green markers and a cable stub entering from the top, readable at small sizes.
- [ ] **Step 5: Translations** (`Supply` row), then run the full suite: `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass, including F01–F04 tests.
- [ ] **Step 6: Inspect** `f05-{start,devices,devices-closeup,enclosure-24,light}.png`. Expected:
  - Devices sit exactly on the rail.
  - Labels are readable above each device and don't overlap neighbours.
  - Ratings are inside the white label areas.
  - Bars tile seamlessly.
  - The supply block reads as L/N/PE above the enclosure.
  - The "Supply" text is legible in both themes.
- [ ] **Step 7: Commit** with the message `F05: editor canvas with enclosure, bars, supply and devices`.

### Task 4: Placing from the palette and moving devices

**Files:**
- Create: `tests/gui/tst_placing.cpp`, `docs/adr/0025-palette-drag-and-drop.md`
- Modify: `src/app/palettewidget.{h,cpp}`, `src/app/editorscene.{h,cpp}`, `src/app/sceneitems.{h,cpp}`, `src/app/mainwindow.cpp` (hints to the status bar, "Ready" restore), `tests/gui/editorhelpers.{h,cpp}`, `tests/CMakeLists.txt`, `docs/adr/README.md`, translations

**Interfaces:**
- Consumes: Task 3's scene, items and helpers; `AddDeviceCommand` and `UpdateDeviceCommand`.
- Produces, `palettewidget.h`:
  - `static constexpr char PresetMimeType[] = "application/x-electricity-preset";`
  - public `QStringList mimeTypes() const override;`
  - public `QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;`, which carries the first preset item's id as UTF-8.
  - The palette starts the `QDrag` itself in `mousePressEvent`/`mouseMoveEvent`, after `QApplication::startDragDistance()`, with the preset's picture as the drag pixmap and `Qt::CopyAction`. `QAbstractItemView` only drags selected items, and presets stay non-selectable.
- Scene drag-and-drop:
  - **Enter/move:** accept every event carrying `PresetMimeType` with a known preset id, even over a spot where the device doesn't fit, so the drop arrives.
  - **Preview:** a `DeviceItem` with id `preview` and opacity 0.6. It sits at `slotAt(pos, width)` with `setRefused(placement != Fits)` and is hidden when `slotAt` is `nullopt`.
  - **Leave:** removes the preview.
  - **Drop:**
    - Fits: push `AddDeviceCommand` and select the new device.
    - Overlaps: `hint(tr("No room here: another device is in the way."))`.
    - No slot: `hint(tr("Devices go on a DIN rail inside the enclosure."))`.
    - Any other placement result shows the Overlaps text.
- Moving:
  - A left press on a device selects it. Moving beyond `startDragDistance()` drags it: the device centre follows the cursor, keeping the grab offset, and snaps to `slotAt(centre, width)`, refused-tinted unless `Fits`. Away from the rails it follows the cursor freely, tinted.
  - On release:
    - Same slot as the model: nothing happens.
    - A different slot that fits: push `UpdateDeviceCommand` with the new row and module.
    - Otherwise: sync back to the model position and show the same hints as a drop.
  - A press with Space held is a pan, not a move (Task 3).
- Produces, `editorhelpers.h`:
  - `void dragPresetOver(QGraphicsView *view, PaletteWidget *palette, const QString &presetId, QPoint viewportPos);` sends `QDragEnterEvent` and `QDragMoveEvent` (`Qt::CopyAction`, left button) to the viewport, with `palette->mimeData({presetItem(...)})`.
  - `void leaveDrag(QGraphicsView *view);`
  - `void dropPreset(QGraphicsView *view, PaletteWidget *palette, const QString &presetId, QPoint viewportPos);` does enter, move and `QDropEvent`.

- [ ] **Step 1: Write the failing tests** in `tests/gui/tst_placing.cpp` (class `TestPlacing`). Use `at(r, m, w) = viewportPoint(canvas, scene->slotRect(r, m, w).center())`.

```cpp
void TestPlacing::dropsPresetsOntoTheRail()
{
    // roadmap scenario: "Breaker 2P C40" and "RCD 40/30" into the enclosure
    dropPreset(canvas, palette, "breaker-2p-c40", at(0, 0, 2));
    dropPreset(canvas, palette, "rcd-2p-40-30", at(0, 2, 2));
    const QList<PlacedDevice> &devices = window->project().devices();
    QCOMPARE(devices.size(), 2);
    QCOMPARE(devices[0].type, QStringLiteral("breaker"));
    QCOMPARE(devices[0].poles, QStringLiteral("2P"));
    QCOMPARE(devices[0].parameters, (QJsonObject{ { "curve", "C" }, { "ratedCurrent", 40 } }));
    QCOMPARE(std::pair(devices[0].row, devices[0].module), std::pair(0, 0));
    QCOMPARE(std::pair(devices[1].row, devices[1].module), std::pair(0, 2));
    QCOMPARE(window->project().designation(devices[0].id), QStringLiteral("QF1"));
    QCOMPARE(window->project().designation(devices[1].id), QStringLiteral("QD1"));
    QCOMPARE(scene->selectedDeviceId(), devices[1].id);
    QCOMPARE(window->undoStack().count(), 2);
    QVERIFY(saveScreenshot(*window, "f05", "placed"));
}
```

  - `paletteProvidesPresetDragData`: `mimeTypes()` contains `PresetMimeType`; `mimeData({item})` for `breaker-2p-c40` has that format with data `breaker-2p-c40`.
  - `previewShowsWhereTheDeviceLands`:
    - With QF1 and QD1 placed, `dragPresetOver` `breaker-1p-c16` at `at(0, 4, 1)` shows a visible `preview` item at `slotRect(0, 4, 1).topLeft()`, not refused. Screenshot `preview`.
    - Over `at(0, 1, 1)`, the preview is refused. Screenshot `preview-refused`.
    - Over the supply block, the preview is hidden.
    - `leaveDrag` → there is no `preview` item.
  - `refusesOverlappingDrop`: drop a 1P at `at(0, 1, 1)`. The device count and `undoStack().count()` are unchanged, and `statusBar()->currentMessage()` is "No room here: another device is in the way.". Then `QTRY_COMPARE_WITH_TIMEOUT(currentMessage, "Ready", 7000)`.
  - `refusesDropAwayFromTheRail`: drop on the supply block's centre. Nothing changes, and the hint reads "Devices go on a DIN rail inside the enclosure.".
  - `snapsToRailEnds`: on an empty rail, drop `breaker-2p-c40` with the cursor at `at(0, 0, 1)` and it lands on module 0. Drop another at `at(0, 11, 1)` and it lands on module 10.
  - `movesDeviceByMouse`: QF1 2P at (0,0), QD1 at (0,2), `breaker-1p-c10` at (0,6).
    - `mouseDrag` the 1P from `at(0, 6, 1)` to `at(0, 8, 1)` → module 8, undo count +1. Screenshot `moved`.
    - Drag it onto `at(0, 3, 1)` → stays at 8, with the overlap hint, undo count unchanged.
    - Drag it 6 px to the right (same slot) → unchanged, undo count unchanged.
    - Drag it to a point above the enclosure → stays at 8, with the rail hint.
    - After `project().setEnclosure("enclosure-24")`, drag it to `at(1, 3, 1)` → row 1, module 3.
  - `clickSelectsWithoutMoving`: `QTest::mouseClick` on the 1P selects it, its position is unchanged and the undo count is unchanged.
- [ ] **Step 2: Verify red.** Run `cmake --build build`. Expected: compile error, because `PresetMimeType` and the helpers are missing.
- [ ] **Step 3: Implement** per Interfaces, plus `MainWindow`: `scene.hint` → `showMessage(text, 5000)`, and `QStatusBar::messageChanged` with an empty text → `showMessage(tr("Ready"))`.
- [ ] **Step 4: Translations** (two `EditorScene` hint rows), then run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass.
- [ ] **Step 5: Write ADR 0025** "Palette drag-and-drop and headless drop tests" and add its index row. It records:
  - The palette starts a `QDrag` with MIME `application/x-electricity-preset` itself, because presets aren't selectable.
  - The scene accepts every preset move and decides at drop.
  - Qt 6.12's offscreen `QOffscreenDrag::drag()` returns `IgnoreAction` at once, so GUI tests send `QDragEnter`/`QDragMove`/`QDrop` events built from the palette's own `mimeData()` to the canvas viewport. The real drag is checked with `xdotool` under Xvfb in the smoke run.
  - The slot is clamped to the rail.
- [ ] **Step 6: Inspect** `f05-{placed,preview,preview-refused,moved}.png`. Expected:
  - The preview is translucent at the snap position.
  - The refused preview is clearly red.
  - The labels renumber correctly.
- [ ] **Step 7: Commit** with the message `F05: drag presets onto the rail, move devices (ADR 0025)`.

### Task 5: Edit and View menus, toolbar, undo/redo, delete

**Files:**
- Create: `resources/svg/ui/undo.svg`, `resources/svg/ui/redo.svg`, `tests/gui/tst_editing.cpp`, `docs/adr/0026-redo-shortcuts-and-undo-texts.md`
- Modify: `src/app/mainwindow.{h,cpp}`, `src/app/editorscene.{h,cpp}`, `src/app/svgpixmap.{h,cpp}`, `src/app/CMakeLists.txt` (icons in the `pictures` block at `:/svg/ui/`), `tests/gui/tst_catalogart.cpp`, `tests/CMakeLists.txt`, `docs/adr/README.md`, translations

**Interfaces:**
- Consumes: `RemoveDeviceCommand`, the scene's selection API, `CanvasView::fitToPanel()`.
- Produces, `svgpixmap.h`: `QIcon svgIcon(const QString &path, int size);`, with pixmaps at 1× and 2×.
- Produces, `editorscene.h`: `void setDeviceContextActions(const QList<QAction *> &actions);`. A right-click on a device selects it and shows a `QMenu` named `deviceContextMenu` with those actions through `popup()`, not `exec()`, so tests can click it. A right-click elsewhere shows no menu.
- `MainWindow`:
  - Menus are `editMenu` (Undo, Redo, separator, Delete) and `viewMenu` (Fit to window). The menu bar order is Edit, View, Settings, Language.
  - `editToolBar` is not movable and holds Undo and Redo with the icons `svgIcon(":/svg/ui/undo.svg", 24)` and `redo.svg`. The arrows are drawn in `#2a82da`, so they read on both themes.
  - Texts are plain "Undo" and "Redo", with no command names.
  - Undo and Redo are enabled from `canUndoChanged`/`canRedoChanged`.
  - `deleteAction` uses `Qt::WidgetShortcut` and is added to the canvas and to the Edit menu. It is enabled only while a device is selected and pushes `RemoveDeviceCommand(selectedDeviceId())`.
  - `fitAction` (Ctrl+0) calls `fitToPanel()`.

- [ ] **Step 1: Write the failing tests** in `tests/gui/tst_editing.cpp` (class `TestEditing`; `QVERIFY(QTest::qWaitForWindowActive(&window))` so shortcuts work).
  - `menusAndActionsExist`:
    - The menu bar titles in order are Edit, View, Settings, Language.
    - Undo and Redo are disabled at start.
    - `redoAction->shortcuts()` contains `Ctrl+Shift+Z` and `Ctrl+Y`.
    - `deleteAction->shortcutContext() == Qt::WidgetShortcut`.
    - After a drop, `undoAction->text()` is still exactly "Undo".
  - `undoRedoThroughToolbarAndShortcuts` (roadmap undo/redo):
    - Drop QF1 2P and QD1.
    - Click the toolbar's Undo button (`editToolBar->widgetForAction(undoAction)`) → QD1 is gone.
    - `QTest::keyClick(&window, Qt::Key_Y, Qt::ControlModifier)` → back.
    - Ctrl+Z twice → empty.
    - Ctrl+Shift+Z → QF1 is back with id `d1`.
    - Edit → Undo through `clickMenuBarItem`/`clickMenuItem` → empty.
    - Screenshot of the open Edit menu (`grab()` of the menu) as `edit-menu`.
  - `deletesWithKeyAndContextMenu` (roadmap delete):
    - Place QF1 2P (0,0), QD1 (0,2), `breaker-1p-c10` (0,4), and `breaker-1p-c16` at (0,5) and (0,6). These are QF2, QF3 and QF4.
    - Click QF2, `canvas->setFocus()`, `QTest::keyClick(canvas, Qt::Key_Delete)` → it's removed, (0,5) is now QF2 and (0,6) QF3. Screenshot `deleted`.
    - Right-click QF2 (`QTest::mouseClick(viewport, Qt::RightButton, ...)`) → `deviceContextMenu` is visible. `clickMenuItem` Delete → removed, and (0,6) is QF2.
    - Ctrl+Z → back, with its id and designation QF2.
    - Right-click the enclosure → no visible `deviceContextMenu`.
  - `deleteNeedsASelectedDevice`: `deleteAction` is disabled with nothing selected and with the enclosure selected, and enabled with a device selected. Edit → Delete through `clickMenuBarItem`/`clickMenuItem` removes the selected device.
  - `refusedEditLeavesNoUndoEntry`: place QF1, then make a refused overlapping drop. Ctrl+Z → no devices.
  - `fitActionRefits`: five wheel notches up, then Ctrl+0 → `fullyVisible(canvas, panelRect())`.
  - In `tst_catalogart.cpp`, extend `everyPictureExistsAndFollowsArtRules` to also check `:/svg/ui/undo.svg` and `:/svg/ui/redo.svg`.
- [ ] **Step 2: Verify red.** Run `cmake --build build && ctest --test-dir build -R tst_editing --output-on-failure`. Expected: FAIL (`editMenu` not found).
- [ ] **Step 3: Implement** per Interfaces.
- [ ] **Step 4: Translations** (`MainWindow` rows except Properties), then run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass.
- [ ] **Step 5: Write ADR 0026** "Redo shortcuts and undo texts" (refines 0011) and add its index row. Redo is on Ctrl+Shift+Z and Ctrl+Y on every platform. Undo/Redo show no command names, so nothing goes stale on a language switch. Delete is a widget shortcut on the canvas.
- [ ] **Step 6: Inspect** `f05-{edit-menu,deleted}.png`. Expected: the icons read in the toolbar, the menu shows the shortcuts, and the labels are renumbered.
- [ ] **Step 7: Commit** with the message `F05: edit menu, toolbar, undo/redo and delete (ADR 0026)`.

### Task 6: Properties panel

**Files:**
- Create: `src/app/propertiespanel.{h,cpp}`, `tests/gui/tst_properties.cpp`
- Modify: `src/app/mainwindow.{h,cpp}`, `src/app/CMakeLists.txt`, `tests/gui/editorhelpers.{h,cpp}`, `tests/CMakeLists.txt`, translations

**Interfaces:**
- Consumes: `Project`, `UpdateDeviceCommand`, `SetEnclosureCommand`, the scene's selection API and `QGraphicsScene::selectionChanged`.
- Produces, `propertiespanel.h`: `class PropertiesPanel : public QWidget`, constructed as `PropertiesPanel(Project &project, QUndoStack &undoStack, EditorScene &scene, QWidget *parent = nullptr)`, with signal `void hint(const QString &text);` connected to the status bar like the scene's.
- Content (visibility is what tests check):
  - **Nothing selected:** only `propertiesHint`.
  - **Device selected:**
    - `designationLabel` (e.g. `QF2`) and `typeNameLabel` (the catalog type name, translated in context `Catalog`).
    - `ratingCombo`: the type's allowed `ratedCurrent` values as `"%1 A"`, item data = the int.
    - Poles: `polesCombo` (variant labels) when the type has more than one variant, otherwise `polesValueLabel`.
    - `curveValueLabel` (`C`) when the parameters have `curve`; `residualValueLabel` (`30 mA`) when they have `residualCurrent`.
  - **Enclosure selected:** the `Enclosure` title and `enclosureSizeCombo` (items `12`, `24`, `36`, data = enclosure id).
  - The panel refreshes on `selectionChanged` and `Project::changed`, with signals blocked while filling. It retranslates on `LanguageChange`.
- Edits come from `QComboBox::activated` (user actions only):
  - The panel builds the changed device. If `placement == Fits`, it pushes `UpdateDeviceCommand`; otherwise it emits `hint(tr("Not enough room next to the device for more poles."))` and refreshes, which reverts the combo.
  - For size: if `fitsEnclosure`, it pushes `SetEnclosureCommand`; otherwise it emits `hint(tr("Move or delete the devices on the rows that would be removed first."))` and refreshes.
- `MainWindow`: `propertiesDock` sits in the right area, movable only (like the palette dock), titled `tr("Properties")`, about 230 px wide.
- Produces, `editorhelpers.h`: `void chooseComboItem(QComboBox *combo, const QString &text);` clicks the combo, waits for the popup view, then clicks the item's visual rect.

- [ ] **Step 1: Write the failing tests** in `tests/gui/tst_properties.cpp` (class `TestProperties`):
  - `showsHintWhenNothingSelected`: `propertiesHint` is visible with "Select a device on the panel to see its properties."; `ratingCombo` and `enclosureSizeCombo` are hidden. Screenshot `properties-empty`.
  - `editsBreakerRating`:
    - Drop `breaker-1p-c16` at (0,4) (it becomes selected).
    - `designationLabel` reads `QF1` and `typeNameLabel` "Circuit breaker".
    - `ratingCombo` items are `6 A, 10 A, 16 A, 20 A, 25 A, 32 A, 40 A, 50 A, 63 A`, current `16 A`.
    - `polesCombo` items are `1P, 2P`, current `1P`; `curveValueLabel` reads `C`; `residualValueLabel` is hidden.
    - `chooseComboItem(ratingCombo, "25 A")` → model `ratedCurrent` 25 and item rating `C25`. Screenshot `properties-breaker`.
    - Ctrl+Z → 16 and the combo shows `16 A`.
  - `refusesPoleChangeWithoutRoom`:
    - Breakers at (0,4) and (0,5); select (0,4) and choose `2P`. It is refused: poles `1P`, combo `1P`, hint "Not enough room next to the device for more poles.", undo count unchanged.
    - Delete (0,5), choose `2P` again → poles `2P`, the item is 36 units wide, still `QF1`.
    - A breaker at (0,11) → `2P` is refused with the same hint.
  - `showsRcdAndRcbo`:
    - RCD: `polesValueLabel` reads `2P` and `polesCombo` is hidden; `residualValueLabel` reads `30 mA`; `curveValueLabel` is hidden; rating items are `25 A, 32 A, 40 A, 63 A`.
    - RCBO: `polesValueLabel` reads `1P+N`, curve `C`, `30 mA`.
  - `changesEnclosureSize`:
    - Click the enclosure interior → `enclosureSizeCombo` items are `12, 24, 36`, current `12`.
    - Choose `24` → `enclosure-24`, 12 `nbar` cells, and the combo is still visible showing `24`.
    - Drop a device at (1,0) and click the enclosure. Choose `12` → refused, hint "Move or delete the devices on the rows that would be removed first.", combo `24`.
    - Ctrl+Z (undoes the drop), then choose `12` → `enclosure-12`.
    - Screenshot `properties-enclosure`.
  - `panelFollowsUndoOfSelectedDevice`: drop a device (the panel shows `QF1`), then Ctrl+Z → `propertiesHint` is visible and `ratingCombo` hidden. Ctrl+Y → the device is back and the panel still shows the hint (the restored device isn't selected).
  - `deleteKeyInComboKeepsDevice`: select a device, `ratingCombo->setFocus()`, `QTest::keyClick(ratingCombo, Qt::Key_Delete)` → the device count is unchanged.
  - `retranslatesAndFollowsTheme`: with a breaker selected, switch to Russian through the Language menu. The dock title is "Свойства", the panel shows "Номинал" and `editMenu` reads "Правка". Screenshot `properties-ru`, then Kazakh `properties-kk`, then back to English. Settings → Theme → Light, screenshot `properties-light`, then back to Dark.
- [ ] **Step 2: Verify red.** Run `cmake --build build && ctest --test-dir build -R tst_properties --output-on-failure`. Expected: FAIL (`propertiesDock` not found).
- [ ] **Step 3: Implement** per Interfaces.
- [ ] **Step 4: Translations** (`PropertiesPanel` rows and `Properties`), then run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass. If a translated label is clipped, shorten the translation; never loosen the test.
- [ ] **Step 5: Inspect** `f05-properties-*.png`. Expected:
  - The labels and values line up and nothing is clipped in ru or kk.
  - The light theme is legible.
  - The canvas keeps enough width to show the whole enclosure.
  - The supply text reads "Ввод" in Russian and "Кіріс" in Kazakh.
- [ ] **Step 6: Commit** with the message `F05: properties panel for rating, poles and enclosure size`.

### Task 7: Verification and roadmap

**Files:**
- Modify: `docs/roadmap.md` (F05 ◐ → ☑)

- [ ] **Step 1: Quality gates.** Run `CI=true scripts/quality_gates.sh` in the background. On `main`, the default clang-tidy step only checks uncommitted files, so `CI=true` makes it check every file. Expected last line: `All quality gates passed`. Fix findings in the code, never in the checks.
- [ ] **Step 2: Release binary.** Run `scripts/build_linux_executable.sh`. Expected: the dependency check passes with no new libraries and the output ends `Created dist/electricity-linux-x86_64`.
- [ ] **Step 3: Smoke test with a real drag.**
  - Set up the config: `CFG=<scratchpad>/f05-config`, and a file `$CFG/electricity/electricity.ini` with the two lines `[General]` and `language=en`.
  - Take a first screenshot: `XDG_CONFIG_HOME=$CFG xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build/electricity & sleep 3; import -window root build/screenshots/f05-smoke.png; kill $!'`. Open it and read the screen coordinates of the "Breaker 2P C40" palette entry and of the rail's first module.
  - Run the drag: in a second identical session, before the screenshot, run `xdotool mousemove <palette x y> mousedown 1`, then about ten `xdotool mousemove` steps (`sleep 0.05` each) to the rail, then `mouseup 1`, then `sleep 1`. Save the screenshot as `build/screenshots/f05-smoke-drag.png`.
  - Expected: a 2P breaker labelled QF1 with `C40` on the left of the rail, and the properties panel showing QF1.
  - On a crash, use `gdb -batch -ex run -ex bt --args ./build/electricity`.
- [ ] **Step 4: Review all `build/screenshots/f05-*.png`** as a set. Expected: nothing clipped, both themes legible, labels in panel order, and no brand-like art.
- [ ] **Step 5: Mark F05 ☑** in `docs/roadmap.md` and commit with the message `F05: mark done`. Run `ps -eo pid,ppid,etime,cmd` and stop any leftover background process.
