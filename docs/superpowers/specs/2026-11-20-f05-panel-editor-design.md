# F05 — Panel editor: enclosure, rails, placing devices

Date: 2026-11-20 · Status: approved by the owner

Roadmap item: [F05](../../roadmap.md#f05-panel-editor-enclosure-rails-placing-devices).
ADRs: [0005](../../adr/0005-ui-qt-widgets-graphics-view.md),
[0006](../../adr/0006-core-and-app-layering.md),
[0007](../../adr/0007-electrical-model.md),
[0009](../../adr/0009-component-catalog.md),
[0011](../../adr/0011-undo-redo.md),
[0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0014](../../adr/0014-progressive-disclosure.md),
[0018](../../adr/0018-testing-and-quality-gates.md),
[0023](../../adr/0023-catalog-schema.md).

## Intent

The app's users are non-experts building a real apartment panel from devices
sold in Kazakhstan. F05 turns the empty canvas into a panel: an enclosure with
DIN rails and N/PE bars, a supply feed, and devices that the user drags from
the palette onto the rails, moves, deletes and undoes, the way they would lay
out a real panel. It is the first feature that creates a project model in the
core; wiring (F06), loads (F07), simulation (F08) and project files (F10) all
build on it, and the tutorial (F11) walks through exactly this placing flow.
The basic flow must stay simple: drag, drop, see the label, change the rating
if needed.

## Goals

### What the user sees

```
┌ Edit  View  Settings  Language ──────────────────────────────────────┐
│ [↶][↷]                                                                │
│ Components      │                                  │ Properties      │
│ Protection      │          ┌ L  N  PE ┐  Supply     │ QF2             │
│ ▮ Breaker 2P C40│   ┌──────┴──────────┴───────┐    │ Circuit breaker │
│ ▮ RCD 40 A 30 mA│   │ QF1  QD1  QF2 QF3       │    │ Rating [16 A  ▾]│
│ ▮ Breaker 1P C10│   │ ▮▮   ▮▮   ▮   ▮   ······│    │ Poles  [1P    ▾]│
│ ...             │   │ ════════════════════════│    │ Curve   C       │
│                 │   │     [N  ▪▪▪▪▪▪▪▪]       │    │                 │
│                 │   │     [PE ▪▪▪▪▪▪▪▪]       │    │                 │
│                 │   └─────────────────────────┘    │                 │
└ Ready ────────────────────────────────────────────────────────────────┘
```

### Starting state

- The app opens with a new project: a single-phase supply block and an empty
  12-module enclosure (`enclosure-12`) with its N and PE bars (8 terminals
  each), fitted to the view.
- The supply block and the enclosure can't be moved or deleted.
- The supply block is a small terminal block with `L`, `N`, `PE` above the
  enclosure, with the feed entering the enclosure (ADR 0007). It is a new
  original SVG.

### Placing a device

- The user drags a preset from the palette onto the canvas. While dragging, a
  preview of the device snaps to the nearest module on the nearest rail.
- Where the device doesn't fit (overlaps another device, runs past the end of
  the rail), the preview turns red; dropping does nothing and the status bar
  says why.
- Away from the enclosure (no rail near the cursor) no preview is shown;
  dropping does nothing and the status bar says devices go on the DIN rail.
- A successful drop places the device with the preset's type, poles and
  ratings, and selects it.

### Moving a device

- The user drags a placed device with the mouse; it snaps like the preview.
- It can move along its rail and to other rails (24 and 36-module enclosures).
- Dropped where it doesn't fit, it goes back to where it was, with the status
  bar hint.

### Labels (designations), decided by the owner: by position

- Each kind is numbered separately: `QF` for breakers, `QD` for RCDs, `QFD`
  for RCBOs.
- Numbers always run by position: top row first, then left to right. Placing,
  moving or deleting a device renumbers the others of its kind (delete QF2 and
  QF3 becomes QF2).
- The label is drawn above the device on the enclosure interior; the rating is
  printed on the device's blank label area (`C16` for a breaker or RCBO,
  `40 A` for an RCD).

### Selecting and deleting

- One thing is selected at a time, by clicking it: a device, or the enclosure
  (clicking the enclosure where there is no device). Clicking empty canvas
  clears the selection. No rubber-band or multiple selection.
