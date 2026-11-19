# 0006. Two layers: GUI-free core library and Widgets app

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Simulation logic must be unit-testable quickly and deterministically, without
a display. Project rules require simulation and model code to live in a core
library with no GUI dependencies.

## Decision

Two CMake targets plus tests:

- **`panelcore`** (static library, `src/core/`): links only `Qt6::Core`.
  Contains:
  - the project model: enclosure, rails, placed devices, loads, wires,
    supply (see [0007](0007-electrical-model.md));
  - the component catalog loader (see [0009](0009-component-catalog.md));
  - the simulation engine (see [0008](0008-simulation-engine.md));
  - project file serialization (see [0010](0010-project-file-format.md));
  - generation of the parts list and single-line diagram as data structures
    (not drawings) (see [0015](0015-exports.md));
  - tutorial step conditions (predicates over the model).
  No `QWidget`, `QPainter`, `QPixmap` or other Gui types. `QObject` signals
  are allowed for change notification.
- **`electricity`** (executable, `src/app/`): links `panelcore`, Gui,
  Widgets, Svg, SvgWidgets, PrintSupport. Contains windows, the Graphics View
  editor, undo commands, rendering, export drawing, tutorial overlay, theme,
  translations loading.
- **Tests:** `tests/core/` link only `panelcore` + `Qt6::Test`;
  `tests/gui/` link the app's sources as a library (`electricity_lib`), so the
  executable's `main.cpp` is just a few lines.

Dependency direction is strictly app → core. The core never includes app
headers.

## Rejected alternatives

- **Pure standard C++ core with no Qt at all** — would need a JSON library
  (a new dependency) and string conversions at every boundary. Qt Core is
  already in the binary and GUI-free.
- **Single target** — simulation tests would need a GUI and become slow and
  fragile.
- **More layers (domain / services / adapters)** — no second front end
  exists; extra layers add indirection without benefit.

## Consequences

- Core model changes are notified to the scene via `QObject` signals; the
  scene updates items, never the other way round.
- Logic that is tempting to write inside a `QGraphicsItem` (e.g. "is this
  wire overloaded?") belongs in core and is read by the item.
