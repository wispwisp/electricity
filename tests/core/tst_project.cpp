// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/project.h"

#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace Qt::StringLiterals;

namespace {

PlacedDevice preset(const char *id, int row, int module)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == QLatin1StringView(id))
            return deviceFromPreset(p, row, module);
    }
    qFatal("unknown preset %s", id);
}

} // namespace

Q_DECLARE_METATYPE(PlacedDevice)
Q_DECLARE_METATYPE(Placement)

class TestProject : public QObject
{
    Q_OBJECT
private slots:
    void startsWithDefaultEnclosure();
    void deviceFromPresetCopiesTypePolesAndParameters();
    void ratingText();
    void placement_data();
    void placement();
    void addAssignsStableIdsAndSignals();
    void restoresGivenIdAndNeverReusesIds();
    void addAtIndexRestoresOrder();
    void designationsFollowPosition();
    void updateDeviceRules();
    void enclosureRules();
    void removeUnknownIsRefused();
};

void TestProject::startsWithDefaultEnclosure()
{
    Project project(embeddedCatalog());
    QCOMPARE(project.enclosure().id, u"enclosure-12"_s);
    QCOMPARE(project.modulesPerRow(), 12);
    QVERIFY(project.devices().isEmpty());
}

void TestProject::deviceFromPresetCopiesTypePolesAndParameters()
{
    const PlacedDevice d = preset("breaker-2p-c40", 0, 3);
    QCOMPARE(d.type, u"breaker"_s);
    QCOMPARE(d.poles, u"2P"_s);
    QCOMPARE(d.parameters, (QJsonObject{ { "curve", "C" }, { "ratedCurrent", 40 } }));
    QCOMPARE(d.row, 0);
    QCOMPARE(d.module, 3);
    QVERIFY(d.id.isEmpty());
}

void TestProject::ratingText()
{
    QCOMPARE(::ratingText(preset("breaker-1p-c16", 0, 0)), u"C16"_s);
    QCOMPARE(::ratingText(preset("rcd-2p-40-30", 0, 0)), u"40 A"_s);
    QCOMPARE(::ratingText(preset("rcbo-c16-30", 0, 0)), u"C16"_s);
}

void TestProject::placement_data()
{
    QTest::addColumn<PlacedDevice>("device");
    QTest::addColumn<Placement>("expected");

    const auto row = [](const char *name, const PlacedDevice &device, Placement expected) {
        QTest::newRow(name) << device << expected;
    };
    const auto edited = [](PlacedDevice d, const std::function<void(PlacedDevice &)> &edit) {
        edit(d);
        return d;
    };
    const PlacedDevice moved
            = edited(preset("breaker-2p-c40", 0, 3), [](PlacedDevice &d) { d.id = u"d1"_s; });
    row("left a", preset("breaker-1p-c16", 0, 0), Placement::Fits);
    row("left b", preset("breaker-1p-c16", 0, 1), Placement::Fits);
    row("on it a", preset("breaker-1p-c16", 0, 2), Placement::Overlaps);
    row("on it b", preset("breaker-1p-c16", 0, 3), Placement::Overlaps);
    row("right after", preset("breaker-1p-c16", 0, 4), Placement::Fits);
    row("2P half on", preset("breaker-2p-c40", 0, 1), Placement::Overlaps);
    row("last module", preset("breaker-1p-c16", 0, 11), Placement::Fits);
    row("past end", preset("breaker-2p-c40", 0, 11), Placement::PastRailEnd);
    row("negative module", preset("breaker-1p-c16", 0, -1), Placement::PastRailEnd);
    row("no such row", preset("breaker-1p-c16", 1, 0), Placement::NoRail);
    row("negative row", preset("breaker-1p-c16", -1, 0), Placement::NoRail);
    row("itself moved", moved, Placement::Fits);
    row("unknown variant",
        edited(preset("breaker-1p-c16", 0, 0), [](PlacedDevice &d) { d.poles = u"3P"_s; }),
        Placement::Invalid);
    row("unknown type",
        edited(preset("breaker-1p-c16", 0, 0), [](PlacedDevice &d) { d.type = u"fuse"_s; }),
        Placement::Invalid);
    row("rating not allowed",
        edited(preset("breaker-1p-c16", 0, 0),
               [](PlacedDevice &d) { d.parameters["ratedCurrent"] = 17; }),
        Placement::Invalid);
    row("missing parameter",
        edited(preset("breaker-1p-c16", 0, 0),
               [](PlacedDevice &d) { d.parameters.remove("curve"); }),
        Placement::Invalid);
}

