# F04 Component Catalog and Device Pictures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The app loads an embedded JSON catalog of device types, enclosures, presets and real market products. It has original SVG pictures of the devices, and the left dock shows six purpose-labelled presets with pictures and tooltips in ru/kk/en.

**Architecture:**
- `panelcore` gets `catalog.{h,cpp}`: plain structs, a loader that parses and validates four JSON files, and `embeddedCatalog()`. It also gets `catalogstrings.cpp`, which registers the catalog's user-visible strings for translation. The JSON lives in `panelcore` resources.
- The app gets the SVG pictures in `electricity_lib` resources, rendered with `QSvgRenderer` (`svgpixmap.{h,cpp}`).
- The app gets `PaletteWidget`, a `QTreeWidget` that replaces the empty dock widget and retranslates on `LanguageChange`.
- Product data is researched from manufacturers' public catalogs, and every entry carries its source.

**Tech Stack:** C++20, Qt 6.12.0 static (Core, Widgets, Svg; LinguistTools), Qt Test (core and offscreen GUI), web research for products and ПУЭ РК.

**Spec:** `docs/superpowers/specs/2026-11-20-f04-catalog-palette-design.md`

## Global Constraints

- New Qt module: `Svg` only (ADR 0004). The `qsvg` and `qsvgicon` plugins stay excluded. Pictures are rendered with `QSvgRenderer`, so the plugins are unused (record in ADR 0023; this refines the spec's plugin line).
- Resource paths: catalog JSON at `:/catalog/{devices,enclosures,presets,products}.json` (embedded in `panelcore`); pictures at `:/svg/devices/<file>.svg` (embedded in `electricity_lib`).
- JSON schema version `1` in every file. Parameter names and units: `curve` (string), `ratedCurrent` (A, number), `residualCurrent` (mA, number), `rcdType` (string).
- Picture units: 1 module = 18 × 90. SVG Tiny 1.2 only: no `<text>`, no CSS (`<style>` or `style=`), no `<image>`, no `<filter>`, no `<script>`. Each file starts with `<!-- SPDX-License-Identifier: GPL-3.0-or-later -->`. Device bodies are light grey with a dark outline of at least 1.5 units. No logos and no brand features.
- Technical notation stays Latin in every language: `2P`, `C16`, `40 A`, `30 mA`, `mm²`. Numbers use the current language's decimal separator (`1,5` in ru/kk).
- Brands, exact strings: `IEK`, `EKF`, `Schneider Electric`, `ABB`, `Chint`. Brand, series, model and article are never translated.
- Product rule: a product goes in only if its page or PDF on the manufacturer's own domain was opened in this session and shows that article (or model) for that rating with matching poles, curve, IΔn, type A and module width. Otherwise it's left out and the omission reported. Never invent.
- Catalog strings, translation context `Catalog` (registered in `src/core/catalogstrings.cpp`); palette string, context `PaletteWidget`:

  | Source | ru | kk |
  |---|---|---|
  | Protection | Защита | Қорғаныс |
  | Breaker 2P C40 | Автомат 2P C40 | Автомат 2P C40 |
  | RCD 40 A 30 mA | УЗО 40 A 30 mA | УЗО 40 A 30 mA |
  | Breaker 1P C10 | Автомат 1P C10 | Автомат 1P C10 |
  | Breaker 1P C16 | Автомат 1P C16 | Автомат 1P C16 |
  | Breaker 1P C32 | Автомат 1P C32 | Автомат 1P C32 |
  | RCBO C16 30 mA | Дифавтомат C16 30 mA | Дифавтомат C16 30 mA |
  | apartment main | ввод в квартиру | пәтерге кіріс |
  | leakage protection | защита от утечки тока | ток ағуынан қорғау |
  | lighting | освещение | жарықтандыру |
  | sockets | розетки | розеткалар |
  | cooktop | варочная панель | пісіру панелі |
  | sockets, breaker and RCD in one | розетки, автомат и УЗО в одном | розеткалар, автомат пен УЗО бірге |
  | Circuit breaker | Автоматический выключатель | Автоматты ажыратқыш |
  | Residual current device (RCD) | Устройство защитного отключения (УЗО) | Қорғаныштық ажырату құрылғысы (УЗО) |
  | Residual current circuit breaker (RCBO) | Дифференциальный автомат | Дифференциалды автомат |
  | Switches a line on and off. Turns it off by itself on overload or short circuit. | Включает и отключает линию. Сам отключает её при перегрузке или коротком замыкании. | Желіні қосады және ажыратады. Асқын жүктеме немесе қысқа тұйықталу кезінде оны өзі ажыратады. |
  | Turns the power off when current leaks to earth, for example through a person. Does not protect against overload. | Отключает питание при утечке тока на землю, например через человека. От перегрузки не защищает. | Ток жерге ағып кеткенде, мысалы адам арқылы, қуатты ажыратады. Асқын жүктемеден қорғамайды. |
  | Breaker and RCD in one device: turns off on overload, short circuit and current leakage. | Автомат и УЗО в одном корпусе: отключается при перегрузке, коротком замыкании и утечке тока. | Бір корпустағы автомат пен УЗО: асқын жүктемеде, қысқа тұйықталуда және ток ағуында ажыратылады. |
  | Usual wire: %1 mm² (`PaletteWidget`) | Обычный провод: %1 mm² | Әдеттегі сым: %1 mm² |

- Object names: `paletteTree` (the palette); existing `paletteDock`, menus and actions from F03.
- Screenshots: `build/screenshots/f04-<step>.png`.
- Every new source file has an SPDX `GPL-3.0-or-later` header; clang-format (Qt style, 100 columns) clean; GUI tests call `useTemporarySettings()` first.
- Never `git push`; commit only as the steps say, ending messages with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- To wait for a long command, run it with `run_in_background: true` and wait for its completion notice. Never poll with `pgrep -f`. Before reporting, stop every background task you started.

## Review Focus

- **Invented or mismatched product data:** an article that isn't on the cited page means a user buys the wrong part. No test can catch it. The Task 5 reviewer re-opens one source per series and checks three articles against it.
- **Long Russian/Kazakh text clipped in the palette:** every name and purpose line fits without eliding in all three languages. Task 4 `textFitsInEveryLanguage`.
- **HiDPI screens (150–200 % scaling on Windows laptops):** icons must stay sharp. `svgPixmap` honours the device pixel ratio. Task 3 `rendersAtDevicePixelRatio`.
- **Light theme:** light-grey device bodies must still read against a light background (dark outline). Task 3 light sheet and Task 4 `f04-palette-light` screenshot, inspected with that criterion.
- **Hand-edited catalog with wrong JSON types** (a rating as `"16"`, a version as `2`): rejected with a clear message rather than silently accepted. Task 1 rows `number as text` and `schema version`.

---

### Task 1: Catalog model, loader and validation (core)

**Files:**
- Create: `src/core/catalog.h`, `src/core/catalog.cpp`, `tests/core/tst_catalog.cpp`
- Modify: `src/core/CMakeLists.txt` (sources), `tests/CMakeLists.txt` (`add_core_test(tst_catalog core/tst_catalog.cpp)`), `docs/roadmap.md` (F04 ☐ → ◐)

**Interfaces:**
- Produces, `catalog.h`:

```cpp
struct TerminalSpec {
    QString name;
    QPointF position; // picture units
    friend bool operator==(const TerminalSpec &, const TerminalSpec &) = default;
};
struct DeviceVariant { QString poles; int modules = 0; QString picture; QList<TerminalSpec> terminals; };
struct DeviceType {
    QString id, name, description;
    QJsonObject allowedValues; // parameter name -> array of allowed values
    QList<DeviceVariant> variants;
    const DeviceVariant *variant(QStringView poles) const; // nullptr if absent
};
struct BarType { QString id; QString picture; double pitch = 0; QPointF terminal; };
struct EnclosureType { QString id; int modules = 0; int rows = 0; int nBarTerminals = 0; int peBarTerminals = 0; QString picture; };
struct Preset { QString id, group, name, purpose, type, poles; QJsonObject parameters; std::optional<double> wireSection; };
struct Product { QString brand, series, model, article, type, poles; QJsonObject parameters; QString sourceUrl; QDate accessed; };
struct Catalog {
    QList<DeviceType> deviceTypes; QList<BarType> bars; QList<EnclosureType> enclosures;
    QList<Preset> presets; QList<Product> products;
    const DeviceType *deviceType(QStringView id) const; // nullptr if absent
};
struct CatalogSources { QByteArray devices, enclosures, presets, products; };
struct CatalogLoad { Catalog catalog; QStringList errors; };
CatalogLoad loadCatalog(const CatalogSources &sources);
```

- JSON shapes (the loader's contract; Task 2's data follows them):
  - `devices.json`: `{"schemaVersion":1, "deviceTypes":[{"id","name","description","allowedValues":{...},"variants":[{"poles","modules","picture","terminals":[{"name","x","y"}]}]}], "bars":[{"id","picture","pitch","terminal":{"x","y"}}]}`
  - `enclosures.json`: `{"schemaVersion":1, "enclosures":[{"id","modules","rows","nBarTerminals","peBarTerminals","picture"}]}`
  - `presets.json`: `{"schemaVersion":1, "presets":[{"id","group","name","purpose","type","poles","parameters":{...},"wireSection"?}]}`
  - `products.json`: `{"schemaVersion":1, "series":[{"brand","series","type","source":{"url","accessed"},"products":[{"model","article"?,"poles","parameters":{...}}]}]}`. Each product inherits brand, series, type and source from its series block.
- Error messages (tests match these substrings). `<where>` is `preset <id>` or `product <brand> <model>`:
  - `<file>: <parse error>`
  - `<file>: unsupported schema version <n>` (a missing version counts as 0)
  - `duplicate id <id>` (device types, bars, enclosures, presets)
  - `duplicate product <brand> <model>`
  - `duplicate terminal <name>`
  - `<where>: unknown device type <type>`
  - `<where>: unknown variant <poles> of <type>`
  - `<where>: missing parameter <key>`
  - `<where>: unknown parameter <key>`
  - `<where>: <key> <value as compact JSON> not allowed`
  - `<where>: missing <field>` (empty id, name, purpose, group, brand, series or model)
  - `series <brand> <series>: missing source`
  - `series <brand> <series>: source URL must start with https://`
  - `series <brand> <series>: source access date must be YYYY-MM-DD`
  - `preset <id>: wireSection must be positive`
  - `<enclosure id>: rows must be positive`

- [ ] **Step 1: Write the failing tests** in `tests/core/tst_catalog.cpp` (class `TestCatalog`, `QTEST_APPLESS_MAIN`). Define a minimal valid fixture as four raw-string `QByteArray`s:
  - one `breaker` type (name "Circuit breaker", description "Protects a line."), `allowedValues` `{"curve":["C"],"ratedCurrent":[10,16]}`, variant `1P` (1 module, `breaker-1p.svg`, terminals `1` (9,8) and `2` (9,82));
  - bar `nbar` (`nbar.svg`, pitch 9, terminal (4.5,8));
  - enclosure `enclosure-12` (12 modules, 1 row, 8 and 8 bar terminals, `enclosure-12.svg`);
  - preset `breaker-1p-c16` (group "Protection", name "Breaker 1P C16", purpose "sockets", `breaker` `1P`, `{"curve":"C","ratedCurrent":16}`, wireSection 2.5);
  - one series `Acme` / `B`, type `breaker`, source `https://example.com/b` accessed `2026-11-20`, with product model `B16 1P`, article `A-16`, `1P`, `{"curve":"C","ratedCurrent":16}`.

  Add a helper `QByteArray patched(const QByteArray &json, const std::function<void(QJsonObject &)> &edit)`.

```cpp
void TestCatalog::loadsValidCatalog()
{
    const CatalogLoad load = loadCatalog(fixture());
    QVERIFY2(load.errors.isEmpty(), qPrintable(load.errors.join(u'\n')));
    const DeviceType *breaker = load.catalog.deviceType(u"breaker");
    QVERIFY(breaker && !load.catalog.deviceType(u"fuse"));
    QCOMPARE(breaker->name, QStringLiteral("Circuit breaker"));
    const DeviceVariant *p1 = breaker->variant(u"1P");
    QVERIFY(p1 && !breaker->variant(u"2P"));
    QCOMPARE(p1->modules, 1);
    QCOMPARE(p1->picture, QStringLiteral("breaker-1p.svg"));
    QCOMPARE(p1->terminals, (QList<TerminalSpec>{ { "1", { 9, 8 } }, { "2", { 9, 82 } } }));
    QCOMPARE(load.catalog.bars.at(0).pitch, 9.0);
    QCOMPARE(load.catalog.enclosures.at(0).peBarTerminals, 8);
    const Preset &preset = load.catalog.presets.at(0);
    QCOMPARE(preset.parameters.value("ratedCurrent").toInt(), 16);
    QCOMPARE(preset.wireSection.value_or(0), 2.5);
    const Product &product = load.catalog.products.at(0);
    QCOMPARE(product.brand, QStringLiteral("Acme"));
    QCOMPARE(product.series, QStringLiteral("B"));
    QCOMPARE(product.article, QStringLiteral("A-16"));
    QCOMPARE(product.type, QStringLiteral("breaker"));
    QCOMPARE(product.sourceUrl, QStringLiteral("https://example.com/b"));
    QCOMPARE(product.accessed, QDate(2026, 11, 20));
}
```

  `rejectsInvalidCatalog` is data-driven, with columns `std::function<void(CatalogSources &)> edit` (`Q_DECLARE_METATYPE`) and `QString expected`. It asserts that some error contains `expected`. Rows:

  | Row | Edit | Expected substring |
  |---|---|---|
  | bad JSON | `devices = "{"` | `devices.json:` |
  | schema version | products `schemaVersion` = 2 | `products.json: unsupported schema version 2` |
  | duplicate id | append a copy of preset 0 | `duplicate id breaker-1p-c16` |
  | duplicate terminal | variant terminal 2 named `1` | `duplicate terminal 1` |
  | unknown type | preset `type` = `fuse` | `preset breaker-1p-c16: unknown device type fuse` |
  | unknown variant | preset `poles` = `3P` | `unknown variant 3P of breaker` |
  | value not allowed | product `ratedCurrent` = 17 | `product Acme B16 1P: ratedCurrent 17 not allowed` |
  | number as text | preset `ratedCurrent` = `"16"` | `ratedCurrent "16" not allowed` |
  | missing parameter | remove preset `curve` | `missing parameter curve` |
  | unknown parameter | product parameter `colour` = `red` | `unknown parameter colour` |
  | missing name | preset `name` = `""` | `preset breaker-1p-c16: missing name` |
  | missing source | remove series `source` | `series Acme B: missing source` |
  | plain http | source `url` = `http://example.com/b` | `source URL must start with https://` |
  | bad date | source `accessed` = `20.11.2026` | `source access date must be YYYY-MM-DD` |
  | duplicate product | append a copy of product 0 | `duplicate product Acme B16 1P` |
  | bad wire section | preset `wireSection` = 0 | `wireSection must be positive` |
  | no rows | enclosure `rows` = 0 | `enclosure-12: rows must be positive` |

- [ ] **Step 2: Verify red.** Run `cmake --preset dev && cmake --build build`. Expected: compile error, because `core/catalog.h` doesn't exist.
- [ ] **Step 3: Implement** `catalog.h`/`catalog.cpp` per Interfaces. Collect every error instead of stopping at the first. A preset or product's parameters must hold exactly the type's `allowedValues` keys, and each value must be `QJsonArray::contains`-equal to an allowed one (so `"16"` ≠ `16`). Product article is optional.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build -R tst_catalog --output-on-failure`. Expected: PASS.
- [ ] **Step 5: Commit** the files above with the message `F04: catalog model, loader and validation`.

### Task 2: Embedded catalog data, translations, ADR 0023

**Files:**
- Create: `resources/catalog/devices.json`, `resources/catalog/enclosures.json`, `resources/catalog/presets.json`, `resources/catalog/products.json`, `src/core/catalogstrings.h`, `src/core/catalogstrings.cpp`, `docs/adr/0023-catalog-schema.md`
- Modify: `src/core/catalog.h`, `src/core/catalog.cpp`, `src/core/CMakeLists.txt` (`qt_add_resources(panelcore "catalog" PREFIX "/" BASE "${PROJECT_SOURCE_DIR}/resources" FILES <the four json>)`, new sources), `tests/core/tst_catalog.cpp`, `translations/electricity_ru.ts`, `translations/electricity_kk.ts`, `docs/adr/README.md`

**Interfaces:**
- Consumes: Task 1's types and `loadCatalog`.
- Produces, `catalog.h`: `CatalogSources embeddedCatalogSources();` reads `:/catalog/*.json`. `const Catalog &embeddedCatalog();` loads once, and calls `qFatal` listing every error if any.
- Produces, `catalogstrings.h`: `QStringList catalogTranslationSources();` returns every `QT_TRANSLATE_NOOP("Catalog", ...)` source string listed in `catalogstrings.cpp` (the Global Constraints table, minus the `PaletteWidget` row).
- Data (exact):
  - `breaker`: name "Circuit breaker", the breaker description. `allowedValues` `curve` `["C"]`, `ratedCurrent` `[6,10,16,20,25,32,40,50,63]`.
    - Variant `1P`: 1 module, `breaker-1p.svg`, terminals `1` (9,8), `2` (9,82).
    - Variant `2P`: 2 modules, `breaker-2p.svg`, terminals `1` (9,8), `3` (27,8), `2` (9,82), `4` (27,82).
  - `rcd`: name "Residual current device (RCD)", the RCD description. `allowedValues` `ratedCurrent` `[25,32,40,63]`, `residualCurrent` `[30]`, `rcdType` `["A"]`.
    - Variant `2P`: 2 modules, `rcd-2p.svg`, terminals `1` (9,8), `N1` (27,8), `2` (9,82), `N2` (27,82).
  - `rcbo`: name "Residual current circuit breaker (RCBO)", the RCBO description. `allowedValues` `curve` `["C"]`, `ratedCurrent` `[6,10,16,20,25,32]`, `residualCurrent` `[30]`, `rcdType` `["A"]`.
    - Variant `1P+N`: 2 modules, `rcbo-1pn.svg`, terminals as the RCD.
  - Bars: `nbar` (`nbar.svg`) and `pebar` (`pebar.svg`), each with pitch 9 and terminal (4.5, 8). Each picture is one terminal cell, tiled by the app.
  - Enclosures, as `id` (modules, rows, N and PE bar terminals): `enclosure-12` (12, 1, 8/8), `enclosure-24` (24, 2, 12/12), `enclosure-36` (36, 3, 16/16). Pictures are `enclosure-<modules>.svg`.
  - Presets, in this order, all with group "Protection":

    | id | name | purpose | type | poles | parameters | wireSection |
    |---|---|---|---|---|---|---|
    | `breaker-2p-c40` | Breaker 2P C40 | apartment main | breaker | 2P | C, 40 | 10 |
    | `rcd-2p-40-30` | RCD 40 A 30 mA | leakage protection | rcd | 2P | 40, 30, A | — |
    | `breaker-1p-c10` | Breaker 1P C10 | lighting | breaker | 1P | C, 10 | 1.5 |
    | `breaker-1p-c16` | Breaker 1P C16 | sockets | breaker | 1P | C, 16 | 2.5 |
    | `breaker-1p-c32` | Breaker 1P C32 | cooktop | breaker | 1P | C, 32 | 6 |
    | `rcbo-c16-30` | RCBO C16 30 mA | sockets, breaker and RCD in one | rcbo | 1P+N | C, 16, 30, A | 2.5 |

  - `products.json`: `{"schemaVersion":1,"series":[]}` (filled in Task 5).

- [ ] **Step 1: Write the failing tests** in `tst_catalog.cpp`:
  - `embeddedCatalogLoadsWithoutErrors`: `loadCatalog(embeddedCatalogSources())` returns no errors.
  - `deviceTypesMatchSpec`: device type ids are `{"breaker","rcd","rcbo"}` in order. Each type's `allowedValues` equals the `QJsonObject` above. Each variant's poles, modules, picture and terminal list (`QCOMPARE` on `QList<TerminalSpec>`) match. The bars match.
  - `enclosuresMatchSpec`: the three enclosures, exactly as listed.
  - `presetsMatchSpec`: the six presets in order, with every column of the table (`wireSection` is `std::nullopt` for the RCD).
  - `catalogStringsAreRegisteredForTranslation`: every type `name` and `description`, and every preset `group`, `name` and `purpose` of `embeddedCatalog()`, is in `catalogTranslationSources()`.

  Note: a rating that no brand offers is removed from the data and from `deviceTypesMatchSpec` together in Task 5, as the spec says. That's the only allowed edit to these expectations.
- [ ] **Step 2: Verify red.** Run `cmake --build build`. Expected: compile error, because `embeddedCatalogSources` isn't declared.
- [ ] **Step 3: Implement** the JSON files, the resource block, `embeddedCatalogSources()`, `embeddedCatalog()` and `catalogstrings.{h,cpp}`.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build -R tst_catalog --output-on-failure`. Expected: PASS.
- [ ] **Step 5: Translations.** Run `cmake --build build --target update_translations && ctest --test-dir build -R translations_complete --output-on-failure`. Expected: FAIL with 19 unfinished entries in each of ru and kk. Translate them per the table, then rerun. Expected: PASS.
- [ ] **Step 6: Write ADR 0023** "Catalog schema v1, presets and picture conventions" from `docs/adr/0000-template.md` and add its row to `docs/adr/README.md`. It records:
  - the four files and their shapes, and the parameter names and units;
  - presets as data (refines 0009);
  - products grouped by series with one source each, a source on the manufacturer's own domain, and article optional;
  - variants keyed by a poles label, each with its own picture (refines 0009's "one SVG per type");
  - enclosure sizes read as total modules in rows of 12, as sold (ЩРН-П-12/24/36), which refines the wording of 0007;
  - bar terminal counts 8/12/16 (real enclosures ship bars with 5–15 holes; 6×9 bars are sold separately with 4–16), and bars drawn as one tiled terminal cell;
  - one enclosure picture per size;
  - picture element ids (`body`, `handle`, `label`, `terminal-<name>`, `test-button`, `rail-<n>`, `nbar-mount`, `pebar-mount`, bar `terminal`);
  - JSON in `panelcore` resources, pictures in `electricity_lib`;
  - rendering with `QSvgRenderer`, so `qsvg`/`qsvgicon` stay excluded (refines 0004);
  - `qFatal` on a broken embedded catalog.
- [ ] **Step 7: Commit** the files above with the message `F04: embedded catalog data, catalog translations (ADR 0023)`.

### Task 3: Device pictures

**Files:**
- Create: `resources/svg/devices/{breaker-1p,breaker-2p,rcd-2p,rcbo-1pn,enclosure-12,enclosure-24,enclosure-36,nbar,pebar}.svg`, `src/app/svgpixmap.h`, `src/app/svgpixmap.cpp`, `tests/gui/tst_catalogart.cpp`
- Modify: `CMakeLists.txt` (add `Svg` to `find_package(Qt6 ...)`), `src/app/CMakeLists.txt`:
  - link `Qt6::Svg`;
  - add the `svgpixmap` sources;
  - add a `qt_add_resources(electricity_lib "pictures" PREFIX "/" BASE "${PROJECT_SOURCE_DIR}/resources" FILES <the nine svg>)` block;
  - change the plugin comment's last sentence to say pictures are rendered with `QSvgRenderer`, so `qsvg`/`qsvgicon` are not needed.

  Also modify `tests/gui/screenshot.h` and `screenshot.cpp` (overload below) and `tests/CMakeLists.txt` (`add_gui_test(tst_catalogart gui/tst_catalogart.cpp)`).

**Interfaces:**
- Consumes: `embeddedCatalog()`, `DeviceVariant`, `BarType`, `EnclosureType`.
- Produces, `svgpixmap.h`:
  - `QString picturePath(const QString &picture);` returns `":/svg/devices/" + picture`.
  - `QPixmap svgPixmap(const QString &path, int height, qreal devicePixelRatio = 1.0);` renders at `height` logical pixels. The width is `qRound(height * viewBox.width / viewBox.height)`, and the physical size is the logical size × `devicePixelRatio`, with `setDevicePixelRatio`. The background is transparent. It returns a null pixmap for an invalid SVG.
- Produces, `screenshot.h`: `bool saveScreenshot(const QPixmap &pixmap, const QString &test, const QString &step);`
- Picture content:
  - **Devices** (viewBox `0 0 <18·modules> 90`): two top-level groups. `body` holds the outline, the top and bottom screws as elements `terminal-<name>` centred on the catalog positions, a blank light `label` rect for the rating, and `test-button` on the RCD and RCBO. `handle` holds the toggle, drawn in the on (up) position. The 2P handles are tied together in one group.
  - **Enclosures:** a dark grey interior, a lighter frame, and one metallic DIN rail per row (`rail-<n>`, at least 216 wide) with room for 12 modules and 90-unit devices. `nbar-mount` and `pebar-mount` mark where the bars go. Width and height are the builder's call.
  - **Bars** (viewBox width 9): one tileable cell, with a brass strip edge to edge and a screw as element `terminal` at (4.5, 8). The insulator is blue on `nbar` and yellow-green on `pebar`.

- [ ] **Step 1: Write the failing tests** in `tests/gui/tst_catalogart.cpp` (class `TestCatalogArt`; `initTestCase` calls `useTemporarySettings(); setupApplication(*qApp);`). Collect `pictures()` = every `picture` in the catalog's variants, bars and enclosures.
  - `everyPictureExistsAndFollowsArtRules`: start with `QTest::failOnWarning(QRegularExpression(".*"))`. For each picture, `QFile::exists(picturePath(p))`, `QSvgRenderer(path).isValid()`, the text contains `SPDX-License-Identifier: GPL-3.0-or-later`, and it contains none of `<text`, `<style`, `style=`, `<image`, `<filter`, `<script`, `<foreignObject`.
  - `devicePicturesMatchCatalog`: for each variant:
    - `viewBoxF() == QRectF(0, 0, 18 * modules, 90)`;
    - elements `body`, `handle` and `label` exist;
    - for each terminal, `terminal-<name>` exists and `r.transformForElement(id).mapRect(r.boundsOnElement(id)).contains(terminal.position)`;
    - `test-button` exists for `rcd` and `rcbo`.
  - `barAndEnclosurePicturesMatchCatalog`:
    - each bar's viewBox width equals `pitch`, and the bar's `terminal` element contains `terminal`;
    - each enclosure has `rail-1` … `rail-<rows>`, each with a mapped width of at least 216, plus `nbar-mount` and `pebar-mount`.
  - `rendersAtDevicePixelRatio`: `svgPixmap(picturePath("breaker-1p.svg"), 48, 2.0)` has `devicePixelRatio() == 2.0`, physical height 96 and logical width `qRound(48 * 18 / 90.0)`. `svgPixmap("nope.svg", 48)` is null.
  - `rendersEveryPictureAt32px`: every `svgPixmap(path, 32)` has height 32 and at least one opaque pixel. Compose three sheets, saved with the new overload:
    - `f04-svg-32px-dark` (background `Theme::darkPalette()` Window): every picture side by side, 8 px apart, plus each bar tiled 8 times;
    - `f04-svg-32px-light` (background `Theme::lightPalette()` Window): the same;
    - `f04-svg-large-dark`: everything at 180 px.
- [ ] **Step 2: Verify red.** Run `cmake --preset dev && cmake --build build`. Expected: compile error, because `app/svgpixmap.h` doesn't exist.
- [ ] **Step 3: Implement** `svgpixmap`, the screenshot overload, the CMake changes and the nine SVGs per Picture content.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build -R "tst_catalog" --output-on-failure`. Expected: `tst_catalog` and `tst_catalogart` pass.
- [ ] **Step 5: Inspect** `build/screenshots/f04-svg-*.png`. Expected:
  - Each device reads as a generic DIN device at 32 px: handle, screws and label area. The RCD and RCBO show a test button.
  - Bodies stand out on both dark and light backgrounds.
  - Tiled bars look continuous, with N blue and PE yellow-green.
  - Enclosures show their rails.
  - Nothing resembles a brand.

  Redraw and rerun until true.
- [ ] **Step 6: Commit** the files above with the message `F04: original device pictures and art checks`.

### Task 4: Palette in the Components dock

**Files:**
- Create: `src/app/palettewidget.h`, `src/app/palettewidget.cpp`, `tests/gui/menuclicks.h`, `tests/gui/menuclicks.cpp`, `tests/gui/tst_palette.cpp`
- Modify: `src/app/mainwindow.cpp`:
  - `kPaletteWidth` 240 → 300, so the Russian and Kazakh lines fit;
  - the dock widget becomes `new PaletteWidget(embeddedCatalog(), m_dock)`.

  Also modify `src/app/CMakeLists.txt`, `tests/CMakeLists.txt` (add `gui/menuclicks.cpp` to `add_gui_test`; `add_gui_test(tst_palette gui/tst_palette.cpp)`), `tests/gui/tst_preferences.cpp` (use the moved helpers, no behaviour change), `translations/electricity_ru.ts`, `translations/electricity_kk.ts`.

**Interfaces:**
- Consumes: `Catalog`, `Preset`, `DeviceType`, `embeddedCatalog()`, `picturePath()`, `svgPixmap()`, `currentLanguage()`, `languageCode()`.
- Produces, `palettewidget.h`: `class PaletteWidget : public QTreeWidget` with `explicit PaletteWidget(const Catalog &catalog, QWidget *parent = nullptr);` and `static constexpr int PresetIdRole = Qt::UserRole;`.
  - Setup: object name `paletteTree`, header hidden, `rootIsDecorated(false)`, `iconSize(24, 48)`, indentation 8.
  - Rows: one top-level item per preset group, in first-appearance order. Group items are bold, expanded and have flags `Qt::ItemIsEnabled`. Under each group come its presets, with flags `Qt::ItemIsEnabled` (dragging comes in F05), `PresetIdRole` = id, and an icon of `svgPixmap(picturePath(variant.picture), 48, devicePixelRatioF())`.
  - Text: the item text is `translate("Catalog", name) + "\n" + translate("Catalog", purpose)`.
  - Tooltip: `"<b>" + type name + "</b><br>" + description` (translated, HTML-escaped), plus `"<br>" + tr("Usual wire: %1 mm²").arg(QLocale(languageCode(currentLanguage())).toString(*wireSection))` when `wireSection` is set.
  - Retranslation: `changeEvent(QEvent::LanguageChange)` resets every text and tooltip.
- Produces, `menuclicks.h`, moved verbatim from `tst_preferences.cpp`: `void clickMenuBarItem(QMainWindow &window, QMenu *menu);` and `void clickMenuItem(QMenu *menu, QAction *action);`. New: `void openSubmenu(QMenu *menu, QMenu *submenu);`, which holds `tst_preferences.cpp`'s current inline mouse-move, click and `QTRY_VERIFY(submenu->isVisible())` code.

- [ ] **Step 1: Move the menu helpers** into `menuclicks.{h,cpp}`, update `tst_preferences.cpp` to use them, and run `cmake --build build && ctest --test-dir build -R tst_preferences --output-on-failure`. Expected: PASS (pure move).
- [ ] **Step 2: Write the failing scenario test** in `tests/gui/tst_palette.cpp` (class `TestPalette`; `initTestCase`: `useTemporarySettings(); Settings::saveLanguage(Language::English); setupApplication(*qApp);`). Helpers:
  - `presetItem(tree, id)` finds a child by `PresetIdRole`;
  - `hover(tree, item)` does `QTest::mouseMove` to the item centre on the viewport, then sends `QHelpEvent(QEvent::ToolTip, centre, viewport->mapToGlobal(centre))` to the viewport.

```cpp
void TestPalette::showsProtectionPresetsWithTooltips()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *tree = window.findChild<QTreeWidget *>("paletteTree");
    QVERIFY(tree);
    QCOMPARE(tree->topLevelItemCount(), 1);
    QTreeWidgetItem *group = tree->topLevelItem(0);
    QCOMPARE(group->text(0), QStringLiteral("Protection"));
    QVERIFY(group->isExpanded());
    const QList<std::pair<QString, QString>> expected {
        { "breaker-2p-c40", "Breaker 2P C40\napartment main" },
        { "rcd-2p-40-30", "RCD 40 A 30 mA\nleakage protection" },
        { "breaker-1p-c10", "Breaker 1P C10\nlighting" },
        { "breaker-1p-c16", "Breaker 1P C16\nsockets" },
        { "breaker-1p-c32", "Breaker 1P C32\ncooktop" },
        { "rcbo-c16-30", "RCBO C16 30 mA\nsockets, breaker and RCD in one" } };
    QCOMPARE(group->childCount(), int(expected.size()));
    for (int i = 0; i < expected.size(); ++i) {
        QTreeWidgetItem *item = group->child(i);
        QCOMPARE(item->data(0, PaletteWidget::PresetIdRole).toString(), expected[i].first);
        QCOMPARE(item->text(0), expected[i].second);
        QVERIFY(!item->icon(0).isNull());
        QVERIFY(tree->viewport()->rect().contains(tree->visualItemRect(item))); // no scrolling
    }
    QTreeWidgetItem *c10 = presetItem(tree, "breaker-1p-c10");
    QVERIFY(c10->toolTip(0).contains("Circuit breaker"));
    QVERIFY(c10->toolTip(0).contains("Turns it off by itself on overload or short circuit."));
    QVERIFY(c10->toolTip(0).contains("Usual wire: 1.5 mm²"));
    QVERIFY(presetItem(tree, "rcd-2p-40-30")->toolTip(0).contains("Residual current device (RCD)"));
    QVERIFY(!presetItem(tree, "rcd-2p-40-30")->toolTip(0).contains("Usual wire"));
    QVERIFY(presetItem(tree, "rcbo-c16-30")->toolTip(0).contains("Usual wire: 2.5 mm²"));
    hover(tree, c10);
    QTRY_VERIFY(QToolTip::isVisible());
    QCOMPARE(QToolTip::text(), c10->toolTip(0));
    // grab the top-level widget named "qtooltip_label"
    QVERIFY(saveScreenshot(*tipLabel, "f04", "tooltip"));
    QToolTip::hideText();
    QVERIFY(saveScreenshot(window, "f04", "palette-dark"));
}
```

  `switchesLanguageAndThemeWithTextFitting` drives the Language menu with `clickMenuBarItem`/`clickMenuItem`:
  - **ru:** group `Защита`; `breaker-1p-c10` text `Автомат 1P C10\nосвещение`; tooltip contains `Автоматический выключатель` and `Обычный провод: 1,5 mm²`. Screenshot `palette-ru`.
  - **kk:** group `Қорғаныс`; `breaker-1p-c10` text `Автомат 1P C10\nжарықтандыру`; `rcbo-c16-30` text `Дифавтомат C16 30 mA\nрозеткалар, автомат пен УЗО бірге`; tooltip contains `Әдеттегі сым: 1,5 mm²`. Screenshot `palette-kk`.
  - **en:** group `Protection` again.
  - **Light theme:** Settings → Theme → Light with `openSubmenu`. Screenshot `palette-light`.

  `textFitsInEveryLanguage`: for ru, kk and en (switched through the menu), for every preset item and each line of its text, `QFontMetrics(tree->font()).horizontalAdvance(line) <= tree->visualItemRect(item).width() - tree->iconSize().width() - 8`.
- [ ] **Step 3: Verify red.** Run `cmake --build build`. Expected: compile error, because `app/palettewidget.h` doesn't exist.
- [ ] **Step 4: Implement** `PaletteWidget` and the `MainWindow` change per Interfaces.
- [ ] **Step 5: Translations.** Run `cmake --build build --target update_translations && ctest --test-dir build -R translations_complete --output-on-failure`. Expected: FAIL with 1 unfinished (`Usual wire: %1 mm²`) in each of ru and kk. Translate it per the table, then rerun. Expected: PASS.
- [ ] **Step 6: Verify green.** Run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass. If a translated line doesn't fit, shorten the translation; never loosen the test.
- [ ] **Step 7: Inspect** `build/screenshots/f04-{palette-dark,palette-ru,palette-kk,palette-light,tooltip}.png`. Expected:
  - The pictures are recognisable and nothing is clipped.
  - The group header stands out.
  - Kazakh letters render.
  - The light theme is legible, with the device bodies outlined.
  - The tooltip shows the bold type name, the description and the wire line.
- [ ] **Step 8: Commit** the files above with the message `F04: device palette with purpose labels and tooltips`.

### Task 5: Market products (five brands)

**Files:**
- Modify: `resources/catalog/products.json`, `tests/core/tst_catalog.cpp`, and only if a rating has no product at all: `resources/catalog/devices.json`, `deviceTypesMatchSpec` and `docs/adr/0023-catalog-schema.md`

**Interfaces:**
- Consumes: the `products.json` shape (Task 1) and `embeddedCatalog()`.

- [ ] **Step 1: Write the failing tests** in `tst_catalog.cpp`:
  - `productsHaveProvenance`: every product's brand is a key of `kBrandDomains`, and its source host equals or ends with `.` plus one of that brand's domains. Prefill `kBrandDomains`: IEK `{"iek.ru", "iek.global"}`, EKF `{"ekfgroup.com"}`, Schneider Electric `{"se.com", "schneider-electric.com"}`, ABB `{"abb.com"}`, Chint `{"chint.com", "chintglobal.com"}`. Add a regional domain only after confirming that the global site links to it.
  - `everyPresetHasAProduct`: each preset matches at least one product with the same type, poles and parameters.
  - `everyAllowedRatingHasAProduct`: for each type, variant and allowed `ratedCurrent`, at least one product exists.

  Run `ctest --test-dir build -R tst_catalog --output-on-failure`. Expected: FAIL on the two coverage tests (no products yet).
- [ ] **Step 2: Research and enter one brand at a time,** in the order IEK, EKF, Chint, Schneider Electric (Easy9), ABB. For research, load WebSearch with `ToolSearch("select:WebSearch")` and open pages with WebFetch (or the `web-fetch` agent; PDFs are read with Read). For each brand:
  - Find one commonly sold series for each kind:
    - curve-C breakers, 1P and 2P, 6–63 A;
    - 2P RCDs, type A, 30 mA, 25/32/40/63 A;
    - 1P+N RCBOs, 2 modules, type A, 30 mA, C6–C32.
  - Suggested starting points: IEK ВА47-29 / ВД1-63 / АВДТ32; EKF ВА 47-63 / ВД-100 / АД-32; Chint NXB-63 / NL1-63 / NXBLE-32; Easy9 MCB / RCCB / RCBO; ABB S200 or SH200 / F202 / DS201.
  - Open the manufacturer's catalog page or PDF, and enter one product per allowed rating that it shows:
    - `model`: the short designation, e.g. `ВА47-29 1P C16`;
    - `article`: exactly as printed;
    - `accessed`: the date you opened the page.
  - Use one series block per source URL.
  - Leave out anything you couldn't open or that doesn't match.

  After each brand, run `ctest --test-dir build -R tst_catalog --output-on-failure` (`productsHaveProvenance` must pass), then commit `products.json` (and `kBrandDomains` if changed) with the message `F04: <brand> products`. The body lists each series URL and three sample articles, with the PDF page if the source is a PDF.
- [ ] **Step 3: Spot-check.** For every series, re-open its source and confirm three articles chosen at random from those entered (rating, poles, type A, 30 mA, width). Fix or remove any mismatch and rerun `tst_catalog`. Then commit any fixes with the message `F04: product spot-check fixes`.
- [ ] **Step 4: Coverage.** Run `ctest --test-dir build -R tst_catalog --output-on-failure`. Expected: PASS. If an allowed rating has no product from any brand, remove it from `devices.json` and from `deviceTypesMatchSpec`, note it in ADR 0023 (spec rule), rerun, and commit with the message `F04: drop ratings no brand offers`.
- [ ] **Step 5: Report** for the session summary: products per brand and kind, and every series or brand left out, with the reason.

### Task 6: ПУЭ РК ampacity check

**Files:**
- Modify: `docs/adr/0023-catalog-schema.md`, or create `docs/adr/0024-*.md` and update `docs/adr/README.md` and the status line of `docs/adr/0016-electrical-reference-data.md`

- [ ] **Step 1: Find the official text.** Search for the ПУЭ РК (Правила устройства электроустановок РК, приказ Министра энергетики РК от 20 марта 2015 года № 230), preferably on `adilet.zan.kz`. Open table 1.3.4 (copper conductors, PVC or rubber insulation) and read the "два одножильных в одной трубе" column for 1.5, 2.5, 4, 6, 10 and 16 mm².
- [ ] **Step 2: Compare** with ADR 0016 (19, 27, 38, 46, 70, 85 A).
  - **Equal:** add a line to ADR 0023, "ПУЭ РК table 1.3.4 checked <date>: matches 0016", with the URL.
  - **Different:** write ADR 0024, superseding 0016, with the corrected table and the URL. Set 0016's status to `Superseded by 0024` and update the index.
  - **Not found:** add a line to ADR 0023 saying the official text wasn't found and the ПУЭ 7 values are kept.
- [ ] **Step 3: Commit** with the message `F04: check wire ampacity against ПУЭ РК`.

### Task 7: Verification and roadmap

**Files:**
- Modify: `docs/roadmap.md` (F04 ◐ → ☑)

- [ ] **Step 1: Quality gates.** Run `scripts/quality_gates.sh` in the background. Expected last line: `All quality gates passed`.
- [ ] **Step 2: Release binary.** Run `scripts/build_linux_executable.sh`. Expected: the dependency check passes with no new libraries, ending `Created dist/electricity-linux-x86_64`.
- [ ] **Step 3: Smoke test.** With `CFG=<scratchpad>/f04-config` containing `electricity/electricity.ini` with `[General]` and `language=ru` on separate lines, run `XDG_CONFIG_HOME=$CFG xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build/electricity & sleep 3; import -window root build/screenshots/f04-smoke.png; kill $!'`. Open the PNG. Expected: the dark window, with the Components dock showing "Защита" and six pictured presets with Russian text.
- [ ] **Step 4: Review all `build/screenshots/f04-*.png`** as a set. Expected: nothing clipped, both themes legible, no brand-like art.
- [ ] **Step 5: Mark F04 ☑** in `docs/roadmap.md` and commit with the message `F04: mark done`. Run `ps -eo pid,ppid,etime,cmd` and stop any leftover background process.
