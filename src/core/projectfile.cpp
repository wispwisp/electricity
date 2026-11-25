// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/projectfile.h"

#include "core/project.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace {

using Status = OpenResult::Status;

constexpr QLatin1StringView FormatName = "electricity-panel"_L1;

// A problem found while reading: the status to report and "where: what".
struct Problem
{
    Status status = Status::Refused;
    QString reason;
};

OpenResult failed(const Problem &problem)
{
    OpenResult r;
    r.status = problem.status;
    r.reason = problem.reason;
    return r;
}

Problem refused(const QString &where, const QString &what)
{
    return { Status::Refused, where + u": "_s + what };
}

bool isInteger(const QJsonValue &v)
{
    return v.isDouble() && v.toDouble() == std::floor(v.toDouble())
            && std::abs(v.toDouble()) <= std::numeric_limits<int>::max();
}

// The integer in o[key]; false if absent, not a number or has a fraction.
bool readInt(const QJsonObject &o, const QString &key, int &value)
{
    if (!isInteger(o[key]))
        return false;
    value = o[key].toInt();
    return true;
}

// Missing keys and wrong types read as empty, which the callers refuse.
QString readString(const QJsonObject &o, const QString &key)
{
    return o[key].toString();
}

bool hasId(const QString &id, QChar prefix)
{
    static const QRegularExpression digits(u"^[1-9][0-9]{0,8}$"_s);
    return id.startsWith(prefix) && digits.match(id.sliced(1)).hasMatch();
}

// Each turns the JSON object of version v into version v + 1; none while the version is 1.
using Upgrade = QJsonObject (*)(const QJsonObject &);
const std::vector<Upgrade> UpgradeSteps;

std::optional<Problem> readDevices(const QJsonObject &root, Project &project)
{
    if (!root[u"devices"_s].isArray())
        return refused(u"devices"_s, u"missing"_s);
    const QJsonArray items = root[u"devices"_s].toArray();
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QString where = u"devices[%1]"_s.arg(i);
        const QJsonObject o = items[i].toObject();
        PlacedDevice d;
        d.id = readString(o, u"id"_s);
        d.type = readString(o, u"type"_s);
        d.poles = readString(o, u"poles"_s);
        d.parameters = o[u"parameters"_s].toObject();
        if (!hasId(d.id, u'd'))
            return refused(where, u"bad id"_s);
        if (!o[u"parameters"_s].isObject() || !readInt(o, u"row"_s, d.row)
            || !readInt(o, u"module"_s, d.module))
            return refused(where, u"missing or malformed field"_s);
        if (project.addDevice(d).isEmpty())
            return refused(where, u"device is not valid here"_s);
    }
    return std::nullopt;
}

std::optional<Problem> readLoads(const QJsonObject &root, Project &project)
{
    if (!root[u"loads"_s].isArray())
        return refused(u"loads"_s, u"missing"_s);
    const QJsonArray items = root[u"loads"_s].toArray();
    // Pass 1 adds the unplugged loads, pass 2 inserts the plugged ones at their file index.
    std::vector<std::pair<qsizetype, PlacedLoad>> plugged;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QString where = u"loads[%1]"_s.arg(i);
        const QJsonObject o = items[i].toObject();
        PlacedLoad l;
        l.id = readString(o, u"id"_s);
        l.type = readString(o, u"type"_s);
        if (!hasId(l.id, u'l'))
            return refused(where, u"bad id"_s);
        if (!o[u"x"_s].isDouble() || !o[u"y"_s].isDouble() || !readInt(o, u"power"_s, l.power))
            return refused(where, u"missing or malformed field"_s);
        l.position = { o[u"x"_s].toDouble(), o[u"y"_s].toDouble() };
        if (o.contains(u"socket"_s) != o.contains(u"place"_s)
            || (o.contains(u"socket"_s)
                && (!o[u"socket"_s].isString() || !readInt(o, u"place"_s, l.place))))
            return refused(where, u"socket and place go together"_s);
        l.socket = readString(o, u"socket"_s);
        if (!l.socket.isEmpty() || l.place != 0)
            plugged.emplace_back(i, l);
        else if (project.addLoad(l).isEmpty())
            return refused(where, u"load is not valid here"_s);
    }
    for (const auto &[index, l] : plugged) {
        if (project.addLoad(l, index).isEmpty())
            return refused(u"loads[%1]"_s.arg(index), u"load is not valid here"_s);
    }
    return std::nullopt;
}

std::optional<Problem> readWires(const QJsonObject &root, Project &project)
{
    if (!root[u"wires"_s].isArray())
        return refused(u"wires"_s, u"missing"_s);
    const QJsonArray items = root[u"wires"_s].toArray();
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QString where = u"wires[%1]"_s.arg(i);
        const QJsonObject o = items[i].toObject();
        Wire w;
        w.id = readString(o, u"id"_s);
        w.from = readString(o, u"from"_s);
        w.to = readString(o, u"to"_s);
        if (!hasId(w.id, u'w'))
            return refused(where, u"bad id"_s);
        if (!o[u"section"_s].isDouble() || !o[u"length"_s].isDouble())
            return refused(where, u"missing or malformed field"_s);
        w.section = o[u"section"_s].toDouble();
        w.length = o[u"length"_s].toDouble();
        const QString conductor = readString(o, u"conductor"_s);
        if (conductor == u"L")
            w.conductor = Conductor::L;
        else if (conductor == u"N")
            w.conductor = Conductor::N;
        else if (conductor == u"PE")
            w.conductor = Conductor::PE;
        else
            return refused(where, u"unknown conductor"_s);
        if (project.addWire(w).isEmpty())
            return refused(where, u"wire is not valid here"_s);
    }
    return std::nullopt;
}

