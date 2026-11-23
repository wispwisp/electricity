# F08 Live Simulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The panel is live. The circuit is solved as wired after every change. The user flips breaker, RCD and RCBO handles and switches lamps, the cooktop and appliances with an on/off button. Lamps glow and running appliances get a halo, and current labels show on devices and wires.

**Architecture:**
- `panelcore`:
  - The catalog lists each device variant's contacts and marks the ceiling lamp as a light.
  - A GUI-free dense complex nodal solver (`nodalsolver`).
  - A `Simulation` object beside `Project`. It holds the on/off settings by id, builds the network from the project, re-solves after every `Project::changed()` and every switch change, and exposes the currents, voltages and energized flags.
- App:
  - `EditorScene` owns the `Simulation` and turns a click on a handle or button into `setOn`. It applies the results to items in `syncLive()`.
  - `DeviceItem` draws the handle up or down and a current chip. `LoadItem` draws the on/off button and the glow or halo. `WireItem` owns a current label item.
  - View → Show currents toggles all labels and persists.
- Switching is never on the undo stack (ADR 0011). Commands and items are unchanged otherwise.

**Tech Stack:** C++20, Qt 6.12.0 static (Core, Widgets, Svg), `std::complex<double>`, Qt Test (core and offscreen GUI), Xvfb + xdotool for the smoke run.

**Spec:** `docs/superpowers/specs/2026-11-23-f08-live-simulation-design.md`

## Global Constraints

- No new Qt modules, plugins or libraries. Pictures are rendered with `QSvgRenderer`.
- Electrical constants (`src/core/simulation.h`, ADR 0007/0008):

  | Name | Value | Meaning |
  |---|---|---|
  | `SupplyVoltage` | 230 | V, supply L to reference, angle 0 |
  | `SupplyImpedance` | 0.4 | Ω, Norton source impedance on supply L |
  | `ContactResistance` | 0.001 | Ω, a closed contact; also each tie of supply N and supply PE to the reference |
  | `CopperResistivity` | 0.0175 | Ω·mm²/m; wire `R = ρ·length/section` |
  | `Gmin` (`nodalsolver.h`) | 1e-9 | S, from every node to the reference |

  - A load is `Z = SupplyVoltage² / power`, resistive.
  - A load is **energized** when it is on, draws from two terminals, and `|U| > 0.5 · SupplyVoltage` (115 V) across them.
- Network: one node per terminal of `Project::terminalIds()`, except that all `nbar:*` holes are one node and all `pebar:*` holes are one node. The supply terminal with role L is the source. Its N and PE terminals tie to the reference.
- Contacts (`devices.json`, `"contacts"` per variant, `[top, bottom]` pairs):
  - breaker 1P: `1–2`;
  - breaker 2P: `1–2`, `3–4`;
  - rcd 2P: `1–2`, `N1–N2`;
  - rcbo 1P+N: `1–2`, `N1–N2`.
- Who draws from where:
  - ceiling lamp and cooktop: their own `L`–`N`;
  - a plug-in appliance that is plugged in: its socket's `L`–`N`;
  - a socket, an unplugged appliance and every `PE` terminal draw from nothing.
- Switch settings:
  - Keyed by object id. A missing entry means off, so everything starts off.
  - Entries outlive their objects, so delete-and-undo keeps the setting. Ids are never reused.
  - Devices and every load except sockets (`LoadType::places` empty) can be switched.
- Handles: `HandleTravel = 12.5` picture units. An off handle is the picture's `handle` element moved down by that much (handles are at y 36.5–49.5 in a 36–62 slot).
- Current labels:
  - A label shows only if `showsCurrent(amperes)`, which is `amperes >= 0.005`.
  - Decimals: 2 below 9.995 A, 1 below 99.95 A, else 0.
  - Format: the current language's `QLocale` with `'f'`, plus the translated unit: "0.04 A", "13.4 A", "575 A", ru "0,04 А".
  - A device shows its largest contact current, a wire its own current.
- Z values: the existing ones are unchanged. `CurrentLabelItem::LabelZ = 1.8`, above wires (1.5) and designations (1.75).
- Scene item ids (`data(0)`): a wire's current label carries `currentLabel`.
- Object names: `showCurrentsAction` (View menu, checkable, after Fit to window).
- Settings key `showCurrents` (bool, default true when unset).
- Strings (English source; enter the translations exactly):

  | Context | Source | ru | kk |
  |---|---|---|---|
  | CurrentText | %1 A | %1 А | %1 А |
  | MainWindow | Show currents | Показывать токи | Токтарды көрсету |

  The task that adds strings runs `cmake --build build --target update_translations` and enters its rows, so `translations_complete` passes after every task.
- Screenshots: `build/screenshots/f08-<step>.png` via `saveScreenshot(widget, "f08", "<step>")`.
- Tutorial fixture positions for GUI tests (`addTutorialLoads`, Task 4) and the expected solved currents. Work them out from the constants, never read them back from the code:

  | Case | Current |
  |---|---|
  | QF1, QD1, QF2 on, Lamp 1 on | 0.043472 A in QF1, QD1, QF2, Lamp 1's L and N wires |
  | QF1, QD1, QF4 on, kettle and microwave on in Socket 3 | 13.409 A in QF4 |
  | the same with only the microwave plugged in | 5.1449 A in QF4 |
  | QF1, QD1, QF2 on, a 0.3 m 1.5 mm² wire `d3:2 → nbar:5` | 556.9 A ("557 A") |

  Tolerance in asserts: ±0.0001 A for the lamp, ±0.005 A otherwise.
