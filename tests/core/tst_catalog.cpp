// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/catalogstrings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtTest>

#include <array>
#include <functional>
#include <map>

using Edit = std::function<void(CatalogSources &)>;
Q_DECLARE_METATYPE(Edit)

using namespace Qt::StringLiterals;

namespace {

// Manufacturer domains a product source may live on. Add a regional domain only after
// confirming that the global site links to it.
const std::map<QString, QStringList> kBrandDomains{
    { "IEK", { "iek.ru", "iek.global" } },
    { "EKF", { "ekfgroup.com" } },
    { "Schneider Electric", { "se.com", "schneider-electric.com" } },
    { "ABB", { "abb.com" } },
    { "Chint", { "chint.com", "chintglobal.com" } },
};

// moc mis-lexes "//" inside raw strings, hence the escaped slashes in the JSON URL.
CatalogSources fixture()
{
    CatalogSources s;
    s.devices = R"json({
        "schemaVersion": 1,
        "deviceTypes": [{
            "id": "breaker", "name": "Circuit breaker", "description": "Protects a line.",
            "designation": "QF",
            "allowedValues": {"curve": ["C"], "ratedCurrent": [10, 16]},
            "variants": [{
                "poles": "1P", "modules": 1, "picture": "breaker-1p.svg",
                "terminals": [{"name": "1", "x": 9, "y": 8}, {"name": "2", "x": 9, "y": 82}]
            }]
        }],
        "bars": [{"id": "nbar", "picture": "nbar.svg", "pitch": 9, "terminal": {"x": 4.5, "y": 8},
                  "role": "N"}]
    })json";
    s.enclosures = R"json({
        "schemaVersion": 1,
        "enclosures": [{"id": "enclosure-12", "modules": 12, "rows": 1, "nBarTerminals": 8,
                        "peBarTerminals": 8, "picture": "enclosure-12.svg",
                        "width": 280, "height": 244}]
    })json";
    s.presets = R"json({
        "schemaVersion": 1,
        "presets": [{"id": "breaker-1p-c16", "group": "Protection", "name": "Breaker 1P C16",
                     "purpose": "sockets", "type": "breaker", "poles": "1P",
                     "parameters": {"curve": "C", "ratedCurrent": 16}, "wireSection": 2.5}]
    })json";
    s.products = R"json({
        "schemaVersion": 1,
        "series": [{
            "brand": "Acme", "series": "B", "type": "breaker",
            "source": {"url": "https:\/\/example.com/b", "accessed": "2026-11-20"},
            "products": [{"model": "B16 1P", "article": "A-16", "poles": "1P",
                          "parameters": {"curve": "C", "ratedCurrent": 16}}]
        }]
    })json";
    s.loads = R"json({
        "schemaVersion": 1,
        "loads": [
            {"id": "lamp", "name": "Ceiling lamp", "description": "A lamp.", "caption": "Lamp",
             "picture": "lamp.svg", "width": 36, "height": 36, "power": 10,
             "terminals": [{"name": "L", "x": 9, "y": 4.5, "role": "L"},
                           {"name": "N", "x": 18, "y": 4.5, "role": "N"},
                           {"name": "PE", "x": 27, "y": 4.5, "role": "PE"}]},
            {"id": "socket", "name": "Double socket", "description": "A socket.",
             "caption": "Socket", "picture": "socket.svg", "width": 54, "height": 27,
             "terminals": [{"name": "L", "x": 13.5, "y": 4.5, "role": "L"},
                           {"name": "N", "x": 27, "y": 4.5, "role": "N"},
                           {"name": "PE", "x": 40.5, "y": 4.5, "role": "PE"}],
             "places": [{"x": 4.5, "y": 40.5}, {"x": 49.5, "y": 40.5}]},
            {"id": "kettle", "name": "Kettle", "description": "An appliance.",
             "picture": "kettle.svg", "width": 36, "height": 36, "power": 2000, "plugsIn": true}
        ]
    })json";
    return s;
}

QByteArray patched(const QByteArray &json, const std::function<void(QJsonObject &)> &edit)
{
    QJsonObject root = QJsonDocument::fromJson(json).object();
    edit(root);
    return QJsonDocument(root).toJson();
}

// Edits the first element of root[key] (an array of objects).
QByteArray patchedFirst(const QByteArray &json, const QString &key,
                        const std::function<void(QJsonObject &)> &edit)
{
    return patched(json, [&](QJsonObject &root) {
        QJsonArray array = root.value(key).toArray();
        QJsonObject first = array.at(0).toObject();
        edit(first);
        array.replace(0, first);
        root.insert(key, array);
    });
}

QByteArray appendCopy(const QByteArray &json, const QString &key)
{
    return patched(json, [&](QJsonObject &root) {
        QJsonArray array = root.value(key).toArray();
        array.append(array.at(0));
        root.insert(key, array);
    });
}

// Edits the first product of the first series.
QByteArray patchedProduct(const QByteArray &json, const std::function<void(QJsonObject &)> &edit)
{
    return patchedFirst(json, u"series"_s, [&](QJsonObject &series) {
        QJsonArray products = series.value("products").toArray();
        QJsonObject product = products.at(0).toObject();
        edit(product);
        products.replace(0, product);
        series.insert("products", products);
    });
}

