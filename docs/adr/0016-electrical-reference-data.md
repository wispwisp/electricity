# 0016. Electrical reference data for Kazakhstan, and the tutorial panel

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Defaults must reflect residential practice in Kazakhstan, governed by ПУЭ РК
(Правила устройства электроустановок Республики Казахстан) and СП РК
4.04-106 (electrical equipment of residential buildings), which follow the
same structure and values as the Russian ПУЭ 7th edition for residential
wiring. During research (2026-11), the common rules below were confirmed by
Russian and Ukrainian sources (СП 256.1325800, manufacturer installation
leaflets), not yet by the ПУЭ РК text itself. The project owner is not an
electrical expert and learns from the tutorial, so defaults must be
conservative and conventional.

## Decision

### Grid

- Single-phase 230 V, three-phase 400 V (230 V phase), 50 Hz (ГОСТ 29322).
- Earthing system TN-C-S: separate N and PE in the apartment. Older TN-C
  buildings (no PE) are out of scope for the tutorial; an engineer can model
  them by not connecting PE.
- Copper conductors only by default (ПУЭ requires copper in residential
  wiring).

### Wire ampacity defaults (copper, PVC insulation, hidden in conduit)

From ПУЭ table 1.3.4/1.3.5, "two single-core wires in one pipe" column, the
conservative case for a hidden run. Engineer-editable per wire.

| Section, mm² | 1.5 | 2.5 | 4 | 6 | 10 | 16 |
|---|---|---|---|---|---|---|
| Iz, A | 19 | 27 | 38 | 46 | 70 | 85 |

Values must be checked against the official ПУЭ РК text when the catalog is
implemented (roadmap feature F04); any correction updates this ADR's table
by superseding ADR.

### Usual pairings (shown as hints in the wire tool and used by the tutorial)

| Circuit | Breaker | Wire |
|---|---|---|
| Lighting | C10 | 1.5 mm² |
| Sockets | C16 | 2.5 mm² |
| Cooktop | C32 | 6 mm² |
| Apartment feed | 2P C40 | 10 mm² |

Sockets are protected by an RCD with IΔn = 30 mA (ПУЭ 7.1.79).

### Tutorial panel (single-phase apartment)

Enclosure: surface-mounted, 12 modules, 1 row, with pre-fitted N and PE bars.

| Pos | Device | Rating | Purpose | Outgoing wire |
|---|---|---|---|---|
| 1 | Main breaker 2P (QF1) | C40 | switches off the whole apartment, protects the feed | feed 10 mm² from supply |
| 2 | RCD 2P (QD1) | 40 A / 30 mA, type A | protects people from leakage | — |
| 3 | Breaker 1P (QF2) | C10 | lighting | 1.5 mm² |
| 4 | Breaker 1P (QF3) | C16 | room sockets | 2.5 mm² |
| 5 | Breaker 1P (QF4) | C16 | kitchen sockets | 2.5 mm² |

Wiring path: supply L/N → QF1 → QD1 → QF2 top, then daisy-chained jumpers
QF2 → QF3 → QF4 (at most two wires per terminal,
[0007](0007-electrical-model.md)); QD1 N out → N bar; PE from supply → PE bar. All three lines behind the RCD keeps
one N bar and is an accepted modern practice; separating lighting from the
RCD is shown later as an engineer example, not in the tutorial.

Loads in the tutorial: 2 ceiling lamps on QF2; 2 sockets on QF3 with a TV
and an air conditioner; 2 sockets on QF4, the first with a kettle and a
microwave, the second free. Each catalog socket is double: at most two
appliances plugged in.

Demonstrations: switch on everything (normal current), plug a washing
machine into the second kitchen socket → overload trip of QF4; short circuit on a
socket → instant trip of QF3; leakage on the kettle → RCD trip.

Voltage relays (very common in Kazakhstan because of grid surges) and meters
are in the catalog later, not in the tutorial.

## Rejected alternatives

- **Separate RCD per group or RCBOs everywhere** — more correct for
  selectivity, but more devices and wiring for a first lesson.
- **Main switch-disconnector instead of main breaker** — common too, but a
  breaker teaches protection better and keeps device kinds fewer.
- **Including the meter** — in most Kazakh apartment buildings the meter is
  in the stairwell panel.

## Consequences

- If ПУЭ РК values differ from the table, catalog defaults and tutorial text
  change together.
- Kettle 2000 W + microwave 1200 W ≈ 14 A on a 16 A line; adding a washing
  machine 2200 W ≈ 23.5 A (1.47·In) gives a visible overload: trip in about
  2 min of simulated time, about 11 s on screen at ×10
  ([0008](0008-simulation-engine.md)).
