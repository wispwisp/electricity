# 0027. Wires: terminal roles, colours and automatic routes

- **Status:** Accepted
- **Date:** 2026-11-22

## Context

F06 lets the user connect terminals with wires. ADR 0007 sketched wires with
route points, ADR 0013 fixed the conductor colours and ADR 0023 defined the
catalog. Several questions were left open: what colour a new wire gets, where
the supply terminals come from, whether routes are stored, and what happens to
wires when a device or the enclosure changes under them.

## Decision

This refines 0007, 0013 and 0023.

- Terminals carry an optional conductor role (L, N, PE) as catalog data, and
  the supply's terminals are in the catalog too (`supplies`). Bars have a role.
  Poles that real devices don't mark (2P breaker) have none.
- A wire's colour is its conductor, set at creation and stored on the wire:
  the role of the first clicked end if it has one, else the role of the other
  end, else L.
- Routes are computed on every change by a GUI-free router in `panelcore` and
  are never stored. So 0007's "route points" and 0010's file carry no route.
- A second wire between the same two terminals is refused. A device or supply
  terminal holds two wires, a bar hole one.
- Deleting a device deletes its wires, in one undo step.
- A pole change or an enclosure change that would leave a wire on a missing
  terminal (a device terminal, or a bar hole) is refused.

## Rejected alternatives

- **The first clicked terminal's role only** — 2P poles carry none, so those
  wires would always be L.
- **Colour traced from the supply** — it changes under the user, and an
  unconnected wire would have none.
- **Stored and editable route points** — a feature nobody asked for, and the
  points go stale after moves.

## Consequences

Saved files carry wires as terminal pairs with section, conductor and length,
and stay valid when the layout changes. The user must unwire a terminal before
removing it by a pole or enclosure change. Routing code can change without a
file migration.