QByteArray patchedParameters(const QByteArray &json, const QString &key,
                             const std::function<void(QJsonObject &)> &edit)
{
    return patchedFirst(json, key, [&](QJsonObject &entry) {
        QJsonObject parameters = entry.value("parameters").toObject();
        edit(parameters);
        entry.insert("parameters", parameters);
    });
}

// Sets "contacts" on the first variant of the first device type.
QByteArray withContacts(const QByteArray &json, const QJsonArray &contacts)
{
    return patchedFirst(json, u"deviceTypes"_s, [&](QJsonObject &type) {
        QJsonArray variants = type.value("variants").toArray();
        QJsonObject variant = variants.at(0).toObject();
        variant["contacts"] = contacts;
        variants.replace(0, variant);
        type["variants"] = variants;
    });
}

} // namespace

class TestCatalog : public QObject
{
    Q_OBJECT
private slots:
    void loadsValidCatalog();
    void loadsDeviceContacts();
    void rejectsInvalidCatalog_data();
    void rejectsInvalidCatalog();
    void embeddedCatalogLoadsWithoutErrors();
    void deviceTypesMatchSpec();
    void terminalRolesMatchSpec();
    void enclosuresMatchSpec();
    void loadsValidLoads();
    void rejectsInvalidLoads_data();
    void rejectsInvalidLoads();
    void loadTypesMatchSpec();
    void deviceContactsMatchSpec();
    void onlyTheCeilingLampIsALight();
    void enclosureSizesMatchSpec();
    void loadStringsAreRegisteredForTranslation();
    void presetsMatchSpec();
    void catalogStringsAreRegisteredForTranslation();
    void productsHaveProvenance();
    void everyPresetHasAProduct();
    void everyAllowedRatingHasAProduct();
};

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
    QCOMPARE(p1->terminals,
             (QList<TerminalSpec>{ { "1", { 9, 8 }, std::nullopt },
                                   { "2", { 9, 82 }, std::nullopt } }));
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

void TestCatalog::loadsDeviceContacts()
{
    const CatalogLoad plain = loadCatalog(fixture());
    QVERIFY2(plain.errors.isEmpty(), qPrintable(plain.errors.join(u'\n')));
    QVERIFY(plain.catalog.deviceType(u"breaker")->variant(u"1P")->contacts.isEmpty());

    CatalogSources sources = fixture();
    sources.devices = withContacts(sources.devices, QJsonArray{ QJsonArray{ "1", "2" } });
    const CatalogLoad load = loadCatalog(sources);
    QVERIFY2(load.errors.isEmpty(), qPrintable(load.errors.join(u'\n')));
    QCOMPARE(load.catalog.deviceType(u"breaker")->variant(u"1P")->contacts,
             (QList<ContactSpec>{ { "1", "2" } }));
}

