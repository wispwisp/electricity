# 0028. Loads: plugging by reference, outlines, cables

- **Status:** Accepted
- **Date:** 2026-11-22

## Context

F07 adds loads: lamps, sockets, appliances and a cooktop, drawn as pictures
outside the enclosure. Wired loads connect to the panel with wires; plug-in
appliances go into a socket. The model has to say how a plug-in is stored, how
loads are named, where they may stand, and what happens when a socket or the
enclosure changes. Design: `docs/superpowers/specs/2026-11-22-f07-loads-design.md`.

## Decision

This refines 0007, 0009, 0010 and 0023.

- A placed load stores its type, position, full power (as ADR 0024 stores full
  ratings), and its socket and place (1 or 2). Ids are `l<n>` from a counter
  that only grows; terminals are `l<n>:L`, `l<n>:N`, `l<n>:PE`, capacity 2.
- A plugged appliance is stored by reference. Its position follows the socket's
  position and plug place; it has no wires of its own. Hidden cord wires were
  rejected.
- Names are derived from placement order per type ("Lamp 1") and never stored.
- Load and enclosure picture sizes are catalog data, so the model checks the
  enclosure and the load outlines. The supply area is checked by the scene,
  because its text width depends on the language. Overlap is a positive-area
  intersection.
- A socket's outline reserves both plug places, so nothing else stands where an
  appliance would hang. Plugged appliances take part only in the plug rules.
- Deleting a socket unplugs its appliances, which stay where they hung.
- A cable is the wires whose ends lie on the same two sides, a side being a
  load or the panel; one side must be a load. A new wire with a load end starts
  at 15 m, or takes its cable's length. Length stays per wire; a cable object
  was rejected as another entity to store and undo.
- Loads below the enclosure follow its bottom edge when the enclosure size
  changes; a shrink that makes loads collide is refused.

## Rejected alternatives

- **Hidden cord wires for plug-ins** — extra wires to store, route and hide.
- **Stored cable object** — a second source of truth for lengths.

## Consequences

- Moving a socket moves its appliances for free, and undo needs no extra state.
- The scene must add the supply-area check to the model's placement result.
- Enclosure size changes now depend on loads.
