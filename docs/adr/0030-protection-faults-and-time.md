# 0030. Protection, faults and simulated time

- **Status:** Accepted
- **Date:** 2026-11-23

## Context

F09 makes breakers and RCDs trip, wires heat, and loads fail on demand. The
simulation needs state that changes with time, a rule for when and why a
device trips, and a place for protection numbers. Design:
`docs/superpowers/specs/2026-11-23-f09-protection-faults-design.md`.

## Decision

This refines 0007, 0008, 0013, 0014 and 0029.

- Protection state (faults, trips, heat) lives in `Simulation`, keyed by id,
  like F08's switch settings: not undoable, not saved, kept after an object
  is removed so undo brings it back.
- A time step jumps exactly to each thermal trip, solves again and goes on,
  so a trip never lands late in a long step.
- Leakage goes from L to earth (the reference node), not to the load's PE;
  an earth fault joins L to the PE of the same object.
- The trip cause follows the curve's lower band: from `lower · In` a trip is
  a short circuit, below it an overload.
- A fault is dangerous when the touch voltage is above 50 V (ПУЭ 1.7.53).
- The stairwell breaker is hidden until it trips; a click on the supply
  resets it.
- Wire thermal constant is 120 s for every section and never below the
  breakers' 73 s.
- Earth fault is in the basic load menu, which refines 0014.
- The status bar shows the newest active condition when no hint is showing,
  and "Ready" when there is none.
- A bare L-N wire now trips a breaker or the stairwell breaker; F08's
  "hundreds of amperes" no longer holds.
- Protection data is optional in the loader (so small test catalogs stay
  valid); the embedded-catalog test checks that it is complete.

## Rejected alternatives

- **Protection state in the design model** — it would enter undo commands and
  the saved design.
- **Fixed time steps only** — a trip would be late by up to one step.
- **Leakage through the load's PE** — it would depend on wiring a person
  does not touch.

## Consequences

- Every new protected device type needs a `protection` entry; the
  embedded-catalog test fails without it.
- The wire time constant is data, so F12 can make it editable.