void TestCatalog::rejectsInvalidCatalog_data()
{
    QTest::addColumn<Edit>("edit");
    QTest::addColumn<QString>("expected");

    const auto row = [](const char *name, const Edit &edit, const QString &expected) {
        QTest::newRow(name) << edit << expected;
    };
    row("bad JSON", [](CatalogSources &s) { s.devices = "{"; }, u"devices.json:"_s);
    row(
            "schema version",
            [](CatalogSources &s) {
                s.products = patched(s.products, [](QJsonObject &o) { o["schemaVersion"] = 2; });
            },
            u"products.json: unsupported schema version 2"_s);
    row(
            "missing designation",
            [](CatalogSources &s) {
                s.devices = patchedFirst(s.devices, u"deviceTypes"_s,
                                         [](QJsonObject &type) { type["designation"] = ""; });
            },
            u"breaker: missing designation"_s);
    row(
            "duplicate id",
            [](CatalogSources &s) { s.presets = appendCopy(s.presets, u"presets"_s); },
            u"duplicate id breaker-1p-c16"_s);
    row(
            "duplicate terminal",
            [](CatalogSources &s) {
                s.devices = patchedFirst(s.devices, u"deviceTypes"_s, [](QJsonObject &type) {
                    QJsonArray variants = type.value("variants").toArray();
                    QJsonObject variant = variants.at(0).toObject();
                    QJsonArray terminals = variant.value("terminals").toArray();
                    QJsonObject second = terminals.at(1).toObject();
                    second["name"] = "1";
                    terminals.replace(1, second);
                    variant["terminals"] = terminals;
                    variants.replace(0, variant);
                    type["variants"] = variants;
                });
            },
            u"duplicate terminal 1"_s);
    row(
            "contact not a pair",
            [](CatalogSources &s) {
                s.devices = withContacts(s.devices, QJsonArray{ QJsonArray{ "1" } });
            },
            u"device type breaker 1P: a contact joins two terminals"_s);
    row(
            "contact unknown terminal",
            [](CatalogSources &s) {
                s.devices = withContacts(s.devices, QJsonArray{ QJsonArray{ "1", "9" } });
            },
            u"device type breaker 1P: contact terminal 9 unknown"_s);
    row(
            "contact to itself",
            [](CatalogSources &s) {
                s.devices = withContacts(s.devices, QJsonArray{ QJsonArray{ "1", "1" } });
            },
            u"device type breaker 1P: contact joins 1 to itself"_s);
    row(
            "terminal in two contacts",
            [](CatalogSources &s) {
                s.devices = withContacts(
                        s.devices, QJsonArray{ QJsonArray{ "1", "2" }, QJsonArray{ "2", "1" } });
            },
            u"device type breaker 1P: terminal 2 is in two contacts"_s);
    row(
            "unknown type",
            [](CatalogSources &s) {
                s.presets = patchedFirst(s.presets, u"presets"_s,
                                         [](QJsonObject &p) { p["type"] = "fuse"; });
            },
            u"preset breaker-1p-c16: unknown device type fuse"_s);
    row(
            "unknown variant",
            [](CatalogSources &s) {
                s.presets = patchedFirst(s.presets, u"presets"_s,
                                         [](QJsonObject &p) { p["poles"] = "3P"; });
            },
            u"unknown variant 3P of breaker"_s);
    row(
            "value not allowed",
            [](CatalogSources &s) {
                s.products = patchedProduct(s.products, [](QJsonObject &p) {
                    QJsonObject parameters = p.value("parameters").toObject();
                    parameters["ratedCurrent"] = 17;
                    p["parameters"] = parameters;
                });
            },
            u"product Acme B16 1P: ratedCurrent 17 not allowed"_s);
    row(
            "number as text",
            [](CatalogSources &s) {
                s.presets = patchedParameters(s.presets, u"presets"_s, [](QJsonObject &parameters) {
                    parameters["ratedCurrent"] = "16";
                });
            },
            u"ratedCurrent \"16\" not allowed"_s);
    row(
            "missing parameter",
            [](CatalogSources &s) {
                s.presets = patchedParameters(s.presets, u"presets"_s, [](QJsonObject &parameters) {
                    parameters.remove("curve");
                });
            },
            u"missing parameter curve"_s);
    row(
            "unknown parameter",
            [](CatalogSources &s) {
                s.products = patchedProduct(s.products, [](QJsonObject &p) {
                    QJsonObject parameters = p.value("parameters").toObject();
                    parameters["colour"] = "red";
                    p["parameters"] = parameters;
                });
            },
            u"unknown parameter colour"_s);
    row(
            "missing name",
            [](CatalogSources &s) {
                s.presets = patchedFirst(s.presets, u"presets"_s,
                                         [](QJsonObject &p) { p["name"] = ""; });
            },
            u"preset breaker-1p-c16: missing name"_s);
    row(
            "missing source",
            [](CatalogSources &s) {
                s.products = patchedFirst(s.products, u"series"_s,
                                          [](QJsonObject &series) { series.remove("source"); });
            },
            u"series Acme B: missing source"_s);
    row(
            "plain http",
            [](CatalogSources &s) {
                s.products = patchedFirst(s.products, u"series"_s, [](QJsonObject &series) {
                    QJsonObject source = series.value("source").toObject();
                    source["url"] = "http://example.com/b";
                    series["source"] = source;
                });
            },
            u"source URL must start with https://"_s);
    row(
            "bad date",
            [](CatalogSources &s) {
                s.products = patchedFirst(s.products, u"series"_s, [](QJsonObject &series) {
                    QJsonObject source = series.value("source").toObject();
                    source["accessed"] = "20.11.2026";
                    series["source"] = source;
                });
            },
            u"source access date must be YYYY-MM-DD"_s);
    row(
            "duplicate product",
            [](CatalogSources &s) {
                s.products = patchedFirst(s.products, u"series"_s, [](QJsonObject &series) {
                    QJsonArray products = series.value("products").toArray();
                    products.append(products.at(0));
                    series["products"] = products;
                });
            },
            u"duplicate product Acme B16 1P"_s);
    row(
            "bad wire section",
            [](CatalogSources &s) {
                s.presets = patchedFirst(s.presets, u"presets"_s,
                                         [](QJsonObject &p) { p["wireSection"] = 0; });
            },
            u"wireSection must be positive"_s);
    row(
            "unknown terminal role",
            [](CatalogSources &s) {
                s.devices = patchedFirst(s.devices, u"deviceTypes"_s, [](QJsonObject &type) {
                    QJsonArray variants = type.value("variants").toArray();
                    QJsonObject variant = variants.at(0).toObject();
                    QJsonArray terminals = variant.value("terminals").toArray();
                    QJsonObject first = terminals.at(0).toObject();
                    first["role"] = "X";
                    terminals.replace(0, first);
                    variant["terminals"] = terminals;
                    variants.replace(0, variant);
                    type["variants"] = variants;
                });
            },
            u"device type breaker 1P: terminal 1 has unknown role X"_s);
    row(
            "bar missing role",
            [](CatalogSources &s) {
                s.devices = patchedFirst(s.devices, u"bars"_s,
                                         [](QJsonObject &bar) { bar.remove("role"); });
            },
            u"bar nbar: missing role"_s);
    row(
            "bar unknown role",
            [](CatalogSources &s) {
                s.devices = patchedFirst(s.devices, u"bars"_s,
                                         [](QJsonObject &bar) { bar["role"] = "Q"; });
            },
            u"bar nbar: unknown role Q"_s);
    row(
            "supply unknown role",
            [](CatalogSources &s) {
                s.devices = patched(s.devices, [](QJsonObject &root) {
                    root["supplies"] = QJsonArray{ QJsonObject{
                            { "id", "s" },
                            { "picture", "s.svg" },
                            { "terminals",
                              QJsonArray{ QJsonObject{ { "name", "L" },
                                                       { "x", 1 },
                                                       { "y", 2 },
                                                       { "role", "X" } } } } } };
                });
            },
            u"supply s: terminal L has unknown role X"_s);
    row(
            "supply duplicate terminal",
            [](CatalogSources &s) {
                const QJsonObject terminal{ { "name", "L" }, { "x", 1 }, { "y", 2 } };
                s.devices = patched(s.devices, [&](QJsonObject &root) {
                    root["supplies"] = QJsonArray{ QJsonObject{
                            { "id", "s" },
                            { "picture", "s.svg" },
                            { "terminals", QJsonArray{ terminal, terminal } } } };
                });
            },
            u"supply s: duplicate terminal L"_s);
    row(
            "no rows",
            [](CatalogSources &s) {
                s.enclosures = patchedFirst(s.enclosures, u"enclosures"_s,
                                            [](QJsonObject &e) { e["rows"] = 0; });
            },
            u"enclosure-12: rows must be positive"_s);
}

