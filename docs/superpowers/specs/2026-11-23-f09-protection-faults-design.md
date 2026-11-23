# F09 — Protection and faults

Date: 2026-11-23 · Status: draft for the owner's approval

Roadmap item: [F09](../../roadmap.md#f09-protection-and-faults).
ADRs: [0006](../../adr/0006-core-and-app-layering.md),
[0007](../../adr/0007-electrical-model.md),
[0008](../../adr/0008-simulation-engine.md),
[0010](../../adr/0010-project-file-format.md),
[0011](../../adr/0011-undo-redo.md),
[0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0014](../../adr/0014-progressive-disclosure.md),
[0016](../../adr/0016-electrical-reference-data.md),
[0017](../../adr/0017-tutorial-engine.md),
[0023](../../adr/0023-catalog-schema.md),
[0029](../../adr/0029-live-simulation-state-and-contacts.md).
Previous feature: [F08 design](2026-11-23-f08-live-simulation-design.md).

## Intent

F08 made the panel live: handles and loads switch, currents are solved as
wired. F09 makes it **protect itself the way real devices do**, which is the
"safety" half of the product (product design: "Safety = protective devices
open the circuit in simulation"). Breakers trip on overload after a
realistic, sped-up delay and instantly on a short circuit; RCDs trip on
leakage; wires warn when they overheat. The user causes faults by
right-clicking a load and resets tripped devices by clicking them. A
time-speed control lets overloads play out in seconds on screen.

This is what tutorial steps 10–12 (F11) teach: overload on QF4 with the
washing machine, short on a room socket, leakage on the kettle. Because the
simulation follows the drawn wires, wiring mistakes show their real
consequences: a neutral that bypasses the RCD trips it with no fault; an
oversized breaker lets a thin wire overheat; leakage where no RCD covers it
keeps flowing through a person.

The basic flow must stay as simple as F05–F08: nothing new to learn except
right-click → fault, click → reset, and the time control.

## Goals

### What the user sees

```
┌ Edit View Settings Language ────────────────────────────────────┐
│ [↶][↷] │ [Wire] [2.5 mm² ▾] │ Time [⏸][×1][×10][×60]             │
│        L  N  PE                                                 │
│   ┌────┃──┃──┃─────────────────────────┐   ┌───────────────┐    │
│   │  QF1   QD1   QF2  QF3 ┏━QF4━┓      │   │ Short circuit │    │
│   │ [▀▀▀] [▀▀▀] [▀▀] [▀▀] ┃ ▄▄ !┃      │   │ Leakage       │    │
│   │                       ┗━━━━━┛      │   │ Earth fault   │    │
│   └─────────────────────────┃──────────┘   │ Remove fault  │    │
│                             ┃ orange       │ Delete        │    │
│                        ┌────┸────┐         └───────────────┘    │
│                        │ ⊡    ⊡ ⚡│          (right-click a load) │
│                        └─────────┘                              │
└ QF4 tripped: overload, 22.1 A on C16 ───────────────────────────┘
   ┏━┓ red outline and "!" on a tripped device; ⚡ a load with a fault
```

### What trips, and when

- **Short circuit** (a load's L touching its N): a current of hundreds of
  amperes; the breaker feeding that line trips **instantly** (magnetic
  trip) when the current reaches 10·In (C curve). When several breakers see
  it, the smallest, closest one trips (QF3, not QF1), per ADR 0008's trip
  order. A **weak short** below that — e.g. at the end of 15 m of 1.5 mm² on
  a C32, ≈ 304 A against 320 A — trips the breaker on heat a moment later
  (≈ 1.4 s of simulated time), as a real one would, and is still **reported
  as a short circuit** (owner's choice B): every trip at a current at or
  above the curve's lower instant-trip bound (C: 5·In) is reported as a
  short circuit, below it as an overload. The tutorial's shorts are all
  strong enough for the instant trip.
- **Overload**: a breaker carrying more than its rating heats up and trips
  **after a delay** that gets shorter the bigger the overload (thermal trip,
  ADR 0008: τ = 73 s, trip at 1.3·In, about 40 s at 2·In). The washing
  machine added to QF4 in the tutorial panel draws ≈ 22.1 A (solved value)
  and trips QF4 after ≈ 2.7 min of simulated time, ≈ 16 s on screen at ×10.
  A breaker stays warm after a trip and cools down slowly, so switching it
  back on under the same overload trips it sooner.
- **Leakage** (current through a person to earth, 1 kΩ, ≈ 0.23 A at 230 V):
  the RCD or RCBO above it trips instantly when the residual current reaches
  its 30 mA. Breakers don't notice leakage.
- **Earth fault** (a load's L touching its earthed case): a current of
  hundreds of amperes through PE; the breaker trips instantly, and the RCD
  above it too.
- **Wiring mistakes behave as in real life** without special cases: a
  neutral that bypasses the RCD makes it trip with no fault at all; a load
  wired before its breaker is not protected by it.
- **Wires never trip; they warn.** A wire turns **orange** when overloaded
  (its heat above what its ampacity allows) and **red** when overheating
  (ADR 0008: heat at (1.45·Iz)²), each with a status bar message.
- **An RCD carrying more than its rating** warms up and shows it (orange
  glow and a status bar message) but never trips: it has no overload
  protection, a classic mistake.
- **The stairwell breaker** (ADR 0008's upstream C50 on the supply L) trips
  like any breaker when nothing in the panel stops a fault. It is **hidden
  until it trips** (owner's choice A): then the supply block turns red with a
  mark, the whole apartment goes dark, the status bar says why, and **a click
  on the supply block resets it**.
- **A fault stays until the user removes it** from the load's menu.
  Resetting a device while its fault is still there trips it again at once.
- **Pause stops only heating and cooling.** Instant trips — a short or
  earth fault at or above 10·In, leakage at or above IΔn — happen even while
  paused, because they don't depend on time. A weak short waits for time to
  run.

### Danger that nothing stops (owner's choice B)

When a fault leaves a person in danger and no device stops it, the faulted
load's ⚡ mark turns **red** and the status bar says so, for as long as the
danger lasts:

- **Leakage nothing turns off** — e.g. the lighting line wired before the
  RCD and a lamp leaking: "Lamp 1: 0.23 A through a person, no RCD turned it
  off".
- **A live case** — e.g. an earth fault on a kettle in a socket whose PE is
  not wired: nothing flows, nothing trips, and the case sits at 230 V:
  "Kettle 1: its case is live, 230 V".

The rule (builder's call): a leakage or earth fault is dangerous while the
touch voltage stays above **50 V** (ПУЭ 1.7.53 requires protection against
indirect contact above 50 V AC): for leakage, the voltage of the L the person touches against
earth; for an earth fault, the voltage of the load's case (its PE terminal,
or its socket's) against earth. A properly protected fault ends in a trip,
which brings the voltage down, so the danger lasts at most a moment.

No popups (owner rejected option C).

### Doing it

- **Faults.** Right-click a lamp, socket, cooktop or plug-in appliance:
  *Short circuit*, *Leakage*, *Earth fault*, *Remove fault*, then *Delete*.
  A load has at most one fault; the current one is shown checked; *Remove
  fault* is enabled only when there is one. A faulted load shows a small ⚡
  mark drawn by the app. Devices and wires keep their menu (*Delete*).
- **Where a fault acts.** A lamp, the cooktop or a socket: between its own
  L, N and PE terminals. A plug-in appliance: between its socket's L, N and
  PE, and only while plugged in. The fault stays on the appliance when it is
  unplugged, so plugging in a shorted kettle trips its line.
- **Earth fault for everyone.** ADR 0014 lists earth fault among engineer
  entries, the roadmap puts it in F09's menu; it is shown to everyone now
  (F12 can move it behind engineering details).
- **A tripped device.** Its handle drops to off with a short animation; a
  red outline and a "!" mark stay until it is reset. A click on the handle
  resets it: the handle goes up and the device is on again (and trips again
  at once if the cause is still there). Switching a device off by hand shows
  no red mark.
- **A breaker heating up** glows orange, stronger as it nears its trip, so
  an overload trip can be seen coming. An overloaded RCD gets the same glow.
- **The RCD and RCBO test button.** A click on the picture's test button
  trips the device with cause "test" if it is on and has power (more than
  half of 230 V between its L and N terminals, top or bottom); without power
  nothing happens and a hint says the test button needs power. As with the
  handle: a drag that starts on it moves the device; with the wire tool on
  it does nothing.
- **The time control** in the toolbar: *Pause*, *×1*, *×10*, *×60*, one of
  them checked. Every launch starts at ×10 (builder's call). The app advances
  simulated time from a 50 ms timer by 50 ms × speed (ADR 0008).
- **The status bar** shows "Ready" or the latest event: trips, wire
  overloaded/overheating, RCD overloaded, danger. A trip message stays until
  the device is reset or a newer message comes; a condition (danger,
  overheating, overload) stays as long as it lasts. When the shown message's
  condition ends, the bar falls back to the newest condition still going on,
  else "Ready" (builder's call). The event takes the place of "Ready" in the
  existing status message rather than a separate fixed label, so a tool hint
  covers it for its 5 s and then the event returns; this keeps the existing
  tests that read "Ready" unchanged (owner agreed, 2026-11-23, replacing the
  roadmap's fixed-label note). F03's empty-StatusTip filter stays.
- **Message examples** (wording left to the builder, all three languages):
  "QF4 tripped: overload, 22.1 A on C16"; "QF3 tripped: short circuit, 371 A
  on C16"; "QD1 tripped: leakage, 230 mA, trips at 30 mA"; "QD1 tripped: test
  button"; "Stairwell breaker tripped: short circuit, 572 A on C50"; "Wire
  QF6 → Cooktop 1 overheating: 34.7 A on 1.5 mm², allowed 19 A"; "QD1
  overloaded: 45 A on 40 A, an RCD doesn't protect against overload".

### What is not undoable and not saved

- Faults, trips, resets, heat and the time speed are experiments (ADR 0011):
  not on the undo stack, never changed by undo or redo, and not saved
  (ADR 0010).
- Like F08's switch settings, they belong to the object's id for the whole
  session: deleting a faulted or tripped object and undoing brings it back
  as it was. Editing wiring, ratings or positions never resets a trip or
  removes a fault.

## Simulation (core)

All of this lives in `panelcore` (ADR 0006), in the same simulation object
that holds F08's switch settings (approach A below).

- **Catalog data** (ADR 0023, schema stays version 1):
  - breaker and RCBO: thermal τ (73 s) and trip factor (1.3); per curve the
    instant-trip band, lower and upper multiple of In (B 3–5, C 5–10,
    D 10–20; only C is offered until F16): the upper one trips (ADR 0008),
    the lower one decides whether a trip is reported as a short circuit;
  - RCD: a thermal τ (73 s) for its overload warm-up, no trip;
  - RCD and RCBO: IΔn from their existing `residualCurrent` parameter (mA);
  - the supply: its upstream breaker (C50, on its L terminal);
  - wires: ampacity per section from ADR 0016 (1.5 → 19 A … 16 → 85 A) and a
    thermal τ of 120 s (builder's call, a teaching default, not a measured
    value). It must not be shorter than the breakers' 73 s: then a correctly
    paired line (1.3·In ≤ Iz, true for every pairing in ADR 0016) can never
    leave "normal" before its breaker trips.
  - The loader validates the new fields: every section in `WireSections`
    has an ampacity, τ and factors are positive.
- **State per id, kept after removal:** tripped (with its cause), heat `H`
  (A²) for each breaker, RCD, RCBO, wire and the stairwell breaker, the
  fault of each load. Everything starts reset, cold and without faults. A
  tripped device counts as off; switching it on resets it.
- **New branches** (refines ADR 0008):
  - short circuit: 1 mΩ between the L and N the load draws from;
  - leakage: 1 kΩ from that L **to earth** (the reference node), not to the
    load's PE terminal — a person touching a live part conducts to earth
    whether or not the PE wire is there (builder's call);
  - earth fault: 1 mΩ between that L and the load's PE (its own, or its
    socket's) — with no PE wired, no fault current flows;
  - stairwell breaker: the supply's Norton source on L conducts only while
    it is not tripped.
- **Currents as phasors.** Contact currents are kept complex, top to
  bottom, so an RCD's residual current is the magnitude of the sum of its
  pole currents (ADR 0008).
- **Instant trips after every solve** (ADR 0008's order): collect breakers
  and RCBOs whose largest pole current is ≥ m·In (the stairwell breaker
  with the source current), and RCDs and RCBOs whose residual is ≥ IΔn;
  trip every triggered RCD/RCBO-residual and the triggered breaker with the
  smallest In (ties: largest current); solve again; at most 32 rounds.
- **Time.** `step(seconds)` of simulated time; the core never reads a clock.
  Heat follows `dH/dt = (I² − H)/τ`, updated exactly
  (`H = I² + (H − I²)·e^(−t/τ)`), with I the largest pole current (a wire's
  current). Between trips the currents don't change (loads are fixed
  resistances), so a step **jumps to the moment of the earliest thermal
  trip** inside it, trips, re-solves (with instant trips), and continues
  with the rest of the interval. The same overload therefore trips at the
  same simulated second at every speed.
- **Results added:** per device tripped and cause; per breaker its heat as a
  fraction of the way from In² to its trip point (for the glow); per RCD
  overloaded (`H > In²`); per wire normal / overloaded (`H > Iz²`) /
  overheating (`H ≥ (1.45·Iz)²`); stairwell breaker tripped; per faulted
  load its touch voltage and dangerous (> 50 V).
- **Events**, each once per change: device or stairwell breaker tripped
  (cause overload / short circuit / leakage / test, the measured current,
  the rating; a breaker's cause is short circuit when the current is at or
  above its curve's lower instant-trip multiple, whether the trip was
  instant or thermal, else overload), wire overloaded and overheating, RCD overloaded, danger
  started, and the end of each condition. The tutorial (F11, ADR 0017)
  waits on them; F12's event log lists them.
- **Notification.** The app is told after every solve (as in F08) and after
  every step that changed heat, at most once per step, so a 50 ms timer
  doesn't redo F08's full label layout when only glows changed.

## Non-goals

- Saving faults, trips or heat (ADR 0010); the time speed is not saved
  either.
- Editable τ, trip factor, curve, fault resistance, supply impedance and the
  stairwell breaker's rating, the event log dock, the currents table (F12).
- A visible stairwell breaker (owner chose A); a supply power button.
- B and D curves, RCD types and 100 mA (F16); three-phase faults and PEN
  break (F17).
- A simulated clock display, waveforms, trip curves, selectivity studies.
- Static design-rule warnings (product design): only what the simulation
  shows.

## Anti-goals

- A correct design (ADR 0016 pairings) that shows warnings or nuisance trips
  in normal use.
- A trip without a visible reason: every trip leaves a red mark on the
  device and a status message with its cause until it is reset.
- A device or switch changing by itself: undo, redo or an edit never trips,
  resets or removes a fault.
- Danger hidden behind "nothing happened": leakage nobody stops and live
  cases are marked.
- Trip timing that depends on the speed setting or the timer.
- The 50 ms timer making the editor sluggish.

## Constraints

- Core logic GUI-free and deterministic (ADR 0006, 0008); app items stay
  thin views re-synced from the simulation.
- Fault actions, time actions, the status label and test-button and supply
  clicks are findable by tests (objectNames, item ids, known positions in
  item coordinates).
- Every new text in Russian, Kazakh and English (ADR 0012).
- Original art only; marks, glows and outlines are drawn by the app; colours
  from the theme (ADR 0013: overloaded orange, overheating/tripped red),
  readable on dark and light.

## Approaches

**Where protection lives** (builder's call, as the owner leaves code design
to the builder):
- *A. Extend F08's simulation object* — **chosen**: trips, heat, faults and
  events sit beside the switch settings, outside undo and files (ADR 0029);
  a trip is a state of the device, not a hand switch-off.
- *B. A separate protection layer* switching devices off when they trip —
  rejected: a trip would look like a manual off, and every trip needs a new
  solve, so the two objects would keep calling each other.

**How time advances** (builder's call):
- *Jump to the next trip moment* — **chosen**: exact, the same trip time at
  every speed, checkable against hand-worked numbers.
- *Fixed small sub-steps* — simpler maths, but trip times shift with speed
  and ×60 needs 60 sub-steps per tick.

**The stairwell breaker** (owner chose A, 2026-11-23):
- *A. Hidden until it trips; the supply block turns red; click it to reset*
  — **chosen**: the basic picture stays as it is.
- *B. Always visible with a handle on the supply block* — consistent, but
  brings back the supply switch F08 left out, and new art.

**Danger that nothing stops** (owner chose B, 2026-11-23):
- *A. Silent* — realistic, but a beginner misses a deadly design.
- *B. Red mark on the load plus a status bar message while it lasts* —
  **chosen**.
- *C. A blocking popup* — interrupts experimenting; reads as rule-checking.

## Decided by the owner

- Stairwell breaker hidden until it trips, reset by a click on the supply
  block (A), 2026-11-23.
- Danger nothing stops: red mark plus status bar message (B), 2026-11-23.
- Played back on 2026-11-23 and confirmed ("all ok - go ahead"): what trips
  and when, faults persisting until removed, reset by clicking the handle,
  the RCD test button in F09, earth fault shown to everyone, tripped look
  (handle down, red outline, "!"), orange glow on heating breakers and
  overloaded RCDs, orange/red wires, the ⚡ fault mark, faults acting through
  a plug-in's socket only while plugged in, the time control in the toolbar,
  the fixed status label, not undoable, not saved, kept by id.
- The roadmap's 7 kW cooktop case raised to 9 kW in the scenario
  (proposed in the first message, not objected to).
- A weak short (below 10·In) trips on heat with realistic timing and is
  reported as a short circuit when its current is at least the curve's lower
  instant-trip multiple (B), 2026-11-23.
- The two F08 tests whose short no longer flows once breakers trip
  (`TestSimulation::shortIsLimitedBySupplyAndWires`,
  `TestSimulating::shortShowsWholeAmperes`) are rewritten to check the same
  short-circuit current through the trip: the breaker trips with cause short
  circuit at ≈ 548 A (core) and the status message shows "557 A on C10"
  where the label was (GUI); approved 2026-11-23 ("yes, rewrite them"). Each
  rewrite gets its own red/green step.
- Code design left to the builder (CLAUDE.md).

## Builder's calls

- Pause stops only heating and cooling; instant trips still happen.
- Leakage goes from L to earth, not to the load's PE.
- Danger rule: touch voltage above 50 V.
- Wire τ 120 s for every section, never below the breaker τ.
- Time speed starts at ×10 on every launch.
- Status label fallback rule as described.
- Faults, trips and heat kept by id after removal.
- New ADR 0030: protection state in the simulation object, exact-jump time
  steps, leakage to earth (refines 0007/0008), the 50 V danger rule, the
  hidden stairwell breaker, wire τ rule, earth fault in the basic menu
  (refines 0014), the status label.
- ADR 0030 also records that F08's "a bare L–N wire shows a current of
  hundreds of amperes" (F08 design, ADR 0029 context) no longer holds: the
  breaker or the stairwell breaker trips.
- Roadmap: F09 scenario uses a 9 kW cooktop; follow-ups for F11 (the allow
  filter also covers fault actions, the test button and the supply click),
  F12 (earth fault behind engineering details if wanted; wire τ editable).

## Left to the builder

- The look of the red outline, "!" and ⚡ marks, the orange glow and the
  wire colouring (e.g. a halo that keeps the conductor colour visible), the
  handle animation, the supply block's tripped look; all must stay readable
  at the default fitted zoom on both themes and not hide terminals.
- Time control widgets and icons (original), shortcuts if any.
- How a wire is named in messages (e.g. by its two ends).
- Message wording in all three languages (Kazakh to be reviewed by a native
  speaker when possible).
- Simulation classes, signals, event types, file layout.

## Tests

- **Catalog:** protection fields load for every breaker, RCD and RCBO; every
  `WireSections` value has an ampacity; the supply has its upstream breaker;
  a missing or non-positive value is an error.
- **Core** (expected values worked out by hand from the model, never read
  back from the code):
  - magnetic: just below m·In no trip, at m·In instant trip; a short trips
    the smallest breaker on its path first (QF3, not QF1); a short with no
    breaker in the panel trips the stairwell breaker;
  - weak short (C32, 15 m of 1.5 mm², ≈ 304 A): no instant trip, a thermal
    trip after ≈ 1.4 s reported as short circuit; while paused (no step) it
    doesn't trip; a thermal trip below 5·In is reported as overload;
  - the two rewritten F08 tests (see Decided by the owner);
  - thermal: trip time at 2·In ≈ 40.1 s; the same trip time with one big
    step and many small ones; a hot breaker re-trips sooner; heat cools with
    no current; no step, no thermal trip;
  - RCD: leakage ≥ IΔn trips, below doesn't; overload never trips it but
    marks it overloaded; RCBO trips on both; a neutral bypassing the RCD
    trips it; earth fault trips breaker and RCD;
  - faults: one per load; removal; a reset with the fault still there trips
    again; a fault on an unplugged appliance does nothing, plugging it in
    trips; leakage trips the RCD with no PE wired; earth fault with no PE:
    no trip, case live, dangerous; leakage with no RCD: dangerous; danger
    ends with the trip when protection works;
  - a 2P trip opens both poles; a tripped device counts as off and
    switching it on resets it;
  - wires: the oversized-breaker case (9 kW cooktop, C32, 15 m of 1.5 mm²,
    ≈ 34.7 A) reaches overheating and C32 never trips; correct pairings never
    leave "normal" before their breaker trips;
  - test button trips only with power;
  - events once per change; trips, heat and faults survive
    remove-and-restore and are untouched by project edits; the trip loop is
    bounded.
- **GUI scenario (offscreen, roadmap):** the tutorial fixture, everything
  switched on by clicks; plug a washing machine into Socket 4 and advance
  time → QF4 trips with cause overload at the hand-worked time, red mark,
  status message; unplug, reset by clicking the handle; Short circuit on
  Socket 1 by right-click → QF3 trips instantly; Remove fault, reset; Leakage
  on the kettle → QD1 trips; remove, reset; the RCD test button; a 9 kW
  cooktop on a C32 with 1.5 mm² → wire red, C32 never trips; a bare L–N wire
  at the supply → stairwell breaker trips, supply block red, a click resets
  it; danger marks (a lamp leaking on a line outside the RCD; an earth fault
  in a socket with no PE); the time control (pause stops an overload from
  tripping); the status label; light theme; Russian and Kazakh.
  Screenshots at each step.
- **Translations:** `translations_complete` covers the new strings.
- **Smoke test** of the real binary under Xvfb: right-click a fault, see the
  trip, reset it; inspect the screenshot.