- Source files and tests:
  - Every new source file has an SPDX `GPL-3.0-or-later` header and is clang-format clean (Qt style, 100 columns).
  - GUI tests call `useTemporarySettings(); Settings::saveLanguage(Language::English); setupApplication(*qApp);` in `initTestCase`, and wait with `QTest::qWaitForWindowActive`.
  - GUI tests that click loads zoom out as `tst_loading` does: `canvas->setTransform(QTransform::fromScale(0.6, 0.6)); canvas->centerOn(QPointF(140, 300));`. Tests that click handles use the fitted view.
- Never change an existing test to make it pass. Adding test functions or data rows to an existing test file is allowed where a step says so.
- Never `git push`. Commit only as the steps say. End each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- To wait for a long command, run it with `run_in_background: true` and wait for its completion notice. Never poll with `pgrep -f`. Before reporting, stop every background task you started (`ps -eo pid,ppid,etime,cmd`).

## Review Focus

- **A quick double click on a handle or a load button** flips it twice and ends where it started. Nothing moves and no move is left hanging. Task 4 `doubleClickOnHandleFlipsTwice`, Task 5 `doubleClickOnButtonFlipsTwice`.
- **Press on a handle, Esc, release:** nothing switches. Task 4 `escapedPressOnHandleDoesNotSwitch`.
- **Deleting a socket whose appliances are running:** they stop, because they are unplugged now. One undo brings them back running. Task 5 `deletingSocketStopsItsAppliancesUntilUndo`.
- **A bare wire from L to N:** finite currents, a whole-ampere label, no hang. Task 3 `shortIsLimitedBySupplyAndWires`, Task 6 `shortShowsWholeAmperes`.
- **Switching the language while currents show:** labels change format ("0,04 А") and stay in place, and no switch changes. Task 6 `labelsFollowLanguage`.

---

### Task 1: Contacts and lights in the catalog, ADR 0029, roadmap

**Files:**
- Modify: `resources/catalog/devices.json`, `resources/catalog/loads.json`, `src/core/catalog.h`, `src/core/catalog.cpp`, `tests/core/tst_catalog.cpp`, `docs/roadmap.md`, `docs/adr/README.md`
- Create: `docs/adr/0029-live-simulation-state-and-contacts.md`

**Interfaces:**
- Produces, `catalog.h`:

```cpp
struct ContactSpec
{
    QString top, bottom; // terminal names a closed contact joins
    friend bool operator==(const ContactSpec &, const ContactSpec &) = default;
};
// DeviceVariant gains:
QList<ContactSpec> contacts; // in catalog order
// LoadType gains:
bool light = false; // glows warm when it runs (the ceiling lamp)
```

