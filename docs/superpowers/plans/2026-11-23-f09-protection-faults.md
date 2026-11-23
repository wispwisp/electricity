# F09 Protection and Faults Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The live panel protects itself. Breakers trip on short circuit at once and on overload after a realistic delay, RCDs trip on leakage, and wires warn when they overheat. The user right-clicks a load to cause a fault, clicks a handle (or the supply block) to reset, presses the RCD test button, and sets the time speed. Danger that nothing stops is marked.

**Architecture:**
- `panelcore`:
  - The catalog gains protection data: τ, trip factor and instant-trip bands per device type, the stairwell breaker on the supply, and a wire ampacity table.
  - `Simulation` (F08) gains faults, trips, heat, simulated time and events, all kept by id beside the switch settings.
  - Every solve runs ADR 0008's instant-trip loop. `step(seconds)` integrates heat exactly and jumps to each thermal trip.
- App:
  - `EditorScene` shows trips, heat, fault marks and danger. It turns clicks into resets and test presses, and the load menu into faults.
  - `MainWindow` owns the time control and the 50 ms timer. It shows events in the status bar.
- Nothing new is undoable (ADR 0011) or saved (ADR 0010).

**Tech Stack:** C++20, Qt 6.12.0 static (Core, Widgets, Svg), Qt Test (core and offscreen GUI), Xvfb + xdotool for the smoke run.

**Spec:** `docs/superpowers/specs/2026-11-23-f09-protection-faults-design.md`

## Global Constraints

- No new Qt modules, plugins or libraries.
- Electrical constants: F08's stay as they are (`src/core/simulation.h`). New ones:

  | Name | Value | Meaning |
  |---|---|---|
  | `FaultResistance` | 0.001 | Ω, short circuit L–N and earth fault L–PE |
  | `LeakageResistance` | 1000 | Ω, from L to earth (the reference node): a person |
  | `TouchVoltageLimit` | 50 | V (ПУЭ 1.7.53); a fault is dangerous above it |
  | `OverheatingFactor` | 1.45 | × Iz: a wire overheats at heat ≥ (1.45·Iz)² |
  | `MaxTripRounds` | 32 | instant-trip rounds per solve |
  | `SupplyBreakerId` | `"supply"` | the stairwell breaker's id in trip queries and events |

- Catalog values (`devices.json`):
  - breaker and RCBO: `protection` with τ 73 s, trip factor 1.3, and curves B `[3, 5]`, C `[5, 10]`, D `[10, 20]`;
  - RCD: `protection` with τ 73 s only;
  - `supply-1p`: `upstreamBreaker` with curve C and rated current 50;
  - `wires`: τ 120 s; ampacity 1.5 → 19, 2.5 → 27, 4 → 38, 6 → 46, 10 → 70, 16 → 85 (ADR 0016).
- Network additions (refine F08's network, ADR 0029):
  - A load's fault acts between the terminals it draws from, as F08 defines them (its own, or its socket's while it is plugged in):
    - short circuit: `FaultResistance` between L and N;
    - leakage: `LeakageResistance` from L to the reference node;
    - earth fault: `FaultResistance` between L and the PE terminal of the same object (load or socket).
  - A socket's fault uses the socket's own terminals. An unplugged appliance's fault does nothing.
  - The supply's Norton source (`E/Zs` injected, `1/Zs` to the reference) exists only while the stairwell breaker is not tripped.
- Protection rules:
  - In = `ratedCurrent`. IΔn = `residualCurrent / 1000` A.
  - Pole current: the complex current of each closed contact, top to bottom. Residual current: `|Σ pole currents|` over an RCD's or RCBO's contacts.
  - The stairwell breaker's current is `|E − V(supply L)| / Zs`.
  - Instant trips, after every solve:
    - A breaker or RCBO is triggered when its largest `|pole current|` ≥ `upper · In`. The stairwell breaker uses its own current.
    - An RCD or RCBO is triggered when its residual current ≥ IΔn.
    - Every residual-triggered RCD trips with cause Leakage. Of the current-triggered breakers (stairwell included), only the one with the smallest In trips (ties: the largest current), with cause ShortCircuit.
    - An RCBO triggered both ways trips with cause ShortCircuit.
    - Then solve again, at most `MaxTripRounds` rounds.
  - Heat `H` (A²), starting at 0, for each breaker, RCD, RCBO, wire and the stairwell breaker. `I` is the device's largest pole current (0 while off or tripped), the wire's current, or the stairwell current.
    - It updates exactly over `t` seconds: `H ← I² + (H − I²)·e^(−t/τ)`.
  - Thermal trip: a breaker, RCBO or the stairwell breaker that is on trips when `H ≥ (f·In)²`.
    - From `H0` under `I² > (f·In)²` it trips after `τ·ln((I² − H0)/(I² − (f·In)²))`.
    - Its cause is ShortCircuit if `I ≥ lower · In`, else Overload.
  - RCD overloaded: `H > In²`. It never trips on heat.
  - Wire states: Overheating if `H ≥ (1.45·Iz)²`, Overloaded if `H > Iz²`, else Normal. A section without an ampacity stays Normal.
  - `heatLevel`:
    - breaker or RCBO: `clamp((H − In²) / ((f·In)² − In²), 0, 1)`;
    - RCD: 1 while overloaded, else 0;
    - anything else: 0.
  - Touch voltage, for a load whose fault acts:
    - leakage: `|V(L)|`;
    - earth fault: `|V(PE)|`, the PE of the object the fault acts on.
    Dangerous when it is above `TouchVoltageLimit`.
  - A tripped device counts as off (`isOn` false). `setOn(id, …)` on it clears the trip.
  - Pressing the test button trips an RCD or RCBO with cause Test, if it is on and `|V(top of contact 1) − V(top of contact 2)| > 0.5 · SupplyVoltage`.
- State by id (fault, trip, heat) outlives its object, like F08's switch settings, and is never touched by `Project::changed()`.
- Events: one per change of state, emitted after the `solved()` (or `heated()`) of the solve or step that settled it. Kinds:
  - Tripped;
  - Overloaded (a wire or an RCD enters it);
  - Overheating;
  - Dangerous;
  - Cleared (a reset; a wire back to Normal; an RCD no longer overloaded; danger over).
  A wire going from Overheating back to Overloaded emits Overloaded.
- Status bar: the existing mechanism stays. A hint is a temporary message; when the message is empty, `MainWindow` shows the **idle text** instead of always "Ready".
  - The idle text is the newest active condition whose object still exists (a device, wire or load of the project, or `SupplyBreakerId`). Active conditions are kept per id: Tripped, Overloaded, Overheating and Dangerous replace that id's entry and make it the newest. Cleared removes it.
  - With no active condition, the idle text is "Ready".
  - Every event, every `Project::changed()` and every language switch re-shows the idle text.
  - Existing tests that read "Ready" from `statusBar()->currentMessage()` keep passing unchanged.
