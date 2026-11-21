# 0025. Palette drag-and-drop and headless drop tests

- **Status:** Accepted
- **Date:** 2026-11-21

## Context

F05 places devices by dragging a preset from the palette onto the canvas
(ADR 0005). Palette presets are not selectable, and `QAbstractItemView` only
drags selected items. GUI tests run on the offscreen platform (ADR 0018),
where Qt 6.12's `QOffscreenDrag::drag()` returns `IgnoreAction` at once
(`src/plugins/platforms/offscreen/qoffscreencommon.h`), so no drag events
ever reach a drop target. A preset dropped near the end of a rail could
also hang a wide device off the rail.

## Decision

- The palette starts the `QDrag` itself, from its mouse press and move
  handlers, once the cursor has moved `QApplication::startDragDistance()`.
  The drag has the MIME type `application/x-electricity-preset`, whose data
  is the preset id in UTF-8, the preset's picture as the pixmap, and
  `Qt::CopyAction`. `PaletteWidget::mimeData()` builds that data.
- The scene accepts every drag enter and move that carries a known preset,
  even where the device doesn't fit, so the drop always arrives. While the
  drag is over the scene, a translucent preview shows the slot, red where
  the device doesn't fit. The scene decides at the drop: it places the
  device through the undo stack, or says why not in the status bar.
- GUI tests send the canvas viewport the `QDragEnterEvent`,
  `QDragMoveEvent`, `QDragLeaveEvent` and `QDropEvent` that Qt would
  deliver, built from the palette's own `mimeData()`
  (`tests/gui/editorhelpers.h`). The real drag is checked with `xdotool`
  under Xvfb in the smoke run.
- The slot under the cursor is clamped to the rail: near a rail end the
  device snaps onto the end modules instead of running past them.

## Rejected alternatives

- **Make presets selectable and use `QAbstractItemView`'s drag** — a
  selected preset would look like a choice the user made, and the palette
  has nothing to select.
- **Home-made drag** (a palette press starts a preview that follows the
  mouse) — drivable with plain mouse events in tests, but it reimplements
  Qt's drag-and-drop and loses the native cursor, Esc to cancel and drops
  from an undocked palette.
- **Accept only moves where the device fits** — the drop would then not
  arrive where it is refused, and the status bar could not say why.
- **Refuse past the rail end** — the user would have to aim at the exact
  module near an end; snapping onto the end modules is friendlier.
  `PastRailEnd` still applies when a pole change would widen a device past
  the end.

## Consequences

The headless tests cover the scene's drop handling and the palette's drag
data, but not the platform drag itself, so the smoke run has to exercise a
real drag. A later drag source (loads in F07) can reuse the same pattern
with its own MIME type.
