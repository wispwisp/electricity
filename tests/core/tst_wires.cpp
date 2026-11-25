// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/project.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

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

Q_DECLARE_METATYPE(Conductor)
Q_DECLARE_METATYPE(WireCheck)

class TestWires : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void terminalIdsListEveryTerminal();
    void rolesAndCapacity();
    void conductorRule_data();
    void conductorRule();
    void addsWireWithStableIdAndSignal();
    void restoresGivenIdAtIndex();
    void refusedWires_data();
    void refusedWires();
    void wireLengthRange_data();
    void wireLengthRange();
    void updateWireChangesSection();
    void removingDeviceRemovesItsWires();
    void poleChangeRefusedWhenItOrphansWires();
    void enclosureShrinkRefusedWithWiredBarHole();

private:
    [[nodiscard]] Wire wire(const QString &from, const QString &to, double section = 2.5) const
    {
        return Wire{ {}, from, to, section, m_project->conductorFor(from, to) };
    }

    std::unique_ptr<Project> m_project;
};

void TestWires::init()
{
    m_project = std::make_unique<Project>(embeddedCatalog());
    QCOMPARE(m_project->addDevice(preset("breaker-2p-c40", 0, 0)), u"d1"_s);
    QCOMPARE(m_project->addDevice(preset("rcd-2p-40-30", 0, 2)), u"d2"_s);
    QCOMPARE(m_project->addDevice(preset("breaker-1p-c10", 0, 4)), u"d3"_s);
}

void TestWires::terminalIdsListEveryTerminal()
{
    const QStringList ids = m_project->terminalIds();
    QCOMPARE(ids.mid(0, 4),
             (QStringList{ u"supply:L"_s, u"supply:N"_s, u"supply:PE"_s, u"nbar:1"_s }));
    for (const QString &id : { u"nbar:8"_s, u"pebar:8"_s, u"d1:4"_s, u"d2:N2"_s, u"d3:2"_s })
        QVERIFY2(ids.contains(id), qPrintable(id));
    QVERIFY(!ids.contains(u"nbar:9"_s));
    QVERIFY(!ids.contains(u"d3:3"_s));
    QCOMPARE(terminalObject(u"d2:N1"_s), u"d2"_s);
    QCOMPARE(terminalName(u"d2:N1"_s), u"N1"_s);
    QCOMPARE(terminalId(u"nbar"_s, u"3"_s), u"nbar:3"_s);
}

void TestWires::rolesAndCapacity()
{
    QCOMPARE(m_project->terminalRole(u"supply:PE"_s), std::optional(Conductor::PE));
    QCOMPARE(m_project->terminalRole(u"nbar:2"_s), std::optional(Conductor::N));
    QVERIFY(!m_project->terminalRole(u"d1:3"_s));
    QCOMPARE(m_project->terminalRole(u"d2:N1"_s), std::optional(Conductor::N));
    QCOMPARE(m_project->terminalRole(u"d3:1"_s), std::optional(Conductor::L));

    QCOMPARE(m_project->wireCapacity(u"pebar:1"_s), 1);
    QCOMPARE(m_project->wireCapacity(u"d1:1"_s), 2);
    QCOMPARE(m_project->wireCapacity(u"supply:L"_s), 2);
    QCOMPARE(m_project->wireCapacity(u"d9:1"_s), 0);
}

void TestWires::conductorRule_data()
{
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("to");
    QTest::addColumn<Conductor>("expected");
    QTest::newRow("supply N first") << u"supply:N"_s << u"d1:3"_s << Conductor::N;
    QTest::newRow("second end has role") << u"d1:4"_s << u"d2:N1"_s << Conductor::N;
    QTest::newRow("first end wins") << u"d1:2"_s << u"d2:1"_s << Conductor::L;
    QTest::newRow("no roles") << u"d1:1"_s << u"d1:3"_s << Conductor::L;
    QTest::newRow("N then L") << u"supply:N"_s << u"d3:1"_s << Conductor::N;
    QTest::newRow("L then N") << u"d3:1"_s << u"supply:N"_s << Conductor::L;
}

void TestWires::conductorRule()
{
    QFETCH(QString, from);
    QFETCH(QString, to);
    QFETCH(Conductor, expected);
    QCOMPARE(m_project->conductorFor(from, to), expected);
}