void TestCatalog::rejectsInvalidCatalog()
{
    QFETCH(Edit, edit);
    QFETCH(QString, expected);
    CatalogSources sources = fixture();
    edit(sources);
    const CatalogLoad load = loadCatalog(sources);
    const bool found = std::any_of(load.errors.cbegin(), load.errors.cend(),
                                   [&](const QString &e) { return e.contains(expected); });
    QVERIFY2(found,
             qPrintable(u"expected \"%1\" in:\n%2"_s.arg(expected, load.errors.join(u'\n'))));
}

void TestCatalog::embeddedCatalogLoadsWithoutErrors()
{
    const CatalogLoad load = loadCatalog(embeddedCatalogSources());
    QVERIFY2(load.errors.isEmpty(), qPrintable(load.errors.join(u'\n')));
}

void TestCatalog::deviceTypesMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    QStringList ids;
    for (const DeviceType &type : catalog.deviceTypes)
        ids << type.id;
    QCOMPARE(ids, (QStringList{ "breaker", "rcd", "rcbo" }));

    const auto numbers = [](std::initializer_list<int> values) {
        QJsonArray array;
        for (int v : values)
            array.append(v);
        return array;
    };
    const QList<TerminalSpec> twoPole{ { "1", { 9, 8 }, std::nullopt },
                                       { "3", { 27, 8 }, std::nullopt },
                                       { "2", { 9, 82 }, std::nullopt },
                                       { "4", { 27, 82 }, std::nullopt } };
    const QList<TerminalSpec> withNeutral{ { "1", { 9, 8 }, Conductor::L },
                                           { "N1", { 27, 8 }, Conductor::N },
                                           { "2", { 9, 82 }, Conductor::L },
                                           { "N2", { 27, 82 }, Conductor::N } };

    QCOMPARE(catalog.deviceType(u"breaker")->designation, QStringLiteral("QF"));
    QCOMPARE(catalog.deviceType(u"rcd")->designation, QStringLiteral("QD"));
    QCOMPARE(catalog.deviceType(u"rcbo")->designation, QStringLiteral("QFD"));

    const DeviceType *breaker = catalog.deviceType(u"breaker");
    QCOMPARE(breaker->name, QStringLiteral("Circuit breaker"));
    QCOMPARE(breaker->allowedValues,
             (QJsonObject{ { "curve", QJsonArray{ "C" } },
                           { "ratedCurrent", numbers({ 6, 10, 16, 20, 25, 32, 40, 50, 63 }) } }));
    QCOMPARE(breaker->variants.size(), 2);
    const DeviceVariant *b1 = breaker->variant(u"1P");
    QCOMPARE(b1->modules, 1);
    QCOMPARE(b1->picture, QStringLiteral("breaker-1p.svg"));
    QCOMPARE(b1->terminals,
             (QList<TerminalSpec>{ { "1", { 9, 8 }, Conductor::L },
                                   { "2", { 9, 82 }, Conductor::L } }));
    const DeviceVariant *b2 = breaker->variant(u"2P");
    QCOMPARE(b2->modules, 2);
    QCOMPARE(b2->picture, QStringLiteral("breaker-2p.svg"));
    QCOMPARE(b2->terminals, twoPole);

    const DeviceType *rcd = catalog.deviceType(u"rcd");
    QCOMPARE(rcd->name, QStringLiteral("Residual current device (RCD)"));
    QCOMPARE(rcd->allowedValues,
             (QJsonObject{ { "ratedCurrent", numbers({ 25, 32, 40, 63 }) },
                           { "residualCurrent", numbers({ 30 }) },
                           { "rcdType", QJsonArray{ "A" } } }));
    QCOMPARE(rcd->variants.size(), 1);
    const DeviceVariant *r2 = rcd->variant(u"2P");
    QCOMPARE(r2->modules, 2);
    QCOMPARE(r2->picture, QStringLiteral("rcd-2p.svg"));
    QCOMPARE(r2->terminals, withNeutral);

    const DeviceType *rcbo = catalog.deviceType(u"rcbo");
    QCOMPARE(rcbo->name, QStringLiteral("Residual current circuit breaker (RCBO)"));
    QCOMPARE(rcbo->allowedValues,
             (QJsonObject{ { "curve", QJsonArray{ "C" } },
                           { "ratedCurrent", numbers({ 6, 10, 16, 20, 25, 32 }) },
                           { "residualCurrent", numbers({ 30 }) },
                           { "rcdType", QJsonArray{ "A" } } }));
    QCOMPARE(rcbo->variants.size(), 1);
    const DeviceVariant *c = rcbo->variant(u"1P+N");
    QCOMPARE(c->modules, 2);
    QCOMPARE(c->picture, QStringLiteral("rcbo-1pn.svg"));
    QCOMPARE(c->terminals, withNeutral);

    QCOMPARE(catalog.bars.size(), 2);
    QCOMPARE(catalog.bars.at(0).id, QStringLiteral("nbar"));
    QCOMPARE(catalog.bars.at(0).picture, QStringLiteral("nbar.svg"));
    QCOMPARE(catalog.bars.at(1).id, QStringLiteral("pebar"));
    QCOMPARE(catalog.bars.at(1).picture, QStringLiteral("pebar.svg"));
    for (const BarType &bar : catalog.bars) {
        QCOMPARE(bar.pitch, 9.0);
        QCOMPARE(bar.terminal, QPointF(4.5, 8));
    }
}

