# 0009. Component catalog: embedded JSON data, generic SVG art

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Components must match what is sold in Kazakhstan and the CIS: modular DIN
devices from brands such as IEK, EKF, Schneider Electric (Easy9), ABB, Chint,
TDM, DEKraft, Legrand. Art must be original and generic (CLAUDE.md, Visual
assets); brand and model are text. Engineers change device parameters; the
parts list needs catalog text.

## Decision

- The catalog is a set of JSON files in `resources/catalog/`, embedded via
  Qt resources and loaded by `panelcore` at startup:
  - `devices.json` — device **types** (behaviour, terminals, picture,
    module width, default parameters);
  - `loads.json` — load types (picture, default power, terminals, whether it
    plugs into a socket);
  - `products.json` — market **products**: brand, series, article
    number/model text, mapped to a device type + parameters (e.g. IEK
    ВА47-29 1P C16 → `breaker`, poles 1, curve C, In 16). Used only for the
    parts list text and as quick presets.
  - `enclosures.json` — enclosure sizes (rows × modules, flush/surface).
- Device type kinds in v1: `breaker` (1P, 2P; 3P, 4P with three-phase),
  `rcd` (2P; 4P later), `rcbo` (1P+N), `enclosure`, `nbar`, `pebar`
  (pre-fitted, [0007](0007-electrical-model.md)). Added later in the
  roadmap: `voltage_relay`, `switch_disconnector`, `meter`, `spd`.
- Each type references one SVG (`resources/svg/devices/*.svg`); ratings and
  labels (e.g. "C16", "30 mA") are drawn as text over the picture at
  runtime, so one SVG serves all ratings.
- Loads in v1: ceiling lamp, wall socket (double: accepts at most two
  plugged-in appliances), kettle 2000 W, washing
  machine 2200 W, refrigerator 150 W, TV 120 W, microwave 1200 W, air
  conditioner 1000 W, water heater 2000 W, electric cooktop 7000 W (needs its
  own 32 A line on 6 mm²). The lamp is an LED lamp, default 10 W.
- **Product provenance:** every entry in `products.json` carries a
  `source` (manufacturer catalog URL and access date). Products are taken
  only from manufacturers' public catalogs (sessions use web search); an
  entry that cannot be verified is left out, never invented, because the
  parts list is used to buy real parts.
- Every catalog value is a default. A placed object stores only its
  overrides.
- All user-visible catalog strings (type names, descriptions) are translation
  keys resolved through Qt's translation system
  ([0012](0012-internationalization.md)); brand and model text is not
  translated.
- A catalog schema version field allows later migration. Loading fails
  loudly in debug builds and a unit test validates every catalog file
  against the device types and SVG list.

## Rejected alternatives

- **Catalog in C++ code** — every addition needs a recompile and touches
  logic files; data is easier to review and extend.
- **External catalog files next to the binary** — breaks the single-binary
  requirement. User-defined catalogs can come later as an ADR.
- **One SVG per product** — implies brand art, violates the asset rules, and
  multiplies drawing work.

## Consequences

- Market data (products) can be extended without code changes.
- Products drift with the market; the catalog is maintained by editing JSON.