- Delete removes the selected device: the Delete key, Edit → Delete, or
  right-click on a device → Delete. The enclosure can't be deleted. The Delete
  key acts only while the canvas has focus, so pressing it in a properties
  drop-down doesn't delete the device.

### Properties panel

A dock on the right, titled "Properties".

- **Device selected:** its label (e.g. QF2), the device type name, and:
  - Rating: a drop-down of the type's allowed `ratedCurrent` values, shown as
    "16 A".
  - Poles: a drop-down of the type's variants; for types with one variant
    (RCD, RCBO) shown as text, not editable.
  - Curve: shown, not editable (breaker, RCBO).
  - Residual current: "30 mA", shown, not editable (RCD, RCBO).
- **Enclosure selected:** its size: 12, 24 or 36 modules. Growing it adds rows
  and makes the bars longer (8 → 12 → 16 terminals, from the catalog).
- **Nothing selected:** a short hint to select a device.
- Changes apply immediately and are undoable.

### Refused edits (each with a status bar hint; the drop-down goes back)

- Changing a breaker from 1P to 2P when the extra module is taken or past the
  end of the rail. The device keeps its left module and grows to the right.
- Shrinking the enclosure while any device sits on a row that would disappear.

### Undo and redo

- Covers: place, move, delete, rating, poles, enclosure size.
- Available from Edit → Undo / Redo, two toolbar buttons, Ctrl+Z, and both
  Ctrl+Shift+Z and Ctrl+Y for redo on every platform (ADR 0011 names Ctrl+Y
  for Windows only; the new ADR records the change).
- The menu entries and tooltips say just "Undo" and "Redo", without the name
  of the edit, so nothing goes stale after a language switch.
- Undoing a delete restores the device with its original id (F06 wires will
  refer to it).
- Simulation actions are not undoable (ADR 0011); there are none yet.

### View

- Mouse wheel zooms around the cursor; middle-button drag or Space+drag pans;
  Ctrl+0 (also View → Fit to window) fits the supply and enclosure to the
  view.

### Languages

Every new text (menus, toolbar tooltips, properties labels, hints, "Supply") ships in Russian, Kazakh and English and changes on
a language switch without a restart.

## Non-goals

- Wires, terminal highlights (F06); loads (F07); switching and simulation
  (F08, F09); saving and opening (F10); the tutorial and its allow-filter
  (F11).
- Engineering details in the properties panel, supply properties, editable
  curve (F12).
- More than one enclosure; copy/paste; multiple selection; dropping onto an
  occupied spot pushing neighbours aside or jumping to the nearest free slot.
- Manually renaming designations.

## Anti-goals

- Losing user work silently: a refused edit changes nothing, and nothing is
  ever removed except by an explicit Delete.
- A basic flow that needs engineering knowledge (only rating and poles are
  editable).
- Labels that disagree with the panel order.

## Constraints

- The project model, placement rules (fit, overlap, rail ends, rows) and
  designations live in `panelcore` (Qt Core only, ADR 0006); scene items are
  thin views updated from model signals; undo commands call model methods and
  never touch scene items (ADR 0011).
- Object ids are stable strings generated on creation (ADR 0007), so F06 and
  F10 can rely on them.
- New SVGs follow ADR 0013 (original, SVG Tiny 1.2, no text, readable on dark
  and light and at small sizes) and carry an SPDX header.
- Pictures stay rendered with `QSvgRenderer`; the excluded `qsvg`/`qsvgicon`
  plugins stay excluded (ADR 0023), so toolbar icons are rendered to pixmaps.
- Every interactive widget has a stable `objectName`; scene items expose their
  model id via `data(0)` (ADR 0005).

## Approaches

**Labels** (owner's choice):
- *A. By position, renumbered on every change* — **chosen**: matches real
  panels and diagrams; the tutorial's "QF3 trips" always means the same spot.
- *B. Fixed from placement, lowest free number* — labels never jump, but after
  edits QF4 can sit left of QF2.

**Dragging from the palette** (builder's call; the owner left code design to
the builder):
- *Qt drag-and-drop* (`QDrag` from the palette, drop handling on the canvas) —
  **chosen**: native drag cursor, Esc cancels, works with an undocked palette.
  Qt 6.12's offscreen platform ends every `QDrag` at once (`QOffscreenDrag`
  returns `IgnoreAction`; checked in the Qt 6.12.0 source,
  `src/plugins/platforms/offscreen/qoffscreencommon.h`), so the scenario test sends the canvas viewport the
  `QDragEnter`/`QDragMove`/`QDrop` events Qt would deliver, built from the
  palette's own drag data; the real drag is checked by hand in the Xvfb smoke
  run with `xdotool`.
