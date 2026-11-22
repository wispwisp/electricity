# Roadmap

Features are implemented **one per session, in this order**. Each session
starts by reading `docs/adr/README.md` and the ADRs the feature lists, writes
a plan, works red/green TDD, and ends only when the feature's scenario test
and the full suite pass, quality gates are green
([0018](adr/0018-testing-and-quality-gates.md)), and the screenshots have
been inspected. New architectural decisions get an ADR in the same session.

Product design: [docs/superpowers/specs/2026-11-19-panel-designer-design.md](superpowers/specs/2026-11-19-panel-designer-design.md).

Status legend: ☐ not started · ◐ in progress · ☑ done

| # | Feature | Status |
|---|---------|--------|
| F01 | Project skeleton, test infrastructure, dark main window | ☑ |
| F02 | CI for Linux and Windows | ☑ |
| F03 | Languages and settings | ☑ |
| F04 | Component catalog and device pictures | ☑ |
| F05 | Panel editor: enclosure, rails, placing devices | ☑ |
| F06 | Wiring | ☑ |
| F07 | Loads | ☐ |
| F08 | Live simulation: supply, switching, currents | ☐ |
| F09 | Protection and faults | ☐ |
| F10 | Project files | ☐ |
| F11 | Tutorial: single-phase apartment panel | ☐ |
| F12 | Properties and engineering details | ☐ |
| F13 | Parts list | ☐ |
| F14 | Panel printout | ☐ |
| F15 | Single-line diagram | ☐ |
| F16 | Catalog extension | ☐ |
| F17 | Three-phase panels | ☐ |
| F18 | Release: About, licenses, packaging | ☐ |

---

## F01. Project skeleton, test infrastructure, dark main window

**Goal:** an empty, dark main window built as a static binary, with every
test layer and quality gate working.

- CMake project per [0003](adr/0003-toolchain.md): `panelcore`,
  `electricity_lib`, `electricity`, `cmake/QtVersion.cmake` (fails on a
  Qt version other than the pinned 6.12.x), `CMakePresets.json` (`dev`,
  `asan`, `release`).
- Main window with empty canvas (`QGraphicsView`), empty palette dock,
  status bar; Fusion style with the dark palette
  ([0013](adr/0013-dark-theme-and-svg-art.md)); embedded DejaVu Sans
  ([0004](adr/0004-dependencies.md)).
- Test infrastructure: one trivial core test, one GUI test with the
  offscreen plugin and the `saveScreenshot` helper.
- `scripts/quality_gates.sh`, `.clang-format`, `.clang-tidy`.
- SPDX headers ([0002](adr/0002-license-gplv3.md)).

**Scenario test:** launch the main window offscreen; assert window title,
dark palette window color, presence of `canvasView`, `paletteDock`;
screenshot `f01-main-window`.
**Done also when:** release preset produces a binary whose `ldd` shows no
Qt libraries; Xvfb smoke screenshot shows the dark window.

## F02. CI for Linux and Windows

**Goal:** every push builds and tests on Linux and Windows and produces both
binaries ([0019](adr/0019-build-ci-distribution.md)).

- GitHub Actions workflow with cached static Qt builds, release preset, CTest
  offscreen, artifacts, Linux quality gates.
- Requires the user to push and confirm the run; the session prepares the
  workflow and verifies Linux parts locally.
- `scripts/build_linux_executable.sh` builds `dist/electricity-linux-x86_64`
  locally; the CI Linux job uses it.

**Done when:** both jobs are green on the user's push and the Windows
artifact is a single `.exe` (checked with `dumpbin /dependents` in CI: only
system DLLs).

## F03. Languages and settings

**Goal:** the UI switches between Русский / Қазақша / English at runtime;
theme switches dark/light; choices persist
([0012](adr/0012-internationalization.md),
[0013](adr/0013-dark-theme-and-svg-art.md)).

- Translation build, `retranslateUi` pattern, Language and Settings menus,
  first-run language chooser dialog, `QSettings` (INI), test for unfinished
  translations.

**Scenario test:** open the language menu, pick Қазақша, assert menu texts
change without restart; pick light theme; restart the window and assert both
persisted. Screenshots per language.

## F04. Component catalog and device pictures

**Goal:** the catalog is loaded from embedded JSON and the palette shows
device presets with their pictures
([0009](adr/0009-component-catalog.md),
[0016](adr/0016-electrical-reference-data.md)).

- Catalog loader and validation test; device types `breaker` 1P/2P, `rcd`
  2P, `rcbo`, enclosures 12×1, 24×2, 36×3 with N/PE bars; products for at
  least IEK, EKF, Schneider Easy9, ABB, Chint.
