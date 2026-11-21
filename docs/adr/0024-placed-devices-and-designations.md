# 0024. Placed devices and designations

- **Status:** Accepted
- **Date:** 2026-11-21

## Context

F05 puts devices on DIN rails. ADR 0023 left open what baseline a placed
device's overrides are measured against, and ADRs 0009 and 0010 say a placed
device "stores only overrides". The editor also needs stable ids for undo and
automatic designations (QF1, QD1, ...).

## Decision

- A placed device stores its catalog type, poles, its full rating parameters
  and its position (row, module). This answers ADR 0023's open baseline
  question and refines the "stores only overrides" wording of 0009 and 0010 for
  ratings. Later engineering constants (τ etc.) remain type defaults with
  overrides.
- Ids are `d<n>` from a counter that only grows, so an id is never reused in a
  session. Undo restores a deleted device under its old id.
- Designations are derived from position, per prefix, ordered by row and then
  module, and are never stored. The prefixes are catalog data (`designation`
  on each device type, new in schema v1): `QF`, `QD`, `QFD`.
- The model emits one `changed()` per mutation and the scene re-syncs all items.
- The scene places rows on the enclosure picture's `rail-<n>` elements and bars
  on `nbar-mount`/`pebar-mount` (ADR 0023 ids).

## Rejected alternatives

- **Store only rating overrides against a preset** — presets can change
  between releases, which would silently change saved panels.
- **Store designations** — they would need renumbering on every move or delete
  and could disagree with the positions.
- **Fine-grained change signals** — more code for no visible gain at this size.

## Consequences

Saved files carry full ratings and are self-contained. Renumbering after a
move or delete is free. Every mutation re-syncs the whole scene, which is
acceptable for panels of a few dozen devices.