void TestCatalog::terminalRolesMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    const auto roles = [](const DeviceVariant *variant) {
        QList<std::optional<Conductor>> result;
        for (const TerminalSpec &t : variant->terminals)
            result << t.role;
        return result;
    };
    using Roles = QList<std::optional<Conductor>>;
    const std::optional<Conductor> L = Conductor::L, N = Conductor::N;
    QCOMPARE(roles(catalog.deviceType(u"breaker")->variant(u"1P")), (Roles{ L, L }));
    QCOMPARE(roles(catalog.deviceType(u"breaker")->variant(u"2P")),
             (Roles{ std::nullopt, std::nullopt, std::nullopt, std::nullopt }));
    // Terminal order is 1, N1, 2, N2.
    QCOMPARE(roles(catalog.deviceType(u"rcd")->variant(u"2P")), (Roles{ L, N, L, N }));
    QCOMPARE(roles(catalog.deviceType(u"rcbo")->variant(u"1P+N")), (Roles{ L, N, L, N }));
    QCOMPARE(catalog.bars.at(0).role, Conductor::N);
    QCOMPARE(catalog.bars.at(1).role, Conductor::PE);

    const SupplyType *supply = catalog.supply(u"supply-1p");
    QVERIFY(supply && !catalog.supply(u"nope"));
    QCOMPARE(supply->picture, QStringLiteral("supply-1p.svg"));
    QCOMPARE(supply->terminals,
             (QList<TerminalSpec>{ { "L", { 9, 28 }, Conductor::L },
                                   { "N", { 27, 28 }, Conductor::N },
                                   { "PE", { 45, 28 }, Conductor::PE } }));
}

void TestCatalog::enclosuresMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    QCOMPARE(catalog.enclosures.size(), 3);
    struct Expected
    {
        const char *id;
        int modules, rows, bars;
    };
    const std::array<Expected, 3> expected = { { { "enclosure-12", 12, 1, 8 },
                                                 { "enclosure-24", 24, 2, 12 },
                                                 { "enclosure-36", 36, 3, 16 } } };
    for (int i = 0; i < 3; ++i) {
        const EnclosureType &e = catalog.enclosures.at(i);
        QCOMPARE(e.id, QString::fromLatin1(expected[i].id));
        QCOMPARE(e.modules, expected[i].modules);
        QCOMPARE(e.rows, expected[i].rows);
        QCOMPARE(e.nBarTerminals, expected[i].bars);
        QCOMPARE(e.peBarTerminals, expected[i].bars);
        QCOMPARE(e.picture, QStringLiteral("enclosure-%1.svg").arg(expected[i].modules));
    }
}

// Edits the load with the given id.
QByteArray patchedLoad(const QByteArray &json, const QString &id,
                       const std::function<void(QJsonObject &)> &edit)
{
    return patched(json, [&](QJsonObject &root) {
        QJsonArray array = root.value("loads").toArray();
        for (int i = 0; i < array.size(); ++i) {
            QJsonObject o = array.at(i).toObject();
            if (o.value("id").toString() != id)
                continue;
            edit(o);
            array.replace(i, o);
        }
        root.insert("loads", array);
    });
}

