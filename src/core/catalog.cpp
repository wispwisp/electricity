// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

const DeviceVariant *DeviceType::variant(QStringView poles) const
{
    for (const DeviceVariant &v : variants) {
        if (v.poles == poles)
            return &v;
    }
    return nullptr;
}

const DeviceType *Catalog::deviceType(QStringView id) const
{
    for (const DeviceType &t : deviceTypes) {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

using namespace Qt::StringLiterals;

namespace {

QString compact(const QJsonValue &value)
{
    return QString::fromUtf8(
            QJsonDocument(QJsonArray{ value }).toJson(QJsonDocument::Compact).sliced(1).chopped(1));
}

// Parses one file and returns its root object; nullopt (with an error recorded) when unusable.
std::optional<QJsonObject> parseFile(const QByteArray &bytes, const QString &file,
                                     QStringList &errors)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        errors << u"%1: %2"_s.arg(file,
                                  parseError.error != QJsonParseError::NoError
                                          ? parseError.errorString()
                                          : u"root must be an object"_s);
        return std::nullopt;
    }
    const int version = doc.object().value("schemaVersion").toInt(0);
    if (version != 1) {
        errors << u"%1: unsupported schema version %2"_s.arg(file).arg(version);
        return std::nullopt;
    }
    return doc.object();
}

void checkUnique(QSet<QString> &seen, const QString &id, QStringList &errors)
{
    if (seen.contains(id))
        errors << u"duplicate id %1"_s.arg(id);
    seen.insert(id);
}

void requireText(const QString &value, const QString &field, const QString &where,
                 QStringList &errors)
{
    if (value.isEmpty())
        errors << u"%1: missing %2"_s.arg(where, field);
}

void checkParameters(const QJsonObject &parameters, const DeviceType &type, const QString &where,
                     QStringList &errors)
{
    for (auto it = type.allowedValues.begin(); it != type.allowedValues.end(); ++it) {
        if (!parameters.contains(it.key()))
            errors << u"%1: missing parameter %2"_s.arg(where, it.key());
    }
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        if (!type.allowedValues.contains(it.key()))
            errors << u"%1: unknown parameter %2"_s.arg(where, it.key());
        else if (!type.allowedValues.value(it.key()).toArray().contains(it.value()))
            errors << u"%1: %2 %3 not allowed"_s.arg(where, it.key(), compact(it.value()));
    }
}

// Checks that type and poles exist and returns the type, or nullptr.
const DeviceType *checkTypeAndPoles(const Catalog &catalog, const QString &typeId,
                                    const QString &poles, const QString &where, QStringList &errors)
{
    const DeviceType *type = catalog.deviceType(typeId);
    if (!type) {
        errors << u"%1: unknown device type %2"_s.arg(where, typeId);
        return nullptr;
    }
    if (!type->variant(poles))
        errors << u"%1: unknown variant %2 of %3"_s.arg(where, poles, typeId);
    return type;
}

void loadDevices(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    QSet<QString> ids;
    for (const auto &tv : root.value("deviceTypes").toArray()) {
        const QJsonObject o = tv.toObject();
        DeviceType type;
        type.id = o.value("id").toString();
        type.name = o.value("name").toString();
        type.description = o.value("description").toString();
        type.allowedValues = o.value("allowedValues").toObject();
        checkUnique(ids, type.id, errors);
        const QString where = u"device type %1"_s.arg(type.id);
        requireText(type.id, u"id"_s, where, errors);
        requireText(type.name, u"name"_s, where, errors);
        for (const auto &vv : o.value("variants").toArray()) {
            const QJsonObject vo = vv.toObject();
            DeviceVariant variant;
            variant.poles = vo.value("poles").toString();
            variant.modules = vo.value("modules").toInt();
            variant.picture = vo.value("picture").toString();
            QSet<QString> names;
            for (const auto &terminalValue : vo.value("terminals").toArray()) {
                const QJsonObject to = terminalValue.toObject();
                TerminalSpec terminal{ to.value("name").toString(),
                                       { to.value("x").toDouble(), to.value("y").toDouble() } };
                if (names.contains(terminal.name))
                    errors << u"%1 %2: duplicate terminal %3"_s.arg(where, variant.poles,
                                                                    terminal.name);
                names.insert(terminal.name);
                variant.terminals << terminal;
            }
            type.variants << variant;
        }
        catalog.deviceTypes << type;
    }
    ids.clear();
    for (const auto &bv : root.value("bars").toArray()) {
        const QJsonObject o = bv.toObject();
        const QJsonObject t = o.value("terminal").toObject();
        BarType bar{ o.value("id").toString(),
                     o.value("picture").toString(),
                     o.value("pitch").toDouble(),
                     { t.value("x").toDouble(), t.value("y").toDouble() } };
        checkUnique(ids, bar.id, errors);
        catalog.bars << bar;
    }
}

