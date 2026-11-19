# 0007. Electrical model: terminals, internal elements, wires

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The user wires everything by hand, terminal to terminal (product decision).
The simulation must follow the wiring actually drawn, including mistakes:
a neutral bypassing the RCD, phase wired to neutral, a load wired before its
breaker. So the model must describe physical connections, not intent.

## Decision

The project model (core) consists of:

- **Supply** — the feed into the apartment: system (single-phase `L N PE` or
  three-phase `L1 L2 L3 N PE`), nominal voltage (230 V phase / 400 V line,
  50 Hz), earthing system TN-C-S (N and PE bonded at the source), and
  source loop impedance (default 0.4 Ω, engineer-editable). The supply is
  drawn as a terminal block entering the enclosure.
- **Enclosure** — size in modules per row and number of rows (e.g. 12×1,
  24×2). Each enclosure comes with one **N bar** and one **PE bar**
  pre-fitted, as the plastic enclosures sold in Kazakhstan do. Bars are
  terminal strips (e.g. 8 terminals each).
- **Placed device** — a catalog device type plus position (row, module
  offset) and per-instance parameter overrides. A device has **terminals**,
  named as printed on real devices and defined in the catalog (a 1-pole
  breaker: `1` top, `2` bottom; a 2-pole breaker: `1`, `3` top and `2`, `4`
  bottom; a 2-pole RCD: `1`, `N` top and `2`, `N` bottom, stored as `N1`,
  `N2`), and **internal elements** between terminals:
  - *contact* (closed or open; poles of one device switch together);
  - *protection sensor* attached to contacts (thermal + magnetic for
    breakers, residual-current for RCDs, both for RCBOs);
  - for loads: *impedance* (from power and voltage).
- **Load** — a catalog load type (lamp, socket, appliance) with position on
  the canvas outside the enclosure, terminals `L N PE`, on/off state,
  power. A **socket** is a connection point: an appliance picture can be
  dropped onto a socket and draws its power through it.
- **Wire** — two terminal endpoints, cross-section (1.5/2.5/4/6/10/16 mm²),
  material (copper by default), length (default: 0.3 m inside the
  enclosure, 15 m for a run to a load; engineer-editable), color (derived
  from the role of the first terminal: L brown, N blue, PE yellow-green;
  editable), and route points for drawing.
- **Fault** — attached to a load: none / short circuit L–N / leakage L–PE
  (default 1 kΩ, about the resistance of a human body, ≈230 mA) / earth
  fault L–PE (bolted).

Every terminal has a stable id `"<object-id>:<terminal-name>"`. Object ids
are stable strings generated on creation and saved in the project file.

The model enforces **only physical constraints**: a device must fit on a rail
without overlapping; a wire must join two distinct terminals; a terminal
takes at most **two wires** (accepted practice for screw terminals;
device terminals and load pass-through terminals alike), and each N/PE bar
terminal takes one wire. So breakers are fed by daisy-chained jumpers
(QD1 → QF2 → QF3 → QF4) and sockets/lamps on one line are chained load to
load. A comb busbar arrives with the catalog extension (F16). It never
rejects an electrically wrong circuit; that is what the simulation shows.

## Rejected alternatives

- **Logical tree model (supply → RCD → breakers → loads)** — cannot represent
  wiring mistakes, which are a core teaching tool, and cannot give a real
  panel layout or enclosure size.
- **Wire as an abstract "cable" bundling L, N, PE** — simpler to draw but
  hides exactly the N-routing mistakes the app should show. Each conductor is
  a separate wire.

## Consequences

- A typical apartment panel has 20–40 wires; the editor must make drawing
  them fast (click terminal, click terminal).
- The single-line diagram has to be derived by tracing the wiring and may
  fail for strange topologies (see [0015](0015-exports.md)).
