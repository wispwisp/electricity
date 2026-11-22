# F07 — Loads

Date: 2026-11-22 · Status: approved by the owner

Roadmap item: [F07](../../roadmap.md#f07-loads).
ADRs: [0005](../../adr/0005-ui-qt-widgets-graphics-view.md),
[0006](../../adr/0006-core-and-app-layering.md),
[0007](../../adr/0007-electrical-model.md),
[0009](../../adr/0009-component-catalog.md),
[0010](../../adr/0010-project-file-format.md),
[0011](../../adr/0011-undo-redo.md),
[0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0014](../../adr/0014-progressive-disclosure.md),
[0015](../../adr/0015-exports.md),
[0016](../../adr/0016-electrical-reference-data.md),
[0023](../../adr/0023-catalog-schema.md),
[0024](../../adr/0024-placed-devices-and-designations.md),
[0025](../../adr/0025-palette-drag-and-drop.md),
[0026](../../adr/0026-redo-shortcuts-and-undo-texts.md),
[0027](../../adr/0027-wires-roles-colours-routes.md).

## Intent

A panel exists to feed lamps, sockets and appliances. F07 puts them on the
canvas as pictures around the enclosure, lets the user plug appliances into
sockets and wire lamps, sockets and the cooktop with the F06 wire tool. Then
F08 can power them, F09 can overload and short them, the tutorial (F11, step
8) can have the user "place loads and run lines", and the parts list (F13)
gets real cable lengths. The basic flow must stay as simple as F05 and F06:
drag, drop, click-click wires, no engineering knowledge needed.

## Goals

### What the user sees

```
┌ Edit  View  Settings  Language ─────────────────────────────────────────┐
│ [↶][↷] │ [Wire] [1.5 mm² — lighting, C10 ▾]                            │
│ Components        │         L  N  PE  Supply            │ Properties   │
│ Protection        │   ┌─────┃──┃──┃──────────────────┐  │ Wire         │
│ ▮ Breaker 2P C40  │   │    QF1  QD1  QF2  QF3         │  │ QF2 2 →      │
│ ...               │   │   [▮ ▮][▮ ▮] [▮]  [▮]         │  │   Lamp 1 L   │
│ Loads             │   │      [N ○○○○] [PE ○○○○]       │  │ Section      │
│ ◯ Ceiling lamp    │   └───────┃──┃──┃─────────┃─┃─┃──┘  │ [1.5 mm² ▾]  │
│   10 W            │           ┃  ┃  ┃         ┃ ┃ ┃     │ Length       │
│ ⊡ Socket (double) │      ┌────┸──┸──┸┐   ┌────┸─┸─┸┐    │ [15 m ]      │
│ ▭ Kettle  2000 W  │      │  (lamp)   │   │  ⊡   ⊡  │    │ The whole    │
│ ...               │      └───────────┘   └──┬───┬──┘    │ cable: L, N, │
│ ▭ Cooktop 7000 W  │         Lamp 1    Socket 1  │   │   │ PE           │
│                   │                       ┌───┴┐ ┌┴───┐ │              │
│                   │                       │kett│ │micr│ │              │
└ Ready ────────────┴───────────────────────┴────┴─┴────┴─┴──────────────┘
```

### The Loads palette group

- A **Loads** group in the Components palette, after Protection, with the ten
  loads of ADR 0009, each with its picture and default power in the second
  line:
  ceiling lamp (LED) 10 W, double socket (no power), kettle 2000 W, washing
  machine 2200 W, refrigerator 150 W, TV 120 W, microwave 1200 W, air
  conditioner 1000 W, water heater 2000 W, electric cooktop 7000 W.
- Hover tooltips describe each load, as for devices (F04).
- **Wired loads:** ceiling lamp, socket and cooktop. They have terminals L, N,
  PE and are connected with the wire tool. The cooktop is wired because it
  draws about 30 A and needs its own C32 line (ADR 0009); no socket takes it.
- **Plug-in appliances:** kettle, washing machine, refrigerator, TV,
  microwave, air conditioner, water heater. They have no terminals; they
  plug into a socket.

### Placing and moving loads

- Drag a load from the palette and drop it **anywhere on the canvas outside
  the enclosure**. A drop on the enclosure, on the supply block (with its
  "Supply" text and letters) or on another load (with its caption, and for
  a socket its two plug places, empty or not) is refused with a status bar hint and changes
  nothing. While dragging, a preview follows the cursor and turns the
  refused colour where the drop would be refused, as for devices (F05).
- Loads are moved by dragging, with the same refusals; they are selected by a
  click (one thing selected at a time), deleted with the Delete key, Edit →
  Delete or right-click → Delete; Esc cancels a move. All of it is undoable,
  exactly like devices (F05).
- With the wire tool on, loads can't be dragged (presses go to the tool), and
  loads can still be dropped from the palette, as for devices (F06).
- The canvas scroll area and View → Fit include the loads.

### Names

- Each wired load shows a **caption** under its picture: its type name and a
  number, counted per type in the order the loads were placed — "Lamp 1",
  "Lamp 2", "Socket 1", "Cooktop 1". Deleting a load renumbers the later ones
  of its type; moving never renumbers. Names are derived, never stored (as
  designations, ADR 0024).
- Plug-in appliances show no caption; their picture says what they are.
- Wire ends read "Lamp 1 L", "Socket 2 PE" in the wire properties.

### Plugging appliances into sockets

- **A socket always keeps room for its two appliances.** Its two plug places
  below it are part of its outline even when empty, and are drawn as faint
  outlines so the user can see why a drop there is refused. So plugging
  never fails for lack of room, and sockets can't be packed so tightly that
  their appliances would collide.
- Dropping an appliance (from the palette or by moving it) **onto a socket**
  plugs it in. It hangs below the socket on a short cord, in one of the
  socket's **two places**. A third appliance is refused with a hint ("This
  socket already has two appliances"). Dropping a wired load onto a socket is
  refused like any overlap.
- **Dragging a plugged appliance away** to free canvas unplugs it; it stays
  where it was dropped. Dragging it onto another socket with a free place
  moves its plug there. Each is one undoable edit. A refused drop puts it
  back where it was, still plugged.
- An unplugged appliance sits on the canvas like any load and can be plugged
  in later. Appliances can also be dropped from the palette straight onto
  empty canvas.
- **Moving a socket** carries its plugged appliances; the move is refused if
  any of them would land on something.
- **Deleting a socket** deletes its wires (as deleting a device does, ADR
  0027) and leaves its appliances on the canvas, unplugged, where they hung.
  One undo brings the socket, its wires and the plugs back.
- The cord is drawn by the app as a thin line; it is not a wire, can't be
  selected and has no length.

### Wiring loads

- The lamp, socket and cooktop terminals are wire tool terminals like any
  other: highlighted under the cursor, refused when full.
- Their roles are L, N and PE (ADR 0027), so wire colours follow F06's rule.
- Each takes **two wires**, so a line runs from the panel to the first load
  and on to the next (ADR 0007's load pass-through terminals).
- Wires to loads are routed automatically like the others (ADR 0027): they
  leave the enclosure, go around device bodies, the supply, other loads and
  their captions and appliances, and enter load terminals from above.

### Wire length and cables

- A new wire with a load at either end (including load to load, and supply to
  load) starts at **15 m** (ADR 0007). Wires with no load end keep 0.3 m,
  hidden until F12.
- A **cable** is the set of wires between the same two sides, where one side
  is a load and the other is "the panel" (any device, bar or the supply) or
  another load (or the same load). So the L wire from QF2, the N wire from
  the N bar and the PE wire from the PE bar to Lamp 1 are one cable; the
  three wires from Lamp 1 to Lamp 2 are another.
- The properties panel shows **Length** for a wire to a load. Changing it
  changes every wire of that cable, as one undoable edit, so L, N and PE of
  one run never disagree and the parts list (F13) counts real lengths.
- A new wire that joins an existing cable starts at **that cable's length**
  instead of 15 m, so a cable set to 8 m stays 8 m when its last conductor is
  drawn.
- The drawn distance on the canvas has nothing to do with the length.
- These defaults apply to newly drawn wires only. An undo that brings back a
  deleted load's wires restores them exactly as they were.

### Properties panel

- **Lamp or cooktop selected:** its name ("Lamp 1"), its type name ("Ceiling
  lamp"), and an editable **Power** in watts.
- **Plug-in appliance selected:** its type name ("Kettle"), an editable
  **Power**, and where it is plugged in ("Plugged into Socket 1" or "Not
  plugged in").
- **Socket selected:** its name, its type name ("Double socket"), and what is
  plugged in (e.g. "Kettle, 2000 W", "Microwave, 1200 W", or "Nothing plugged
  in").
- **Wire to a load selected:** as in F06 (ends, Section), plus **Length** in
  metres with a short note that it applies to the whole cable.
- Power is a basic property (ADR 0014). Edits apply immediately and are
  undoable; a value the field doesn't accept is not an edit.

### Enclosure size changes

- When the enclosure changes size it grows or shrinks downwards (as today).
  Loads below it **keep their distance from its bottom edge**, moving with
  it, in the same undo step. So growing the enclosure never covers a load and
  is never refused because of one. Loads beside or above it don't move.
  "Below" means the load's whole outline (a socket's including its plug
  places) is under the enclosure's bottom edge.
- A shrink that would move a load onto a load that doesn't move (one beside
  the enclosure reaching below its new bottom edge) is refused with a hint,
  like the existing shrink refusals for devices and wired bar holes.

### Undo and redo

- Covers: add, move, delete a load (with its wires; a socket also with its
  plugs), plug, unplug, re-plug, change power, change a cable's length, and
  the load moves of an enclosure size change, alongside F05 and F06 edits.
  Undo texts stay plain (ADR 0026).
- An undo that removes the load a wire in progress starts at cancels the
  wire in progress (as F06).

### Languages

Every new text (palette group, load names and descriptions, captions,
properties labels and notes, refusal hints) ships in Russian, Kazakh and
English and changes on a language switch without a restart. Load names and
descriptions are catalog strings (ADR 0023: registered in
`src/core/catalogstrings.cpp`).

## Load data (core model)

- **Catalog:** load types in `resources/catalog/loads.json` (ADR 0009 named
  this file), schema version 1: id, name, description, picture, default
  power (W; none for the socket), and either terminals (L, N, PE with
  positions and roles) or "plugs into a socket". The socket has two plug
  places. Load pictures live in `resources/svg/loads/` (ADR 0013).
- **Placed load:** id from a counter that only grows, never reused in a
  session (as devices and wires); catalog type; position on the canvas;
  power in watts, stored in full (as device ratings, ADR 0024, so a catalog
  change never changes a saved design); for a plug-in appliance, the socket
  id and place (1 or 2) it is plugged into, or none. A plugged appliance's
  position comes from its socket.
- **Terminals:** `"<load-id>:L"`, `":N"`, `":PE"` (ADR 0007), in
  `terminalIds()`, with roles, capacity two.
- **Rules enforced by the model:** known type; power within the allowed
  range; only a plug-in appliance can be plugged, only into a socket, into a
  free place; removing a load removes its wires; removing a socket unplugs
  its appliances; wire length rules above (default by load end, cable
  grouping, a new wire taking its cable's length).
- **Geometry:** whether a position is free (outside the enclosure, not on
  the supply or another load) needs picture sizes; where that check lives is
  the builder's call, as long as the model stays GUI-free (ADR 0006).
- **Not saved yet:** loads go into the `.panel` file in F10 (`loads` in ADR
  0010). The on/off state comes with F08.

## Non-goals

- Switching loads on and off, lamps glowing, currents (F08); faults and
  trips (F09); saving (F10); the tutorial and its allow filter (F11);
  engineering fields such as cos φ (F12).
- Wall light switches, rooms, floor plans, building wiring (product design
  non-goal).
- An "Unplug" context menu entry (drag away instead), single sockets, more
  load types (F16), editing a wire's colour.
- Captions for plug-in appliances; renaming loads.
- Per-conductor lengths inside one cable.

## Anti-goals

- L, N and PE of one cable silently ending up with different lengths.
- A load, appliance or wire disappearing without the user deleting it
  (deleting a socket must not delete what was plugged into it).
- An enclosure change covering a load.
- Load pictures that are unreadable on the dark theme, the light theme or at
  palette size; brand-like appliance art.
- A basic flow that needs engineering knowledge.

## Constraints

- Load data, plugging rules, the length and cable rules live in `panelcore`
  (Qt Core only, ADR 0006). Scene items are thin views re-synced from the
  model's `changed()`; undo commands call model methods only (ADR 0011, ADR
  0024).
- Terminal and plug place positions come from the catalog, not the SVG (ADR
  0013).
- SVGs: original, generic, no text, no brands; flat shapes with a mid-tone
  fill and a light outline (ADR 0013); SPDX header; legible at 32 px on both
  themes; rendered with `QSvgRenderer` (ADR 0023).
- Routing must stay quick enough to re-route every wire on every change, with
  a few dozen devices, loads and wires spread over the canvas, without
  visible lag.
- Every new interactive widget has a stable `objectName`; load items expose
  their id via `data(0)` (ADR 0005).

## Approaches

**Cable length** (owner chose B):
- *A. Length per wire, each edited separately* (the roadmap as written) —
  simple, but L, N and PE of one run drift apart and the parts list and
  short-circuit simulation get wrong lengths.
- *B. Length per cable* — **chosen**: editing one wire's length changes the
  other wires of the same run, in one undo step; the three conductors can't
  differ, which nobody needs.

**How a cable's length is kept** (builder's call):
- *Length stays on each wire; the core works out the cable when one is
  edited* — **chosen**: the file format (ADR 0007, 0010) and the simulation
  already want per-wire lengths, and the grouping follows the drawn wires.
- *A separate cable object owning the length* — must be kept in step with
  every wire drawn and deleted, and changes the saved file.

**Plugged appliances** (builder's call):
- *The appliance records its socket and place; its position follows the
  socket* — **chosen**.
- *Hidden cord wires from the appliance to the socket terminals* — F08 would
  see only wires, but cords collide with the two-wire terminal limit on a
  socket that is fed and chained, and can fall out of step with the socket.

**Wires to loads** (builder's call):
- *The F06 router, with loads as obstacles and load terminals entered from
  above* — **chosen**.
- *Fixed cable entries in the enclosure wall* — more realistic, adds a
  concept nobody asked for.

## Decided by the owner

- Cable length per cable (B), 2026-11-22.
- The behaviour played back on 2026-11-22 ("all ok - go ahead"): the Loads
  group with the ten loads; drop anywhere outside the enclosure, refusals on
  the enclosure, supply or another load; move, delete and undo like devices;
  captions "Lamp 1", "Socket 2"; appliances hang below a socket on a cord,
  two per socket, third refused, drag away to unplug, moving a socket carries
  them; deleting a socket leaves its appliances unplugged; lamp, socket and
  cooktop wired with L, N, PE, two wires per terminal; wires to loads start
  at 15 m including load to load; a new wire joining a cable takes its
  length; editable power for lamps and appliances; a socket shows what's
  plugged in; a wire to a load shows section and length; out of scope:
  switching, faults, wall switches, rooms.
- Code design left to the builder (CLAUDE.md).

## Builder's calls

- Cooktop wired, the other six appliances plug in.
- Captions only for wired loads, numbered per type in placement order.
- Deleting a socket unplugs its appliances rather than deleting them.
- A new wire joining a cable takes the cable's length.
- Loads below the enclosure move with its bottom edge on a size change; a
  shrink that would move one onto another load is refused.
- A socket's outline always includes its two plug places, drawn faintly when
  empty.
- Length defaults and cable adoption apply to newly drawn wires only; undo
  restores wires as they were.
- Power stored in full on the placed load.
- Load terminals entered from above.
- New ADR (0028): loads in the model (placed loads, plugging by reference,
  names derived from placement order, deleting a socket unplugs), the cable
  rule and wire length defaults, loads moving with the enclosure's bottom
  edge.
- Roadmap follow-ups added for F08 (a plugged appliance draws power between
  its socket's L and N), F10 (save loads and the load id counter, migrate
  power) and F11 (load drops and moves also go through the allow filter).

## Left to the builder

- Load picture art, sizes and the exact place of terminals, captions and the
  two plug places; the cord's look.
- Which plug place a dropped appliance takes (e.g. the free one nearest the
  drop, or the first free one).
- Whether load positions snap to a grid, and which.
- The power field's range and step (it must cover 1 W to at least 10 kW),
  the length field's range and step (at least 0.5 m to 100 m).
- Where the free-position check lives and how load sizes reach it.
- Model classes, signals, command classes, file layout, load id prefix;
  wording in all three languages (Kazakh to be reviewed by a native speaker
  when possible).

## Tests

- **Catalog tests:** `loads.json` loads and validates (pictures exist,
  terminals only on wired loads with roles L/N/PE, plug places only on the
  socket, powers present where expected); the 32 px SVG review screenshots
  (F04) include every load picture on both themes.
- **Core unit tests:** add, move, remove a load; ids never reused, restore
  with the same id; names per type in placement order, renumbering on
  delete; load terminals in `terminalIds()` with roles and capacity two;
  removing a load removes its wires; plugging: only appliances, only into a
  socket, two places, third refused, re-plug, unplug; removing a socket
  unplugs its appliances; power range; wire default length (15 m with a load
  end, 0.3 m without); cable grouping (panel side merges devices, bars and
  supply; load to load; same load on both ends); changing a cable's length
  changes all its wires; a new wire joining a cable takes its length; loads
  below the enclosure follow its bottom edge on a size change, a shrink that
  would make loads collide is refused; a drop on an empty socket's plug
  place is refused; undo of a load deletion restores its wires' lengths
  unchanged; `changed()`
  once per mutation.
- **Routing tests:** a wire to a load enters its terminal from above and
  doesn't cross another load, its caption or its plugged appliances.
- **Undo tests:** each new command restores the exact model state on undo
  and redo, including delete socket (wires and plugs) and the cable length
  edit.
- **GUI scenario (offscreen, roadmap):** drag a ceiling lamp and a socket
  from the palette below the enclosure → loads in the model, captions
  "Lamp 1" and "Socket 1"; drop a lamp on the enclosure → refused with a
  hint; drag a kettle onto the socket → plugged in; a microwave → second
  place; a TV → refused with a hint; drag the kettle away → unplugged where
  dropped; drag it back; with the wire tool, wire QF2 2 → Lamp 1 L, N bar →
  Lamp 1 N, PE bar → Lamp 1 PE → three wires, 15 m, colours L/N/PE; select
  one, set Length 8 m → all three 8 m; undo → 15 m; a chained wire Lamp 1 →
  Lamp 2 → 15 m; after setting that cable to 5 m, the next conductor drawn
  to Lamp 2 starts at 5 m; change the kettle's power → model; move the
  socket → appliances and wires follow; delete the socket → its wires gone,
  appliances unplugged on the canvas; undo → all back; grow the enclosure →
  loads below it move down. Screenshots at each step, including the Loads
  palette group, the drag preview and a refused drop, a plugged socket,
  wired loads, a selected wire with Length, load properties, the light
  theme, and properties in Russian and Kazakh.
- **Translations:** the existing `translations_complete` test covers the new
  strings.
- **Smoke test** of the real binary under Xvfb, dropping a load and plugging
  an appliance with `xdotool`; screenshots inspected.
