# 0010. Project file format: versioned JSON (`.panel`)

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Users save designs for future installation and reopen them later, possibly
in a newer app version. Files should be diffable and debuggable.

## Decision

- A project is one UTF-8 JSON file with extension `.panel`, written with
  `QJsonDocument` (indented).
- Top level: `{"format": "electricity-panel", "version": 1, "supply": {...},
  "enclosures": [...], "devices": [...], "loads": [...], "wires": [...],
  "view": {...}}`.
- Objects reference catalog types by stable string id (`"breaker"`,
  `"kettle"`) and store only parameter overrides
  ([0009](0009-component-catalog.md)). Wires reference terminal ids
  (`"<object-id>:<terminal>"`).
- Simulation state (switch positions, tripped devices, faults, heat) is
  **not** saved, except the intended position of each switch/handle. Opening
  a file starts with all protective devices reset.
- Loading: unknown fields are ignored; a newer `version` than supported shows
  an error; older versions are migrated in code, one function per version
  step, each covered by a test with a fixture file in `tests/core/data/`.
- Saves are atomic (`QSaveFile`).

## Rejected alternatives

- **Binary / `QDataStream`** — not diffable, brittle across versions.
- **SQLite** — overkill for one document; new module.
- **XML** — more verbose, no benefit over JSON for Qt.

## Consequences

- Hand-editing files is possible and useful for test fixtures.
- Every model change that affects saved data needs a format version check.
