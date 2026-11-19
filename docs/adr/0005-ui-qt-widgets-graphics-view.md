# 0005. UI: Qt Widgets with a Graphics View editor canvas

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The main screen is a 2D editor: an enclosure with DIN rails, modular devices
dragged onto the rails, wires drawn terminal to terminal, load pictures placed
around the panel and wired to it. Alongside it: a device palette, a
properties panel, context menus, a parts-list table and dialogs. The
simulation updates the picture live: handle positions, glowing devices,
red wires, current labels. GUI tests must drive the real UI with
`QTest::mouseClick` and find widgets by `objectName`. The build is static.

## Decision

We use **Qt Widgets** for the application shell and **Qt Graphics View**
(`QGraphicsScene` / `QGraphicsView`) for the editor canvas.

- One `QGraphicsScene` holds the enclosure, rails, device items, wire items
  and load items. Items are thin views over core model objects; they never
  own electrical state.
- Device and load pictures are SVG files rendered with `QSvgRenderer`
  (shared per file), drawn by custom `QGraphicsItem`s that add terminals,
  handle state and highlight overlays.
- Interaction (drag from palette, snap to rail, wire drawing, context menus,
  click to toggle) is implemented in scene/item event handlers.
- The view supports zoom (wheel, Ctrl+0 to fit) and pan (middle button or
  Space+drag).
- Every interactive widget has a stable `objectName`; scene items expose a
  stable id (`data(0)`) so GUI tests can locate them and click their scene
  positions mapped to the viewport.
- The same scene rendering is reused for PNG/PDF export via
  `QGraphicsScene::render`.

## Rejected alternatives

- **Qt Quick / QML** — better animation and a modern look, but two
  languages joined by a bridge layer, QML plugin import boilerplate in the
  static build, and GUI tests that reach into QML item trees. Property-heavy
  screens (engineer settings, tables) take more effort. Its benefits are
  mostly cosmetic for this app.
- **Widgets shell with a `QQuickWidget` canvas** — the complexity of both
  stacks for a small visual gain.
- **Custom `QWidget::paintEvent` canvas** — reimplements selection, hit
  testing, z-order and scene-to-view transforms that Graphics View already
  provides.

## Consequences

- Animations (handle flip, glow) are done with `QVariantAnimation` /
  `QTimer` on items; keep them short and simple.
- Qt Quick modules in the container's Qt build stay unused and are not
  linked.