- JSON: a variant's `"contacts": [["1", "2"], ["3", "4"]]` is optional (no contacts = never conducts). A load's `"light": true` is optional.
- Loader errors (`where` = `device type <id> <poles>`, as for duplicate terminals):
  - `<where>: a contact joins two terminals` (an entry that isn't an array of two strings);
  - `<where>: contact terminal <name> unknown`;
  - `<where>: contact joins <name> to itself`;
  - `<where>: terminal <name> is in two contacts`.

- [ ] **Step 1: Write the failing tests** in `tests/core/tst_catalog.cpp` (new functions; new rows in `rejectsInvalidCatalog_data`):
  - `loadsDeviceContacts`: patch the fixture's first variant with `"contacts": [["1", "2"]]` → no errors. `deviceType(u"breaker")->variant(u"1P")->contacts == QList<ContactSpec>{ { "1", "2" } }`. The unpatched fixture also loads with no errors.
  - Four `rejectsInvalidCatalog` rows, one per error above: `[["1"]]`, `[["1", "9"]]`, `[["1", "1"]]`, `[["1", "2"], ["2", "1"]]`.
  - `deviceContactsMatchSpec`: the embedded catalog has exactly the contacts table of Global Constraints.
  - `onlyTheCeilingLampIsALight`: the embedded `light` is true for `ceiling-lamp` and false for the other nine loads.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_catalog --output-on-failure`
  Expected: compile errors (`ContactSpec`, `contacts`, `light` unknown).

- [ ] **Step 3: Implement.**
  - The structs, and parsing in `loadDevices` after a variant's terminals. Read `light` in the loads loader.
  - Add the contacts to `devices.json` and `"light": true` to the ceiling lamp in `loads.json`.

- [ ] **Step 4: Run the tests**
  Run: `cmake --build build && ctest --test-dir build -R tst_catalog --output-on-failure`
  Expected: PASS.

- [ ] **Step 5: ADR and roadmap.**
  - Write ADR 0029, "Live simulation: state beside the design, contacts in the catalog", from the template. It refines 0007, 0008, 0011, 0014 and 0023. Decisions:
    - switch settings live in a core `Simulation` beside `Project`, keyed by id (missing = off, so everything starts off), not undoable, not in `PlacedDevice`/`PlacedLoad` (undo commands copy those whole), kept after an object is removed so undo restores it;
    - contacts are per-variant catalog data;
    - the network: bars are one conductor each; sockets and `PE` terminals have no branch; plug-ins draw through their socket's L and N; loads are fixed resistances, so currents are solved values a few percent under P/U;
    - the solve runs synchronously after every change; there is no timer until F09;
    - there is no supply power button (refines 0014's toolbar list); QF1 switches the apartment;
    - the ceiling lamp's `light` flag picks the warm glow.
    - Rejected: state in the design model; a contact naming rule in code; a second "off" picture per variant; constant-power loads (non-linear, and wrong for heaters).
    - Add the row to `docs/adr/README.md`.
  - In `docs/roadmap.md`:
    - F08 ☐ → ◐;
    - remove F08's "simulation timer and time-speed control (paused, ×1, ×10, ×60)" and add to F09's App bullet "simulation timer and time-speed control (paused, ×1, ×10, ×60), moved from F08";
    - add follow-up bullets:
      - F09: with solved currents, the 7 kW cooktop on 15 m of 1.5 mm² draws ≈ 27.7 A, barely over the 27.6 A overheating threshold, so pick a case that clears it well; the washing-machine overload on QF4 is ≈ 22.1 A and trips in about 2.7 min of simulated time instead of ADR 0016's 2 min;
      - F10: save each device's handle position from the simulation state (ADR 0010); decide whether load on/off is saved;
      - F11: handle clicks and load buttons go through the allow filter.

- [ ] **Step 6: Commit** (`F08: contacts and lights in the catalog (ADR 0029)`).

### Task 2: Nodal solver (core)

**Files:**
- Create: `src/core/nodalsolver.h`, `src/core/nodalsolver.cpp`, `tests/core/tst_nodalsolver.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt` (`add_core_test(tst_nodalsolver core/tst_nodalsolver.cpp)`)

**Interfaces:**
- Produces, `nodalsolver.h`:

```cpp
using Complex = std::complex<double>;
inline constexpr int ReferenceNode = -1;
inline constexpr double Gmin = 1e-9; // S, from every node to the reference

struct Admittance
{
    int a = ReferenceNode;
    int b = ReferenceNode;
    Complex y; // S
};

// Node voltages solving Y·V = I for nodes 0..injected.size()-1, with Gmin from every node to the
// reference. A branch whose ends are the same node is ignored. injected[i] flows into node i.
[[nodiscard]] QList<Complex> solveNodeVoltages(const QList<Admittance> &branches,
                                               const QList<Complex> &injected);
```

- [ ] **Step 1: Write the failing tests** in `tst_nodalsolver.cpp`:
  - `dividerFromNortonSource`: `injected = {230 / 0.4, 0}`, branches `{0, Ref, 1/0.4}`, `{0, 1, 1/10.0}`, `{1, Ref, 1/100.0}`. Expect `|V1 − 230·100/110.4| < 1e-4` and `|V0 − 230·110/110.4| < 1e-4`.
  - `phaseCarriesThrough`: the same with `injected[0] = std::polar(230 / 0.4, -2π/3)`. Expect `|arg(V1) + 2π/3| < 1e-9`.
  - `floatingNodesSitAtReference`: node 2 has no branches; nodes 3 and 4 are joined only to each other (`{3, 4, 1.0}`). Expect `|V2|`, `|V3|`, `|V4|` all `< 1e-9`, while V1 is as in the divider.
  - `branchWithinOneNodeIsIgnored`: adding `{1, 1, 5.0}` to the divider changes nothing.
  - `noNodes`: empty input → empty result.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_nodalsolver --output-on-failure`
  Expected: compile error (`nodalsolver.h` missing).

- [ ] **Step 3: Implement `solveNodeVoltages`.**
  - Build a dense row-major `n×n` matrix. A branch adds `y` to `(a,a)` and `(b,b)` and subtracts it from `(a,b)` and `(b,a)`, skipping the reference.
  - Add `Gmin` to each diagonal entry.
  - Run Gaussian elimination with partial pivoting (the largest `|entry|` in the column), then back-substitute.
  - Skip rows whose entry in the pivot column is exactly zero. The matrix is sparse, and Task 3's timing test relies on this.

- [ ] **Step 4: Run the tests**
  Run: `cmake --build build && ctest --test-dir build -R tst_nodalsolver --output-on-failure`
  Expected: PASS.

- [ ] **Step 5: Commit** (`F08: dense complex nodal solver`).

### Task 3: Simulation (core)

**Files:**
- Create: `src/core/simulation.h`, `src/core/simulation.cpp`, `tests/core/tst_simulation.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt` (`add_core_test(tst_simulation core/tst_simulation.cpp)`)

**Interfaces:**
- Consumes: `ContactSpec` (Task 1), `solveNodeVoltages`, `Admittance`, `ReferenceNode` (Task 2), the existing `Project` API.
- Produces, `simulation.h`:

```cpp
inline constexpr double SupplyVoltage = 230;        // V
inline constexpr double SupplyImpedance = 0.4;      // Ω, on supply L (ADR 0007)
inline constexpr double ContactResistance = 0.001;  // Ω, a closed contact and the supply N/PE ties
inline constexpr double CopperResistivity = 0.0175; // Ω·mm²/m

// The live circuit of a project: on/off settings by object id and the latest solve (ADR 0008,
// 0029). Solves at once, after every Project::changed() and after every setting that changes.
class Simulation : public QObject
{
    Q_OBJECT
public:
    explicit Simulation(const Project &project, QObject *parent = nullptr);
    // a device's handle is up, or a load is switched on; false if never switched on
    [[nodiscard]] bool isOn(QStringView id) const;
    // a device or a non-socket load of the project; false (nothing stored) otherwise. Setting the
    // value it already has stores it and doesn't solve.
    bool setOn(const QString &id, bool on);
    [[nodiscard]] double wireCurrent(QStringView wireId) const;     // A; 0 if unknown
    [[nodiscard]] double deviceCurrent(QStringView deviceId) const; // A, its largest contact
    [[nodiscard]] double loadCurrent(QStringView loadId) const;     // A
    // V across the terminals it draws from; 0 if it draws from none
    [[nodiscard]] double loadVoltage(QStringView loadId) const;
    [[nodiscard]] bool isEnergized(QStringView loadId) const;

signals:
    void solved(); // after every solve
};
```

- [ ] **Step 1: Write the failing tests** in `tst_simulation.cpp`.
  - The fixture builds small circuits through `Project` with explicit wire lengths and sections. A local helper gives the expected current `230 / (0.4 + Σseries R + 230²/P)`, where the series R counts each wire `0.0175·L/S`, each closed contact on the path and the supply N tie at `0.001`.
  - The base circuit: a 1P C10 breaker `d1` at (0, 0); wires `supply:L → d1:1` (1 m, 2.5 mm²); a lamp `l1` (10 W) at (0, 270) with `d1:2 → l1:L` and `l1:N → nbar:1` (15 m, 1.5 mm²); and `nbar:2 → supply:N` (1 m, 2.5 mm²).
  - Tests:
    - `everythingStartsOff`: `isOn` false for `d1` and `l1`; every current is 0; `l1` is not energized.
    - `lampOnClosedBreaker`: `setOn(d1)`, `setOn(l1)` → `loadCurrent(l1)`, `deviceCurrent(d1)` and every wire current equal the helper's value within 1e-9 relative. `loadVoltage(l1)` ≈ `I · 5290`. `l1` is energized.
    - `wireResistanceFollowsLengthAndSection_data/…`: rows (length, section) = (15, 1.5), (100, 1.5), (15, 4) for the lamp wires, with the lamp at 2000 W so the wires matter. The current matches the helper within 1e-9 relative.
    - `parallelLampsAddCurrents`: a second lamp `l2` chained from `l1` (L, N), both on → `deviceCurrent(d1) ≈ loadCurrent(l1) + loadCurrent(l2)` within 1e-9 relative.
    - `sharedNeutralCarriesTheSum`: a second breaker `d2` feeds lamp `l2` whose N returns through `nbar:3` → `wireCurrent(nbar:2 → supply:N) ≈ I1 + I2` within 1e-9 relative.
    - `openContactStopsCurrent`: lamp on, `d1` off → every current `< 1e-6`, `l1` not energized, `loadVoltage(l1) < 1`.
    - `twoPoleDeviceSwitchesBothPoles`: a 2P C40 in L and N. On: both poles carry the lamp current and `deviceCurrent` equals it. Off: all currents `< 1e-6`.
    - `floatingPartsSolve`: an unwired RCD and a lamp wired on L only. Every result is finite (`std::isfinite`), the half-wired lamp draws `< 1e-6`, and the base lamp is unchanged.
    - `pluggedApplianceDrawsThroughItsSocket`: a socket instead of the lamp, a kettle plugged into place 1 and switched on → `loadCurrent(kettle)` matches the helper with P = 2000, and the kettle is energized. Kettle off → 0. `setOn(socket)` returns false.
    - `unpluggedApplianceDrawsNothing`: an unplugged kettle, on → current 0, not energized, `loadVoltage` 0.
    - `lampWiredToPeLights`: `l1:N → pebar:1` and `pebar:2 → supply:PE` instead of N → energized, and the PE wires carry the lamp current.
    - `loadBeforeBreakerStaysEnergized`: `supply:L → l1:L` directly, the breaker off → energized.
    - `shortIsLimitedBySupplyAndWires`: `d1:2 → nbar:1` (0.3 m, 1.5 mm²), breaker on → current `230 / (0.4 + R_wires + 0.002)`, finite, over 100 A.
    - `energizedNeedsMoreThanHalfTheVoltage`: a 10 W lamp in series with a 100 W lamp (`l1:N → l2:L`, `l2:N` to N) → `l1` gets about 209 V and is energized; `l2` gets about 21 V and is not.
    - `settingsSurviveEdits`: `setOn(d1)`, then each of these keeps `isOn(d1)` true: `updateDevice` (moved), `removeDevice` then `addDevice` with the same id, and `addDevice` of a new device (which is off). A plugged kettle that is on stays on after unplugging and re-plugging. `setOn(u"d99"_s, true)` returns false.
    - `solvesOncePerChange`: `QSignalSpy(&simulation, &Simulation::solved)`. One emission per `Project` mutation, one per `setOn` that changes a value, none for `setOn` to the same value.
    - `largePanelSolvesQuickly`: `enclosure-36` with 36 1P breakers, 60 lamps in a grid below it (spaced 54 vertically, 45 horizontally, starting at (0, 540)), and one wired lamp. `setOn` on that lamp takes `< 100 ms` (`QElapsedTimer`).

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_simulation --output-on-failure`
  Expected: compile error (`simulation.h` missing).

- [ ] **Step 3: Implement `Simulation`.**
  - The constructor connects `Project::changed` to a private `solve()` and calls it once.
  - `solve()`:
    - maps terminals to nodes (bars merged);
    - adds the supply source (`injected = SupplyVoltage / SupplyImpedance`, admittance `1 / SupplyImpedance` to the reference) and the N and PE ties;
    - adds wires, the contacts of on devices, and the loads per Global Constraints;
    - calls `solveNodeVoltages`;
    - stores currents (`|ΔV|·|y|`), voltages and energized flags in hashes by id;
    - emits `solved()`.

- [ ] **Step 4: Run the tests, then the whole core suite**
  Run: `cmake --build build && ctest --test-dir build -L core --output-on-failure`
  Expected: PASS.

- [ ] **Step 5: Commit** (`F08: simulation state and solve beside the project`).

### Task 4: Switching devices on the canvas

**Files:**
- Create: `tests/gui/tst_simulating.cpp`
- Modify: `src/app/svgpixmap.h`, `src/app/svgpixmap.cpp`, `src/app/sceneitems.h`, `src/app/sceneitems.cpp`, `src/app/editorscene.h`, `src/app/editorscene.cpp`, `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `tests/gui/editorhelpers.h`, `tests/gui/editorhelpers.cpp`, `tests/gui/tst_catalogart.cpp`, `tests/CMakeLists.txt` (`add_gui_test(tst_simulating gui/tst_simulating.cpp)`)

**Interfaces:**
- Consumes: `Simulation` (Task 3).
- Produces:

```cpp
// svgpixmap.h
inline constexpr qreal HandleTravel = 12.5; // picture units a handle moves down when off
// The picture with its "handle" element moved down by HandleTravel; one renderer per path.
QSvgRenderer &switchedOffRenderer(const QString &path);

// DeviceItem
void setOn(bool on); // repaints; a new item is off
[[nodiscard]] bool isOn() const;
[[nodiscard]] QRectF handleRect() const; // item coordinates, where the handle is drawn now

// EditorScene
[[nodiscard]] Simulation &simulation(); // created and owned by the scene
// MainWindow
Simulation &simulation(); // the scene's

// editorhelpers.h
// The tutorial loads (ADR 0016) and their wires, after addTutorialDevices and addTutorialWires:
// l1 Lamp 1 (0, 270), l2 Lamp 2 (45, 270), l3 Socket 1 (94.5, 270), l4 Socket 2 (175.5, 270),
// l5 Socket 3 (256.5, 270), l6 Socket 4 (337.5, 270); l7 TV in l3 place 1, l8 air conditioner in
// l4 place 1, l9 kettle in l5 place 1, l10 microwave in l5 place 2. Wires, each with
// newWireLength() (15 m): d3:2→l1:L, nbar:2→l1:N, pebar:2→l1:PE, then l1→l2 L, N, PE, all 1.5;
// d4:2→l3:L, nbar:3→l3:N, pebar:3→l3:PE, l3→l4 L, N, PE, and d5:2→l5:L, nbar:4→l5:N,
// pebar:4→l5:PE, l5→l6 L, N, PE, all 2.5.
void addTutorialLoads(Project &project);
```

- `switchedOffRenderer` reads the SVG bytes and adds `transform="translate(0 12.5)"` to the `<g id="handle"` element.
- `DeviceItem::paint` uses `switchedOffRenderer` when off, else `svgRenderer`. It keeps the handle's on-position bounds from `setDevice`, as it keeps the `label` rect.
- In the `EditorScene` constructor, create the `Simulation` **after** the scene's own `Project::changed` connections. Then a solve after a change runs after the syncs and finds every item the change added. Connect `Simulation::solved` to a new private `syncLive()` and call it at the end of the constructor. In this task `syncLive()` sets each device item's `setOn(simulation.isOn(id))`.
- Clicks:
  - `pressItem` remembers the pressed id as a switch press when the press, in item coordinates, is inside `handleRect()`.
  - `mouseReleaseEvent`, when the press didn't become a move, flips that id with `setOn(id, !isOn(id))`.
  - `cancelMove` forgets the switch press. Wire-tool presses never reach `pressItem`.

- [ ] **Step 1: Write the failing tests.**
  - New file `tst_simulating.cpp`, fixture as in `tst_loading` (window, canvas, scene; tutorial devices and wires in `init`; fitted view; `cleanup` restores English and the dark theme). A helper `clickHandle(id)` clicks the viewport point of `handleRect().center()`. Tests:
    - `devicesStartOffWithHandleDown`: for d1..d5, `simulation().isOn` and the item's `isOn()` are false. `handleRect()` equals the picture's `handle` bounds translated by (0, 12.5). A device dropped from the palette (`dropPreset`) is off too.
    - `clickOnHandleSwitches`: `clickHandle("d1")` → on in the simulation and on the item, `handleRect()` equals the picture's bounds, and d1 is selected. Click again → off. Screenshot `f08-handles` with d1 and d2 on.
    - `clickOnBodySelectsWithoutSwitching`: a click on the rating label of d3 selects it, and it stays off.
    - `dragFromHandleMovesWithoutSwitching`: `mouseDrag` from d5's handle to the slot at module 8 → `device("d5")->module == 8`, still off.
    - `escapedPressOnHandleDoesNotSwitch`: press on d3's handle, `Qt::Key_Escape`, release → off, unmoved.
    - `doubleClickOnHandleFlipsTwice`: `QTest::mouseDClick` on d3's handle → off, unmoved. Then a plain click → on.
    - `wireToolClicksDoNotSwitch`: wire tool on, click d1's handle → off.
    - `switchingIsNotAnEdit`: the click leaves `undoStack().count()` unchanged. Switch d1 on, move d3 by dragging its body, undo → d1 is still on.
    - `switchSurvivesDeleteAndUndo`: d3 on, select it, Delete, undo → d3 on and drawn on.
    - `tutorialLoadsFixture`: `addTutorialLoads` adds 10 loads and 18 wires, and every load wire is 15 m.
  - In `tst_catalogart.cpp`, the new function `switchedOffPicturesMoveOnlyTheHandle`: for every device variant, `switchedOffRenderer` is valid. Its mapped `handle` bounds equal the on renderer's translated by (0, 12.5), and its `body` bounds are equal. Save the sheet `f08-handles-on-off` with each variant on and off at 180 px.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R "tst_simulating|tst_catalogart" --output-on-failure`
  Expected: compile errors (`switchedOffRenderer`, `simulation()`, `handleRect`, `addTutorialLoads`).

- [ ] **Step 3: Implement** the interfaces above.

- [ ] **Step 4: Run the tests, then look at `f08-handles.png` and `f08-handles-on-off.png`.**
  Run: `cmake --build build && ctest --test-dir build -R "tst_simulating|tst_catalogart" --output-on-failure`
  Expected: PASS. Off handles sit at the bottom of their slot, on handles at the top, and nothing else changes.

- [ ] **Step 5: Run the whole suite** (`ctest --test-dir build --output-on-failure`). Expected: PASS, since existing tests don't depend on the handle position.

- [ ] **Step 6: Commit** (`F08: switch devices by clicking their handle`).

### Task 5: Switching loads, glow and halo

**Files:**
- Modify: `src/app/theme.h`, `src/app/theme.cpp`, `src/app/sceneitems.h`, `src/app/sceneitems.cpp`, `src/app/editorscene.cpp`, `tests/gui/tst_simulating.cpp`

**Interfaces:**
- Consumes: `LoadType::light` (Task 1), `Simulation::isOn/isEnergized` (Task 3), `syncLive()` and switch presses (Task 4).
- Produces:

```cpp
// theme.h, the same in both themes
QColor liveColor();      // a running appliance's halo: {0x7c, 0xe0, 0x5a}
QColor lampGlowColor();  // a lit lamp: {0xff, 0xd5, 0x4f}

// LoadItem
void setOn(bool on);
[[nodiscard]] bool isOn() const;
// item coordinates: a circle of radius 4 centred 6 units in from the picture's bottom-right
// corner; empty for a socket
[[nodiscard]] QRectF buttonRect() const;
void setRunning(bool running); // warm glow for a light, green halo otherwise
[[nodiscard]] bool isRunning() const;
```

- The button is filled light (`#e6e8eb`) with a dark glyph when on, and hollow (dark `#2b2d31` fill) with a light ring and glyph when off. The glyph is a power symbol drawn with a line and an arc.
- The glow or halo fades out over 4.5 units around the picture and is painted under it. `boundingRect()` always includes that margin for switchable loads; `shape()` is unchanged.
- `syncLive()` also sets each switchable load's `setOn(isOn)` and `setRunning(isEnergized)`. A press inside `buttonRect()` is a switch press, like a handle press.

- [ ] **Step 1: Write the failing tests** in `tst_simulating.cpp` (zoomed-out view; `addTutorialLoads` where noted):
  - `switchableLoadsShowAButton`: with the tutorial loads, the lamp, kettle and a dropped cooktop have a `buttonRect()` inside their picture rect. It contains no terminal position (with a 3.6 radius around each). Sockets have an empty `buttonRect()`.
  - `clickOnButtonSwitchesLoad`: click l1's button → `simulation().isOn("l1")`, the item's `isOn()`, and l1 is selected. Click again → off. `undoStack().count()` is unchanged.
  - `clickElsewhereOnLoadSelectsWithoutSwitching`: a click in the middle of l1's picture selects it, and it stays off.
  - `dragFromButtonMovesLoad`: a drag from l2's button to free canvas moves it (`load("l2")->position` changed), and it stays off.
  - `doubleClickOnButtonFlipsTwice`: `mouseDClick` on l1's button → off, unmoved.
  - `lampGlowsOnlyWhenOnAndPowered`: l1 on with QF2 off → not running. Click the handles of d1, d2, d3 → running. Screenshot `f08-lamp-glow`.
  - `applianceRunsThroughItsSocket`: d1, d2, d5 on, l9 on → running. Drag the kettle to free canvas at scene (500, 300) → unplugged, still on, not running. Drag it back onto l5 → running. Screenshot `f08-appliance-halo`.
  - `deletingSocketStopsItsAppliancesUntilUndo`: d1, d2, d5, l9, l10 on. Delete l5 → l9 and l10 are unplugged, still on, not running. Undo → plugged and running.
  - `wireToolClicksDoNotSwitchLoads`: wire tool on, click l1's button → off.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_simulating --output-on-failure`
  Expected: compile errors (`buttonRect`, `setRunning`, `liveColor`).

- [ ] **Step 3: Implement** the interfaces above.

- [ ] **Step 4: Run the tests, then look at `f08-lamp-glow.png` and `f08-appliance-halo.png`.**
  Run: `cmake --build build && ctest --test-dir build -R tst_simulating --output-on-failure`
  Expected: PASS. The button is visible and doesn't hide a terminal. The glow reads as light, and the halo is distinct from the PE wire green.

- [ ] **Step 5: Commit** (`F08: switch loads with their button, lamps glow, appliances run`).

### Task 6: Current labels and View → Show currents

**Files:**
- Create: `src/app/currenttext.h`, `src/app/currenttext.cpp`
- Modify: `src/app/CMakeLists.txt`, `src/app/sceneitems.h`, `src/app/sceneitems.cpp`, `src/app/editorscene.h`, `src/app/editorscene.cpp`, `src/app/settings.h`, `src/app/settings.cpp`, `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `translations/*.ts`, `tests/gui/tst_simulating.cpp`

**Interfaces:**
- Consumes: `Simulation::wireCurrent/deviceCurrent` (Task 3), `syncLive()` (Task 4).
- Produces:

```cpp
// currenttext.h
[[nodiscard]] bool showsCurrent(double amperes); // amperes >= 0.005
// "0.04 A", "13.4 A", "575 A", in the current language (QCoreApplication::translate
// ("CurrentText", "%1 A"))
[[nodiscard]] QString currentText(double amperes);

// A wire's current: light text (#e6e8eb) on a dark rounded chip (#1c1e21, alpha 220), pixel size
// 5, the same in both themes, centred on its position, drawn over the wires; clicks go through.
class CurrentLabelItem : public QGraphicsItem
{
public:
    static constexpr qreal LabelZ = 1.8;
    CurrentLabelItem();
    void setText(const QString &text); // "" hides it
    [[nodiscard]] QString text() const;
    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};

// DeviceItem: a white chip with black text at pixel size 5 on the body, centred, 7 units high,
// from y 65, below the slot of an off handle and above the bottom terminals
void setCurrentText(const QString &text); // "" hides it
[[nodiscard]] QString currentText() const;
[[nodiscard]] QRectF currentRect() const; // item coordinates; empty when hidden

// WireItem: owns a top-level CurrentLabelItem (like DeviceItem's designation), centred on the
// midpoint of the route's longest segment
void setCurrentText(const QString &text);
[[nodiscard]] QString currentText() const;
[[nodiscard]] CurrentLabelItem *currentLabel() const;

// EditorScene
void setCurrentsShown(bool shown); // default true; re-syncs the labels
[[nodiscard]] bool currentsShown() const;

// settings.h
bool savedShowCurrents(); // true when unset
void saveShowCurrents(bool shown);
```

- `syncLive()` sets each device's text to `currentText(deviceCurrent)` and each wire's to `currentText(wireCurrent)`, or "" when `!showsCurrent` or labels are hidden. `retranslate()` and `setCurrentsShown()` call `syncLive()`.
- `MainWindow` adds the checkable `showCurrentsAction` to the View menu after Fit to window. It is checked from `savedShowCurrents()`, which is applied to the scene at start. On toggle it updates the scene and saves. `retranslateUi()` sets "Show currents".

- [ ] **Step 1: Write the failing tests** in `tst_simulating.cpp`:
  - `currentTextFormats_data/…` (English): 0.043472 → "0.04 A"; 9.994 → "9.99 A"; 9.996 → "10.0 A"; 13.409 → "13.4 A"; 99.96 → "100 A"; 556.9 → "557 A". `showsCurrent(0.0049)` is false and `showsCurrent(0.005)` true.
  - `labelsWhereCurrentFlows`: tutorial loads; click the handles of d1, d2, d3 and l1's button.
    - Device texts are "0.04 A" for d1, d2, d3, and "" for d4 and d5.
    - Each wire carrying the lamp current shows "0.04 A": supply L/N → d1, d1 → d2 (both), d2:2 → d3:1, d2:N2 → nbar:1, d3:2 → l1:L, nbar:2 → l1:N.
    - The l1 PE wire, the l1 → l2 wires and the jumpers d3:1 → d4:1 → d5:1 show "".
    - Screenshot `f08-labels`.
  - `labelsDrawAboveWiresAndLetClicksThrough`: a shown label has `zValue() == CurrentLabelItem::LabelZ`, `> WireItem::WireZ`, and `acceptedMouseButtons() == Qt::NoButton`. A click at the label's centre selects its wire.
  - `deviceLabelKeepsHandleRatingAndTerminalsClear`: for d1 and d3, on and off, `currentRect()` doesn't intersect `handleRect()`, the picture's `label` rect, or a 3.6-radius square around any terminal.
  - `showCurrentsTogglesAndPersists`: click View → Show currents (`clickMenuBarItem`, `clickMenuItem`) → every device and wire text is "", and `savedShowCurrents()` is false. A new `MainWindow` has the action unchecked. Click it again → labels back, saved true. Screenshot `f08-currents-hidden`. `cleanup` saves `true`.
  - `labelsFollowLanguage`: as `labelsWhereCurrentFlows`, then the Language menu → Русский → d1 "0,04 А", switches unchanged, the label positions unchanged. Қазақша → "0,04 А". Screenshots `f08-labels-ru`, `f08-labels-kk`.
  - `labelsInLightTheme`: the light theme with labels showing. Screenshot `f08-labels-light`.
  - `shortShowsWholeAmperes`: add the wire `d3:2 → nbar:5` (1.5 mm², 0.3 m) through the model, switch d1, d2, d3 on → d3's text is "557 A".

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_simulating --output-on-failure`
  Expected: compile errors (`currenttext.h`, `setCurrentText`, `showCurrentsAction` missing).

- [ ] **Step 3: Implement** the interfaces above.

- [ ] **Step 4: Translations.** Run `cmake --build build --target update_translations` and enter the strings table rows in `electricity_ru.ts` and `electricity_kk.ts`.

- [ ] **Step 5: Run the tests, then look at every `f08-labels*.png` and `f08-currents-hidden.png`.**
  Run: `cmake --build build && ctest --test-dir build -R "tst_simulating|translations" --output-on-failure`
  Expected: PASS. Labels are readable on both themes and don't cover terminals or designations. If wire labels in the enclosure crowd each other, adjust their placement (for example, the longest segment outside the enclosure interior when there is one), not the tests.

- [ ] **Step 6: Commit** (`F08: current labels on devices and wires, View → Show currents`).

### Task 7: Roadmap scenario, smoke test, done

**Files:**
- Modify: `tests/gui/tst_simulating.cpp`, `docs/roadmap.md`

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Write the scenario test** `roadmapScenario` (zoomed out, tutorial loads, real clicks and drags only):
  1. Click the handles of d1, d2, d3 → all three drawn on, every current 0 (nothing is on yet). Screenshot `f08-scenario-1-breakers`.
  2. Click l1's button → `deviceCurrent("d3")` = 0.043472 ± 0.0001, l1 energized and glowing, labels as in `labelsWhereCurrentFlows`. Screenshot `f08-scenario-2-lamp`.
  3. Click d3's handle → l1 not energized, no glow, d3's and the lamp wires' labels "". Screenshot `f08-scenario-3-off`.
  4. Click d5's handle, l9's and l10's buttons → `deviceCurrent("d5")` = 13.409 ± 0.005, both haloed, d5's label "13.4 A". Screenshot `f08-scenario-4-kitchen`.
  5. Drag l9 (the kettle) to scene (500, 300) → unplugged, not running, d5 = 5.1449 ± 0.005, label "5.14 A". Screenshot `f08-scenario-5-unplugged`.
  6. Russian, then Kazakh: screenshots `f08-scenario-ru` and `f08-scenario-kk`.

- [ ] **Step 2: Run it.** `ctest --test-dir build -R tst_simulating --output-on-failure`. Expected: PASS, with no product code changes needed. If it fails, find the cause in the code (systematic debugging); never change the expected values.

- [ ] **Step 3: Look at the scenario screenshots** and fix any visual defects in code.

- [ ] **Step 4: Full suite and quality gates.**
  Run: `scripts/quality_gates.sh` (in the background; wait for the notice).
  Expected: "All quality gates passed".

- [ ] **Step 5: Smoke test the real binary** under `xvfb-run -a -s "-screen 0 1600x900x24"`:
  - start `./build/electricity`;
  - with `xdotool`, drop a 1P breaker, wire supply L → its top and its bottom to a dropped lamp's L, and N bar → the lamp's N;
  - click the breaker handle and the lamp button;
  - `import -window root build/screenshots/f08-smoke.png`, then kill the app.
  - Open the PNG: the handle is up, the lamp glows, and labels show.

- [ ] **Step 6: Roadmap.** F08 ◐ → ☑ in `docs/roadmap.md`.

- [ ] **Step 7: Commit** (`F08: roadmap scenario, mark done`).
