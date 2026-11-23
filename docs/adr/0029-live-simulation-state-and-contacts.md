# 0029. Live simulation: state beside the design, contacts in the catalog

- **Status:** Accepted
- **Date:** 2026-11-23

## Context

F08 makes the circuit live: handles and loads switch, currents are solved, the
ceiling lamp glows. The simulation needs somewhere to keep switch settings, a
way to know which terminals of a device a closed handle joins, and a network
model. Design: `docs/superpowers/specs/2026-11-23-f08-live-simulation-design.md`.

## Decision

This refines 0007, 0008, 0011, 0014 and 0023.

- Switch settings live in a core `Simulation` beside `Project`, keyed by object
  id. A missing entry means off, so everything starts off. They are not
  undoable and not stored in `PlacedDevice` or `PlacedLoad`, because undo
  commands copy those whole. Entries are kept after an object is removed, so
  undo restores the setting.
- Contacts are per-variant catalog data: `"contacts"` lists `[top, bottom]`
  terminal pairs a closed device joins. No contacts means the variant never
  conducts.
- The network: the bars are one conductor each; sockets and `PE` terminals have
  no branch; a plug-in draws through its socket's L and N; loads are fixed
  resistances, so solved currents come out a few percent under P/U.
- The solve runs synchronously after every change. There is no timer until F09.
- There is no supply power button, which refines the toolbar list of 0014. QF1
  switches the apartment.
- A load type's `light` flag picks the warm glow; only the ceiling lamp has it.

## Rejected alternatives

- **State in the design model** — it would enter undo commands and the saved
  design.
- **A contact naming rule in code** — terminal names would carry meaning that
  belongs in data.
- **A second "off" picture per variant** — the handle moves on one picture.
- **Constant-power loads** — non-linear, and wrong for heaters.

## Consequences

- Currents are a few percent under P/U, which later tasks must account for
  (F09 thresholds).
- F10 must save handle positions from the simulation state.
- Adding a device variant means writing its contacts in the catalog.
