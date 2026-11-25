// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/project.h"
#include "core/projectfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

#include <functional>

using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(OpenResult::Status)

namespace {

PlacedLoad load(const char *type, QPointF pos)
{
    const LoadType *t = embeddedCatalog().loadType(QString::fromLatin1(type));
    if (!t)
        qFatal("unknown load type %s", type);
    return loadFromType(*t, pos);
}

PlacedDevice preset(const char *id, int row, int module)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == QLatin1StringView(id))
            return deviceFromPreset(p, row, module);
    }
    qFatal("unknown preset %s", id);
}

QByteArray smallFile()
{
    QFile f(QStringLiteral(TEST_DATA_DIR "/small.panel"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open small.panel");
    return f.readAll();
}

// The small file with one change made to its JSON object.
QByteArray changed(const std::function<void(QJsonObject &)> &edit)
{
    QJsonObject root = QJsonDocument::fromJson(smallFile()).object();
    edit(root);
    return QJsonDocument(root).toJson();
}

// Sets root[list][index][key] to value, or removes the key if value is undefined.
void setField(QJsonObject &root, const QString &list, int index, const QString &key,
              const QJsonValue &value)
{
    QJsonArray items = root[list].toArray();
    QJsonObject item = items[index].toObject();
    if (value.isUndefined())
        item.remove(key);
    else
        item[key] = value;
    items[index] = item;
    root[list] = items;
}

void appendTo(QJsonObject &root, const QString &list, const QJsonObject &item)
{
    QJsonArray items = root[list].toArray();
    items.append(item);
    root[list] = items;
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void compareModels(const Project &a, const Project &b)
{
    QCOMPARE(a.enclosure().id, b.enclosure().id);
    QCOMPARE(a.devices(), b.devices());
    QCOMPARE(a.wires(), b.wires());
    QCOMPARE(a.loads(), b.loads());
}

} // namespace

class TestProjectFile : public QObject
{
    Q_OBJECT
private:
    void fill(Project &p, QStringList &on);
private slots:
    void roundTripKeepsDesignAndSwitches();
    void writesVersion1Format();
    void refuses_data();
    void refuses();
    void ignoresUnknownFields();
    void ignoresOnForSockets();
    void savesAndOpensCyrillicPath();
    void saveFailsWithoutFolder();
    void saveFailureKeepsExistingFile();
    void openMissingFileIsNotPanelFile();
};

// The appliance comes before its socket in loads().
void TestProjectFile::fill(Project &p, QStringList &on)
{
    QVERIFY(p.setEnclosure(u"enclosure-24"_s));
    QCOMPARE(p.addDevice(preset("breaker-1p-c16", 1, 2)), u"d1"_s);
    PlacedLoad tv = load("tv", { 400, 400 });
    QCOMPARE(p.addLoad(tv), u"l1"_s);
    QCOMPARE(p.addLoad(load("socket", { 0, 500 })), u"l2"_s);
    tv.id = u"l1"_s;
    tv.socket = u"l2"_s;
    tv.place = 2;
    QVERIFY(p.updateLoad(tv));
    const QString lamp = p.addLoad(load("ceiling-lamp", { 200, 500 }));
    QVERIFY(!lamp.isEmpty());
    Wire w;
    w.from = u"d1:2"_s;
    w.to = terminalId(lamp, u"L"_s);
    w.section = 1.5;
    w.length = 22.5;
    QVERIFY(!p.addWire(w).isEmpty());
    on = { u"d1"_s, lamp };
}

void TestProjectFile::roundTripKeepsDesignAndSwitches()
{
    Project source(embeddedCatalog());
    QStringList on;
    fill(source, on);

    Project copy(embeddedCatalog());
    const OpenResult r = readProject(writeProject(source, on), copy);
    QCOMPARE(r.status, OpenResult::Status::Opened);
    QVERIFY(r.reason.isEmpty());
    compareModels(source, copy);
    QCOMPARE(r.on, on);
    QCOMPARE(copy.addLoad(load("kettle", { 500, 0 })), u"l4"_s);
}

void TestProjectFile::writesVersion1Format()
{
    Project source(embeddedCatalog());
    QStringList on;
    fill(source, on);
    const QJsonObject o = QJsonDocument::fromJson(writeProject(source, on)).object();

    QCOMPARE(o["format"].toString(), u"electricity-panel"_s);
    QCOMPARE(o["version"].toInt(), 1);
    QCOMPARE(o["supply"].toObject()["type"].toString(), u"supply-1p"_s);
    QCOMPARE(o["enclosures"].toArray().size(), 1);
    QVERIFY(!o.contains(u"view"_s));

    const QJsonArray devices = o["devices"].toArray();
    QCOMPARE(devices.size(), 1);
    QVERIFY(devices[0].toObject()["on"].toBool());
    const QJsonArray loads = o["loads"].toArray();
    QCOMPARE(loads.size(), 3);
    const QJsonObject tv = loads[0].toObject();
    QCOMPARE(tv["socket"].toString(), u"l2"_s);
    QCOMPARE(tv["place"].toInt(), 2);
    QVERIFY(!tv.contains(u"on"_s));
    const QJsonObject socket = loads[1].toObject();
    QVERIFY(!socket.contains(u"socket"_s));
    QVERIFY(!socket.contains(u"place"_s));
    QVERIFY(!socket.contains(u"on"_s));
    QVERIFY(loads[2].toObject()["on"].toBool());
    QCOMPARE(o["wires"].toArray().size(), 1);
}

void TestProjectFile::refuses_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<OpenResult::Status>("status");
    QTest::addColumn<QString>("prefix");

    using S = OpenResult::Status;
    const auto row = [](const char *name, const QByteArray &data, S status, const char *prefix) {
        QTest::newRow(name) << data << status << QString::fromLatin1(prefix);
    };
    const auto top = [](const QString &key, const QJsonValue &value) {
        return changed([&](QJsonObject &o) {
            if (value.isUndefined())
                o.remove(key);
            else
                o[key] = value;
        });
    };
    const auto field
            = [](const QString &list, int index, const QString &key, const QJsonValue &value) {
                  return changed([&](QJsonObject &o) { setField(o, list, index, key, value); });
              };
    const QJsonValue gone(QJsonValue::Undefined);

    row("not JSON", "{", S::NotPanelFile, "format");
    row("array", "[]", S::NotPanelFile, "format");
    row("wrong format", top(u"format"_s, u"x"_s), S::NotPanelFile, "format");
    row("no version", top(u"version"_s, gone), S::NotPanelFile, "version");
    row("version 0", top(u"version"_s, 0), S::NotPanelFile, "version");
    row("version 1.5", top(u"version"_s, 1.5), S::NotPanelFile, "version");
    row("newer", top(u"version"_s, 2), S::NewerVersion, "version");
    row("unknown supply", top(u"supply"_s, QJsonObject{ { "type", "supply-3p" } }), S::Refused,
        "supply");
    row("two enclosures",
        top(u"enclosures"_s,
            QJsonArray{ QJsonObject{ { "type", "enclosure-12" } },
                        QJsonObject{ { "type", "enclosure-12" } } }),
        S::Refused, "enclosures");
    row("unknown enclosure",
        top(u"enclosures"_s, QJsonArray{ QJsonObject{ { "type", "enclosure-99" } } }), S::Refused,
        "enclosures");
    row("unknown device type", field(u"devices"_s, 0, u"type"_s, u"relay"_s), S::Refused,
        "devices[0]");
    row("row as text", field(u"devices"_s, 0, u"row"_s, u"0"_s), S::Refused, "devices[0]");
    row("row 0.5", field(u"devices"_s, 0, u"row"_s, 0.5), S::Refused, "devices[0]");
    row("device id x1", field(u"devices"_s, 0, u"id"_s, u"x1"_s), S::Refused, "devices[0]");
    row("device id d0", field(u"devices"_s, 0, u"id"_s, u"d0"_s), S::Refused, "devices[0]");
    row("overlapping device", changed([](QJsonObject &o) {
            appendTo(o, u"devices"_s,
                     QJsonObject{ { "id", "d2" },
                                  { "type", "breaker" },
                                  { "poles", "1P" },
                                  { "parameters",
                                    QJsonObject{ { "curve", "C" }, { "ratedCurrent", 16 } } },
                                  { "row", 0 },
                                  { "module", 0 } });
        }),
        S::Refused, "devices[1]");
    row("duplicate id", changed([](QJsonObject &o) {
            appendTo(o, u"devices"_s,
                     QJsonObject{ { "id", "d1" },
                                  { "type", "breaker" },
                                  { "poles", "1P" },
                                  { "parameters",
                                    QJsonObject{ { "curve", "C" }, { "ratedCurrent", 16 } } },
                                  { "row", 0 },
                                  { "module", 4 } });
        }),
        S::Refused, "devices[1]");
    row("load id d9", field(u"loads"_s, 0, u"id"_s, u"d9"_s), S::Refused, "loads[0]");
    row("plug into non-socket", field(u"loads"_s, 1, u"socket"_s, u"d1"_s), S::Refused, "loads[1]");
    row("socket without place", field(u"loads"_s, 1, u"place"_s, gone), S::Refused, "loads[1]");
    row("no devices key", top(u"devices"_s, gone), S::Refused, "devices");
    row("missing terminal", field(u"wires"_s, 0, u"to"_s, u"d1:9"_s), S::Refused, "wires[0]");
    row("length 0", field(u"wires"_s, 0, u"length"_s, 0), S::Refused, "wires[0]");
    row("length -1", field(u"wires"_s, 0, u"length"_s, -1), S::Refused, "wires[0]");
    row("length 500.5", field(u"wires"_s, 0, u"length"_s, 500.5), S::Refused, "wires[0]");
    row("unknown conductor", field(u"wires"_s, 0, u"conductor"_s, u"X"_s), S::Refused, "wires[0]");
}

void TestProjectFile::refuses()
{
    QFETCH(QByteArray, data);
    QFETCH(OpenResult::Status, status);
    QFETCH(QString, prefix);

    Project project(embeddedCatalog());
    const OpenResult r = readProject(data, project);
    QCOMPARE(r.status, status);
    QVERIFY2(r.reason.startsWith(prefix), qPrintable(r.reason));
}

void TestProjectFile::ignoresUnknownFields()
{
    const QByteArray data = changed([](QJsonObject &o) {
        o["extra"] = 1;
        o["supply"] = QJsonObject{ { "type", "supply-1p" }, { "extra", true } };
        o["enclosures"] = QJsonArray{ QJsonObject{ { "type", "enclosure-12" }, { "extra", 1 } } };
        for (const QString &list : { u"devices"_s, u"loads"_s, u"wires"_s })
            setField(o, list, 0, u"extra"_s, u"x"_s);
    });
    Project project(embeddedCatalog());
    const OpenResult r = readProject(data, project);
    QCOMPARE(r.status, OpenResult::Status::Opened);
    QCOMPARE(project.devices().size(), 1);
    QCOMPARE(project.loads().size(), 2);
    QCOMPARE(project.wires().size(), 1);
}

void TestProjectFile::ignoresOnForSockets()
{
    const QByteArray data
            = changed([](QJsonObject &o) { setField(o, u"loads"_s, 0, u"on"_s, true); });
    Project project(embeddedCatalog());
    const OpenResult r = readProject(data, project);
    QCOMPARE(r.status, OpenResult::Status::Opened);
    QCOMPARE(r.on, QStringList{ u"d1"_s });
}

void TestProjectFile::savesAndOpensCyrillicPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Project source(embeddedCatalog());
    QStringList on;
    fill(source, on);
    const QString path = dir.filePath(u"щит кухня.panel"_s);

    QString error;
    QVERIFY2(saveProject(path, source, on, error), qPrintable(error));

    Project copy(embeddedCatalog());
    const OpenResult r = openProject(path, copy);
    QCOMPARE(r.status, OpenResult::Status::Opened);
    compareModels(source, copy);
    QCOMPARE(r.on, on);
}

