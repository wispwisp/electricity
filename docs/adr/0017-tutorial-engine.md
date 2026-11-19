# 0017. Tutorial engine: data-driven steps with model predicates

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The app starts with a tutorial that teaches a beginner to assemble the
single-phase apartment panel ([0016](0016-electrical-reference-data.md)).
It must work in three languages, be testable headless, and not fork the
editor into a special mode.

## Decision

- The tutorial is a list of steps in `resources/tutorial/apartment.json`.
  Each step has: `id`, `title` and `text` (English source strings, translated
  per [0012](0012-internationalization.md)), `highlight` (palette entry,
  object id, or terminal id), `allow` (which interactions are permitted:
  e.g. place only `breaker`, draw wire only between two given terminals,
  click only `QF1`), and `done` (a named condition).
- **Conditions** are C++ predicates in core, registered by name
  (`deviceAt("breaker", row 0, module 4, {In:10})`, `wireBetween(a, b,
  section)`, `loadEnergized(id)`, `eventOccurred("tripped", "QF4",
  "overload")`). A step completes when its condition becomes true after any
  model change or simulation event.
- The tutorial runs in the **normal editor** with an overlay panel (step
  text, Back/Next, Skip tutorial) and a highlight ring drawn on the scene or
  palette. Disallowed actions are ignored with a short hint.
- Steps that need prior steps' results can **auto-complete** them when the
  user jumps back or restarts: each step has an optional `setup` that brings
  the model to its starting state (a fixture project).
- First run opens language choice then the tutorial. It can be reopened from
  Help → Tutorial. Completion is stored in `QSettings`.
- Step outline:
  1. What a panel is; the enclosure with N and PE bars.
  2. Place the main breaker QF1 (2P C40) — what a breaker does.
  3. Place the RCD QD1 (40 A/30 mA) — what leakage is and why 30 mA.
  4. Place QF2 C10, QF3 C16, QF4 C16 — why ratings match wire sections.
  5. Wire the feed: supply L and N into QF1 (10 mm²), PE to the PE bar.
  6. Wire QF1 → QD1.
  7. Wire QD1 → QF2 top, then jumpers QF2 → QF3 → QF4 (two wires per
     terminal at most); QD1 N → N bar.
  8. Place loads and run lines (L from breaker, N from N bar, PE from PE
     bar) with the right sections; the second load on a line is chained
     from the first.
  9. Power on: flip QF1, QD1, each breaker; switch on lamps; watch currents.
  10. Overload: plug the washing machine into the second kitchen socket → QF4
      trips after a few seconds. Unplug it, reset QF4.
  11. Short circuit on a room socket → QF3 trips instantly. Remove fault,
      reset.
  12. Leakage on the kettle → QD1 trips. Remove fault, reset.
  13. Summary; open the parts list; save the project.

## Rejected alternatives

- **Hard-coded tutorial in C++ UI code** — every text change touches logic;
  hard to test step by step.
- **Video or static images** — not interactive, three translations of media.
- **Sandbox mode separate from the editor** — users would have to relearn
  the real editor after the tutorial.

## Consequences

- The editor needs an "allow" filter hook for palette, scene and toolbar
  actions; normal use passes everything.
- Each step's condition is unit-tested in core with fixture projects, and the
  whole tutorial is walked through by one GUI scenario test.
