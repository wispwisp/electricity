# 0026. Redo shortcuts and undo texts

- **Status:** Accepted
- **Date:** 2026-11-21

## Context

[0011](0011-undo-redo.md) says Redo is Ctrl+Shift+Z, plus Ctrl+Y on Windows
only, and leaves the Undo/Redo menu texts open. `QUndoStack::createUndoAction`
appends the command name ("Undo Add device"), and those names are set when the
command is created, so they would stay in the old language after a language
switch. The app also has Delete, which must not fire while the user types in a
properties field.

## Decision

- Redo is on Ctrl+Shift+Z and Ctrl+Y on every platform; Undo uses
  `QKeySequence::Undo`.
- The Undo and Redo actions are our own, plain "Undo" and "Redo" with no
  command names, enabled from `canUndoChanged` / `canRedoChanged`. Nothing
  stale remains after a language switch.
- Delete is a widget shortcut (`Qt::WidgetShortcut`) on the canvas, so it is
  active only while the canvas has focus. It is also in the Edit menu and in
  the device context menu, and is enabled only while a device is selected.

## Rejected alternatives

- **`createUndoAction` / `createRedoAction`** — command names in the texts
  go stale on a language switch.
- **Window-wide Delete shortcut** — it would delete a device while the user
  edits text in the properties panel.

## Consequences

- Refines 0011: the Redo shortcut is the same on all platforms.
- Menus and toolbar show no hint of what will be undone.