void TestProjectFile::saveFailsWithoutFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Project project(embeddedCatalog());
    QString error;
    QVERIFY(!saveProject(dir.filePath(u"missing/a.panel"_s), project, {}, error));
    QVERIFY(!error.isEmpty());
}

void TestProjectFile::saveFailureKeepsExistingFile()
{
#ifdef Q_OS_WIN
    QSKIP("Folder permissions work differently on Windows");
#endif
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(u"a.panel"_s);
    Project first(embeddedCatalog());
    QString error;
    QVERIFY2(saveProject(path, first, {}, error), qPrintable(error));
    const QByteArray before = readAll(path);
    QVERIFY(!before.isEmpty());

    const QFileDevice::Permissions original = QFile::permissions(dir.path());
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    // Running as root, the folder stays writable.
    QFile probe(dir.filePath(u"probe"_s));
    const bool writable = probe.open(QIODevice::WriteOnly);
    probe.close();
    probe.remove();
    if (writable) {
        QFile::setPermissions(dir.path(), original);
        QSKIP("The folder is writable despite being read-only");
    }

    Project second(embeddedCatalog());
    QVERIFY(second.setEnclosure(u"enclosure-24"_s));
    const bool saved = saveProject(path, second, {}, error);
    QFile::setPermissions(dir.path(), original);
    QVERIFY(!saved);
    QVERIFY(!error.isEmpty());
    QCOMPARE(readAll(path), before);
}

void TestProjectFile::openMissingFileIsNotPanelFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Project project(embeddedCatalog());
    const OpenResult r = openProject(dir.filePath(u"none.panel"_s), project);
    QCOMPARE(r.status, OpenResult::Status::NotPanelFile);
    QVERIFY(!r.reason.isEmpty());
}

QTEST_APPLESS_MAIN(TestProjectFile)
#include "tst_projectfile.moc"
