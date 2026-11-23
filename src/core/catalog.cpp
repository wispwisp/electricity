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

const SupplyType *Catalog::supply(QStringView id) const
{
    for (const SupplyType &s : supplies) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

const LoadType *Catalog::loadType(QStringView id) const
{
    for (const LoadType &l : loads) {
        if (l.id == id)
            return &l;
    }
    return nullptr;
}

double Catalog::ampacity(double section) const
{
    for (const WireRating &r : wireRatings) {
        if (r.section == section)
            return r.ampacity;
    }
    return 0;
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

// Maps "L"/"N"/"PE"; nullopt for anything else.
std::optional<Conductor> parseRole(const QString &text)
{
    if (text == u"L")
        return Conductor::L;
    if (text == u"N")
        return Conductor::N;
    if (text == u"PE")
        return Conductor::PE;
    return std::nullopt;
}

// Reads the optional "role" of a terminal; records an error for an unknown one.
std::optional<Conductor> terminalRole(const QJsonObject &o, const QString &where,
                                      QStringList &errors)
{
    if (!o.contains("role"))
        return std::nullopt;
    const QString text = o.value("role").toString();
    const std::optional<Conductor> role = parseRole(text);
    if (!role)
        errors << u"%1: terminal %2 has unknown role %3"_s.arg(where, o.value("name").toString(),
                                                               text);
    return role;
}

// Reads a device type's optional "protection" object.
std::optional<ProtectionSpec> parseProtection(const QJsonObject &o, const DeviceType &type,
                                              const QString &where, QStringList &errors)
{
    if (!o.contains("protection"))
        return std::nullopt;
    const QJsonObject po = o.value("protection").toObject();
    ProtectionSpec spec;
    spec.timeConstant = po.value("timeConstant").toDouble();
    if (spec.timeConstant <= 0)
        errors << u"%1: protection time constant must be positive"_s.arg(where);
    if (po.contains("tripFactor")) {
        spec.tripFactor = po.value("tripFactor").toDouble();
        if (*spec.tripFactor <= 0)
            errors << u"%1: trip factor must be positive"_s.arg(where);
    }
    const QJsonObject curves = po.value("curves").toObject();
    for (auto it = curves.begin(); it != curves.end(); ++it) {
        const QJsonArray band = it.value().toArray();
        const bool valid = band.size() == 2 && band.at(0).isDouble() && band.at(1).isDouble()
                && band.at(0).toDouble() > 0 && band.at(0).toDouble() <= band.at(1).toDouble();
        if (!valid) {
            errors << u"%1: curve %2 needs 0 < lower <= upper"_s.arg(where, it.key());
            continue;
        }
        spec.curves.insert(it.key(), { band.at(0).toDouble(), band.at(1).toDouble() });
    }
    for (const auto &curve : type.allowedValues.value("curve").toArray()) {
        if (!curves.contains(curve.toString()))
            errors << u"%1: allowed curve %2 has no band"_s.arg(where, curve.toString());
    }
    return spec;
}

void loadWires(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    if (!root.contains("wires"))
        return;
    const QJsonObject o = root.value("wires").toObject();
    catalog.wireTimeConstant = o.value("timeConstant").toDouble();
    if (catalog.wireTimeConstant <= 0)
        errors << u"wires: time constant must be positive"_s;
    QSet<double> sections;
    for (const auto &rv : o.value("ratings").toArray()) {
        const QJsonObject ro = rv.toObject();
        const WireRating rating{ ro.value("section").toDouble(), ro.value("ampacity").toDouble() };
        const QString section = QString::number(rating.section);
        if (rating.ampacity <= 0)
            errors << u"wires: ampacity of %1 mm\u00b2 must be positive"_s.arg(section);
        if (sections.contains(rating.section))
            errors << u"wires: duplicate section %1"_s.arg(section);
        sections.insert(rating.section);
        catalog.wireRatings << rating;
    }
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
        type.designation = o.value("designation").toString();
        type.allowedValues = o.value("allowedValues").toObject();
        checkUnique(ids, type.id, errors);
        const QString where = u"device type %1"_s.arg(type.id);
        requireText(type.id, u"id"_s, where, errors);
        requireText(type.name, u"name"_s, where, errors);
        requireText(type.designation, u"designation"_s, where, errors);
        type.protection = parseProtection(o, type, where, errors);
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
                                       { to.value("x").toDouble(), to.value("y").toDouble() },
                                       terminalRole(to, u"%1 %2"_s.arg(where, variant.poles),
                                                    errors) };
                if (names.contains(terminal.name))
                    errors << u"%1 %2: duplicate terminal %3"_s.arg(where, variant.poles,
                                                                    terminal.name);
                names.insert(terminal.name);
                variant.terminals << terminal;
            }
            QSet<QString> contacted;
            for (const auto &contactValue : vo.value("contacts").toArray()) {
                const QJsonArray pair = contactValue.toArray();
                if (pair.size() != 2 || !pair.at(0).isString() || !pair.at(1).isString()) {
                    errors << u"%1 %2: a contact joins two terminals"_s.arg(where, variant.poles);
                    continue;
                }
                const ContactSpec contact{ pair.at(0).toString(), pair.at(1).toString() };
                bool valid = true;
                for (const QString &name : { contact.top, contact.bottom }) {
                    if (!names.contains(name)) {
                        errors << u"%1 %2: contact terminal %3 unknown"_s.arg(where, variant.poles,
                                                                              name);
                        valid = false;
                    }
                }
                if (!valid)
                    continue;
                if (contact.top == contact.bottom) {
                    errors << u"%1 %2: contact joins %3 to itself"_s.arg(where, variant.poles,
                                                                         contact.top);
                    continue;
                }
                for (const QString &name : { contact.top, contact.bottom }) {
                    if (contacted.contains(name))
                        errors << u"%1 %2: terminal %3 is in two contacts"_s.arg(
                                where, variant.poles, name);
                    contacted.insert(name);
                }
                variant.contacts << contact;
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
        const QString barWhere = u"bar %1"_s.arg(bar.id);
        if (!o.contains("role")) {
            errors << u"%1: missing role"_s.arg(barWhere);
        } else if (const auto role = parseRole(o.value("role").toString())) {
            bar.role = *role;
        } else {
            errors << u"%1: unknown role %2"_s.arg(barWhere, o.value("role").toString());
        }
        catalog.bars << bar;
    }
    ids.clear();
    for (const auto &sv : root.value("supplies").toArray()) {
        const QJsonObject o = sv.toObject();
        SupplyType supply{ o.value("id").toString(), o.value("picture").toString(), {}, {}, 0 };
        checkUnique(ids, supply.id, errors);
        const QString where = u"supply %1"_s.arg(supply.id);
        QSet<QString> names;
        for (const auto &terminalValue : o.value("terminals").toArray()) {
            const QJsonObject to = terminalValue.toObject();
            TerminalSpec terminal{ to.value("name").toString(),
                                   { to.value("x").toDouble(), to.value("y").toDouble() },
                                   terminalRole(to, where, errors) };
            if (names.contains(terminal.name))
                errors << u"%1: duplicate terminal %2"_s.arg(where, terminal.name);
            names.insert(terminal.name);
            supply.terminals << terminal;
        }
        if (o.contains("upstreamBreaker")) {
            const QJsonObject uo = o.value("upstreamBreaker").toObject();
            supply.upstreamCurve = uo.value("curve").toString();
            supply.upstreamRating = uo.value("ratedCurrent").toDouble();
            const DeviceType *breaker = catalog.deviceType(u"breaker");
            if (!breaker || !breaker->protection
                || !breaker->protection->curves.contains(supply.upstreamCurve))
                errors << u"%1: upstream breaker curve %2 unknown"_s.arg(where,
                                                                         supply.upstreamCurve);
            if (supply.upstreamRating <= 0)
                errors << u"%1: upstream breaker rating must be positive"_s.arg(where);
        }
        catalog.supplies << supply;
    }
    loadWires(root, catalog, errors);
}