std::optional<Problem> readDesign(const QJsonObject &root, Project &project)
{
    if (readString(root[u"supply"_s].toObject(), u"type"_s) != project.supply().id)
        return refused(u"supply"_s, u"unknown type"_s);
    const QJsonArray enclosures = root[u"enclosures"_s].toArray();
    if (enclosures.size() != 1
        || !project.setEnclosure(readString(enclosures[0].toObject(), u"type"_s)))
        return refused(u"enclosures"_s, u"need exactly one known enclosure"_s);
    if (const auto problem = readDevices(root, project))
        return problem;
    if (const auto problem = readLoads(root, project))
        return problem;
    return readWires(root, project);
}

bool isSocket(const Catalog &catalog, const QString &typeId)
{
    const LoadType *type = catalog.loadType(typeId);
    return type && !type->places.isEmpty();
}

// Ids of the devices and non-socket loads that are on, in file order.
QStringList onIds(const QJsonObject &root, const Project &project)
{
    QStringList on;
    for (const QString &list : { u"devices"_s, u"loads"_s }) {
        for (const QJsonValue item : root[list].toArray()) {
            const QJsonObject o = item.toObject();
            const bool socket
                    = list == u"loads" && isSocket(project.catalog(), o[u"type"_s].toString());
            if (o[u"on"_s].toBool() && !socket)
                on.append(o[u"id"_s].toString());
        }
    }
    return on;
}

QString conductorName(Conductor c)
{
    switch (c) {
    case Conductor::L:
        return u"L"_s;
    case Conductor::N:
        return u"N"_s;
    case Conductor::PE:
        return u"PE"_s;
    }
    return {};
}

} // namespace

OpenResult readProject(const QByteArray &data, Project &project)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject() || doc.object()[u"format"_s].toString() != FormatName)
        return failed({ Status::NotPanelFile, u"format: not an electricity-panel file"_s });
    QJsonObject root = doc.object();
    if (!isInteger(root[u"version"_s]) || root[u"version"_s].toInt() < 1)
        return failed({ Status::NotPanelFile, u"version: missing or not a version number"_s });
    const int version = root[u"version"_s].toInt();
    if (version > ProjectFileVersion)
        return failed({ Status::NewerVersion, u"version: %1 is newer"_s.arg(version) });
    for (int v = version; v < ProjectFileVersion; ++v)
        root = UpgradeSteps[v - 1](root);

    if (const auto problem = readDesign(root, project))
        return failed(*problem);
    OpenResult result;
    result.on = onIds(root, project);
    return result;
}

OpenResult openProject(const QString &path, Project &project)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return failed(
                { Status::NotPanelFile, u"format: cannot read the file: "_s + file.errorString() });
    return readProject(file.readAll(), project);
}

QByteArray writeProject(const Project &project, const QStringList &on)
{
    QJsonArray devices;
    for (const PlacedDevice &d : project.devices()) {
        QJsonObject o{ { "id", d.id },       { "type", d.type },
                       { "poles", d.poles }, { "parameters", d.parameters },
                       { "row", d.row },     { "module", d.module } };
        if (on.contains(d.id))
            o["on"] = true;
        devices.append(o);
    }
    QJsonArray loads;
    for (const PlacedLoad &l : project.loads()) {
        QJsonObject o{ { "id", l.id },
                       { "type", l.type },
                       { "x", l.position.x() },
                       { "y", l.position.y() },
                       { "power", l.power } };
        if (!l.socket.isEmpty()) {
            o["socket"] = l.socket;
            o["place"] = l.place;
        }
        if (on.contains(l.id) && !isSocket(project.catalog(), l.type))
            o["on"] = true;
        loads.append(o);
    }
    QJsonArray wires;
    for (const Wire &w : project.wires()) {
        wires.append(QJsonObject{ { "id", w.id },
                                  { "from", w.from },
                                  { "to", w.to },
                                  { "section", w.section },
                                  { "conductor", conductorName(w.conductor) },
                                  { "length", w.length } });
    }
    const QJsonObject root{ { "format", FormatName },
                            { "version", ProjectFileVersion },
                            { "supply", QJsonObject{ { "type", project.supply().id } } },
                            { "enclosures",
                              QJsonArray{ QJsonObject{ { "type", project.enclosure().id } } } },
                            { "devices", devices },
                            { "loads", loads },
                            { "wires", wires } };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool saveProject(const QString &path, const Project &project, const QStringList &on, QString &error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(writeProject(project, on)) < 0
        || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}
