# 0013. Dark theme via Fusion palette; SVG art rules

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The UI is dark by default. Native styles differ between Windows and Linux and
ignore custom palettes in places. Device pictures must stay readable on the
dark background and at small sizes (palette icons, zoomed-out panel).

## Decision

### Theme

- Use the **Fusion** style on all platforms with a custom `QPalette`. Two
  palettes: dark (default) and light; a menu item switches them at runtime.
- Colors are defined once in `src/app/theme.cpp` as named roles (window,
  base, text, accent, live, tripped, overloaded, overheating, wire colors)
  and used everywhere through that file. A small application style sheet is
  allowed only for things the palette cannot express.
- Simulation state colors are fixed across themes: live/energized green
  highlight, overloaded orange, overheating/tripped red. Conductor colors
  follow wiring convention: L brown (L1 brown, L2 black, L3 grey for
  three-phase), N blue, PE yellow-green.
- The theme choice and language persist in `QSettings` (INI format in the
  user config directory).

### SVG art

- Original drawings, generic devices, no logos or brand marks.
- One file per device type or load type, in `resources/svg/devices/` and
  `resources/svg/loads/`. Module width 18 mm maps to a fixed scene unit;
  device SVGs are drawn at 1 module = 18 × 90 units so geometry is exact.
- Devices are light grey/white bodies (as real DIN devices are), which read
  well on a dark enclosure background, with a dark outline (≥ 1.5 units) so
  they also hold on the light theme. Loads use flat shapes with a mid-tone
  fill and a light outline.
- No text baked into SVGs: ratings and labels are drawn by the app
  ([0009](0009-component-catalog.md)) so they translate and scale.
- Terminal positions are defined in the catalog, not inferred from the SVG.
- Each SVG must be legible at 32 px high (palette size); a GUI test renders
  every catalog SVG at 32 px into the screenshot folder for review.
- SVGs stay within SVG Tiny 1.2 features supported by Qt Svg (no filters,
  no CSS, no embedded raster).

## Rejected alternatives

- **Native platform styles** — inconsistent dark mode, especially on
  Windows 10.
- **Full Qt Style Sheet theme** — large, hard to maintain, and fights the
  palette; a palette covers almost everything for Fusion.
- **Raster PNG art** — blurry when zoomed, larger binary.

## Consequences

- One look on Windows and Linux; screenshots in tests are stable.
- Art can be drawn by hand in an SVG editor and checked in as text.