void TestProject::placement()
{
    QFETCH(PlacedDevice, device);
    QFETCH(Placement, expected);
    Project project(embeddedCatalog());
    QCOMPARE(project.addDevice(preset("breaker-2p-c40", 0, 2)), u"d1"_s);
    QCOMPARE(project.placement(device), expected);
}

void TestProject::addAssignsStableIdsAndSignals()
{
    Project project(embeddedCatalog());
    QSignalSpy changed(&project, &Project::changed);
    QCOMPARE(project.addDevice(preset("breaker-1p-c16", 0, 0)), u"d1"_s);
    QCOMPARE(project.addDevice(preset("breaker-1p-c16", 0, 1)), u"d2"_s);
    QCOMPARE(changed.size(), 2);
    QCOMPARE(project.addDevice(preset("breaker-1p-c16", 0, 1)), QString());
    QCOMPARE(changed.size(), 2);
    QCOMPARE(project.devices().size(), 2);
}

void TestProject::restoresGivenIdAndNeverReusesIds()
{
    Project project(embeddedCatalog());
    project.addDevice(preset("breaker-1p-c16", 0, 0));
    project.addDevice(preset("breaker-1p-c16", 0, 1));
    const PlacedDevice saved = *project.device(u"d1");
    QVERIFY(project.removeDevice(u"d1"));
    QCOMPARE(project.addDevice(saved), u"d1"_s);
    QCOMPARE(*project.device(u"d1"), saved);
    QVERIFY(project.removeDevice(u"d2"));
    QCOMPARE(project.addDevice(preset("breaker-1p-c16", 0, 5)), u"d3"_s);
}

void TestProject::addAtIndexRestoresOrder()
{
    Project project(embeddedCatalog());
    for (int module = 0; module < 3; ++module)
        project.addDevice(preset("breaker-1p-c16", 0, module));
    const QList<PlacedDevice> original = project.devices();
    QVERIFY(project.removeDevice(u"d2"));
    QSignalSpy changed(&project, &Project::changed);
    QCOMPARE(project.addDevice(original[1], 1), u"d2"_s);
    QCOMPARE(project.devices(), original);
    QCOMPARE(changed.size(), 1);

    QVERIFY(project.removeDevice(u"d2"));
    changed.clear();
    QCOMPARE(project.addDevice(original[1], 3), QString());
    QCOMPARE(project.addDevice(original[1], -2), QString());
    QCOMPARE(changed.size(), 0);
    QCOMPARE(project.addDevice(original[1], 2), u"d2"_s);
    QCOMPARE(project.devices().last().id, u"d2"_s);
}

void TestProject::designationsFollowPosition()
{
    Project project(embeddedCatalog());
    const QString b4 = project.addDevice(preset("breaker-1p-c10", 0, 4));
    const QString rcd = project.addDevice(preset("rcd-2p-40-30", 0, 2));
    const QString b0 = project.addDevice(preset("breaker-2p-c40", 0, 0));
    const QString rcbo = project.addDevice(preset("rcbo-c16-30", 0, 8));
    QCOMPARE(project.designation(b4), u"QF2"_s);
    QCOMPARE(project.designation(rcd), u"QD1"_s);
    QCOMPARE(project.designation(b0), u"QF1"_s);
    QCOMPARE(project.designation(rcbo), u"QFD1"_s);
    QCOMPARE(project.designation(u"nope"), QString());

    PlacedDevice moved = *project.device(b0);
    moved.module = 5;
    QVERIFY(project.updateDevice(moved));
    QCOMPARE(project.designation(b4), u"QF1"_s);
    QCOMPARE(project.designation(b0), u"QF2"_s);

    QVERIFY(project.removeDevice(b4));
    QCOMPARE(project.designation(b0), u"QF1"_s);

    QVERIFY(project.setEnclosure(u"enclosure-24"_s));
    QCOMPARE(project.addDevice(preset("breaker-1p-c16", 0, 11)).isEmpty(), false);
    const QString second = project.addDevice(preset("breaker-1p-c16", 1, 0));
    QCOMPARE(project.designation(second), u"QF3"_s);
}