void TestCatalog::loadsValidLoads()
{
    const CatalogLoad load = loadCatalog(fixture());
    QVERIFY2(load.errors.isEmpty(), qPrintable(load.errors.join(u'\n')));
    const Catalog &catalog = load.catalog;
    QVERIFY(!catalog.loadType(u"fridge"));
    const LoadType *kettle = catalog.loadType(u"kettle");
    QVERIFY(kettle);
    QVERIFY(kettle->plugsIn);
    QCOMPARE(kettle->power, 2000);
    QCOMPARE(kettle->size, QSizeF(36, 36));
    const LoadType *socket = catalog.loadType(u"socket");
    QVERIFY(socket);
    QCOMPARE(socket->places.size(), 2);
    QCOMPARE(socket->power, 0);
    QCOMPARE(socket->terminals.size(), 3);
    QCOMPARE(socket->terminals.at(0).role, std::optional(Conductor::L));
    QCOMPARE(socket->terminals.at(1).role, std::optional(Conductor::N));
    QCOMPARE(socket->terminals.at(2).role, std::optional(Conductor::PE));
    QCOMPARE(catalog.loadType(u"lamp")->caption, QStringLiteral("Lamp"));
    QCOMPARE(catalog.enclosures.at(0).size, QSizeF(280, 244));
}

void TestCatalog::rejectsInvalidLoads_data()
{
    QTest::addColumn<Edit>("edit");
    QTest::addColumn<QString>("expected");

    const auto row = [](const char *name, const Edit &edit, const QString &expected) {
        QTest::newRow(name) << edit << expected;
    };
    const auto load = [](const QString &id, const std::function<void(QJsonObject &)> &change) {
        return Edit([=](CatalogSources &s) { s.loads = patchedLoad(s.loads, id, change); });
    };
    row("missing name", load(u"lamp"_s, [](QJsonObject &o) { o["name"] = ""; }),
        u"load lamp: missing name"_s);
    row("missing description", load(u"lamp"_s, [](QJsonObject &o) { o["description"] = ""; }),
        u"load lamp: missing description"_s);
    row("missing picture", load(u"lamp"_s, [](QJsonObject &o) { o["picture"] = ""; }),
        u"load lamp: missing picture"_s);
    row("zero width", load(u"lamp"_s, [](QJsonObject &o) { o["width"] = 0; }),
        u"load lamp: width and height must be positive"_s);
    row("negative height", load(u"lamp"_s, [](QJsonObject &o) { o["height"] = -1; }),
        u"load lamp: width and height must be positive"_s);
    row("zero power", load(u"lamp"_s, [](QJsonObject &o) { o["power"] = 0; }),
        u"load lamp: power must be 1 to 15000 W"_s);
    row("huge power", load(u"kettle"_s, [](QJsonObject &o) { o["power"] = 15001; }),
        u"load kettle: power must be 1 to 15000 W"_s);
    row("socket with power", load(u"socket"_s, [](QJsonObject &o) { o["power"] = 5; }),
        u"load socket: a socket has no power"_s);
    row("plug-in with terminals",
        load(u"kettle"_s,
             [](QJsonObject &o) {
                 o["terminals"] = QJsonArray{ QJsonObject{ { "name", "L" }, { "role", "L" } } };
             }),
        u"load kettle: a plug-in appliance has no terminals, places or caption"_s);
    row("plug-in with places",
        load(u"kettle"_s,
             [](QJsonObject &o) { o["places"] = QJsonArray{ QJsonObject{}, QJsonObject{} }; }),
        u"load kettle: a plug-in appliance has no terminals, places or caption"_s);
    row("plug-in with caption", load(u"kettle"_s, [](QJsonObject &o) { o["caption"] = "Kettle"; }),
        u"load kettle: a plug-in appliance has no terminals, places or caption"_s);
    row("two terminals",
        load(u"lamp"_s,
             [](QJsonObject &o) {
                 QJsonArray terminals = o.value("terminals").toArray();
                 terminals.removeLast();
                 o["terminals"] = terminals;
             }),
        u"load lamp: terminals must be L, N and PE with matching roles"_s);
    row("mismatched role",
        load(u"lamp"_s,
             [](QJsonObject &o) {
                 QJsonArray terminals = o.value("terminals").toArray();
                 QJsonObject first = terminals.at(0).toObject();
                 first["role"] = "N";
                 terminals.replace(0, first);
                 o["terminals"] = terminals;
             }),
        u"load lamp: terminals must be L, N and PE with matching roles"_s);
    row("missing caption", load(u"lamp"_s, [](QJsonObject &o) { o["caption"] = ""; }),
        u"load lamp: missing caption"_s);
    row("one place",
        load(u"socket"_s,
             [](QJsonObject &o) {
                 QJsonArray places = o.value("places").toArray();
                 places.removeLast();
                 o["places"] = places;
             }),
        u"load socket: a socket needs two places"_s);
    row(
            "duplicate load", [](CatalogSources &s) { s.loads = appendCopy(s.loads, u"loads"_s); },
            u"duplicate id lamp"_s);
    row(
            "enclosure without size",
            [](CatalogSources &s) {
                s.enclosures = patchedFirst(s.enclosures, u"enclosures"_s,
                                            [](QJsonObject &e) { e["width"] = 0; });
            },
            u"enclosure-12: width and height must be positive"_s);
}

