# 0008. Simulation engine: steady-state phasor nodal solve plus thermal time steps

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Simulation is "logical": switching, summed load currents, breaker trips on
overload and short circuit, RCD trips on leakage, wire overheating. No
waveforms or transients. But wiring is arbitrary and user-drawn
([0007](0007-electrical-model.md)): parallel paths, a neutral shared between
RCD groups, phase-to-neutral shorts, three-phase loads with neutral current.
Rule-based path tracing ("find the path from supply to each load and add its
current to every device on the path") breaks on exactly those cases: parallel
paths split current, a shared neutral splits return current between RCDs,
three-phase neutral current is a phasor sum, and a short has no "load" to
trace.

## Decision

### Electrical solve (steady state, 50 Hz)

Each solve is a linear AC nodal analysis with complex phasors
(`std::complex<double>`):

- **Nodes:** every terminal. The reference node is the supply star point
  (N and PE bonded there, TN-C-S).
- **Branches** (admittances):
  - wire: `R = ρ·L / S`, copper `ρ = 0.0175 Ω·mm²/m`;
  - closed contact: 1 mΩ; open contact: no branch;
  - load: `Z = U² / P` (resistive; engineer can set cos φ for motors);
    switched-off load: no branch;
  - short-circuit fault: 1 mΩ L–N; leakage fault: 1 kΩ L–PE (default);
    earth fault: 1 mΩ L–PE;
  - supply: each phase terminal is a Norton source, `I = E/Zs` injected with
    admittance `1/Zs`, `E = 230 V ∠ 0°, −120°, +120°`; supply N and PE
    terminals tie to the reference through 1 mΩ.
  - `gmin = 1e−9 S` from every node to reference, so floating parts of the
    circuit do not make the matrix singular.
- **Solver:** dense Gaussian elimination with partial pivoting. Panels have
  up to a few hundred terminals, so a solve takes well under a millisecond.
  No sparse library.
- **Results:** node voltages, current in every branch, per-device pole
  currents, per-load voltage and "energized" flag (|U| > 0.5·Un).

### Protection and heating (time-stepped)

The core exposes `step(seconds)`, called with *simulated* seconds. The app
drives it from a 50 ms `QTimer` with a time-acceleration factor (default
×10, so an overload that trips in 40 s real time trips in about 4 s on
screen). The core never reads the wall clock, so tests are deterministic.

- **Breaker, magnetic (short circuit):** trips immediately when any pole
  current ≥ `m · In`, with `m` the upper bound of its curve: B = 5, C = 10,
  D = 20 (the guaranteed-trip point).
- **Breaker, thermal (overload):** heat state `H` (A²) with
  `dH/dt = (I² − H) / τ`, trip when `H ≥ (1.3 · In)²`. 1.3 is between the
  standard no-trip (1.13·In) and must-trip (1.45·In) currents. Default
  `τ = 73 s` (≈40 s to trip at 2·In). `τ` and the factors come from the
  catalog and can be changed in engineer mode. `H` keeps its value after a
  trip and cools by the same equation, so closing a hot breaker on a
  persisting overload trips it sooner, as in real life.
- **RCD:** trips immediately when the residual current (phasor sum of all
  its pole currents, top to bottom) ≥ `IΔn`. An RCD has no overload
  protection. If its current stays above its rated `In`, it gets the same
  heating model as a wire and shows "overloaded", but does not trip.
- **RCBO:** breaker + RCD behaviour in one device.
- **Wire:** same heat model with its ampacity `Iz`
  ([0016](0016-electrical-reference-data.md)) and `τ` from the catalog.
  States: *normal* (`H ≤ Iz²`), *overloaded* (`H > Iz²`, orange),
  *overheating* (`H ≥ (1.45 · Iz)²`, red, warning in status bar). Wires
  never open the circuit; they only warn.
- **Supply upstream breaker:** the supply includes the stairwell breaker
  (default C50, engineer-editable). It trips like any breaker, so a short
  with no breaker in the panel still ends, and the user sees the whole
  apartment go dark plus an overheating wire.

### Order of trips

After each solve, collect devices whose instant condition holds. Trip every
triggered RCD and the triggered breaker with the smallest `In` (ties: the one
whose current is largest), then solve again. Repeat until nothing triggers
(at most 32 rounds). This makes the closest, smallest breaker trip first on a
short, which is the behaviour the tutorial teaches. Thermal trips are
checked after each `step`.

### Events

Every state change produces an event (`device tripped`, `cause: overload |
short circuit | leakage`, measured current, rating). The app shows events in
the status bar and the event log; the tutorial waits on them.

### When to solve

After every model change (wire, switch, fault, load on/off) and on every
`step`. No caching until profiling shows a need.

## Rejected alternatives

- **Rule-based path tracing** — fails for parallel paths, shared neutrals,
  three-phase neutral current and shorts (see Context). Its special cases
  would outgrow the solver.
- **DC analysis with real numbers** — simpler, but three-phase current
  sums are wrong without phase angles. Complex numbers cost nothing extra in
  code size.
- **Full transient SPICE-like simulation / ngspice** — new dependency, far
  beyond "logical" simulation, slow to explain.
- **Instant trip on any overload** — hides the real behaviour (breakers
  tolerate small overloads for minutes) and the risk of an oversized
  breaker overheating a wire.

## Consequences

- Short-circuit currents depend on supply impedance and wire lengths and
  will be in the hundreds to thousands of amperes, so magnetic trips are
  realistic without special cases.
- The core needs thorough unit tests per behaviour: overload timing, short,
  leakage, shared-neutral RCD nuisance trip, neutral bypassing RCD, missing
  breaker, three-phase neutral current.
- "Engineer mode" parameters (`τ`, curve, `Zs`, wire length, cos φ) plug
  directly into the solve; no second model.