void loadEnclosures(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    QSet<QString> ids;
    for (const auto &ev : root.value("enclosures").toArray()) {
        const QJsonObject o = ev.toObject();
        EnclosureType enclosure{ o.value("id").toString(),
                                 o.value("modules").toInt(),
                                 o.value("rows").toInt(),
                                 o.value("nBarTerminals").toInt(),
                                 o.value("peBarTerminals").toInt(),
                                 o.value("picture").toString(),
                                 { o.value("width").toDouble(), o.value("height").toDouble() } };
        checkUnique(ids, enclosure.id, errors);
        if (enclosure.rows <= 0)
            errors << u"%1: rows must be positive"_s.arg(enclosure.id);
        if (enclosure.size.width() <= 0 || enclosure.size.height() <= 0)
            errors << u"%1: width and height must be positive"_s.arg(enclosure.id);
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

void loadLoads(const QJsonObject &root, Catalog &catalog, QStringList &errors)
{
    QSet<QString> ids;
    for (const auto &lv : root.value("loads").toArray()) {
        const QJsonObject o = lv.toObject();
        LoadType load;
        load.id = o.value("id").toString();
        load.name = o.value("name").toString();
        load.description = o.value("description").toString();
        load.caption = o.value("caption").toString();
        load.picture = o.value("picture").toString();
        load.size = { o.value("width").toDouble(), o.value("height").toDouble() };
        load.power = o.value("power").toInt();
        load.plugsIn = o.value("plugsIn").toBool();
        load.light = o.value("light").toBool();
        for (const auto &pv : o.value("places").toArray()) {
            const QJsonObject po = pv.toObject();
            load.places << QPointF(po.value("x").toDouble(), po.value("y").toDouble());
        }
        checkUnique(ids, load.id, errors);
        const QString where = u"load %1"_s.arg(load.id);
        requireText(load.name, u"name"_s, where, errors);
        requireText(load.description, u"description"_s, where, errors);
        requireText(load.picture, u"picture"_s, where, errors);
        if (load.size.width() <= 0 || load.size.height() <= 0)
            errors << u"%1: width and height must be positive"_s.arg(where);
        QSet<QString> names;
        for (const auto &terminalValue : o.value("terminals").toArray()) {
            const QJsonObject to = terminalValue.toObject();
            TerminalSpec terminal{ to.value("name").toString(),
                                   { to.value("x").toDouble(), to.value("y").toDouble() },
                                   terminalRole(to, where, errors) };
            if (names.contains(terminal.name))
                errors << u"%1: duplicate terminal %2"_s.arg(where, terminal.name);
            names.insert(terminal.name);
            load.terminals << terminal;
        }
        const bool isSocket = o.contains("places");
        if (isSocket && load.power != 0)
            errors << u"%1: a socket has no power"_s.arg(where);
        else if (!isSocket && (load.power < 1 || load.power > MaxLoadPower))
            errors << u"%1: power must be 1 to %2 W"_s.arg(where, QString::number(MaxLoadPower));
        if (load.plugsIn) {
            if (!load.terminals.isEmpty() || isSocket || !load.caption.isEmpty())
                errors << u"%1: a plug-in appliance has no terminals, places or caption"_s.arg(
                        where);
        } else {
            const QList<TerminalSpec> &t = load.terminals;
            const bool wired = t.size() == 3 && t[0].name == u"L" && t[0].role == Conductor::L
                    && t[1].name == u"N" && t[1].role == Conductor::N && t[2].name == u"PE"
                    && t[2].role == Conductor::PE;
            if (!wired)
                errors << u"%1: terminals must be L, N and PE with matching roles"_s.arg(where);
            requireText(load.caption, u"caption"_s, where, errors);
        }
        if (isSocket && load.places.size() != 2)
            errors << u"%1: a socket needs two places"_s.arg(where);
        catalog.loads << load;
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
    if (const auto root = parseFile(sources.loads, u"loads.json"_s, load.errors))
        loadLoads(*root, load.catalog, load.errors);
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
             readResource(u"products"_s), readResource(u"loads"_s) };
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