void TestCatalog::rejectsInvalidLoads()
{
    QFETCH(Edit, edit);
    QFETCH(QString, expected);
    CatalogSources sources = fixture();
    edit(sources);
    const CatalogLoad load = loadCatalog(sources);
    QVERIFY2(load.errors.contains(expected),
             qPrintable(u"expected \"%1\" in:\n%2"_s.arg(expected, load.errors.join(u'\n'))));
}

void TestCatalog::loadTypesMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    struct Expected
    {
        const char *id, *name, *caption;
        int power;
        bool plugsIn;
        QSizeF size;
        QList<QPointF> terminals; // L, N, PE
    };
    const QList<QPointF> noTerminals;
    const std::array<Expected, 10> expected = { {
            { "ceiling-lamp",
              "Ceiling lamp",
              "Lamp",
              10,
              false,
              { 36, 36 },
              { { 9, 4.5 }, { 18, 4.5 }, { 27, 4.5 } } },
            { "socket",
              "Double socket",
              "Socket",
              0,
              false,
              { 54, 27 },
              { { 13.5, 4.5 }, { 27, 4.5 }, { 40.5, 4.5 } } },
            { "kettle", "Kettle", "", 2000, true, { 36, 36 }, noTerminals },
            { "washing-machine", "Washing machine", "", 2200, true, { 36, 36 }, noTerminals },
            { "refrigerator", "Refrigerator", "", 150, true, { 36, 36 }, noTerminals },
            { "tv", "TV", "", 120, true, { 36, 36 }, noTerminals },
            { "microwave", "Microwave", "", 1200, true, { 36, 36 }, noTerminals },
            { "air-conditioner", "Air conditioner", "", 1000, true, { 36, 36 }, noTerminals },
            { "water-heater", "Water heater", "", 2000, true, { 36, 36 }, noTerminals },
            { "cooktop",
              "Electric cooktop",
              "Cooktop",
              7000,
              false,
              { 54, 36 },
              { { 18, 4.5 }, { 27, 4.5 }, { 36, 4.5 } } },
    } };
    QCOMPARE(catalog.loads.size(), 10);
    for (int i = 0; i < 10; ++i) {
        const LoadType &l = catalog.loads.at(i);
        const Expected &e = expected[i];
        QCOMPARE(l.id, QString::fromLatin1(e.id));
        QCOMPARE(l.name, QString::fromLatin1(e.name));
        QCOMPARE(l.caption, QString::fromLatin1(e.caption));
        QCOMPARE(l.power, e.power);
        QCOMPARE(l.plugsIn, e.plugsIn);
        QCOMPARE(l.picture, QString::fromLatin1(e.id) + u".svg"_s);
        QCOMPARE(l.size, e.size);
        QVERIFY(!l.description.isEmpty());
        QCOMPARE(l.terminals.size(), e.terminals.size());
        const std::array<Conductor, 3> roles = { Conductor::L, Conductor::N, Conductor::PE };
        const std::array<const char *, 3> names = { "L", "N", "PE" };
        for (int t = 0; t < e.terminals.size(); ++t) {
            QCOMPARE(l.terminals.at(t).name, QString::fromLatin1(names[t]));
            QCOMPARE(l.terminals.at(t).position, e.terminals.at(t));
            QCOMPARE(l.terminals.at(t).role, std::optional(roles[t]));
        }
        const bool isSocket = l.id == u"socket";
        const QList<QPointF> socketPlaces{ { 4.5, 40.5 }, { 49.5, 40.5 } };
        QCOMPARE(l.places, isSocket ? socketPlaces : QList<QPointF>());
    }
}

void TestCatalog::deviceContactsMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    const auto contacts = [&](QStringView type, QStringView poles) {
        return catalog.deviceType(type)->variant(poles)->contacts;
    };
    QCOMPARE(contacts(u"breaker", u"1P"), (QList<ContactSpec>{ { "1", "2" } }));
    QCOMPARE(contacts(u"breaker", u"2P"), (QList<ContactSpec>{ { "1", "2" }, { "3", "4" } }));
    QCOMPARE(contacts(u"rcd", u"2P"), (QList<ContactSpec>{ { "1", "2" }, { "N1", "N2" } }));
    QCOMPARE(contacts(u"rcbo", u"1P+N"), (QList<ContactSpec>{ { "1", "2" }, { "N1", "N2" } }));
}

void TestCatalog::onlyTheCeilingLampIsALight()
{
    const Catalog &catalog = embeddedCatalog();
    QCOMPARE(catalog.loads.size(), 10);
    for (const LoadType &load : catalog.loads)
        QCOMPARE(load.light, load.id == u"ceiling-lamp");
}

void TestCatalog::enclosureSizesMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    QCOMPARE(catalog.enclosures.size(), 3);
    QCOMPARE(catalog.enclosures.at(0).size, QSizeF(280, 244));
    QCOMPARE(catalog.enclosures.at(1).size, QSizeF(280, 368));
    QCOMPARE(catalog.enclosures.at(2).size, QSizeF(280, 492));
}

void TestCatalog::loadStringsAreRegisteredForTranslation()
{
    const QStringList registered = catalogTranslationSources();
    const auto check
            = [&](const QString &text) { QVERIFY2(registered.contains(text), qPrintable(text)); };
    check(u"Loads"_s);
    for (const LoadType &l : embeddedCatalog().loads) {
        check(l.name);
        check(l.description);
        if (!l.caption.isEmpty())
            check(l.caption);
    }
}

