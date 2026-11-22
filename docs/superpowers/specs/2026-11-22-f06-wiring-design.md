# F06 — Wiring

Date: 2026-11-22 · Status: approved by the owner

Roadmap item: [F06](../../roadmap.md#f06-wiring).
ADRs: [0005](../../adr/0005-ui-qt-widgets-graphics-view.md),
[0006](../../adr/0006-core-and-app-layering.md),
[0007](../../adr/0007-electrical-model.md),
[0011](../../adr/0011-undo-redo.md),
[0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0014](../../adr/0014-progressive-disclosure.md),
[0016](../../adr/0016-electrical-reference-data.md),
[0023](../../adr/0023-catalog-schema.md),
[0024](../../adr/0024-placed-devices-and-designations.md),
[0026](../../adr/0026-redo-shortcuts-and-undo-texts.md).

## Intent

The owner chose a panel that is **wired by hand, every conductor separately**
(product design, approach A), because a beginner learns from the wiring
mistakes the simulation will show: a neutral bypassing the RCD, a load before
its breaker. F06 lets the user draw those wires: pick a cross-section, click
one terminal, click another. The simulation (F08, F09) follows exactly the
wires drawn here, the tutorial (F11) guides each wire, and the parts list and
single-line diagram (F13, F15) are derived from them. A panel has 20–40 wires,
so drawing must be quick, and the basic flow must not need engineering
knowledge.

## Goals

### What the user sees

```
┌ Edit  View  Settings  Language ──────────────────────────────────────┐
│ [↶][↷] │ [Wire] [2.5 mm² — sockets, C16 ▾]                            │
│ Components      │                                  │ Properties      │
│ Protection      │        L  N  PE    Supply        │ Wire            │
│ ▮ Breaker 2P C40│        ┃  ┃  ┃                   │ Supply L →      │
│ ▮ RCD 40 A 30 mA│   ┌────╂──╂──╂────────────────┐  │   QF1 1         │
│ ...             │   │    ┃  ┃  ┗━━━━━━━━━━━━━┓  │  │ Section [10 mm²▾]│
│                 │   │   ┌┸──┸┐┌─────┐┌───┐   ┃  │  │                 │
│                 │   │   │QF1 ││ QD1 ││QF2│   ┃  │  │                 │
│                 │   │   └────┘└─────┘└───┘   ┃  │  │                 │
│                 │   │      [N  ○○○○○○○○]     ┃  │  │                 │
│                 │   │      [PE ○○○○○○○●]━━━━━┛  │  │                 │
│                 │   └───────────────────────────┘  │                 │
└ Click the second terminal. Esc cancels. ─────────────────────────────┘
```

### The wire tool

- The toolbar gets, after Undo and Redo, a checkable **Wire** button (new
  original SVG icon) and a **section drop-down** for new wires.
- The drop-down lists 1.5, 2.5, 4, 6, 10 and 16 mm². Entries with a usual
  pairing (ADR 0016) say what they are for: at least "1.5 mm² — lighting,
  C10", "2.5 mm² — sockets, C16", "6 mm² — cooktop, C32", "10 mm² — apartment
  feed, 2P C40". The default is 2.5 mm².
- The section is **always the user's choice**. It is never picked
  automatically from the breaker a wire starts at: a jumper between group
  breakers carries several lines' current behind the main breaker, so
  "C10 means 1.5 mm²" would make it dangerously thin, and hiding that would
  defeat the overheating lesson of F09.
- The tool **stays on** after each wire, so the next wire starts with the next
  click.
- **Esc** cancels the wire in progress; with none in progress, Esc turns the
  tool off. Clicking the Wire button also turns it off (cancelling a wire in
  progress).
- While the tool is on, devices can't be dragged to move them (a press near a
  terminal must not start a move). Dropping presets from the palette still
  works and leaves the tool on.

### Drawing a wire

- With the tool on, the terminal under the cursor is highlighted. A terminal
  that can't take another wire is highlighted in the refused colour.
- Terminals are: the supply block's L, N, PE; every device terminal; every
  hole of the N and PE bars.
- First click on a terminal starts a wire there. A preview line follows the
  cursor from that terminal, and the status bar says to click the second
  terminal.
- Second click on another terminal creates the wire with the selected section
  and colour, as one undoable edit, and selects it, so its section can be
  fixed straight away in the properties panel.