- Original SVGs for breaker, RCD, RCBO, enclosure, N/PE bars.
- Palette dock with presets (e.g. "Breaker 1P C16"), grouped, translated.
- Products only with a verified `source` ([0009](adr/0009-component-catalog.md)).
- Check the ampacity table against the ПУЭ РК text via web search; supersede
  0016 if values differ. If the official text cannot be found, keep the
  ПУЭ 7 values and say so in the session report.

**Scenario test:** palette shows the expected groups and presets; hover
tooltip shows description. Screenshot of the palette and of every SVG at
32 px (dark and light).

## F05. Panel editor: enclosure, rails, placing devices

**Goal:** the user drags devices from the palette into the enclosure, they
snap onto DIN rails, can be moved and deleted, with undo/redo
([0005](adr/0005-ui-qt-widgets-graphics-view.md),
[0007](adr/0007-electrical-model.md), [0011](adr/0011-undo-redo.md)).

- New project = supply block + default enclosure (12×1).
- Drag-and-drop with snap and overlap rejection; move by drag; Delete key and
  context menu; automatic designations (QF1, QD1…); zoom/pan, fit.
- Compact properties panel (basic only, per [0014](adr/0014-progressive-disclosure.md)):
  rating and poles, curve read-only, for the selected device; change
  enclosure size.

**Scenario test:** drag "Breaker 2P C40" and "RCD 40/30" into the enclosure,
assert positions and designations in the model; try overlapping drop →
rejected; undo/redo; delete. Screenshots at each step.

## F06. Wiring

**Goal:** the user draws wires terminal to terminal with a chosen
cross-section ([0007](adr/0007-electrical-model.md)).

- Wire tool (toolbar), section selector 1.5–16 mm² with pairing hints
  ([0016](adr/0016-electrical-reference-data.md)), click terminal → click
  terminal, automatic orthogonal route, color by terminal role, highlight of
  terminals under the cursor, select/delete wire, change section, undo.
- Wires follow devices when moved. A terminal accepts at most two wires
  ([0007](adr/0007-electrical-model.md)); a third is refused with a hint.
- Supply block: draw the letters L, N, PE next to its terminals; since F05 they are told apart by colour only, which a beginner can't map to "supply L → QF1".

**Scenario test:** wire supply L → QF1 terminal 1 with 10 mm², QF1 → QD1,
assert wires in model with sections and colors; delete one; undo.
Screenshots.

## F07. Loads

**Goal:** the user places load pictures around the panel and wires them
([0009](adr/0009-component-catalog.md)).

- Load SVGs: lamp, socket, kettle, washing machine, refrigerator, TV,
  microwave, air conditioner, water heater, cooktop.
- Loads palette group; place on canvas outside the enclosure; drop an
  appliance onto a socket to plug it in (two per socket), drag away to
  unplug; wires to loads default to 15 m, length editable in the basic
  properties panel.

**Scenario test:** place a lamp and a socket, plug a kettle into the
socket, wire L/N/PE to them; assert model. Screenshots.

## F08. Live simulation: supply, switching, currents

**Goal:** the circuit is live: flipping handles and switching loads changes
currents, lamps light up ([0008](adr/0008-simulation-engine.md)).

- Core: nodal phasor solver, supply with Norton source, element currents,
  energized flags; extensive unit tests (series, parallel, shared neutral,
  open contact, floating parts).
- App: click device handle to switch; click lamp/appliance to switch on/off;
  current labels on devices and wires (toggle); lamps glow when energized;
  simulation timer and time-speed control (paused, ×1, ×10, ×60).

**Scenario test:** build the tutorial panel from a fixture, flip QF1, QD1,
QF2, switch on a lamp; assert current ≈ 10 W / 230 V on QF2 and the lamp
energized; flip QF2 off → lamp de-energized. Screenshots.

## F09. Protection and faults

**Goal:** breakers trip on overload and short circuit, RCDs on leakage, wires
overheat ([0008](adr/0008-simulation-engine.md)).

- Core: thermal and magnetic models, RCD residual current, RCBO, upstream
  supply breaker, wire heating states, trip order, events; unit tests for
  each case including neutral bypassing the RCD and an oversized breaker on
  a thin wire.
- App: context menu faults on loads (short, leakage, earth fault, remove);
  tripped handle animation; glow for heating; wire orange/red; status bar
  events; reset by clicking the handle.
- Status bar: show "Ready" and events in a permanent status-bar label, replacing F03's empty-StatusTip filter in `MainWindow::event` (it would leave real status tips stuck, and `retranslateUi()` resets the message on language change).

**Scenario test:** on the tutorial fixture, overload QF4 by plugging a
washing machine and stepping time → QF4 trips with cause overload; short a
socket → QF3 trips instantly; leakage on kettle → QD1 trips; a 7 kW cooktop
(≈30 A) on a C32 breaker with 1.5 mm² wire (Iz 19 A) → wire reaches
"overheating" (threshold 1.45·Iz ≈ 27.6 A) while C32 never trips (its
thermal threshold is 1.3·32 ≈ 41.6 A). Screenshots per case.