- Colours (`theme.h`, the same in both themes):
  - `trippedColor()` `{0xe5, 0x39, 0x35}`, also used for overheating and danger;
  - `overloadedColor()` `{0xff, 0x98, 0x00}`;
  - `faultMarkColor()` `{0xff, 0xc1, 0x07}`.
- Marks:
  - A device's "!" badge is a circle of radius 4, centred at (width − 5, 17) in item coordinates. A red outline (1 unit) goes around the body.
  - The supply's badge is centred at the supply picture's (49, 14), with a red outline around the picture.
  - A load's ⚡ mark is a 7×7 square bottom-left in the picture: x 1–8, y h−8 to h−1.
- Object names:
  - load menu actions: `shortCircuitAction`, `leakageAction`, `earthFaultAction`, `removeFaultAction`;
  - time control: `timeLabel`, `pauseAction`, `speed1Action`, `speed10Action`, `speed60Action`, timer `simulationTimer`;
  - scene item id `supplyTripMark`.
  - Speed texts `×1`, `×10`, `×60` are not translated.
- Strings (English source; enter the translations exactly):

  | Context | Source | ru | kk |
  |---|---|---|---|
  | EditorScene | Short circuit | Короткое замыкание | Қысқа тұйықталу |
  | EditorScene | Leakage | Утечка тока | Ток ағуы |
  | EditorScene | Earth fault | Замыкание на корпус | Корпусқа тұйықталу |
  | EditorScene | Remove fault | Убрать неисправность | Ақауды жою |
  | EditorScene | The test button works only when the RCD is on and has power | Кнопка «Тест» работает, только когда УЗО включено и под напряжением | «Тест» батырмасы УЗО қосулы және кернеуде болғанда ғана істейді |
  | MainWindow | Time | Время | Уақыт |
  | MainWindow | Pause | Пауза | Үзіліс |
  | EventText | %1 tripped: overload, %2 on %3 | %1: срабатывание от перегрузки, %2 на %3 | %1: асқын жүктемеден ажыратылды, %2, номиналы %3 |
  | EventText | %1 tripped: short circuit, %2 on %3 | %1: срабатывание от короткого замыкания, %2 на %3 | %1: қысқа тұйықталудан ажыратылды, %2, номиналы %3 |
  | EventText | %1 tripped: leakage %2, trips at %3 | %1: срабатывание от утечки %2, порог %3 | %1: ток ағуынан ажыратылды, %2, шегі %3 |
  | EventText | %1 tripped: test button | %1: срабатывание от кнопки «Тест» | %1: «Тест» батырмасынан ажыратылды |
  | EventText | %1 overloaded: %2 on %3, an RCD doesn't protect against overload | %1: перегрузка %2 при номинале %3, УЗО не защищает от перегрузки | %1: асқын жүктеме %2, номиналы %3, УЗО асқын жүктемеден қорғамайды |
  | EventText | Wire %1 overloaded: %2 on %3, allowed %4 | Провод %1 перегружен: %2 на %3, допустимо %4 | %1 сымы асқын жүктелген: %2, қимасы %3, рұқсат етілгені %4 |
  | EventText | Wire %1 overheating: %2 on %3, allowed %4 | Провод %1 перегревается: %2 на %3, допустимо %4 | %1 сымы қызып барады: %2, қимасы %3, рұқсат етілгені %4 |
  | EventText | %1: %2 through a person, no RCD turned it off | %1: через человека идёт %2, ни одно УЗО не отключило | %1: адам арқылы %2 өтіп жатыр, ешбір УЗО ажыратпады |
  | EventText | %1: its case is live, %2 | %1: на корпусе напряжение %2 | %1: корпуста %2 кернеу бар |
  | EventText | Stairwell breaker | Этажный автомат | Қабат қалқанының автоматы |
  | EventText | Supply | Ввод | Кіріс |
  | EventText | N bar | Шина N | N шинасы |
  | EventText | PE bar | Шина PE | PE шинасы |
  | EventText | %1 mA | %1 мА | %1 мА |
  | EventText | %1 V | %1 В | %1 В |

  - Placeholders:
    - names: a device designation, "Stairwell breaker", a wired load's caption ("Lamp 1"), or a plug-in appliance's translated catalog name ("Kettle");
    - currents: `currentText`;
    - ratings: `ratingText`, and "C50" for the stairwell breaker;
    - leakage currents: below 1 A in whole mA ("230 mA"), else `currentText`;
    - IΔn: "30 mA";
    - ampacity: whole amperes with CurrentText's "%1 A" ("19 A");
    - section: `sectionText`;
    - voltage: whole volts ("230 V");
    - a wire's name: its two ends joined by " → ": a designation, a load name, "Supply", "N bar" or "PE bar".
  - A task that adds strings runs `cmake --build build --target update_translations` and enters its rows, so `translations_complete` passes after every task.
- Expected values: work them out from the constants and the formulas above, never read them back from the code. Hand-checked values for the tutorial fixture (`addTutorialDevices/Wires/Loads`, wires 0.3 m inside the enclosure, 15 m to loads):

  | Case | Value |
  |---|---|
  | QF1, QD1, QF4 on; kettle, microwave in Socket 3 and a washing machine in Socket 4, all on | QF4 22.005 A, thermal trip after 163.48 s from cold, cause Overload, "22.0 A on C16" |
  | QF1, QD1, QF3 on, short circuit on Socket 1 (`l3`) | 370.1 A, instant, "370 A on C16" |
  | QF1, QD1, QF2 on, wire `d3:2 → nbar:5` (0.3 m, 1.5 mm²) | 556.9 A: QF2 (C10), QF1 (400 A) and the stairwell breaker (500 A) are all triggered; only QF2 trips, "557 A on C10" |
  | Leakage on the kettle, QD1 on | ≈ 0.2299 A residual → "230 mA, trips at 30 mA" |
  | Wire `supply:L → supply:N` (0.3 m, 2.5 mm²) | 570.6 A, the stairwell breaker trips, "571 A on C50" |
  | `d6` 1P C32 at (0, 7), jumper `d5:1 → d6:1` (0.3 m, 6 mm²), 9 kW cooktop with L, N, PE wires of 15 m 1.5 mm² | 34.64 A; overloaded after 42.95 s, overheating after 120.15 s (τ 120); C32 never trips (41.6 A) |
  | The same with a short circuit on the cooktop | 301.4 A, under the C32's 320 A: thermal trip after 1.404 s, cause ShortCircuit |
  | A breaker at 2·In from cold | trips after 40.08 s |

- Source files and tests:
  - Every new source file has an SPDX `GPL-3.0-or-later` header and is clang-format clean (Qt style, 100 columns).
  - GUI tests follow `tst_simulating`'s fixture: `initTestCase` uses temporary settings and English; tutorial devices and wires in `init`; `cleanup` restores English and the dark theme; zoomed out for loads.
  - New GUI tests go in `tests/gui/tst_protecting.cpp`, new core tests in `tests/core/tst_protection.cpp`. Copy the few helpers they need from `tst_simulating` and `tst_simulation`, don't share them.
