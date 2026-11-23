# F08 — Live simulation: supply, switching, currents

Date: 2026-11-23 · Status: draft for the owner's approval

Roadmap item: [F08](../../roadmap.md#f08-live-simulation-supply-switching-currents).
ADRs: [0005](../../adr/0005-ui-qt-widgets-graphics-view.md),
[0006](../../adr/0006-core-and-app-layering.md),
[0007](../../adr/0007-electrical-model.md),
[0008](../../adr/0008-simulation-engine.md),
[0009](../../adr/0009-component-catalog.md),
[0010](../../adr/0010-project-file-format.md),
[0011](../../adr/0011-undo-redo.md),
[0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0014](../../adr/0014-progressive-disclosure.md),
[0016](../../adr/0016-electrical-reference-data.md),
[0017](../../adr/0017-tutorial-engine.md),
[0023](../../adr/0023-catalog-schema.md),
[0024](../../adr/0024-placed-devices-and-designations.md),
[0026](../../adr/0026-redo-shortcuts-and-undo-texts.md),
[0027](../../adr/0027-wires-roles-colours-routes.md),
[0028](../../adr/0028-loads-plugging-cables.md).

## Intent

Until now the panel is a drawing. F08 makes it live: the circuit is solved
exactly as the user wired it, the user flips breaker and RCD handles and
switches lamps and appliances on and off, and sees the current in each device
and wire. Lamps glow when they get power. This is the "watch it behave" half
of the product (product design: "Live simulation, always running"), and the
base for F09 (trips and overheating), F11 (tutorial step 9, "Power on: flip
QF1, QD1, each breaker; switch on lamps; watch currents") and F12 (currents
table, engineering parameters).

Because the simulation follows the drawn wires, wiring mistakes behave as in
real life from the start: a lamp fed from before its breaker stays lit when
the breaker is off; a neutral that bypasses the RCD carries its current
outside it.

The basic flow must stay as simple as F05–F07: no engineering knowledge is
needed to switch things and read currents.

## Goals

### What the user sees

```
┌ Edit  View  Settings  Language ────────────────────────────────────────┐
│ [↶][↷] │ [Wire] [1.5 mm² — lighting, C10 ▾]                           │
│ Components   │          L  N  PE  Supply                │ Properties  │
│ Protection   │   ┌──────┃──┃──┃──────────────────────┐   │             │
│ ...          │   │   QF1    QD1   QF2  QF3  QF4      │   │             │
│ Loads        │   │  [▀▀▀▀] [▀▀▀▀] [▀▀] [▄▄] [▀▀]     │   │             │
│ ...          │   │  0.04 A 0.04 A 0.04 A               │   │             │
│              │   │      [N ○○○○] [PE ○○○○]          │   │             │
│              │   └───────┃──┃──┃────────────────────┘   │             │
│              │           ┃0.04 A                        │             │
│              │       ░┌───┸──┸──┸┐░      ┌──┸─┸─┸──┐    │             │
│              │       ░│  (lamp) ●│░      │  ⊡   ⊡  │    │             │
│              │       ░└──────────┘░      └──┬───┬──┘    │             │
│              │           Lamp 1      Socket 1  │   │    │             │
│              │                           ░┌───┴┐░┌┴───┐ │             │
│              │                           ░│ket●│░│mic○│ │             │
└ Ready ───────┴───────────────────────────┴────┴─┴────┴─┴─────────────┘
   ▀ handle up = on, ▄ handle down = off; ░ glow (lamp, warm) or halo
   (appliance, green); ● button on, ○ button off
```

### Switching devices

- Breakers, RCDs and RCBOs are switched by **clicking their handle**. The
  handle is drawn up when on and down when off, using the picture's existing
  `handle` element shifted down when off (no new art files). All poles of a
  device switch together (a 2P breaker's tie bar).
- A click on the handle is a press and release without moving beyond the
  drag distance. A drag that starts on the handle moves the device, as a drag
  anywhere on the device does today. A click or drag on the body does what it
  does today (select, move).
- The click that switches also selects the device, as any click on it does.
- With the wire tool on, every press belongs to the tool (F06), so handles
  don't switch.
- The RCD and RCBO **test button** does nothing in F08 (it needs trips, F09);
  a click on it is a click on the body.

### Switching loads

- Every ceiling lamp, cooktop and plug-in appliance shows a **small round
  on/off button** drawn by the app on its picture: filled when on, hollow
  when off. Sockets have none.
- A click on the button switches the load; a drag that starts on it moves the
  load; a click or drag elsewhere on the load does what it does today. The
  click also selects the load. With the wire tool on, buttons don't switch.
- The button shows the switch only; whether the load actually runs is shown
  by its glow (below).
- An appliance keeps its on/off setting when it is plugged in, unplugged,
  re-plugged or moved, and when its socket moves.

### Starting state

- **Everything starts off.** A device dropped from the palette has its handle
  down; a lamp, cooktop or appliance dropped from the palette is off. Nothing
  runs until the user switches it on.
- The **supply is always live**: as soon as a wire joins a supply terminal,
  that wire is live. (ADR 0014's supply power button is left out; QF1 already
  switches the whole apartment.)
- Switch settings belong to the object for as long as it exists in the
  session: moving it, changing its rating or poles, or deleting it and
  undoing the deletion keeps its setting. Object ids are never reused
  (ADR 0024, 0028), so a newly dropped object always starts off.

### Seeing it live

- A **ceiling lamp glows** (a warm yellow halo around its picture) when it is
  switched on *and* the voltage between its L and N terminals is more than
  half of 230 V.
- A **running appliance or cooktop** gets a soft green halo (ADR 0013's live
  colour) when it is switched on and gets more than half of 230 V: a cooktop
  between its own L and N, a plug-in appliance between its socket's L and N.
  An unplugged appliance never runs.
- **Current labels**: each device shows one label with the largest current
  through any of its poles; each wire shows a label with its current. A label
  appears only where current flows (it would read at least 0.01 A); a wire or
  device with no current has none.
- Label format: amperes, in the UI language's number format and unit
  ("0.04 A", "0,04 А"); two decimals below 10 A, one decimal from 10 A, whole
  amperes from 100 A ("0.04 A", "13.9 A", "575 A").
- **View → Show currents** turns all current labels on and off. It is on by
  default and the choice persists like the theme.
- The labels, glows and halos follow every change at once: a click on a
  handle or button, a new or deleted wire, a plug or unplug, a moved socket,
  a power or length edit, an undo or redo. There is no simulation timer in
  F08.
- Labels and halos are readable on the dark and the light theme and follow a
  theme switch; labels change on a language switch without a restart.

### What is not undoable

- Switching a device or a load is an experiment, not an edit (ADR 0011): it
  isn't on the undo stack, and undo and redo never change a switch setting.
- An edit that changes currents (a wire, a plug, a power value) is undone as
  before, and the currents follow the restored design.

### Wiring mistakes and shorts

- Whatever is wired is solved as wired (ADR 0007): a lamp wired L to PE
  lights with its current through PE; a load wired before its breaker stays
  powered when the breaker is off; a bare wire from L to N shows a current of
  hundreds of amperes, limited only by the supply and wire resistance. F08
  has no trips, warnings or overheating; they arrive in F09.

## Simulation (core)

All of this lives in `panelcore` (Qt Core only, ADR 0006) and follows ADR
0008's electrical solve.

- **Simulation state beside the design.** A core simulation object holds the
  switch settings (device id → on/off, load id → on/off, missing means off)
  and the latest results. It is not part of `Project`: undo commands copy
  whole `PlacedDevice` and `PlacedLoad` values, so state stored there would
  be undone with them. It re-solves after every `Project::changed()` and
  every switch change, then notifies the app once per solve.
- **Contacts come from the catalog.** Each device variant lists its contacts
  as pairs of terminal names: breaker 1P `1–2`; breaker 2P `1–2`, `3–4`; RCD
  2P and RCBO 1P+N `1–2`, `N1–N2`. The catalog loader validates them: each
  pair joins two different terminals of the variant, and no terminal is in
  two contacts. (Schema stays version 1, as the catalog is embedded and
  versioned with the code.)
- **Nodes:** every terminal of `Project::terminalIds()`. All holes of the N
  bar are one conductor, and so are all holes of the PE bar.
- **Branches:**
  - wire: `R = ρ·L / S`, copper `ρ = 0.0175 Ω·mm²/m`, using the wire's
    stored section and length;
  - closed contact: 1 mΩ; open contact: no branch;
  - ceiling lamp and cooktop, switched on: `Z = U²/P` between their L and N
    terminals (`U` = 230 V, `P` = the load's stored power, resistive);
  - plug-in appliance, switched on and plugged in: `Z = U²/P` between its
    socket's L and N terminals; unplugged or off: no branch;
  - socket: no branch of its own; a load's PE terminal: no branch of its own;
  - supply: Norton source on L, `E = 230 V ∠ 0°`, `Zs` = 0.4 Ω (ADR 0007's
    default loop impedance); supply N and PE tie to the reference through
    1 mΩ (TN-C-S, bonded at the source);
  - `gmin = 1e−9 S` from every node to the reference.
- **Solver:** dense complex Gaussian elimination with partial pivoting
  (ADR 0008). No sparse library.
- **Results:** node voltages; current in every wire; current in every contact
  and per device the largest of them; per load the voltage between the
  terminals its power is drawn from, its current, and **energized** (switched
  on, drawing from somewhere, and `|U| > 0.5 · 230 V`).
- **Currents are the solved values, not P/U.** A load is a fixed resistance
  (ADR 0008), so with the supply impedance and wire resistance it draws a
  little less than its rated power: a 2000 W kettle plus a 1200 W microwave
  on QF4 of the tutorial panel (15 m of 2.5 mm²) draw ≈ 13.4 A, not the
  nominal 13.9 A. This is how resistive loads (kettles, heaters, cooktops)
  really behave. Expected values in tests are worked out from this model by
  hand, never read back from the code.
- Supply voltage and impedance are core constants in F08; F12 makes them
  editable, F17 adds three phases.
- No `step()`, no thermal state, no trips, no events in F08 (all F09).

## Non-goals

- Trips, overheating, faults, events, the RCD test button, the upstream
  supply breaker (F09).
- The simulation timer and the time-speed control (paused, ×1, ×10, ×60):
  moved to F09, where overload trips first need time.
- Saving switch settings (F10, ADR 0010).
- The tutorial and its allow filter for handles and buttons (F11).
- Engineering parameters (supply impedance, cos φ), the currents table, the
  event log, currents in the properties panel (F12).
- A supply power button; wall light switches; three-phase (F17).
- Voltage labels, current direction arrows, animated current flow.

## Anti-goals

- A design that would overheat a wire looking "all fine": currents must be the
  real solved values, not a guess from intended paths.
- A switch flipping on its own: undo, redo, moving or editing a device or load
  never changes its on/off setting.
- A lamp glowing or an appliance haloed without power reaching it; a current
  label that disagrees with the wiring drawn.
- Labels or halos that are unreadable on either theme or that hide terminals,
  handles or wire ends needed for editing.
- Switching that makes ordinary select-and-move awkward.

## Constraints

- Solver, simulation state and results in `panelcore`; scene items stay thin
  views re-synced from the project and the simulation (ADR 0006, 0011).
- Contacts are catalog data (ADR 0023 picture conventions and terminals stay
  as they are).
- A full re-solve after every change must not lag with a few dozen devices,
  loads and wires (a few hundred nodes).
- New interactive things are findable by tests: the View menu action has an
  objectName; handles and buttons are reachable through their item's id
  (`data(0)`) and a known position in item coordinates.
- Every new text ships in Russian, Kazakh and English (ADR 0012).
- Original art only; the button and halos are drawn by the app (ADR 0013).

## Approaches

**How a load is switched** (owner chose B, 2026-11-23):
- *A. Click the picture* — easiest to find, but selecting a lamp to change its
  power also switches it.
- *B. A small on/off button on each switchable load*, like the breaker handle
  — **chosen**: no accidental switching; a small target when zoomed out.
- *C. Double-click* — no clutter, but a beginner won't discover it.

**Where switch settings live** (builder's call):
- *In the design (`PlacedDevice`, `PlacedLoad`)* — one place, but undo of a
  move would also flip switches back, and every click would be an edit.
- *A core simulation object beside the design* — **chosen**: undo never
  touches switches (ADR 0011); F10 saves handle positions from it.

**How the circuit is solved** (decided in ADR 0008): complex nodal solve.
Path tracing was rejected there (parallel paths, shared neutrals, shorts).

**Which terminals a contact joins** (builder's call):
- *Listed per variant in the catalog* — **chosen**: explicit, and F16's
  switch-disconnector and voltage relay need it.
- *A naming rule in code* (top terminal pairs with the one below) — no catalog
  change, but breaks for devices laid out differently.

**Off handles** (builder's call):
- *The picture's `handle` element drawn shifted down* — **chosen**: no new art.
- *A second "off" picture per variant* — more art to keep in step.

## Decided by the owner

- Load switching by an on/off button on each switchable load (B), 2026-11-23.
- The simulation timer and time-speed control move from F08 to F09,
  2026-11-23.
- The behaviour played back on 2026-11-23 ("all ok - go ahead"): handles
  clicked to switch, drawn up/down, poles together, body still selects and
  moves; buttons on lamps, the cooktop and appliances, not sockets; no
  switching with the wire tool on; everything starts off; an appliance keeps
  its setting when plugged, unplugged or moved; supply always live, no supply
  power button; lamps glow warm yellow only when on and powered; running
  appliances and the cooktop get a green halo; a plugged appliance draws
  through its socket's L and N, an unplugged one draws nothing; current
  labels on devices and wires only where current flows; View → Show currents,
  on by default; currents update at once after every change.
- Code design left to the builder (CLAUDE.md).

## Builder's calls

- Simulation state kept beside the design in a core object, keyed by id;
  missing means off.
- Contacts listed per variant in `devices.json`, validated by the loader.
- Off handle = the `handle` element shifted down.
- A click on a handle or button also selects the item; a drag from it moves
  the item.
- Switch settings survive delete-and-undo (kept by id).
- One label per device: its largest pole current.
- Label threshold 0.01 A as displayed; number format by UI language.
- The Show currents choice persists in settings.
- Supply constants 230 V and 0.4 Ω in core until F12.
- Loads stay fixed resistances as ADR 0008 says, so labels show solved
  currents a few percent under P/U; constant-power loads were not chosen
  (non-linear solve, and wrong for heaters and kettles).
- New ADR 0029: simulation state beside the design (not undoable, not in
  `Project`, kept by id, everything starts off), contacts in the catalog,
  nodes and branches as above (bars as one conductor each, sockets and PE
  terminals with no branch, plug-ins drawing through their socket), solve on
  every change with no timer until F09, no supply power button (refines ADR
  0014).
- Roadmap: F08's time-speed bullet moves to F09; follow-ups for F10 (save
  each device's handle position from the simulation state per ADR 0010;
  decide whether load on/off is saved), F11 (handle clicks and load
  buttons go through the allow filter) and F09 (with solved currents its
  scenario numbers shift: the 7 kW cooktop on 15 m of 1.5 mm² draws
  ≈ 27.7 A, barely over the 27.6 A overheating threshold, so F09 must pick a
  case that clears it well; the washing-machine overload on QF4 is ≈ 22.1 A,
  tripping in about 2.7 min of simulated time instead of ADR 0016's 2 min).

## Left to the builder

- The size, place and look of the on/off button (it must be clickable at the
  default fitted zoom and not cover a load's terminals), the halo and glow
  look, and the handle's hit area (at least the handle element's bounds).
- Where device and wire labels go and how they stay readable (e.g. a small
  chip behind the text), as long as they don't hide handles, rating labels or
  terminals, and draw above wires.
- Simulation classes, signals, how results reach scene items, file layout.
- Wording in all three languages (Kazakh to be reviewed by a native speaker
  when possible).

## Tests

- **Catalog tests:** every variant's contacts load; a contact naming an
  unknown terminal, joining a terminal to itself, or a terminal in two
  contacts is an error.
- **Core unit tests (solver and simulation):** a single lamp on a closed
  breaker draws `U / (Zs + wire resistance + U²/P)`; series wires add resistance; two lamps in parallel add
  currents; a shared neutral carries the sum; an open contact stops current
  and de-energizes the load after it; a 2P device opens both poles; floating
  parts (an unwired device, a lamp wired on one side only) solve without a
  singular matrix and carry no current; N bar holes are one conductor; a
  plugged appliance draws between its socket's L and N only when on; an
  unplugged or off appliance draws nothing; a socket draws nothing; a lamp
  wired L–PE lights with current through PE; a load fed before its breaker
  stays energized with the breaker off; an L–N short gives a current limited
  by `Zs` and wire resistance; wire length and section change the result;
  switch settings start off, survive device and load updates and remove-then-
  restore, and are not changed by project changes; one notification per
  solve; a few-hundred-node panel solves quickly.
- **GUI scenario (offscreen, roadmap):** the tutorial panel from a fixture
  (tutorial devices and wires, two lamps on QF2, sockets on QF3 and QF4, a
  kettle and a microwave in the first kitchen socket); click the handles of
  QF1, QD1, QF2 → handles drawn up, still no current; click Lamp 1's button
  → QF2 ≈ 0.043 A in the model, Lamp 1 energized and glowing, labels on QF1,
  QD1, QF2 and the wires carrying the lamp's current, none on its PE wire;
  click QF2's handle → Lamp 1 de-energized, labels gone below QF2; switch
  QF4 on and the kettle and microwave on → ≈ 13.4 A on QF4 (solved value,
  see Simulation), both haloed; unplug the kettle → it stops, QF4 ≈ 5.14 A;
  a drag that starts on a handle moves the device
  without switching it; with the wire tool on a handle click doesn't switch;
  undo after a switch leaves the switch as it was; View → Show currents off →
  no labels, on → back; light theme; Russian and Kazakh label format.
  Screenshots at each step.
- **Translations:** the existing `translations_complete` test covers the new
  strings.
- **Smoke test** of the real binary under Xvfb: build a small circuit with
  `xdotool`, flip handles and a lamp button, inspect the screenshot.