- With the tool on and no wire in progress, clicks on anything that isn't a
  terminal do nothing (they don't select).
- Clicking the start terminal again cancels the wire in progress. Clicks on
  anything that isn't a terminal do nothing while a wire is in progress.
- Right-click cancels a wire in progress; with none in progress it opens the
  context menu as with the tool off.
- A wire may join two terminals of the same device (a bypass mistake that the
  simulation will show). The editor never refuses a wire for being
  electrically wrong (ADR 0007).

### Refused wires (each with a status bar hint; nothing changes)

- A terminal that already has two wires (device and supply terminals) or a
  bar hole that already has one: refused as the start, and refused as the end
  (the wire in progress stays started so the user can pick another terminal).
- A second wire between the same two terminals: refused as the end, same as
  above. It is pointless and would hide a mistake.

### Wire colour

- Colours follow ADR 0013: L brown, N blue, PE yellow-green.
- Terminals carry an optional **role** (L, N or PE) as catalog data, matching
  what real devices mark or what practice fixes:
  - supply block: `L` → L, `N` → N, `PE` → PE;
  - N bar holes → N, PE bar holes → PE;
  - RCD and RCBO: `1`, `2` → L; `N1`, `N2` → N;
  - 1-pole breaker: `1`, `2` → L (single-pole devices go in the phase);
  - 2-pole breaker: no role (its poles are identical).
- A new wire takes the role of the **first clicked terminal that has one**;
  if only the second end has a role, that one; if neither does, L (brown).
  So both ends having different roles (a mistake) gives the first click's
  colour.
- The colour is stored on the wire when it is created and doesn't change
  afterwards (a real wire keeps the colour the installer chose).

### Routes

- Wires are drawn with **automatic orthogonal routes** (horizontal and
  vertical segments), recomputed after every change. Routes are not stored and
  not editable.
- A wire leaves and enters a terminal from a fixed side: device top terminals
  from above, bottom terminals from below; supply terminals from below; bar
  holes from the side that doesn't cross the other bar (N bar from above, PE
  bar from below, as laid out in the current enclosure pictures).
- **Routes never cross a device body, the supply block or a bar**, except the
  short final stretch into their own terminal, so they never cover the
  handles the user will click in F08 or the rating labels. They run in the
  free space above and below rows and down the sides of the enclosure
  interior, and may leave the interior (supply wires come in from above;
  F07 load wires will go out).
- Wires sharing a stretch run **side by side, not on top of each other**, so
  each one can be followed by eye and clicked. Crossings are allowed.
- Routes prefer few bends and short length, and are deterministic (the same
  panel always draws the same way), so screenshots and tests are stable.
- **Wires follow devices**: after a device is moved (or the enclosure size
  changes and bars and rows move), its wires end at the terminals' new
  positions.

### Wire look

- A thick line in the conductor colour with a dark edge, like the wires in the
  supply picture; PE green with yellow stripes. Thicker sections draw a little
  thicker, within what stays readable.
- Wire ends land on the terminal screw.
- A selected wire is drawn highlighted in the selection colour.
- Designations (QF1, QD1…) are drawn on top of wires and stay readable: a wire
  leaving a 1-pole breaker's top terminal runs up through its designation.

### Selecting, deleting, editing

- With the tool off, clicking a wire selects it. One thing is selected at a
  time, as in F05.
- The Delete key, Edit → Delete, and right-click on a wire → Delete remove the
  selected wire.
- **Properties panel, wire selected:** title "Wire", its two ends as
  designation and terminal (e.g. "Supply L", "QF1 2", "N bar 3", "PE bar 1"),
  and a **Section** drop-down (1.5–16 mm²). Changes apply immediately and are
  undoable. Length, material and ampacity are engineering details (F12) and
  not shown.

### Edits that would orphan wires

- **Deleting a wired device** deletes its wires with it, as one undoable edit;
  undo brings back the device and its wires with their original ids.
- **Changing a 2P breaker to 1P** while terminal 3 or 4 has a wire is refused
  with a hint to remove those wires first; the drop-down goes back (as F05's
  refused edits).
- **Shrinking the enclosure** while a bar hole that would disappear has a wire
  is refused with a hint, like the existing refusal for devices on a vanishing
  row.