- Tests:
  - Never change an existing test to make it pass.
  - The owner approved rewriting exactly two existing tests, `TestSimulation::shortIsLimitedBySupplyAndWires` (Task 2) and `TestSimulating::shortShowsWholeAmperes` (Task 3), each in its own red/green step.
  - Adding test functions or data rows to an existing file is allowed where a step says so.
  - If any other existing test fails, stop and report it.
- Screenshots: `build/screenshots/f09-<step>.png` via `saveScreenshot(widget, "f09", "<step>")`.
- Never `git push`. Commit only as the steps say. End each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- To wait for a long command, run it with `run_in_background: true` and wait for its completion notice. Never poll with `pgrep -f`. Before reporting, stop every background task you started (`ps -eo pid,ppid,etime,cmd`).

## Review Focus

- **A tripped device or an overheating wire is deleted.** Its message leaves the status bar, and undo brings it back, still tripped. Task 3 `messageOfADeletedObjectGoes`.
- **The language is switched while a trip message shows.** The message is re-translated, and no trip, fault or switch changes. Task 3 `statusBarFollowsLanguage`.
- **A tripped handle is clicked while the fault is still there.** The device trips again at once, with one new Tripped event and no loop or hang. Task 5 `clickOnTrippedHandleResets`.
- **Undo and redo after faults and trips** never reset a trip, remove a fault or switch anything. Task 6 `faultActionsAreNotEdits`.
- **A trip from the timer while the user drags a device or draws a wire.** The drag finishes where it is dropped, and the wire in progress stays. Task 7 `tripDuringDragKeepsTheDrag`.

---

### Task 1: Protection data in the catalog, ADR 0030, roadmap

**Files:**
- Modify: `resources/catalog/devices.json`, `src/core/catalog.h`, `src/core/catalog.cpp`, `tests/core/tst_catalog.cpp`, `docs/roadmap.md`, `docs/adr/README.md`
- Create: `docs/adr/0030-protection-faults-and-time.md`

**Interfaces:**
- Produces, `catalog.h`:

```cpp
struct InstantTripBand
{
    double lower = 0; // × In: from here a trip is reported as a short circuit
    double upper = 0; // × In: an instant (magnetic) trip (ADR 0008)
    friend bool operator==(const InstantTripBand &, const InstantTripBand &) = default;
};

struct ProtectionSpec
{
    double timeConstant = 0;               // s, of the heat state
    std::optional<double> tripFactor;      // thermal trip at (factor·In)²; none: never on heat
    QMap<QString, InstantTripBand> curves; // by curve letter; empty: no instant trip on current
};
// DeviceType gains:
std::optional<ProtectionSpec> protection; // none: never trips, never heats

// SupplyType gains (the stairwell breaker on its L, ADR 0008; empty curve: none):
QString upstreamCurve;
double upstreamRating = 0; // A

struct WireRating
{
    double section = 0;  // mm²
    double ampacity = 0; // A, Iz (ADR 0016)
};
// Catalog gains:
double wireTimeConstant = 0; // s
QList<WireRating> wireRatings;
[[nodiscard]] double ampacity(double section) const; // A; 0 if the section has none
```

- JSON (all optional, so the existing test fixture stays valid):
  - in a device type: `"protection": {"timeConstant": 73, "tripFactor": 1.3, "curves": {"C": [5, 10]}}`;
  - in a supply: `"upstreamBreaker": {"curve": "C", "ratedCurrent": 50}`;
  - top level of `devices.json`: `"wires": {"timeConstant": 120, "ratings": [{"section": 1.5, "ampacity": 19}, …]}`.
