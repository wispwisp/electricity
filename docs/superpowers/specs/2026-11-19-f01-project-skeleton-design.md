# F01 — Project skeleton, test infrastructure, dark main window

Date: 2026-11-19 · Status: approved by the owner

Roadmap item: [F01](../../roadmap.md#f01-project-skeleton-test-infrastructure-dark-main-window).
Product design: [2026-11-19-panel-designer-design.md](2026-11-19-panel-designer-design.md).

## Intent

Lay the foundation every later feature builds on: an empty, dark main
window built as one static binary, with every test layer (core unit test,
offscreen GUI test with screenshots, Xvfb smoke test) and every quality
gate already working. The repository has no code yet.

## Goals

Exactly the roadmap's F01 list:

- CMake project per [0003](../../adr/0003-toolchain.md): `panelcore`
  (static library, Qt Core only), `electricity_lib` (app sources),
  `electricity` (executable, a few lines of `main`),
  `cmake/QtVersion.cmake` failing on any Qt other than 6.12.0,
  `CMakePresets.json` with `dev`, `asan`, `release`.
- Main window: empty `QGraphicsView` canvas, empty palette dock, status bar;
  Fusion style with the dark palette from `src/app/theme.cpp`
  ([0013](../../adr/0013-dark-theme-and-svg-art.md)); embedded DejaVu Sans
  Regular and Bold as the application font, license text in
  `resources/fonts/LICENSE-DejaVu.txt` ([0004](../../adr/0004-dependencies.md)).
- Test infrastructure: one trivial core test; one GUI test using the
  offscreen plugin and a shared `saveScreenshot(test, step)` helper
  ([0018](../../adr/0018-testing-and-quality-gates.md)).
- `scripts/quality_gates.sh` running the six gates of 0018, `.clang-format`,
  `.clang-tidy`.
- SPDX headers on every source file ([0002](../../adr/0002-license-gplv3.md)).

## Non-goals

- No menus yet: Language and Settings menus come in F03, File in F10.
- Dark palette only; the light theme and its switch come in F03.
- No translations, catalog, SVG art or canvas content.
- No CI workflow (F02).

## Constraints

- Static Qt 6.12.0 at `/opt/qt` in the container.
- Warnings as errors on project targets.
- Linux platform plugin: `xcb` only ([0004](../../adr/0004-dependencies.md)).
  The installed Qt also has a Wayland plugin; it is not linked. The app runs
  on Wayland desktops through XWayland, and the binary's dynamic
  dependencies stay those listed in 0004.

## Approaches

There is one sensible way. ADRs 0003–0006, 0013 and 0018 already fix the
stack: Qt Widgets with a Graphics View canvas, a GUI-free core plus a
Widgets app, the Fusion style with a custom palette, and Qt Test with
offscreen GUI tests. Alternatives (Qt Quick, a single target, native styles,
another test framework) were rejected in those ADRs.

## Decisions (builder's calls, approved by the owner)

- Window title "Electricity" (working name); default size 1280×800.
- Canvas fills the centre with a plain dark background.
- An empty dock titled "Components" on the left, filled in F04.
- Status bar shows "Ready".
- Build directories: `build/` (`dev`, matching the commands in CLAUDE.md),
  `build-asan/`, `build-release/`.
- `.gitignore`: the Qt template line `*build-*` also matched
  `docs/adr/0019-build-ci-distribution.md`, so that ADR was never committed.
  Narrow it to `/build-*/` and commit ADR 0019 with this feature.

## Done when

- Scenario test passes: the window launches offscreen; it asserts the window
  title, the dark palette window colour, and the presence of `canvasView`
  and `paletteDock`; it saves screenshot `f01-main-window`.
- The full suite passes and `scripts/quality_gates.sh` is green.
- The `release` preset binary's `ldd` shows no Qt libraries.
- The Xvfb smoke screenshot of the real binary shows the dark window.
- Screenshots have been inspected; F01 marked ☑ in the roadmap.

## Left to the builder

Exact dark palette colours, the content of the trivial core test, CMake and
source layout details within ADR 0003, and the `.clang-tidy` check list
within ADR 0018.
