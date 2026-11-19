# 0014. Complexity levels: one UI, details hidden behind context menus and an engineer setting

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The app serves three levels of user: a beginner in the tutorial, a regular
user designing a real apartment panel, and an electrical engineer who edits
detailed parameters. The basic flow must stay simple; complexity is allowed
only where it is needed, and even then is undesirable.

## Decision

- **One main window for everyone.** No separate "modes" with different
  layouts.
- **Basic surface (always visible):** device palette (only common types,
  grouped: Protection, Loads), the canvas, a compact properties panel with
  only the basics (device rating and poles, breaker curve shown read-only,
  wire section, length of a wire to a load, load power), the toolbar
  (wire tool with section selector, undo/redo, power on/off of the supply,
  time speed), and the status bar with simulation events.
- **Engineer details** (setting *Show engineering details*, off by default,
  in the Settings menu): adds to the properties panel editable breaker
  curve, thermal τ, length of wires inside the enclosure, wire material and
  ampacity, supply impedance and upstream
  breaker, cos φ, fault resistance; adds the full catalog in the palette;
  adds the event log and a per-object currents table.
- **Context menus** carry the actions relevant to the clicked object for all
  users (Delete, Properties, for loads: Short circuit / Leakage / Remove
  fault). Advanced entries appear only with engineering details on.
- **Tutorial** restricts the palette and canvas interaction to what the
  current step needs and highlights the target ([0017](0017-tutorial-engine.md)).
- Defaults are chosen so that a regular user never needs engineering details
  to get a correct, realistic design.

## Rejected alternatives

- **Separate beginner and expert applications or modes** — doubles UI
  surface and makes the jump from tutorial to real work abrupt.
- **Everything visible with collapsible sections** — still visually heavy for
  beginners.

## Consequences

- Each property is tagged basic or engineer in the properties panel code.
- GUI tests cover both states of the setting for the properties panel.