void loadEnclosures(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    QSet<QString> ids;
    for (const auto &ev : root.value("enclosures").toArray()) {
        const QJsonObject o = ev.toObject();
        EnclosureType enclosure{
            o.value("id").toString(),          o.value("modules").toInt(),
            o.value("rows").toInt(),           o.value("nBarTerminals").toInt(),
            o.value("peBarTerminals").toInt(), o.value("picture").toString()
        };
        checkUnique(ids, enclosure.id, errors);
        if (enclosure.rows <= 0)
            errors << u"%1: rows must be positive"_s.arg(enclosure.id);
        catalog.enclosures << enclosure;
    }
}

void loadPresets(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    QSet<QString> ids;
    for (const auto &pv : root.value("presets").toArray()) {
        const QJsonObject o = pv.toObject();
        Preset preset;
        preset.id = o.value("id").toString();
        preset.group = o.value("group").toString();
        preset.name = o.value("name").toString();
        preset.purpose = o.value("purpose").toString();
        preset.type = o.value("type").toString();
        preset.poles = o.value("poles").toString();
        preset.parameters = o.value("parameters").toObject();
        checkUnique(ids, preset.id, errors);
        const QString where = u"preset %1"_s.arg(preset.id);
        requireText(preset.id, u"id"_s, where, errors);
        requireText(preset.group, u"group"_s, where, errors);
        requireText(preset.name, u"name"_s, where, errors);
        requireText(preset.purpose, u"purpose"_s, where, errors);
        if (o.contains("wireSection")) {
            preset.wireSection = o.value("wireSection").toDouble();
            if (*preset.wireSection <= 0)
                errors << u"%1: wireSection must be positive"_s.arg(where);
        }
        if (const DeviceType *type
            = checkTypeAndPoles(catalog, preset.type, preset.poles, where, errors))
            checkParameters(preset.parameters, *type, where, errors);
        catalog.presets << preset;
    }
}

void loadProducts(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    QSet<QString> keys;
    for (const auto &sv : root.value("series").toArray()) {
        const QJsonObject so = sv.toObject();
        Product base;
        base.brand = so.value("brand").toString();
        base.series = so.value("series").toString();
        base.type = so.value("type").toString();
        const QString seriesWhere = u"series %1 %2"_s.arg(base.brand, base.series);
        requireText(base.brand, u"brand"_s, seriesWhere, errors);
        requireText(base.series, u"series"_s, seriesWhere, errors);
        if (!so.value("source").isObject()) {
            errors << u"%1: missing source"_s.arg(seriesWhere);
        } else {
            const QJsonObject source = so.value("source").toObject();
            base.sourceUrl = source.value("url").toString();
            if (!base.sourceUrl.startsWith(u"https://"))
                errors << u"%1: source URL must start with https://"_s.arg(seriesWhere);
            base.accessed = QDate::fromString(source.value("accessed").toString(), Qt::ISODate);
            if (!base.accessed.isValid())
                errors << u"%1: source access date must be YYYY-MM-DD"_s.arg(seriesWhere);
        }
        for (const auto &pv : so.value("products").toArray()) {
            const QJsonObject po = pv.toObject();
            Product product = base;
            product.model = po.value("model").toString();
            product.article = po.value("article").toString();
            product.poles = po.value("poles").toString();
            product.parameters = po.value("parameters").toObject();
            const QString where = u"product %1 %2"_s.arg(product.brand, product.model);
            requireText(product.model, u"model"_s, where, errors);
            const QString key = product.brand + u'\n' + product.model;
            if (keys.contains(key))
                errors << u"duplicate product %1 %2"_s.arg(product.brand, product.model);
            keys.insert(key);
            if (const DeviceType *type
                = checkTypeAndPoles(catalog, product.type, product.poles, where, errors))
                checkParameters(product.parameters, *type, where, errors);
            catalog.products << product;
        }
    }
}

} // namespace

CatalogLoad loadCatalog(const CatalogSources &sources)
{
    CatalogLoad load;
    // Devices load first: presets and products are validated against the device types.
    if (const auto root = parseFile(sources.devices, u"devices.json"_s, load.errors))
        loadDevices(*root, load.catalog, load.errors);
    if (const auto root = parseFile(sources.enclosures, u"enclosures.json"_s, load.errors))
        loadEnclosures(*root, load.catalog, load.errors);
    if (const auto root = parseFile(sources.presets, u"presets.json"_s, load.errors))
        loadPresets(*root, load.catalog, load.errors);
    if (const auto root = parseFile(sources.products, u"products.json"_s, load.errors))
        loadProducts(*root, load.catalog, load.errors);
    return load;
}

namespace {

QByteArray readResource(const QString &name)
{
    QFile file(u":/catalog/%1.json"_s.arg(name));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("missing embedded catalog file %s", qPrintable(name));
    return file.readAll();
}

} // namespace

CatalogSources embeddedCatalogSources()
{
    return { readResource(u"devices"_s), readResource(u"enclosures"_s), readResource(u"presets"_s),
             readResource(u"products"_s) };
}

const Catalog &embeddedCatalog()
{
    static const Catalog catalog = [] {
        const CatalogLoad load = loadCatalog(embeddedCatalogSources());
        if (!load.errors.isEmpty())
            qFatal("broken embedded catalog:\n%s", qPrintable(load.errors.join(u'\n')));
        return load.catalog;
    }();
    return catalog;
}
