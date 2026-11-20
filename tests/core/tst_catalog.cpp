// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <functional>

using Edit = std::function<void(CatalogSources &)>;
Q_DECLARE_METATYPE(Edit)

using namespace Qt::StringLiterals;

namespace {

// moc mis-lexes "//" inside raw strings, hence the escaped slashes in the JSON URL.
CatalogSources fixture()
{
    CatalogSources s;
    s.devices = R"json({
        "schemaVersion": 1,
        "deviceTypes": [{
            "id": "breaker", "name": "Circuit breaker", "description": "Protects a line.",
            "allowedValues": {"curve": ["C"], "ratedCurrent": [10, 16]},
            "variants": [{
                "poles": "1P", "modules": 1, "picture": "breaker-1p.svg",
                "terminals": [{"name": "1", "x": 9, "y": 8}, {"name": "2", "x": 9, "y": 82}]
            }]
        }],
        "bars": [{"id": "nbar", "picture": "nbar.svg", "pitch": 9, "terminal": {"x": 4.5, "y": 8}}]
    })json";
    s.enclosures = R"json({
        "schemaVersion": 1,
        "enclosures": [{"id": "enclosure-12", "modules": 12, "rows": 1, "nBarTerminals": 8,
                        "peBarTerminals": 8, "picture": "enclosure-12.svg"}]
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

} // namespace

class TestCatalog : public QObject
{
    Q_OBJECT
private slots:
    void loadsValidCatalog();
    void rejectsInvalidCatalog_data();
    void rejectsInvalidCatalog();
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

QTEST_APPLESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