## F10. Project files

**Goal:** New / Open / Save / Save As, recent files, unsaved-changes prompt
([0010](adr/0010-project-file-format.md)).

- Loading: add a load/replace call to `Project` that sets the device-id counter past every `d<n>` in the file, so ids are never reused ([0024](adr/0024-placed-devices-and-designations.md)).
- `Project::placement()` accepts only parameters that exactly match the type's allowed keys, so a parameter added to the catalog later (F12, F16) needs a per-version migration that fills it in for older files.

**Scenario test:** build a small panel, save to a temp file, New, Open it
back, assert model equality; modify and close → prompt shown. Fixture-based
migration test scaffold for version 1.

## F11. Tutorial: single-phase apartment panel

**Goal:** first run opens the language chooser, then the tutorial that walks
through the panel in [0016](adr/0016-electrical-reference-data.md) step by
step ([0017](adr/0017-tutorial-engine.md)).

- Tutorial engine (steps JSON, conditions, allow filter, highlight overlay,
  setup fixtures), all 13 steps, texts translated to ru/kk/en.
- Help → Tutorial to restart; Skip.
- Allow filter: since F05, edits enter from four places (palette drop and device move in `EditorScene`, delete in `MainWindow`, edits in `PropertiesPanel`); put one gate in front of all of them.

**Scenario test:** walk the whole tutorial by real clicks and drags, asserting
each step's completion; screenshot every step in Russian; spot-check
Kazakh and English screenshots.

## F12. Properties and engineering details

**Goal:** the "Show engineering details" setting reveals advanced parameters
in the properties panel, palette and context menus
([0014](adr/0014-progressive-disclosure.md)).

- Curve, thermal τ, wire length/material/ampacity, supply impedance and
  upstream breaker, cos φ, fault resistance, designations editable, full
  catalog, event log dock, currents table. All edits undoable and affect
  the simulation.

**Scenario test:** with the setting off, assert advanced fields hidden;
turn on, change supply impedance and a breaker curve, assert model and
simulation results change. Screenshots both states.

## F13. Parts list

**Goal:** a parts list table generated from the design, exported to PDF and
CSV, no prices ([0015](adr/0015-exports.md)).

**Scenario test:** open parts list for the tutorial fixture, assert rows and
wire totals; export CSV and PDF to temp, assert files exist and CSV content.
Screenshot of the table; render PDF page check by size.

## F14. Panel printout

**Goal:** export the panel picture to PNG and PDF with a title block; print
dialog ([0015](adr/0015-exports.md)).

**Scenario test:** export PNG/PDF of the tutorial fixture, assert files and
image size; open the PNG for inspection.

## F15. Single-line diagram

**Goal:** generate the single-line diagram from the wiring, show it, export
to PDF/PNG ([0015](adr/0015-exports.md)).

- Core trace with tests (tree, meshes, unconnected devices); app drawing with
  ГОСТ-style symbols and the line table; warnings for untraceable parts.

**Scenario test:** open the diagram for the tutorial fixture, assert the
trace structure (QF1 → QD1 → QF2/QF3/QF4 with sections and loads); export;
inspect screenshots.

## F16. Catalog extension

**Goal:** more devices and presets used in Kazakhstan: voltage relay (with a
supply voltage control in engineer details to demonstrate it), switch-
disconnector, comb busbar, B/D curves, RCD type AC/A and 100 mA, more
loads, more products. New SVGs.

**Scenario test:** place a voltage relay, raise supply voltage above its
limit → relay opens and loads de-energize; lower → it recloses after its
delay.

## F17. Three-phase panels

**Goal:** design and simulate three-phase panels
([0007](adr/0007-electrical-model.md), [0008](adr/0008-simulation-engine.md)).

- Three-phase supply, 3P/4P breakers, 4P RCD, three-phase loads (cooktop
  3-phase, motor), phase colors, per-phase current and neutral current
  display, phase balance hint, diagram and parts list support.
- Supply fault "PEN break" (upstream of the TN-C-S split, so N and PE both
  lose the star point), triggered from the supply block's context menu in
  engineer details; stored as simulation state, not in the file.

**Scenario test:** build a three-phase panel from a fixture with unbalanced
single-phase loads, assert per-phase and neutral currents; trigger the
supply fault "PEN break" → assert overvoltage on the lightly loaded phase
and touch voltage on PE; screenshots.

## F18. Release: About, licenses, packaging

**Goal:** a releasable build ([0002](adr/0002-license-gplv3.md),
[0019](adr/0019-build-ci-distribution.md)).

- About dialog (version, GPL notice, Qt LGPL notice and source link, DejaVu
  license), app icon (original SVG), Windows version resource, user README
  in three languages, release workflow on tags.

**Scenario test:** open About, assert license texts and version are present;
screenshot. CI release dry run.