- Moving a device and growing the enclosure keep every wire.

### Supply letters

- The supply block shows the letters **L**, **N**, **PE** next to its
  terminals, drawn by the app as text (ADR 0013: no text in SVGs), in the
  theme's text colour, placed where wires leaving the terminals don't cover
  them. They are conductor codes and are not translated.

### Undo and redo

- Covers: add wire, delete wire, change section, and delete device with its
  wires (one step), alongside F05's edits. Undo texts stay plain (ADR 0026).
- Turning the tool on or off and choosing the toolbar section are not edits.
- If an undo removes the terminal a wire in progress started from, the wire in
  progress is cancelled.

### Languages

Every new text (Wire button and tooltip, section entries and their hints,
status bar hints, properties labels, "N bar", "PE bar") ships in Russian,
Kazakh and English and changes on a language switch without a restart.

## Wire data (core model)

- Id `w<n>` from a counter that only grows, never reused in a session (as
  device ids, ADR 0024). Undo restores a deleted wire under its old id.
- Two endpoints, as terminal ids `"<object-id>:<terminal>"` (ADR 0007).
  Object ids: devices `d<n>`, `supply`, `nbar`, `pebar`; bar holes are
  numbered from 1, left to right (`nbar:3`).
- Section in mm² (one of the six values), colour role (L, N, PE), and length
  0.3 m (ADR 0007's default inside the enclosure; not shown until F07/F12).
  Copper is implied (ADR 0016).
- Rules enforced by the model: both endpoints exist; they differ; capacity
  (two wires per device or supply terminal, one per bar hole); no second wire
  between the same pair. Removing a device removes its wires; a device change
  or enclosure change that would drop a wired terminal is refused.

## Non-goals

- Loads, wires to loads and wire length editing (F07); switching and
  simulation (F08, F09); saving and opening (F10); the tutorial and its allow
  filter (F11); engineering fields (length, material, ampacity, F12).
- Editing a wire's colour (ADR 0007 calls it editable; deferred to a later
  feature).
- Editing routes or dragging wire bends; moving a wire's end to another
  terminal (delete and redraw instead); chained drawing; wire labels or
  numbers; comb busbar (F16); multiple selection.
- Choosing the section automatically.

## Anti-goals

- Wires covering device handles or rating labels, or making designations
  unreadable.
- A wire disappearing or changing its ends without the user deleting it.
- A section chosen for the user in a way that hides an undersized wire.
- A basic flow that needs engineering knowledge.

## Constraints

