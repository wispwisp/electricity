# 0011. Undo/redo with QUndoStack commands for edits only

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Beginners make mistakes while placing devices and drawing wires; undo is
expected in any editor. Simulation actions (flipping a breaker, switching a
lamp, injecting a fault) are experiments, not edits.

## Decision

- Every **design edit** is a `QUndoCommand` pushed onto one `QUndoStack`
  per open project (in the app layer): add/move/remove device, add/remove
  wire, change wire section or route, add/move/remove load, change any
  parameter, change supply.
- Commands call core model methods; the scene updates from model signals
  ([0006](0006-core-and-app-layering.md)). Commands never touch scene items
  directly.
- Continuous drags merge into one command (`mergeWith`).
- **Simulation actions are not undoable**: switch positions, load on/off,
  faults, resets.
- The stack's clean state drives the "unsaved changes" marker.
- Ctrl+Z / Ctrl+Shift+Z (and Ctrl+Y on Windows) via `QKeySequence::Undo` /
  `Redo`.

## Rejected alternatives

- **Snapshot the whole model per edit** — simple but memory-heavy and makes
  merging drags awkward; commands are idiomatic Qt.
- **Undo inside core** — core would need a command framework duplicating
  `QUndoStack`, which lives in Qt Gui.

## Consequences

- Every new editing feature must ship its command and an undo/redo test.