- *Home-made drag* (palette press starts a preview that follows the mouse) —
  drivable with plain mouse events in tests, but re-implements Qt's
  drag-and-drop and loses its native behaviour.

**What a placed device stores** (builder's call; ADR 0023 left it to F05):
- *Type, poles and the full rating parameters* (`curve`, `ratedCurrent`,
  `residualCurrent`, `rcdType` as applicable), copied from the preset on drop —
  **chosen**: a design doesn't depend on the palette list, which changes in
  F12 and F16. Later engineering constants (thermal τ etc., F09/F12) remain
  type defaults with per-device overrides, as ADR 0009 says.
- *Preset id plus overrides* — ties designs to the palette; editing a preset
  would silently change existing projects.

**Designations stored or derived** (builder's call): derived from positions
whenever asked, never stored — follows from choice A and can't go stale.

## Decided by the owner

- Labels by position, renumbered on change (A).
- The behaviour described under Goals, including the refused edits, as played
  back ("nothing missing, go ahead").
- Code design left to the builder (CLAUDE.md).

## Builder's calls

- Menu order Edit, View, Settings, Language; one toolbar with Undo and Redo;
  View menu holds only "Fit to window" for now, so Ctrl+0 is discoverable.
- Undo/redo toolbar icons are new original SVGs (curved arrows).
- Properties panel on the right; supply not selectable in F05.
- Device context menu has only Delete (the properties panel is always
  visible, so a "Properties" entry would add nothing).
- RCBO prefix `QFD` (product design: CIS designations QF/QD/QFD).
- The view refits after the enclosure size changes (including by undo), so a
  grown enclosure isn't left off-screen.
- Status bar hints are temporary messages; "Ready" returns afterwards. F09
  replaces this with a permanent label (roadmap note).
- New ADRs: placed device data and derived designations (refines the
  "stores only its overrides" wording of ADRs 0009 and 0010 for ratings);
  palette drag-and-drop and how it is tested headless; Ctrl+Y redo on every
  platform (refines ADR 0011).
- The supply block draws no wires: its terminals are plain endpoints for F06.

## Left to the builder

- Model classes, signals, id format, command classes and file layout.
- Snapping rule details (which module the cursor maps to, how far from a rail
  a preview still shows).
- Supply block drawing and exact placement above the enclosure, its "Supply"
  text label. The SVG should name its terminals `terminal-L`, `terminal-N`,
  `terminal-PE` (ADR 0023 id convention) for F06.
- Rail geometry source (the enclosure SVG's `rail-<n>` and bar-mount element
  bounds via `QSvgRenderer::boundsOnElement`, or data).
- Preview and selection colours (from the theme), zoom limits, label fonts.
- Hint wording in all three languages (Kazakh to be reviewed by a native
  speaker when possible).

## Tests

- **Core unit tests:** new project has supply and `enclosure-12`; placing at
  rail start and end, past the end, overlapping (refused, model unchanged);
  rows in 24/36 enclosures; move (allowed, refused, onto its own old spot);
  remove and restore with the same id; designations per prefix by row then
  module, renumbering after place/move/delete; rating change limited to
  allowed values; pole change 1P→2P allowed and refused; enclosure grow and
  refused shrink; model signals fired for each change.
- **Undo tests:** each command's undo/redo restores the exact model state.
- **GUI scenario (offscreen, roadmap):** drag "Breaker 2P C40" and "RCD 40 A
  30 mA" into the enclosure → row 0 module 0 and module 2, labels QF1 and
  QD1; overlapping drop → refused with a hint; undo/redo (toolbar, Ctrl+Z,
  Ctrl+Y); delete with the Delete key and with the context menu; move a device
  by mouse drag; delete a middle breaker → the next one is renumbered; change
  rating and poles in the properties panel, including a refused 1P→2P; grow
  the enclosure to 24 and a refused shrink; Ctrl+0 fit. Screenshots at each
  step, plus the properties panel in Russian and Kazakh and the editor in the
  light theme.
- **Translations:** the existing `translations_complete` test covers the new
  strings.
- **Smoke test** of the real binary under Xvfb, including a real palette drag
  with `xdotool`; screenshots inspected.