void TestCatalog::presetsMatchSpec()
{
    const Catalog &catalog = embeddedCatalog();
    const auto params
            = [](const QString &curve, int rated, int residual = 0, const QString &rcdType = {}) {
                  QJsonObject o;
                  if (!curve.isEmpty())
                      o.insert("curve", curve);
                  o.insert("ratedCurrent", rated);
                  if (residual)
                      o.insert("residualCurrent", residual);
                  if (!rcdType.isEmpty())
                      o.insert("rcdType", rcdType);
                  return o;
              };
    struct Expected
    {
        const char *id, *name, *purpose, *type, *poles;
        QJsonObject parameters;
        std::optional<double> wireSection;
    };
    const std::array<Expected, 6> expected = { {
            { "breaker-2p-c40", "Breaker 2P C40", "apartment main", "breaker", "2P",
              params("C", 40), 10 },
            { "rcd-2p-40-30", "RCD 40 A 30 mA", "leakage protection", "rcd", "2P",
              params({}, 40, 30, "A"), std::nullopt },
            { "breaker-1p-c10", "Breaker 1P C10", "lighting", "breaker", "1P", params("C", 10),
              1.5 },
            { "breaker-1p-c16", "Breaker 1P C16", "sockets", "breaker", "1P", params("C", 16),
              2.5 },
            { "breaker-1p-c32", "Breaker 1P C32", "cooktop", "breaker", "1P", params("C", 32), 6 },
            { "rcbo-c16-30", "RCBO C16 30 mA", "sockets, breaker and RCD in one", "rcbo", "1P+N",
              params("C", 16, 30, "A"), 2.5 },
    } };
    QCOMPARE(catalog.presets.size(), 6);
    for (int i = 0; i < 6; ++i) {
        const Preset &p = catalog.presets.at(i);
        QCOMPARE(p.id, QString::fromLatin1(expected[i].id));
        QCOMPARE(p.group, QStringLiteral("Protection"));
        QCOMPARE(p.name, QString::fromLatin1(expected[i].name));
        QCOMPARE(p.purpose, QString::fromLatin1(expected[i].purpose));
        QCOMPARE(p.type, QString::fromLatin1(expected[i].type));
        QCOMPARE(p.poles, QString::fromLatin1(expected[i].poles));
        QCOMPARE(p.parameters, expected[i].parameters);
        QCOMPARE(p.wireSection, expected[i].wireSection);
    }
}

void TestCatalog::catalogStringsAreRegisteredForTranslation()
{
    const Catalog &catalog = embeddedCatalog();
    const QStringList registered = catalogTranslationSources();
    const auto check
            = [&](const QString &text) { QVERIFY2(registered.contains(text), qPrintable(text)); };
    for (const DeviceType &type : catalog.deviceTypes) {
        check(type.name);
        check(type.description);
    }
    for (const Preset &preset : catalog.presets) {
        check(preset.group);
        check(preset.name);
        check(preset.purpose);
    }
}

void TestCatalog::productsHaveProvenance()
{
    for (const Product &product : embeddedCatalog().products) {
        const QString label = product.brand + u' ' + product.model;
        const auto brand = kBrandDomains.find(product.brand);
        QVERIFY2(brand != kBrandDomains.end(), qPrintable(u"unknown brand: "_s + label));
        const QString host = QUrl(product.sourceUrl).host();
        const bool official = std::any_of(brand->second.cbegin(), brand->second.cend(),
                                          [&](const QString &domain) {
                                              return host == domain || host.endsWith(u'.' + domain);
                                          });
        QVERIFY2(official, qPrintable(label + u": source not on the brand's domain: "_s + host));
    }
}

void TestCatalog::everyPresetHasAProduct()
{
    const Catalog &catalog = embeddedCatalog();
    for (const Preset &preset : catalog.presets) {
        const bool found = std::any_of(catalog.products.cbegin(), catalog.products.cend(),
                                       [&](const Product &p) {
                                           return p.type == preset.type && p.poles == preset.poles
                                                   && p.parameters == preset.parameters;
                                       });
        QVERIFY2(found, qPrintable(u"no product for preset "_s + preset.id));
    }
}

void TestCatalog::everyAllowedRatingHasAProduct()
{
    const Catalog &catalog = embeddedCatalog();
    QStringList missing;
    for (const DeviceType &type : catalog.deviceTypes) {
        for (const DeviceVariant &variant : type.variants) {
            for (const QJsonValue rating : type.allowedValues.value("ratedCurrent").toArray()) {
                const bool found = std::any_of(
                        catalog.products.cbegin(), catalog.products.cend(), [&](const Product &p) {
                            return p.type == type.id && p.poles == variant.poles
                                    && p.parameters.value("ratedCurrent") == rating;
                        });
                if (!found)
                    missing << u"%1 %2 %3 A"_s.arg(type.id, variant.poles).arg(rating.toInt());
            }
        }
    }
    QVERIFY2(missing.isEmpty(), qPrintable(u"no product for: "_s + missing.join(u", "_s)));
}

QTEST_APPLESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