void TestProject::updateDeviceRules()
{
    Project project(embeddedCatalog());
    project.addDevice(preset("breaker-1p-c16", 0, 4));
    project.addDevice(preset("breaker-1p-c16", 0, 7));
    QSignalSpy changed(&project, &Project::changed);

    PlacedDevice d = *project.device(u"d1");
    const PlacedDevice original = d;
    d.module = 7;
    QVERIFY(!project.updateDevice(d));
    QCOMPARE(*project.device(u"d1"), original);
    QCOMPARE(changed.size(), 0);

    d = original;
    d.parameters["ratedCurrent"] = 25;
    QVERIFY(project.updateDevice(d));
    QCOMPARE(changed.size(), 1);
    d.parameters["ratedCurrent"] = 17;
    QVERIFY(!project.updateDevice(d));
    QCOMPARE(changed.size(), 1);

    d = *project.device(u"d1");
    d.poles = u"2P"_s;
    QCOMPARE(project.placement(d), Placement::Fits);
    QVERIFY(project.updateDevice(d));
    QCOMPARE(project.moduleWidth(*project.device(u"d1")), 2);

    d = *project.device(u"d1");
    d.module = 6;
    d.poles = u"1P"_s;
    QVERIFY(project.updateDevice(d));
    d.poles = u"2P"_s;
    QCOMPARE(project.placement(d), Placement::Overlaps);
    QVERIFY(!project.updateDevice(d));

    d = *project.device(u"d1");
    d.module = 11;
    QVERIFY(project.updateDevice(d));
    d.poles = u"2P"_s;
    QCOMPARE(project.placement(d), Placement::PastRailEnd);
    QVERIFY(!project.updateDevice(d));

    PlacedDevice unknown = original;
    unknown.id = u"d99"_s;
    QVERIFY(!project.updateDevice(unknown));
}

void TestProject::enclosureRules()
{
    Project project(embeddedCatalog());
    QSignalSpy changed(&project, &Project::changed);
    QSignalSpy enclosureChanged(&project, &Project::enclosureChanged);
    QStringList order;
    connect(&project, &Project::changed, this, [&] { order << u"changed"_s; });
    connect(&project, &Project::enclosureChanged, this, [&] { order << u"enclosureChanged"_s; });
    QVERIFY(project.setEnclosure(u"enclosure-24"_s));
    QCOMPARE(order, (QStringList{ u"changed"_s, u"enclosureChanged"_s }));
    QCOMPARE(project.modulesPerRow(), 12);

    project.addDevice(preset("breaker-1p-c16", 1, 0));
    QVERIFY(!project.fitsEnclosure(u"enclosure-12"));
    QVERIFY(!project.fitsEnclosure(u"nope"));
    const qsizetype before = changed.size();
    QVERIFY(!project.setEnclosure(u"enclosure-12"_s));
    QCOMPARE(changed.size(), before);
    QCOMPARE(enclosureChanged.size(), 1);

    QVERIFY(project.removeDevice(u"d1"));
    project.addDevice(preset("breaker-1p-c16", 0, 0));
    QVERIFY(project.fitsEnclosure(u"enclosure-12"));
    QVERIFY(project.setEnclosure(u"enclosure-12"_s));
    QVERIFY(!project.setEnclosure(u"nope"_s));
}

void TestProject::removeUnknownIsRefused()
{
    Project project(embeddedCatalog());
    QSignalSpy changed(&project, &Project::changed);
    QVERIFY(!project.removeDevice(u"d1"));
    QCOMPARE(changed.size(), 0);
}

QTEST_APPLESS_MAIN(TestProject)
#include "tst_project.moc"