void TestWires::addsWireWithStableIdAndSignal()
{
    QSignalSpy changed(m_project.get(), &Project::changed);
    QCOMPARE(m_project->addWire(wire(u"supply:L"_s, u"d1:1"_s, 10)), u"w1"_s);
    QCOMPARE(changed.size(), 1);
    const Wire *stored = m_project->wire(u"w1");
    QVERIFY(stored);
    QCOMPARE(stored->section, 10.0);
    QCOMPARE(stored->conductor, Conductor::L);
    QCOMPARE(stored->length, 0.3);
    QCOMPARE(m_project->wireCount(u"d1:1"_s), 1);

    QVERIFY(m_project->removeWire(u"w1"));
    QCOMPARE(m_project->addWire(wire(u"supply:L"_s, u"d1:1"_s)), u"w2"_s);
}

void TestWires::restoresGivenIdAtIndex()
{
    QCOMPARE(m_project->addWire(wire(u"supply:L"_s, u"d1:1"_s)), u"w1"_s);
    QCOMPARE(m_project->addWire(wire(u"supply:N"_s, u"d1:2"_s)), u"w2"_s);
    QCOMPARE(m_project->addWire(wire(u"supply:PE"_s, u"pebar:1"_s)), u"w3"_s);
    const Wire old = *m_project->wire(u"w2");
    QVERIFY(m_project->removeWire(u"w2"));
    QCOMPARE(m_project->addWire(old, 1), u"w2"_s);
    QStringList ids;
    for (const Wire &w : m_project->wires())
        ids << w.id;
    QCOMPARE(ids, (QStringList{ u"w1"_s, u"w2"_s, u"w3"_s }));

    Wire seven = wire(u"nbar:1"_s, u"d3:1"_s);
    seven.id = u"w7"_s;
    QCOMPARE(m_project->addWire(seven), u"w7"_s);
    QCOMPARE(m_project->addWire(wire(u"nbar:2"_s, u"d3:2"_s)), u"w8"_s);
    // an id already in use is refused
    QVERIFY(m_project->addWire(seven).isEmpty());
}

void TestWires::refusedWires_data()
{
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("to");
    QTest::addColumn<double>("section");
    QTest::addColumn<WireCheck>("expected");
    QTest::newRow("unknown device") << u"d9:1"_s << u"d1:1"_s << 2.5 << WireCheck::UnknownTerminal;
    QTest::newRow("unknown bar hole")
            << u"nbar:9"_s << u"d1:1"_s << 2.5 << WireCheck::UnknownTerminal;
    QTest::newRow("same terminal") << u"d1:1"_s << u"d1:1"_s << 2.5 << WireCheck::SameTerminal;
    QTest::newRow("section") << u"d1:1"_s << u"d1:2"_s << 3.0 << WireCheck::InvalidSection;
    QTest::newRow("third wire") << u"d1:1"_s << u"d3:1"_s << 2.5 << WireCheck::TerminalFull;
    QTest::newRow("second on bar hole")
            << u"supply:PE"_s << u"pebar:1"_s << 2.5 << WireCheck::TerminalFull;
    QTest::newRow("duplicate") << u"d1:2"_s << u"d2:1"_s << 2.5 << WireCheck::Duplicate;
}

void TestWires::refusedWires()
{
    QFETCH(QString, from);
    QFETCH(QString, to);
    QFETCH(double, section);
    QFETCH(WireCheck, expected);

    // d1:1 holds two wires, pebar:1 one, d2:1 -> d1:2 is the duplicate's twin
    QVERIFY(!m_project->addWire(wire(u"supply:L"_s, u"d1:1"_s)).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"d2:2"_s, u"d1:1"_s)).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"supply:PE"_s, u"pebar:1"_s)).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"d2:1"_s, u"d1:2"_s)).isEmpty());
    const QList<Wire> before = m_project->wires();

    QSignalSpy changed(m_project.get(), &Project::changed);
    const Wire candidate = wire(from, to, section);
    QCOMPARE(m_project->wireCheck(candidate), expected);
    QVERIFY(m_project->addWire(candidate).isEmpty());
    QCOMPARE(m_project->wires(), before);
    QCOMPARE(changed.size(), 0);
}

void TestWires::wireLengthRange_data()
{
    QTest::addColumn<double>("length");
    QTest::addColumn<WireCheck>("expected");
    QTest::newRow("zero") << 0.0 << WireCheck::InvalidLength;
    QTest::newRow("negative") << -1.0 << WireCheck::InvalidLength;
    QTest::newRow("past the maximum") << 500.5 << WireCheck::InvalidLength;
    QTest::newRow("not a number") << std::numeric_limits<double>::quiet_NaN()
                                  << WireCheck::InvalidLength;
    QTest::newRow("maximum") << 500.0 << WireCheck::Ok;
    QTest::newRow("short") << 0.3 << WireCheck::Ok;
}