- Wire data, terminal roles, the wiring rules and the colour rule live in
  `panelcore` (Qt Core only, ADR 0006). Scene items are thin views re-synced
  from model signals; undo commands call model methods and never touch scene
  items (ADR 0011, ADR 0024's one `changed()` per mutation).
- Terminal positions come from the catalog, not from the SVG (ADR 0013); the
  supply's terminals need the same treatment.
- Routing must be quick enough to re-route every wire on each change for
  panels of a few dozen devices, loads and wires, without visible lag.
- New SVGs follow ADR 0013 and carry an SPDX header; toolbar icons are
  rendered to pixmaps (qsvg plugins stay excluded, ADR 0023).
- Every new interactive widget has a stable `objectName`; wire items expose
  their id via `data(0)` (ADR 0005).

## Approaches

**Wire colour** (owner approved the proposal):
- *Role of the first clicked terminal* (ADR 0007 as written) — a 2P breaker's
  poles carry no L/N marking, so wires from them would get an arbitrary
  colour.
- *First clicked terminal that has a role, else the other end, else L* —
  **chosen**: the same simple rule, with roles only where real devices or
  practice fix them. Refines ADR 0007.
- *Colour traced from what the wire is connected to* — always "true", but
  colours change when upstream wiring changes, unconnected wires have none,
  and real wires keep the installer's colour.

**Section** (owner approved): *user picks in the toolbar, with pairing
hints* — **chosen**; *picked automatically from the breaker* — wrong for
jumpers and hides undersized wires.

**Routes** (owner approved the proposal; the algorithm is the builder's, per
the product design):
- *Around devices, side by side* — **chosen**: keeps handles (F08) and labels
  clear, every wire traceable.
- *Fixed three-segment route over devices* — much simpler, but covers handles
  and labels and stacks wires on top of each other.

**Edits that would orphan wires** (owner approved): *refuse with a hint* —
**chosen**, matching F05's "nothing removed silently"; *remove the wires
along with the edit* — silent loss of work. Deleting a device is an explicit
removal, so its wires go with it.

**Tool behaviour** (owner approved): *stays on after each wire* —
**chosen**, for 20–40 wires per panel; *one wire per activation* — more
clicks for no gain.

## Decided by the owner

- The behaviour played back on 2026-11-22 ("all ok, proceed"): the wire tool
  with section drop-down and hints, click-click drawing, automatic right-angle
  routes, colour by terminal, wires follow devices, select/delete/change
  section/undo, the two-wire and one-wire limits, supply letters; deleting a
  device deletes its wires; refusing pole changes and enclosure shrinks that
  would orphan wires; colour from the marked end; section always the user's;
  routes around devices; the tool stays on, Esc cancels then turns it off, no
  device moves while it is on; the properties panel shows the section.
- Code design left to the builder (CLAUDE.md).

## Builder's calls

- Default section 2.5 mm²; not persisted between runs.
- 1-pole breaker terminals have role L; 2-pole breaker terminals have none.
- Clicking the start terminal again cancels; right-click cancels; clicks on
  non-terminals do nothing while the tool is on, with or without a wire in
  progress.
- A newly drawn wire becomes selected.
- Designations are drawn above wires.
- A duplicate wire between the same pair is refused.
- Palette drops stay available while the tool is on.
- Routes are not stored (refines ADR 0007's "route points"); F10 saves no
  routes.
- Properties show the wire's ends and section only.
- New ADR: wires — terminal roles and the colour rule, automatic unstored
  routes, duplicate refusal, deleting a device with its wires, refusing edits
  that orphan wires.

## Left to the builder

- Routing algorithm, spacing between side-by-side wires, how far routes keep
  from device bodies, fallback if no clean route exists.
- Whether wires re-route live while a device is dragged or only after the
  drop (after the drop they must be final).
- Preview line look, terminal highlight look and hit radius, wire thickness
  per section, Wire icon art, keyboard shortcut for the tool, if any.
- Where the supply's terminal positions and roles live as data, and where the
  supply letters sit.
- Whether section hints are written out or derived from the presets'
  `wireSection` (ADR 0023), as long as the four ADR 0016 pairings appear.
- Model classes, signals, command classes, file layout; hint wording in all
  three languages (Kazakh to be reviewed by a native speaker when possible).

## Tests

- **Core unit tests:** add a wire between valid terminals; refused: unknown
  terminal, same terminal, third wire on a device or supply terminal, second
  wire on a bar hole, duplicate pair; ids never reused; remove and restore
  with the same id; colour rule (role on first end, only on second end,
  neither, both differing); removing a device removes its wires; pole change
  2P→1P refused with 3/4 wired, allowed without; enclosure shrink refused with
  a wired vanishing bar hole, grow keeps wires; section limited to the six
  values; `changed()` fired for each change.
- **Routing tests:** routes are orthogonal, start and end on the terminal
  points from the fixed side, never cross a device body, the supply or a bar
  except the final stretch, wires sharing a stretch don't overlap, and the
  same input gives the same routes.
- **Undo tests:** each new command (and delete-device-with-wires) restores the
  exact model state on undo and redo.
- **GUI scenario (offscreen, roadmap):** on a panel with QF1 (2P C40) and QD1,
  turn on the wire tool, choose 10 mm², click supply L then QF1 1 → wire in
  the model with 10 mm² and colour L; wire QF1 → QD1 → assert ends, sections
  and colours; a third wire on a terminal → refused with a hint; select a wire
  and delete it with the Delete key; undo; change a section in the properties
  panel; move a wired device → its wires end at the new terminal positions;
  delete a wired device and undo → wires back with their ids; refused 2P→1P
  with terminal 3 wired; Esc cancels a wire in progress, then turns the tool
  off. Screenshots at each step, including the terminal highlight, the wire in
  progress, the wired panel, a selected wire with its properties, a refused
  hint, the light theme, and the properties panel in Russian and Kazakh.
- **Translations:** the existing `translations_complete` test covers the new
  strings.
- **Smoke test** of the real binary under Xvfb, drawing a wire with `xdotool`
  clicks; screenshots inspected.
