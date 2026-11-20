# F04 — Component catalog and device pictures

Date: 2026-11-20 · Status: approved by the owner

Roadmap item: [F04](../../roadmap.md#f04-component-catalog-and-device-pictures).
ADRs: [0006](../../adr/0006-core-and-app-layering.md),
[0007](../../adr/0007-electrical-model.md),
[0009](../../adr/0009-component-catalog.md),
[0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0014](../../adr/0014-progressive-disclosure.md),
[0016](../../adr/0016-electrical-reference-data.md),
[0018](../../adr/0018-testing-and-quality-gates.md).

## Intent

The app's users are non-experts building a real apartment panel from devices
sold in Kazakhstan. F04 gives the app its device knowledge and shows it. It
adds a catalog of device types, enclosures and real market products, embedded
in the binary. It adds original pictures of the devices, and a palette in the
left dock that offers a short list of ready-to-use devices, each saying what
it is for. A beginner should be able to pick the right device from its
purpose without knowing ratings. Placing devices on the panel comes in F05.

The product data serves the owner's goal of buying real parts from the design.
The parts list (F13) will use it, and a rating changed in F05 (say C25) must
still have real products behind it.

## Goals

### Palette (decided by the owner: option A, presets by job)

The left dock ("Components") shows one group, **Protection**, with six
presets in this order (the order a panel is assembled):

```
Protection
 [pic] Breaker 2P C40     apartment main
 [pic] RCD 40 A 30 mA     leakage protection
 [pic] Breaker 1P C10     lighting
 [pic] Breaker 1P C16     sockets
 [pic] Breaker 1P C32     cooktop
 [pic] RCBO C16 30 mA     sockets, breaker and RCD in one
```

- Each entry shows the device picture, its name and a purpose line, all
  translated (ru/kk/en) and retranslated on a language switch.
- Hovering an entry shows a tooltip. It says in plain words what the device
  does and, for breakers, the wire it is usually paired with (ADR 0016:
  C10 → 1.5 mm², C16 → 2.5 mm², C32 → 6 mm², 2P C40 → 10 mm²).
- Presets are catalog data, not code. Each one has a stable id (for tests,
  F05 drag-and-drop and the F11 tutorial highlight), a device type, its
  parameters, a name and a purpose text.
- Entries can't be dragged yet (F05); hovering is the only interaction.

### Catalog

Embedded JSON in `resources/catalog/` (ADR 0009), loaded by `panelcore` at
startup, with a schema version field.

**Device types** (terminal names per ADR 0007; positions in SVG units, defined
in the catalog and matching the drawn screws):

| Type | Variants | Width | Parameters and allowed values | Terminals |
|---|---|---|---|---|
| `breaker` | 1P, 2P | 1 module per pole | curve C; In 6, 10, 16, 20, 25, 32, 40, 50, 63 A | 1P: `1` top, `2` bottom; 2P: `1` `3` top, `2` `4` bottom |
| `rcd` | 2P | 2 modules | In 25, 32, 40, 63 A; IΔn 30 mA; type A | `1` `N1` top, `2` `N2` bottom |
| `rcbo` | 1P+N | 2 modules | curve C; In 6, 10, 16, 20, 25, 32 A; IΔn 30 mA; type A | `1` `N1` top, `2` `N2` bottom |
| `nbar`, `pebar` | — | — | number of terminals | numbered |

B/D curves, type AC and 100 mA wait for F16, so they are left out of F04's
allowed values. Simulation constants (τ, trip factors) arrive with F09.

**Enclosures:** 12, 24 and 36 modules in 1, 2 and 3 rows of 12 (as sold, for
example ЩРН-П-12/24/36), each with a pre-fitted N bar and PE bar (ADR 0007).
Each bar must hold at least 8 terminals; the builder takes the counts from real
enclosures of that size.

**Products:** five brands: IEK, EKF, Schneider Electric Easy9, ABB and Chint.
For each brand, one commonly sold series per device kind (1P/2P breaker, RCD,
RCBO), with a product for every allowed rating the series offers. That comes
to about 140 entries. Each entry has a brand, a series, a model text, the
article number when the catalog gives one, a device type with parameters,
and a `source` (manufacturer catalog URL plus access date). One source per
series is fine when it lists every article. Products must match the device
type exactly (module width, curve, IΔn, type A); anything else is left out.
Nothing is invented: a series or brand that can't be verified from the
manufacturer's own public catalog is left out, and the session report says
which (ADR 0009). Products don't appear in the UI yet.

### Pictures

Original SVGs (ADR 0013 rules: SVG Tiny 1.2, no text, no CSS, no raster,
1 module = 18 × 90 units, light grey body with an outline of at least 1.5
units), embedded through Qt resources:

- 1P breaker, 2P breaker, RCD, RCBO: generic DIN devices with a toggle
  handle, top and bottom terminal screws, a blank area where the app will draw
  the rating later, and a test button on the RCD and RCBO. The handle is drawn
  as its own named element so F08 can show it on, off and tripped without new
  art.
- Enclosure, N bar, PE bar: the N bar is coloured blue, the PE bar
  yellow-green (the wire colours from ADR 0013).
- No brand features: generic shapes only.

### Wire ampacity check

Search for the official ПУЭ РК text (table 1.3.4, copper, two single-core
wires in one pipe). If its values differ from ADR 0016, supersede 0016 with
a new ADR. If the official text can't be found, keep the ПУЭ 7 values and
say so in the session report. F04 only checks the table; the wire features
(F06, F09) bring it into the catalog data.

## Non-goals

- Dragging or placing devices, properties panel, changing ratings (F05).
- Loads and `loads.json` (F07).
- The engineer's full catalog in the palette (F12); products in any UI
  (F12/F13).
- Enclosure products and flush/surface variants (F13 adds them with the parts
  list).
- B/D curves, RCD type AC, 100 mA, other device kinds (F16); 3P/4P (F17).

## Anti-goals

- Any invented product, article number or source.
- Logos, brand marks, or pictures traced from manufacturer photos or
  catalogs.
- A palette that needs electrical knowledge to choose from.

## Constraints

- Catalog loading and validation live in `panelcore` (Qt Core only, ADR 0006).
- Catalog strings (type names, preset names, purposes, descriptions, group
  name) are English source strings registered for translation in the
  `Catalog` context; brand and model text is not translated (ADR 0012). Every
  new string ships with Russian and Kazakh translations.
- The app now imports the `qsvg` and `qsvgicon` plugins (ADR 0004; currently
  excluded in `src/app/CMakeLists.txt`). The release binary's dependency
  checks must still pass on Linux and Windows.
- New SVG files carry an SPDX header comment like the sources (ADR 0002).

## Approaches

**What the palette lists** (owner's choice):
- *A. Presets by job* — **chosen**: six entries, each saying what it's for;
  short, teaches the usual pairings; other ratings come from the properties
  panel in F05.
- *B. Every common rating* — about 25 entries; complete but long, and silent
  about purpose.
- *C. One entry per device kind* — shortest, but the beginner must pick a
  rating unaided.

**Where presets live** (builder's call, accepted by the owner):
- *In the catalog data* — **chosen**: adding or changing a preset is a data
  edit (F12's full list, F16's new devices).
- *In the palette code* — rejected by ADR 0009 (no catalog data in code).

**How many products** (builder's call, accepted by the owner):
- *Every allowed rating of one series per kind and brand, one source per
  series* — **chosen**: any rating set in F05 still maps to real products for
  the parts list.
- *Only the six presets* (about 30 entries) — a quarter of the research, but
  a breaker changed to C25 would have no real product.

## Decided by the owner

- Palette option A, presets by job.
- Approaches for preset storage and product coverage as proposed.
- Approved the design and the builder's calls below as written.

## Builder's calls

- The six presets, their order, and the RCBO as the sixth entry.
- Allowed rating lists per device type (the table above).
- RCD and RCBO products are type A, 30 mA only, matching the tutorial's RCD
  (ADR 0016); type AC is common in shops but waits for F16.
- RCBO width is 2 modules (the common 1P+N size); 1-module compact RCBOs are
  left out.
- Enclosure sizes read as total modules in rows of 12, as the market sells
  them. (ADR 0007's "modules per row × rows" wording reads differently; the
  builder records the reading in the catalog ADR.)
- The tooltip includes the usual wire section for breakers and the RCBO
  (2.5 mm² for the C16 RCBO).
- A source must be on the manufacturer's own domain, with the Kazakhstan site
  preferred. Model text is the short catalog designation, not a shop title.
- An allowed rating that no brand offers is dropped from the list and the
  change noted in the catalog ADR.
- The tooltip screenshot grabs the tooltip window itself, since it is a
  separate window from the main one.
- Suggested series to verify first (not decisions): IEK ВА47-29 / ВД1-63 /
  АВДТ32; EKF ВА 47-63 / ВД-100 / АД-32; Easy9 MCB / RCCB / RCBO; ABB S200 or
  SH200 / F202 / DS201; Chint NXB-63 / NL1-63 / NXBLE-32. Pick what's actually
  sold in Kazakhstan and offered in type A.
- Russian and Kazakh wording by the builder; Kazakh to be reviewed by a native
  speaker when possible (ADR 0012).

## Left to the builder

- File layout inside `resources/catalog/` and the JSON schema details. If the
  schema adds something ADR 0009 doesn't describe (presets, allowed values,
  terminal positions), record it in a new ADR.
- Palette widget and layout, icon size (the pictures must also read at 32 px,
  per ADR 0013), whether palette icons show the rating overlay.
- Whether 1P and 2P breakers share one SVG or get one each. ADR 0009 says one
  per type, so if they split, note it in the catalog ADR.
- How the enclosure picture serves all three sizes (stretchable frame plus
  rail pieces, or one file per size); F05 is its first real user.
- Where the catalog JSON and SVG resources are embedded, so that both the core
  validation test and the app can read them.
- Behaviour of a broken catalog: fail loudly in debug builds (ADR 0009).

## Tests

- **Core unit tests:** the embedded catalog loads, and validation passes for
  every file. Every product maps to an existing type with allowed parameter
  values and has a brand, model text and a source with a URL and date. Every
  preset references a valid type and parameters. Every referenced SVG exists.
  Ids are unique. Terminal names match ADR 0007. Loader error cases use small
  fixture files (bad schema version, unknown type, value not allowed, missing
  source).
- **Translations:** the existing `translations_complete` test covers catalog
  strings once they are registered. A test fails if a user-visible catalog
  string isn't registered for translation.
- **GUI scenario (offscreen):** the palette shows the Protection group and the
  six presets in order, with English names and purpose lines. Hovering an
  entry shows its description tooltip. Switching to Russian changes the texts
  without a restart. Screenshots: palette in dark and light, palette in
  Russian and Kazakh, a tooltip, and every catalog SVG at 32 px on dark and
  light backgrounds.
- **Smoke test** of the real binary under Xvfb; screenshot inspected.