void TestWires::wireLengthRange()
{
    QFETCH(double, length);
    QFETCH(WireCheck, expected);

    Wire candidate = wire(u"supply:L"_s, u"d1:1"_s);
    candidate.length = length;
    QCOMPARE(m_project->wireCheck(candidate), expected);
    const QString id = m_project->addWire(candidate);
    QCOMPARE(id.isEmpty(), expected != WireCheck::Ok);

    // an update to the same length is refused the same way
    const QString existing = m_project->addWire(wire(u"d1:2"_s, u"d2:1"_s));
    QVERIFY(!existing.isEmpty());
    Wire updated = *m_project->wire(existing);
    updated.length = length;
    QCOMPARE(m_project->updateWire(updated), expected == WireCheck::Ok);
}

void TestWires::updateWireChangesSection()
{
    QVERIFY(!m_project->addWire(wire(u"supply:L"_s, u"d1:1"_s)).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"d2:2"_s, u"d1:1"_s)).isEmpty());
    Wire w = *m_project->wire(u"w1");
    QCOMPARE(m_project->wireCheck(w), WireCheck::Ok);

    QSignalSpy changed(m_project.get(), &Project::changed);
    w.section = 6;
    QVERIFY(m_project->updateWire(w));
    QCOMPARE(m_project->wire(u"w1")->section, 6.0);
    QCOMPARE(changed.size(), 1);

    w.section = 3;
    QVERIFY(!m_project->updateWire(w));
    QCOMPARE(m_project->wire(u"w1")->section, 6.0);
    QCOMPARE(changed.size(), 1);
}

void TestWires::removingDeviceRemovesItsWires()
{
    QVERIFY(!m_project->addWire(wire(u"supply:L"_s, u"d1:1"_s)).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"d1:2"_s, u"d2:1"_s)).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"supply:PE"_s, u"pebar:1"_s)).isEmpty());
    QSignalSpy changed(m_project.get(), &Project::changed);
    QVERIFY(m_project->removeDevice(u"d1"));
    QCOMPARE(m_project->wires().size(), 1);
    QCOMPARE(m_project->wires().first().id, u"w3"_s);
    QCOMPARE(changed.size(), 1);
}

void TestWires::poleChangeRefusedWhenItOrphansWires()
{
    QCOMPARE(m_project->addDevice(preset("breaker-2p-c40", 0, 6)), u"d4"_s);
    PlacedDevice oneP = *m_project->device(u"d4");
    oneP.poles = u"1P"_s;

    QVERIFY(m_project->orphanedTerminals(oneP).isEmpty());
    QVERIFY(!m_project->addWire(wire(u"supply:L"_s, u"d4:3"_s)).isEmpty());
    QCOMPARE(m_project->orphanedTerminals(oneP), (QStringList{ u"3"_s }));
    const PlacedDevice before = *m_project->device(u"d4");
    QSignalSpy changed(m_project.get(), &Project::changed);
    QVERIFY(!m_project->updateDevice(oneP));
    QCOMPARE(*m_project->device(u"d4"), before);
    QCOMPARE(changed.size(), 0);

    QVERIFY(m_project->removeWire(u"w1"));
    QVERIFY(m_project->updateDevice(oneP));
    QCOMPARE(m_project->device(u"d4")->poles, u"1P"_s);
}

void TestWires::enclosureShrinkRefusedWithWiredBarHole()
{
    QVERIFY(m_project->setEnclosure(u"enclosure-24"_s));
    QVERIFY(!m_project->addWire(wire(u"supply:N"_s, u"nbar:12"_s)).isEmpty());
    QVERIFY(!m_project->keepsBarWires(u"enclosure-12"));
    QVERIFY(!m_project->fitsEnclosure(u"enclosure-12"));
    QVERIFY(!m_project->setEnclosure(u"enclosure-12"_s));
    QVERIFY(!m_project->keepsBarWires(u"no-such-enclosure"));

    QVERIFY(m_project->removeWire(u"w1"));
    QVERIFY(!m_project->addWire(wire(u"supply:N"_s, u"nbar:8"_s)).isEmpty());
    QVERIFY(m_project->keepsBarWires(u"enclosure-12"));
    QVERIFY(m_project->setEnclosure(u"enclosure-12"_s));
    QCOMPARE(m_project->wires().size(), 1);
}

QTEST_APPLESS_MAIN(TestWires)
#include "tst_wires.moc"
