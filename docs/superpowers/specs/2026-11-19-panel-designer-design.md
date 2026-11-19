# Electrical panel designer — product design

Date: 2026-11-19 · Working name: **Electricity** (binary `electricity`)

Architecture decisions: [docs/adr/README.md](../../adr/README.md).
Implementation order: [docs/roadmap.md](../../roadmap.md).

## Intent

An open-source desktop app in which a person with no electrical background
assembles a home distribution board (single-phase first, three-phase later)
from devices sold in Kazakhstan and the CIS, connects pictures of lamps,
sockets and appliances, and watches it behave realistically.

Why it exists, in the owner's words: learn how a panel works through the
tutorial, get a parts list, create a design for a future real installation,
and check that the protection works. It is a real production tool, but its
complexity is hidden in levels.

## Users and levels

1. **Beginner (tutorial).** Learns what each device does by building the
   apartment panel step by step.
2. **Regular user (default).** Designs a real panel through a simple,
   intuitive interface: drag, drop, wire, try it, get outputs. Never needs
   to dive deeper.
3. **Engineer.** Same window; uses the Settings menu ("Show engineering
   details"), context menus, detail lists and device settings to edit
   curves, wire lengths, supply parameters and so on.

## Goals

- Drag devices onto DIN rails in an enclosure and **wire every conductor by
  hand**, terminal to terminal, choosing a wire cross-section.
- Live simulation, always running: flip handles, switch loads, see currents.
  Breakers trip on overload (with realistic, time-accelerated delay) and
  short circuit (instantly); RCDs trip on leakage; **wires overheat** when
  current exceeds their rating and no breaker stops it.
- Simulation follows the wiring actually drawn, so mistakes behave as in real
  life (neutral bypassing the RCD → nuisance trip; load before the breaker →
  unprotected).
- Faults are triggered by right-clicking a load: short circuit, leakage,
  earth fault. Tripped devices are reset by clicking them.
- Outputs for a real installation: **parts list** (no prices), **panel
  printout**, **generated single-line diagram**.
- Save and open projects.
- UI, catalog and tutorial in Russian, Kazakh and English; dark theme by
  default.
- Single statically linked executable for Windows 10/11 and Linux.

## Non-goals

- Prices, shops, or ordering.
- Static design-rule warnings; "safety" means the simulated protection
  behaves like the real devices.
- Waveforms, transients, harmonics, selectivity studies, cable voltage-drop
  calculation beyond what the solver shows.
- Building wiring outside the panel (room plans, wall routing).
- Strict ГОСТ drawing compliance (frames, stamps).

## Anti-goals

- A basic flow that requires understanding engineering parameters.
- A simulation that says "all fine" for a design that would overheat a wire.
- Brand logos, manufacturer photos or copied catalog images.

## Constraints

- C++20, Qt 6.12.x static, CMake; Windows 10 support (no newer Qt minor
  without an ADR). GPLv3.
- Original SVG art, readable on dark theme and at small sizes.
- Headless-testable: GUI-free core, offscreen GUI tests with screenshots.

## Approaches considered

**How the user builds the panel** (owner chose):
- *A. Real panel, wired by hand* — **chosen**. Most realistic; mistakes
  become lessons; tutorial guides each wire.
- *B. Real panel, wired automatically* — simpler, but hides N-routing
  mistakes.
- *C. Diagram only* — simplest; doesn't look like a panel, no enclosure size.

**UI technology** (builder's call, CLAUDE.md delegates architecture):
- *Qt Widgets + Graphics View* — **chosen**: drag/selection/context menus
  built in, simplest static build, widget-based tests.
- *Qt Quick* — nicer animation, two languages, harder tests.
- *Hybrid* — both complexities.

**Simulation** (builder's call): steady-state complex nodal solve plus
thermal time steps, chosen over rule-based path tracing which fails on
parallel paths, shared neutrals and three-phase. See ADR 0008.

## Decided by the owner

- Purpose and three levels (above).
- No prices anywhere.
- Safety = protective devices open the circuit in simulation.
- Manual drag-and-drop placement and manual wiring.
- Wires have cross-sections and can overheat.
- Single-line diagram is generated.
- Simulation always live; faults via right-click; reset by click; time
  accelerated; tutorial panel composition; catalog with brands as text;
  single-phase complete before three-phase; save/open; PDF/PNG exports —
  proposed by the builder and confirmed by the owner ("looks right").

## Builder's calls (recorded in ADRs)

GPLv3 (repo already had it), Widgets/Graphics View, two-layer core/app,
phasor nodal solver, thermal constants, trip order rule, JSON catalog and
project files, `QUndoStack`, Linguist with English source, Fusion palette,
embedded DejaVu font, Qt Test, GitHub Actions, CSV in addition to PDF for
the parts list, CIS designations (QF/QD/QFD), N/PE bars pre-fitted in every
enclosure.

## Left to the builder of each feature

Exact visual layout of docks and toolbars, SVG drawings, animation details,
wire routing algorithm, tutorial wording, translation wording (Kazakh to be
reviewed by a native speaker when possible).