- Loader errors:
  - `device type <id>: protection time constant must be positive`
  - `device type <id>: trip factor must be positive`
  - `device type <id>: curve <c> needs 0 < lower <= upper` (also for an entry that isn't two numbers)
  - `device type <id>: allowed curve <c> has no band` (a value of `allowedValues.curve` missing from `curves`, when `protection` is present)
  - `supply <id>: upstream breaker curve <c> unknown` (not a curve of the `breaker` type)
  - `supply <id>: upstream breaker rating must be positive`
  - `wires: time constant must be positive`
  - `wires: ampacity of <section> mm² must be positive`
  - `wires: duplicate section <section>`

- [ ] **Step 1: Write the failing tests** in `tests/core/tst_catalog.cpp`: new functions, and new rows in `rejectsInvalidCatalog_data`, one per loader error above.
  - `loadsProtection`: patch the fixture's breaker type with the JSON example → no errors, and the parsed spec equals it. The unpatched fixture also loads with no errors.
  - `protectionMatchesSpec`: the embedded catalog has exactly the Global Constraints values for breaker, RCD, RCBO, `supply-1p` and the wires.
  - `everyWireSectionHasAnAmpacity`: `ampacity(s) > 0` for every `s` in `WireSections`, and `ampacity(3)` is 0.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_catalog --output-on-failure`
  Expected: compile errors (`ProtectionSpec`, `ampacity` unknown).

- [ ] **Step 3: Implement the parsing** in `loadDevices` (supplies after device types, since they look up the `breaker` type), and add the data to `devices.json`.

- [ ] **Step 4: Run the tests.** Same command. Expected: PASS.

- [ ] **Step 5: ADR 0030 and the index.** Write `0030-protection-faults-and-time.md` from the template, refining 0007, 0008, 0013, 0014 and 0029. It records:
  - protection state (faults, trips, heat) in `Simulation` by id, not undoable, not saved;
  - step jumps to each thermal trip;
  - leakage from L to earth, not the load's PE;
  - the trip cause rule (lower band);
  - the 50 V danger rule;
  - the stairwell breaker hidden until it trips, reset by a click on the supply;
  - wire τ 120 s, never below the breakers' τ;
  - earth fault in the basic load menu (refines 0014);
  - the status bar's idle text (see Global Constraints);
  - that a bare L–N wire now trips a breaker or the stairwell breaker (F08 showed hundreds of amperes);
  - protection data optional in the loader, completeness checked by the embedded-catalog test.
  Add the row to `docs/adr/README.md`.

- [ ] **Step 6: Roadmap.**
  - F09 ☐ → ◐.
  - In the F09 scenario, "a 7 kW cooktop (≈30 A)" becomes "a 9 kW cooktop (≈34.6 A)".
  - Replace the bullet "With solved currents, the 7 kW cooktop…" with: "Solved values: the washing-machine overload on QF4 is ≈ 22.0 A and trips after ≈ 163 s of simulated time; the 9 kW cooktop draws ≈ 34.6 A."
  - F11: add "Allow filter: the load menu's fault actions, the RCD test button and a click on the tripped supply block also go through the filter."
  - F12: add "Earth fault is in everyone's load menu since F09; move it behind engineering details if wanted (ADR 0014). Wire τ (120 s, ADR 0030) editable; it must stay ≥ the breakers' τ, or say what a shorter one means."

- [ ] **Step 7: Commit** (`F09: protection data in the catalog (ADR 0030)`).

### Task 2: Faults and instant trips (core)

**Files:**
- Create: `tests/core/tst_protection.cpp`
- Modify: `src/core/simulation.h`, `src/core/simulation.cpp`, `tests/core/tst_simulation.cpp` (rewrite `shortIsLimitedBySupplyAndWires` only), `tests/CMakeLists.txt` (`add_core_test(tst_protection core/tst_protection.cpp)`)

**Interfaces:**
- Consumes: `ProtectionSpec`, `SupplyType::upstreamCurve/upstreamRating` (Task 1).
- Produces, `simulation.h`:

```cpp
inline constexpr double FaultResistance = 0.001;  // Ω, short circuit L–N and earth fault L–PE
inline constexpr double LeakageResistance = 1000; // Ω, L to earth: about a person (ADR 0007)
inline constexpr double TouchVoltageLimit = 50;   // V (ПУЭ 1.7.53)
inline constexpr int MaxTripRounds = 32;
inline constexpr QStringView SupplyBreakerId = u"supply"; // the stairwell breaker

enum class Fault : std::uint8_t { None, ShortCircuit, Leakage, EarthFault };
enum class TripCause : std::uint8_t { None, Overload, ShortCircuit, Leakage, Test };

struct SimulationEvent
{
    enum class Kind : std::uint8_t { Tripped, Overloaded, Overheating, Dangerous, Cleared };
    Kind kind = Kind::Tripped;
    QString id;                        // a device, SupplyBreakerId, a wire or a load
    TripCause cause = TripCause::None; // Tripped
    double current = 0; // A: at the trip (residual for Leakage), in the wire or RCD, or through
                        // the person (Dangerous leakage)
    double voltage = 0; // V: touch voltage (Dangerous)
    double limit = 0;   // A: In, IΔn or the wire's ampacity
    double time = 0;    // s, elapsed() when it happened
};

// Simulation gains:
// the cause while tripped, else None; a device id or SupplyBreakerId
[[nodiscard]] TripCause tripCause(QStringView id) const;
// clears the stairwell breaker's trip and solves (it may trip again); false if not tripped
bool resetSupplyBreaker();
// any load of the project; false (nothing stored) otherwise; the value it already has doesn't solve
bool setFault(const QString &loadId, Fault fault);
[[nodiscard]] Fault fault(QStringView loadId) const; // None if never set
// trips an RCD or RCBO that is on and has power (cause Test); false otherwise
bool pressTest(const QString &deviceId);
[[nodiscard]] double touchVoltage(QStringView loadId) const; // V; 0 unless its fault acts
[[nodiscard]] bool isDangerous(QStringView loadId) const;    // touchVoltage > TouchVoltageLimit
[[nodiscard]] double elapsed() const;                        // simulated s; 0 until Task 4
signals:
void occurred(const SimulationEvent &event);
```

- `setOn` keeps F08's contract and adds the following. On a tripped device it clears the trip (a Cleared event), stores the value and solves. On an on device the solve may trip it again at once.
- `solve()` keeps one `solved()` per call. Its instant-trip rounds stay inside it.

- [ ] **Step 1: Rewrite `shortIsLimitedBySupplyAndWires` (approved).** Keep its circuit and its `current` formula. Assert instead:
  - `allResultsFinite()`;
  - `tripCause(u"d1") == TripCause::ShortCircuit` and `!isOn(u"d1")`;
  - exactly one `occurred` event (a `QSignalSpy` set up before `setOn`): Tripped `d1`, `near(current)`, limit 10;
  - `tripCause(SupplyBreakerId) == None`, even though 548 A ≥ 500 A also triggered the stairwell breaker;
  - every wire current `< 1e-6`.

- [ ] **Step 2: Write the failing tests** in `tst_protection.cpp`. The base circuit is as in `tst_simulation`: `d1` 1P C10, supply L → `d1:1` (1 m, 2.5 mm²), `d1:2 → l1:L` and `l1:N → nbar:1` (15 m, 1.5 mm²), `nbar:2 → supply:N` (1 m, 2.5 mm²). A helper adds a breaker or RCD with any allowed rating by copying a preset and changing `ratedCurrent`. Tests:
  - `shortTripsBreakerInstantly`: lamp, `d1` on, `setFault(l1, ShortCircuit)` → ShortCircuit. Event current = `230 / (0.4 + base R + 0.001)` (≈ 299.9 A). Every current `< 1e-6`.
  - `magneticTripAtUpperBand_data/…`: rows 99 A (no trip) and 101 A (trips). The test computes the lamp wires' length so that the short current is that value.
  - `smallestBreakerTripsFirst`: `d1` 1P C40 feeds `d2` 1P C16 through 0.3 m wires and a socket with a short. Only `d2` trips; `d1` and the stairwell breaker stay on.
  - `shortWithNoBreakerTripsTheStairwellBreaker`: wire `supply:L → supply:N` (0.3 m, 2.5 mm²) → `tripCause(SupplyBreakerId) == ShortCircuit` at 570.6 A. `resetSupplyBreaker()` trips it again. After removing the wire, `resetSupplyBreaker()` returns true and it stays reset. A second call returns false.
  - `leakageTripsTheRcd`: RCD 2P `d1` feeding the lamp's L and N, leakage on `l1` → Leakage. Residual current = `V(l1:L) / 1000` within 1e-3 relative (≈ 0.2299 A).
  - `neutralBypassingTheRcdTrips_data/…`: a lamp fed through the RCD with its N wired straight to `supply:N`. Rows: 6 W (≈ 26 mA, no trip) and 10 W (≈ 43 mA, Leakage).
  - `earthFaultTripsBreakerAndRcd`: C16 → RCD → socket with PE to the PE bar, earth fault → `d1` ShortCircuit and the RCD Leakage, in one `setFault`.
  - `rcboTripsOnShortLeakageAndEarthFault_data/…`: causes ShortCircuit, Leakage, ShortCircuit.
  - `faultOnUnpluggedApplianceWaitsForItsSocket`: a kettle with a short, unplugged → nothing trips. Plug it in (`updateLoad`) → ShortCircuit. The fault stays across unplugging.
  - `resetWithFaultStillThereTripsAgain`: `setOn(d1, true)` → tripped again, one Cleared then one Tripped event. Fault None, then `setOn` → on, lamp energized.
  - `tripOpensBothPolesOfA2PDevice`: 2P C40 with a short behind it → both contacts carry 0, `!isOn`.
  - `handSwitchOffClearsTheTrip`: after a trip, `setOn(d1, false)` → cause None, still off.
  - `leakageFlowsToEarthWithoutPe`: as `leakageTripsTheRcd`, with no PE wire → the RCD trips.
  - `earthFaultWithoutPeLeavesTheCaseLive`: lamp behind C10 and RCD, PE not wired, earth fault → no trip. Touch voltage > 200 V, dangerous; Dangerous event with that voltage. Fault None → Cleared event, not dangerous.
  - `unprotectedLeakageIsDangerous`: C10 only, leakage → no trip, dangerous, event current = touch voltage / 1000. `setOn(d1, false)` → Cleared.
  - `protectedLeakageIsNotDangerous`: behind an RCD → trips. Touch voltage `< 1`, no Dangerous event.
  - `testButtonTripsOnlyWithPower`:
    - RCD on and fed → `pressTest` true, cause Test, event limit 0.03;
    - RCD off → false;
    - RCD on with no supply wire → false;
    - a breaker → false.
  - `tripsAndFaultsSurviveRemoveAndRestore`: a fault on `l1` and `d1` tripped. Remove both and re-add them with the same ids → both kept.
  - `projectEditsKeepTripsAndFaults`: a rating change (`updateDevice`) and a new wire leave the trip and the fault as they were.
  - `setFaultRefusesUnknownLoadsAndRepeats`: `setFault(u"l99"_s, …)` is false. Setting the same fault again emits no `solved()`.
  - `eventsOncePerChange`: an unrelated project change after a trip emits no new event.

- [ ] **Step 3: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R "tst_protection|tst_simulation" --output-on-failure`
  Expected: compile errors (`TripCause`, `setFault`, `SupplyBreakerId` unknown).

- [ ] **Step 4: Implement** per Global Constraints. Keep complex contact currents for the residual sum. Compute danger after the last round, and compare it and the trips with the previous solve to emit events.

- [ ] **Step 5: Run the core suite**
  Run: `cmake --build build && ctest --test-dir build -L core --output-on-failure`
  Expected: PASS.
  `ctest --test-dir build -R tst_simulating` now fails only `shortShowsWholeAmperes`, which Task 3 rewrites. Any other failure: stop and report.

- [ ] **Step 6: Commit** (`F09: faults and instant trips`).

### Task 3: Events in the status bar

**Files:**
- Create: `src/app/eventtext.h`, `src/app/eventtext.cpp`, `tests/gui/tst_protecting.cpp`
- Modify: `src/app/CMakeLists.txt`, `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `tests/gui/tst_simulating.cpp` (rewrite `shortShowsWholeAmperes` only), `tests/CMakeLists.txt` (`add_gui_test(tst_protecting gui/tst_protecting.cpp)`), `translations/*.ts`

**Interfaces:**
- Consumes: `SimulationEvent`, `occurred` (Task 2); `currentText`, `sectionText`, `ratingText`, `loadCaption`; catalog names through `QCoreApplication::translate("Catalog", …)`, as `sectionChoiceText` does (`catalogText` is file-local elsewhere).
- Produces:

```cpp
// eventtext.h
// The status bar text of an event that starts a condition, in the current language (Strings
// table): "QF4 tripped: overload, 22.0 A on C16"; "" for Cleared.
[[nodiscard]] QString eventText(const Project &project, const SimulationEvent &event);
```

- `MainWindow` keeps the active conditions and the idle text as Global Constraints describe. It replaces both "Ready" sources, the `messageChanged` hook and `retranslateUi()`, with the idle text. The StatusTip filter stays.
- `eventText` covers every kind now, including wire and RCD Overloaded and Overheating, which the core emits from Task 4.
- Strings: all EventText rows of the Strings table.

- [ ] **Step 1: Rewrite `shortShowsWholeAmperes` (approved).** Same wire and clicks. Assert instead:
  - `simulation().tripCause(u"d3") == TripCause::ShortCircuit`;
  - `window->statusBar()->currentMessage() == "QF2 tripped: short circuit, 557 A on C10"`;
  - keep the screenshot `f08-labels-short`.

- [ ] **Step 2: Write the failing tests** in `tst_protecting.cpp`, with faults set through `simulation().setFault` (the menu comes in Task 6):
  - `eventTextForEveryKind_data/…`: synthetic `SimulationEvent`s on the tutorial fixture, English. Each row gives the event and the expected text:
    - d5 Overload 22.005 A, limit 16 → "QF4 tripped: overload, 22.0 A on C16";
    - d2 Leakage 0.2299 A, limit 0.03 → "QD1 tripped: leakage 230 mA, trips at 30 mA";
    - d2 Leakage 370.1 A → "QD1 tripped: leakage 370 A, trips at 30 mA";
    - d2 Test → "QD1 tripped: test button";
    - `SupplyBreakerId` ShortCircuit 570.6 A, limit 50 → "Stairwell breaker tripped: short circuit, 571 A on C50";
    - d2 Overloaded 47.3 A, limit 40 → "QD1 overloaded: 47.3 A on 40 A, an RCD doesn't protect against overload";
    - wire `d3:2 → l1:L` Overloaded 20.5 A, limit 19 → "Wire QF2 → Lamp 1 overloaded: 20.5 A on 1.5 mm², allowed 19 A";
    - wire `d2:N2 → nbar:1` Overheating 75.0 A, limit 70 → "Wire QD1 → N bar overheating: 75.0 A on 10 mm², allowed 70 A";
    - `l9` Dangerous with 0.2299 A → "Kettle: 230 mA through a person, no RCD turned it off";
    - `l1` Dangerous with 229.6 V → "Lamp 1: its case is live, 230 V";
    - Cleared → "".
  - `statusBarShowsTripsAndFallsBack`: click the handles of d1, d2, d4, then a short on `l3` → "QF3 tripped: short circuit, 370 A on C16". Fault None, click d4's handle → "Ready".
  - `statusBarFollowsLanguage`: during that trip, switch to Русский → "QF3: срабатывание от короткого замыкания, 370 А на C16", then Қазақша. The trip and every switch are unchanged. Screenshots `f09-status-ru`, `f09-status-kk`.
  - `dangerMessageWhileItLasts`: remove the wire `pebar:4 → l5:PE` through the model. Click the handles of d1, d2, d5, then an earth fault on `l9` → no trip, "Kettle: its case is live, 230 V". Click d5's handle → "Ready".
  - `newerConditionShowsThenFallsBack`: QF3 tripped as above, then leakage on `l9` with QF4 on → "QD1 tripped: leakage 230 mA, trips at 30 mA". Reset QD1 by clicking its handle → the QF3 message again.
  - `messageOfADeletedObjectGoes`: QF3 tripped; select d4 and Delete → "Ready". Undo → the QF3 message, and d4 still tripped.
  - `hintThenBackToTheEvent`: during a trip, a refused drop shows its hint. After its timeout (`QTRY_COMPARE_WITH_TIMEOUT`, 7000 ms), the trip message is back.

- [ ] **Step 3: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R "tst_protecting|tst_simulating" --output-on-failure`
  Expected: `tst_protecting` doesn't compile until `eventtext.h` exists; `shortShowsWholeAmperes` fails on the message.

- [ ] **Step 4: Implement.** Then run `cmake --build build --target update_translations` and enter the EventText rows.

- [ ] **Step 5: Run the tests and the whole suite**
  Run: `cmake --build build && ctest --test-dir build --output-on-failure`
  Expected: PASS, including the unchanged "Ready" tests of `tst_mainwindow`, `tst_wiring`, `tst_placing` and `tst_preferences`. Look at `f09-status-*.png`.

- [ ] **Step 6: Commit** (`F09: simulation events in the status bar`).

### Task 4: Heat, time and thermal trips (core)

**Files:**
- Modify: `src/core/simulation.h`, `src/core/simulation.cpp`, `tests/core/tst_protection.cpp`

**Interfaces:**
- Consumes: Task 2; `Catalog::ampacity`, `wireTimeConstant` (Task 1).
- Produces:

```cpp
inline constexpr double OverheatingFactor = 1.45; // × Iz (ADR 0008)
enum class WireHeat : std::uint8_t { Normal, Overloaded, Overheating };
// Simulation gains:
// advances simulated time by seconds (> 0): heat and cooling, each thermal trip at its exact moment
void step(double seconds);
[[nodiscard]] WireHeat wireHeat(QStringView wireId) const;
[[nodiscard]] double heatLevel(QStringView deviceId) const; // 0..1, Global Constraints
[[nodiscard]] bool isOverloaded(QStringView deviceId) const; // an RCD above In²
signals:
void heated(); // after a step that tripped nothing; a step that trips emits solved() instead
```

- How `step` works:
  - Loop while time is left: find the earliest thermal trip moment `t* ≤ remaining` among breakers, RCBOs and the stairwell breaker (Global Constraints formula).
  - Advance every heat state by `t*`, or by everything left when none is due.
  - Trip what is due, with the cause rule and `time = elapsed()`, then solve (instant trips included).
  - After each advance, emit the state events of wires and RCDs.
  - Currents don't change between solves, so `step` never solves without a trip.

- [ ] **Step 1: Write the failing tests** (new functions in `tst_protection.cpp`):
  - `thermalTripTimeFollowsTau`: C10 with a cooktop of 4600 W (I by the series formula, ≈ 18.7 A) → one `step(1000)`. The Tripped event's `time` = `73·ln(I²/(I² − 169))` within 1e-6; cause Overload; `elapsed()` 1000.
  - `sameTripTimeAtAnyStep_data/…`: rows 0.05, 0.5, 3, 1000 (steps repeated up to 1000 s) → the same `time` within 1e-6.
  - `noStepNoThermalTrip`: the same overload with no `step` → on, `heatLevel` 0.
  - `hotBreakerTripsSooner`:
    - trip;
    - switch the cooktop off and `step(30)`;
    - switch it on and reset;
    - the next trip comes after `73·ln((I² − H0)/(I² − 169))` with `H0 = 169·e^(−30/73)`, shorter than the first.
  - `heatCoolsWithoutCurrent`: after the trip, `step(73)` → H is `169·e^(−1)`, so `heatLevel` is 0.
  - `weakShortTripsOnHeatAsShortCircuit`: C32, 15 m of 1.5 mm², short on a cooktop:
    - no trip on `setFault`;
    - `step(1.0)` → still on;
    - `step(0.5)` → ShortCircuit, at `73·ln(I²/(I² − 41.6²))` (≈ 1.404 s).
  - `rcdOverloadNeverTrips`: an RCD 25 A with a 7 kW cooktop on 1 m wires (≈ 28.8 A) → `step(3600)`:
    - not tripped, `isOverloaded`, `heatLevel` 1;
    - one Overloaded event with limit 25.
    Cooktop off, `step(3600)` → Cleared.
  - `oversizedBreakerLetsThinWireOverheat`: C32 with a 9 kW cooktop on 15 m of 1.5 mm². Steps to 42, 44, 119 and 121 s give Normal, Overloaded, Overloaded and Overheating (times by formula). Each change emits one event: Overloaded with limit 19, then Overheating. After `step(3600)` the C32 is still on.
  - `correctPairingsNeverWarnBeforeTheirBreakerTrips_data/…`: rows (C10, 1.5, 3500 W), (C16, 2.5, 5500 W), (C32, 6, 11000 W), (C40, 10, 15000 W), each a cooktop on 1 m wires. In steps of 0.5 s every wire is Normal until the breaker trips with Overload, within 600 s.
  - `stairwellBreakerTripsOnOverload`: two 15 kW cooktops chained straight from the supply (no breaker) → Overload of the stairwell breaker at `73·ln(I²/(I² − 65²))`.
  - `heatSurvivesRemoveAndRestore`: `heatLevel` and `wireHeat` are kept by id.
  - `heatedOncePerQuietStep`: one `heated()` per step without a trip, and no `solved()`.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R tst_protection --output-on-failure`
  Expected: compile errors (`step`, `WireHeat` unknown).

- [ ] **Step 3: Implement** per Global Constraints and the interface notes.

- [ ] **Step 4: Run the core suite.** `ctest --test-dir build -L core --output-on-failure`. Expected: PASS.

- [ ] **Step 5: Commit** (`F09: heat, simulated time and thermal trips`).

### Task 5: Trips on the canvas, resets, test button, stairwell breaker

**Files:**
- Modify: `src/app/theme.h`, `src/app/theme.cpp`, `src/app/sceneitems.h`, `src/app/sceneitems.cpp`, `src/app/editorscene.h`, `src/app/editorscene.cpp`, `tests/gui/tst_protecting.cpp`, `translations/*.ts`

**Interfaces:**
- Consumes: `tripCause`, `resetSupplyBreaker`, `pressTest` (Task 2).
- Produces:

```cpp
// theme.h
QColor trippedColor();    // also overheating and danger
QColor overloadedColor();
QColor faultMarkColor();

// DeviceItem
// red outline and "!" badge; a trip (on → tripped) animates the handle down over 150 ms
void setTripped(bool tripped);
[[nodiscard]] bool isTripped() const;
[[nodiscard]] QRectF tripMarkRect() const;   // item coordinates; empty when not tripped
[[nodiscard]] QRectF testButtonRect() const; // the picture's "test-button"; empty without one

// A red outline and "!" badge over the supply picture while the stairwell breaker is tripped,
// drawn with DeviceItem's badge code; id "supplyTripMark"; clicks go through to the supply.
class TripMarkItem : public QGraphicsItem { /* boundingRect, paint */ };
```

- `handleRect()` follows the animation, so tests wait for the off position with `QTRY_COMPARE`.
- `syncLive()` sets each device's `setTripped` and shows or hides the supply mark.
- A left press inside `testButtonRect()` is a test press (like a switch press). A release that didn't move calls `pressTest`. When that returns false, the scene emits the test-button hint (Strings table).
- A left press on the supply picture while the stairwell breaker is tripped calls `resetSupplyBreaker()`. With the wire tool on, presses stay the tool's.

- [ ] **Step 1: Write the failing tests** in `tst_protecting.cpp` (fitted view):
  - `tripShowsOnTheDevice`: d1, d2, d4 on, a short on `l3` → d4's item `isTripped` and `!isOn`. `QTRY_COMPARE(handleRect(), off position)`, and `tripMarkRect()` is non-empty. Screenshot `f09-tripped`.
  - `clickOnTrippedHandleResets`:
    - with the fault still there, a click on d4's handle → tripped again, one new Tripped event;
    - fault None, click → on, not tripped, no mark.
  - `handSwitchOffShowsNoMark`: click d1 off → not tripped.
  - `tripMarkKeepsTerminalsAndHandleClear`: for d1 (2P), d2 (RCD) and d3 (1P), tripped, `tripMarkRect()` doesn't intersect `handleRect()` or a 3.6-radius square around any terminal.
  - `testButtonTripsRcd`: d1, d2 on, click the centre of d2's `testButtonRect()` → cause Test, d2 tripped, message "QD1 tripped: test button". Click d2's handle → reset.
  - `testButtonWithoutPowerHints`: d1 off, d2 on, test click → not tripped, message = the hint.
  - `dragFromTestButtonMoves`: drag from d2's test button to module 8 → moved, not tripped.
  - `wireToolTestButtonDoesNothing`.
  - `stairwellTripShowsOnSupplyAndClickResets`:
    1. Add a wire `supply:L → supply:N` (0.3 m, 2.5 mm²) → `supplyTripMark` visible, "Stairwell breaker tripped: short circuit, 571 A on C50".
    2. Click the supply → still tripped.
    3. Select the wire and Delete, then click the supply → reset, mark hidden, "Ready".
    Screenshot `f09-stairwell`.
  - `supplyClickWhenNotTrippedDoesNothing`.
  - In `tst_catalogart.cpp`, the new function `trippedLookOnBothThemes`: a sheet `f09-tripped-sheet` of every variant tripped, dark and light.

- [ ] **Step 2: Run them and see them fail**
  Run: `cmake --build build && ctest --test-dir build -R "tst_protecting|tst_catalogart" --output-on-failure`
  Expected: compile errors (`setTripped`, `testButtonRect`, `trippedColor`).

- [ ] **Step 3: Implement.** Then `update_translations` and enter the hint row.

- [ ] **Step 4: Run the tests, then look at `f09-tripped.png`, `f09-stairwell.png` and `f09-tripped-sheet.png`.**
  Run: `cmake --build build && ctest --test-dir build -R "tst_protecting|tst_catalogart|translations" --output-on-failure`
  Expected: PASS. The badge reads at the fitted zoom and covers no terminal or handle.

- [ ] **Step 5: Commit** (`F09: trips on the canvas, resets, test button`).

### Task 6: Fault menu and fault marks

**Files:**
- Modify: `src/app/sceneitems.h`, `src/app/sceneitems.cpp`, `src/app/editorscene.h`, `src/app/editorscene.cpp`, `tests/gui/tst_protecting.cpp`, `translations/*.ts`

**Interfaces:**
- Consumes: `setFault`, `fault`, `isDangerous` (Task 2), the colours (Task 5).
- Produces:

```cpp
// LoadItem
// a ⚡ mark bottom-left in the picture, faultMarkColor(), trippedColor() when dangerous
void setFault(bool faulted, bool dangerous);
[[nodiscard]] QRectF faultMarkRect() const; // item coordinates; empty without a fault
[[nodiscard]] bool isDangerous() const;
```

- The scene owns the four fault actions. Short circuit, Leakage and Earth fault are checkable in one exclusive group, with the load's current fault checked. Remove fault is enabled only with a fault.
  - The right-click menu of a load is the fault actions, a separator, then the context actions (Delete). Devices and wires keep only the context actions. The menu keeps the objectName `deviceContextMenu`.
  - Triggering an action calls `setFault` on the right-clicked load. `retranslate()` sets the action texts.
- `syncLive()` sets each load's `setFault(fault != None, isDangerous)`.

- [ ] **Step 1: Write the failing tests** (zoomed out):
  - `loadMenuOffersFaults`: right-click `l3`:
    - the actions are, in order, Short circuit, Leakage, Earth fault, Remove fault, a separator, Delete;
    - none is checked, and Remove fault is disabled;
    - right-clicking a device shows only Delete.
  - `shortCircuitFromMenuTrips`:
    1. d1, d2, d4 on; choose Short circuit on `l3` → d4 tripped, `fault("l3")` is ShortCircuit, and `faultMarkRect()` is non-empty.
    2. Reopen the menu → Short circuit checked, Remove fault enabled.
    3. Choose Remove fault → no fault, no mark, d4 still tripped.
    4. Click its handle → on.
  - `leakageOnKettleTripsRcd`: d1, d2, d5 on, Leakage on `l9` → QD1 Leakage, "QD1 tripped: leakage 230 mA, trips at 30 mA". Screenshot `f09-leakage`.
  - `faultStaysWithUnpluggedAppliance`:
    1. QF4 off, Short circuit on `l9`.
    2. Drag the kettle to scene (500, 300) to unplug it, then click d1, d2, d5 on → nothing trips.
    3. Drag it back onto `l5` → QF4 trips.
  - `dangerMarkIsRed`: remove `pebar:4 → l5:PE`, d1, d2, d5 on, Earth fault on `l9` → `isDangerous()`, no trip. Screenshot `f09-danger`.
  - `faultActionsAreNotEdits`: the undo stack count is unchanged by fault actions, and undo or redo after them keeps the fault, the trips and the switches.
  - `faultSurvivesDeleteAndUndo`: delete `l3` with a fault, then undo → the fault and its mark are back.
  - `faultMarkKeepsTerminalsAndButtonClear`: for the lamp, the socket, the cooktop and the kettle, `faultMarkRect()` doesn't intersect `buttonRect()` or a 3.6-radius square around any terminal.

- [ ] **Step 2: Run them and see them fail.** `ctest --test-dir build -R tst_protecting --output-on-failure`. Expected: compile errors (`faultMarkRect`).

- [ ] **Step 3: Implement.** Then `update_translations` and enter the four action rows.

- [ ] **Step 4: Run the tests, then look at `f09-leakage.png` and `f09-danger.png`.** Expected: PASS. The mark reads on both themes and is distinct from the button.

- [ ] **Step 5: Commit** (`F09: fault menu on loads, fault and danger marks`).

### Task 7: Time control and heat on the canvas

**Files:**
- Create: `resources/svg/ui/pause.svg`
- Modify: `src/app/CMakeLists.txt` (resource list), `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `src/app/sceneitems.h`, `src/app/sceneitems.cpp`, `src/app/editorscene.h`, `src/app/editorscene.cpp`, `tests/gui/tst_protecting.cpp`, `tests/gui/tst_catalogart.cpp` (add the icon to the UI icon list at line 140), `translations/*.ts`

**Interfaces:**
- Consumes: `step`, `heated`, `heatLevel`, `wireHeat`, `isOverloaded` (Task 4); `eventText` (Task 3).
- Produces:

```cpp
// DeviceItem: an orange glow around the body, opacity ∝ level; 0 hides it
void setHeatLevel(double level);
[[nodiscard]] double heatLevel() const;
// WireItem: an orange (Overloaded) or red (Overheating) halo under the wire, conductor colour kept
void setHeat(WireHeat heat);
[[nodiscard]] WireHeat heat() const;
```

- `MainWindow`:
  - After the wire section combo, add a separator, the `timeLabel` QLabel ("Time") and the four checkable actions in an exclusive group. `pauseAction` uses `pause.svg` (original: two rounded bars), with the text "Pause".
  - `speed10Action` is checked at every start.
  - `simulationTimer` fires every 50 ms and calls `simulation().step(0.05 * speed)`. Pause stops it.
- `EditorScene`: `heated()` → a private `syncHeat()` that sets `setHeatLevel` and `setHeat` only; `syncLive()` calls it too. No label layout on `heated()`.

- [ ] **Step 1: Write the failing tests:**
  - `timeControlStartsAtTenTimes`: `speed10Action` checked, `simulationTimer` active with interval 50. A new window starts at ×10 again.
  - `pauseStopsHeatingNotInstantTrips`: Pause → timer inactive. Then:
    - plug a washing machine into `l6` and switch on QF1, QD1, QF4, the kettle, the microwave and the machine by clicks;
    - `QTest::qWait(500)` → `elapsed()` unchanged and QF4 on;
    - a short on `l3` with QF3 on → QF3 trips at once while paused.
  - `overloadTripsAtSixtyTimes`: the same overload, ×60 → `QTRY_VERIFY_WITH_TIMEOUT(tripped, 10000)`, cause Overload, "QF4 tripped: overload, 22.0 A on C16". On the way, d5's `heatLevel()` rose above 0. Screenshots `f09-heating` (before) and `f09-overload` (after).
  - `wireTurnsOrangeThenRed`: `d6` C32 with the 9 kW cooktop per the Expected values table, set up through the model; QF1, QD1, d6 and the cooktop on; ×60.
    - `QTRY` the L wire reaches Overloaded, then Overheating;
    - the message contains "overheating: 34.6 A on 1.5 mm², allowed 19 A";
    - d6 never trips.
    Screenshot `f09-wire-overheating`, and light theme `f09-wire-overheating-light`.
  - `overloadedRcdGlows`: the cooktop case plus the kettle and microwave on → `QTRY` QD1 `isOverloaded`, its item `heatLevel()` 1, and a message starting "QD1 overloaded:". QF1 doesn't trip (below 52 A).
  - `quietTimerDoesNoSolves`: the tutorial loads with everything on and nothing overloaded, ×60 → over 1 s of `QTest::qWait`, a `QSignalSpy` on `Simulation::solved` counts 0 while `heated` counts at least 10. The F08 label layout never runs from the timer.
  - `tripDuringDragKeepsTheDrag`:
    - press on d3's body and move part way;
    - call `simulation().step(200)` with the QF4 overload running, so QF4 trips;
    - finish the drag at module 8 → d3 is at module 8.
    - Then turn the wire tool on, click one terminal, trip again, and the wire in progress is unchanged.

- [ ] **Step 2: Run them and see them fail.** `ctest --test-dir build -R "tst_protecting|tst_catalogart" --output-on-failure`. Expected: compile errors (`speed10Action` missing, `setHeatLevel`, `setHeat`).

- [ ] **Step 3: Implement.** Then `update_translations` and enter the MainWindow rows (Time, Pause).

- [ ] **Step 4: Run the whole suite, then look at every `f09-*` screenshot from this task.**
  Run: `cmake --build build && ctest --test-dir build --output-on-failure`
  Expected: PASS. The glow and halos read on both themes, and the conductor colour stays recognisable under a halo.

- [ ] **Step 5: Commit** (`F09: time control, heating glow, overheating wires`).

### Task 8: Roadmap scenario, smoke test, done

**Files:**
- Modify: `tests/gui/tst_protecting.cpp`, `docs/roadmap.md`

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Write the scenario test** `roadmapScenario`. Use the tutorial fixture and real clicks, drags and menus, except the setup noted in step 5:
  1. Switch on QF1, QD1, QF4, the kettle and the microwave by clicks. Drop a washing machine from the palette onto Socket 4 (`dropLoad`) and click its button. Choose ×60 → QF4 trips with cause Overload, and its time is 163.48 ± 0.05 s. Screenshot `f09-scenario-1-overload`.
  2. Drag the washing machine away and click QF4's handle → on. Click QF3's handle, then Short circuit on Socket 1 → QF3 trips at once, "…370 A on C16". Screenshot `f09-scenario-2-short`.
  3. Remove fault, reset QF3. Leakage on the kettle → QD1 trips, Leakage. Screenshot `f09-scenario-3-leakage`.
  4. Remove fault, reset QD1 → the kettle runs again.
  5. The 9 kW cooktop case (set up through the model as in Task 7), switched on by clicks → the wire reaches Overheating, and the C32 is still on after 600 s of simulated time. Screenshot `f09-scenario-4-overheating`.
  6. Russian and Kazakh screenshots: `f09-scenario-ru`, `f09-scenario-kk`.

- [ ] **Step 2: Run it.** `ctest --test-dir build -R tst_protecting --output-on-failure`. Expected: PASS without product changes. If it fails, find the cause in the code (systematic debugging); never change the expected values.

- [ ] **Step 3: Look at the scenario screenshots** and fix visual defects in code.

- [ ] **Step 4: Full suite and quality gates.**
  Run: `scripts/quality_gates.sh` (in the background; wait for the notice).
  Expected: "All quality gates passed".

- [ ] **Step 5: Smoke test the real binary** under `xvfb-run -a -s "-screen 0 1600x900x24"`:
  1. Start `./build/electricity`.
  2. With `xdotool`, drop a 1P breaker, wire supply L → its top and its bottom to a dropped socket's L, and the N bar → the socket's N.
  3. Click the breaker handle, right-click the socket and choose Short circuit.
  4. `import -window root build/screenshots/f09-smoke.png`, then kill the app.
  5. Open the PNG: the breaker is tripped with its badge, the socket has its mark, and the status bar names the trip.

- [ ] **Step 6: Roadmap.** F09 ◐ → ☑. Remove F09's status-bar bullet, which is done by the idle text.

- [ ] **Step 7: Commit** (`F09: roadmap scenario, mark done`).
