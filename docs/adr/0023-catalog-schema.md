# 0023. Catalog schema v1, presets and picture conventions

- **Status:** Accepted
- **Date:** 2026-11-20

## Context

[0009](0009-component-catalog.md) chose embedded JSON data and generic SVG
art but left the file shapes, presets and picture conventions open. F04
needs them to build the palette. Real shop data shows that devices come in
several pole layouts, that enclosures are sold by total module count
(ЩРН-П-12/24/36), and that their bars ship with 5-15 holes (separate 6x9 bars
are sold with 4-16).

## Decision

- **Files.** Four JSON files, each with `"schemaVersion": 1`:
  `devices.json` (`deviceTypes`, `bars`), `enclosures.json` (`enclosures`),
  `presets.json` (`presets`) and `products.json` (`series`). They live in
  `resources/catalog/` and are embedded into `panelcore` as
  `:/catalog/<name>.json`. JSON files carry no SPDX header, because JSON
  has no comments.
- **Parameters.** Names and units: `curve` (string), `ratedCurrent` (A,
  number), `residualCurrent` (mA, number), `rcdType` (string). Each device
  type lists the allowed values per parameter in `allowedValues`.
- **Presets are data.** `presets.json` lists the palette presets (group,
  name, purpose, type, poles, parameters, optional `wireSection` in mm²).
  This refines 0009, which left presets to code.
- **Products.** Grouped by series; each series has one source (a page on
  the manufacturer's own domain, with an access date). The article is
  optional.
- **Variants.** A device type has variants keyed by a poles label (`1P`,
  `2P`, `1P+N`), each with its own picture, module width and terminals.
  This refines 0009's "one SVG per type".
- **Enclosure sizes** are total modules, laid out in rows of 12, as sold
  (12, 24, 36). This refines the wording of [0007](0007-electrical-model.md).
- **Bars.** N and PE bar terminal counts are 8, 12 and 16 per enclosure
  size. A bar picture is one terminal cell (pitch 9) that the app tiles.
- **Enclosures** have one picture per size, `enclosure-<modules>.svg`.
- **Picture element ids:** `body`, `handle`, `label`, `terminal-<name>`,
  `test-button`, `rail-<n>`, `nbar-mount`, `pebar-mount`, and `terminal`
  in bar pictures. Units: 1 module = 18 x 90.
- **Where things live.** JSON in `panelcore` resources, pictures
  (`:/svg/devices/`) in `electricity_lib`.
- **Rendering.** Pictures are drawn with `QSvgRenderer` (Qt `Svg` module),
  so the `qsvg` and `qsvgicon` plugins stay excluded. This refines
  [0004](0004-dependencies.md).
- **Broken embedded catalog.** `embeddedCatalog()` calls `qFatal` with
  every load error, because a bad embedded file is a build defect.

## Rejected alternatives

- **Presets in code** — translators and data editors could not change them
  without touching C++.
- **One picture per type** — pole layouts differ in width and terminals.
- **Full-width bar pictures** — the terminal count varies by enclosure.

## Consequences

Adding a device, preset or enclosure is a data change plus a picture. Every
shown catalog string must be registered in `src/core/catalogstrings.cpp`
and translated. Loader validation (Task 1) rejects inconsistent data at
start-up.
