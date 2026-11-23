// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/nodalsolver.h"
#include "core/project.h"
#include "core/simulation.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QtTest>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace {

// The model worked out by hand (ADR 0007, 0008), with its constants written out.
constexpr double Contact = 0.001; // Ω, a closed contact, also the supply N and PE ties

double wireResistance(double length, double section)
{
    return 0.0175 * length / section;
}

// A load of this power in series with the supply impedance and these resistances.
double seriesCurrent(double resistance, double power)
{
    return 230 / (0.4 + resistance + 230.0 * 230.0 / power);
}

// The base circuit from supply L to the N tie, without its load: two 1 m feeder wires, the two
// load wires, d1's contact and the N tie.
double baseResistance(double length = 15, double section = 1.5)
{
    return 2 * wireResistance(1, 2.5) + 2 * wireResistance(length, section) + 2 * Contact;
}

} // namespace

class TestSimulation : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void everythingStartsOff();
    void lampOnClosedBreaker();
    void wireResistanceFollowsLengthAndSection_data();
    void wireResistanceFollowsLengthAndSection();
    void parallelLampsAddCurrents();
    void sharedNeutralCarriesTheSum();
    void openContactStopsCurrent();
    void twoPoleDeviceSwitchesBothPoles();
    void floatingPartsSolve();
    void pluggedApplianceDrawsThroughItsSocket();
    void unpluggedApplianceDrawsNothing();
    void lampWiredToPeLights();
    void loadBeforeBreakerStaysEnergized();
    void shortIsLimitedBySupplyAndWires();
    void energizedNeedsMoreThanHalfTheVoltage();
    void settingsSurviveEdits();
    void solvesOncePerChange();
    void largePanelSolvesQuickly();

private:
    QString addDevice(const char *presetId, int row, int module);
    // power 0 keeps the type's default
    QString addLoad(const char *type, QPointF position, int power = 0);
    QString plugIn(const char *type, const QString &socket, int place);
    QString addWire(const QString &from, const QString &to, double length, double section);
    // d1, a 1P C10 at (0, 0), feeding the load l1 at (0, 270): supply:L → d1:1 (1 m, 2.5 mm²),
    // d1:2 → l1:L and l1:N → nbar:1 (the load wires), nbar:2 → supply:N (1 m, 2.5 mm²).
    void addBaseCircuit(const char *loadType = "ceiling-lamp", int power = 0, double length = 15,
                        double section = 1.5);
    [[nodiscard]] QString wireBetween(QStringView from, QStringView to) const;
    [[nodiscard]] bool near(double actual, double expected) const;
    [[nodiscard]] bool allResultsFinite() const;

    std::unique_ptr<Project> m_project;
    std::unique_ptr<Simulation> m_simulation;
};

QString TestSimulation::addDevice(const char *presetId, int row, int module)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == QLatin1StringView(presetId)) {
            const QString id = m_project->addDevice(deviceFromPreset(p, row, module));
            if (id.isEmpty())
                qFatal("device refused: %s at %d, %d", presetId, row, module);
            return id;
        }
    }
    qFatal("unknown preset %s", presetId);
}

QString TestSimulation::addLoad(const char *type, QPointF position, int power)
{
    const LoadType *t = embeddedCatalog().loadType(QString::fromLatin1(type));
    if (!t)
        qFatal("unknown load type %s", type);
    PlacedLoad load = loadFromType(*t, position);
    if (power > 0)
        load.power = power;
    const QString id = m_project->addLoad(load);
    if (id.isEmpty())
        qFatal("load refused: %s at %g, %g", type, position.x(), position.y());
    return id;
}

QString TestSimulation::plugIn(const char *type, const QString &socket, int place)
{
    PlacedLoad load = loadFromType(*embeddedCatalog().loadType(QString::fromLatin1(type)), {});
    load.socket = socket;
    load.place = place;
    const QString id = m_project->addLoad(load);
    if (id.isEmpty())
        qFatal("plug refused: %s into %s", type, qPrintable(socket));
    return id;
}

QString TestSimulation::addWire(const QString &from, const QString &to, double length,
                                double section)
{
    const QString id = m_project->addWire(
            Wire{ {}, from, to, section, m_project->conductorFor(from, to), length });
    if (id.isEmpty())
        qFatal("wire refused: %s -> %s", qPrintable(from), qPrintable(to));
    return id;
}

void TestSimulation::addBaseCircuit(const char *loadType, int power, double length, double section)
{
    addDevice("breaker-1p-c10", 0, 0);
    addLoad(loadType, QPointF(0, 270), power);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, length, section);
    addWire(u"l1:N"_s, u"nbar:1"_s, length, section);
    addWire(u"nbar:2"_s, u"supply:N"_s, 1, 2.5);
}

QString TestSimulation::wireBetween(QStringView from, QStringView to) const
{
    for (const Wire &w : m_project->wires()) {
        if (w.from == from && w.to == to)
            return w.id;
    }
    qFatal("no wire %s -> %s", qPrintable(from.toString()), qPrintable(to.toString()));
}

// Gmin draws at most Gmin · 230 V from each terminal, so a solved current can be off the hand
// worked series value by up to that much per terminal, besides rounding.
bool TestSimulation::near(double actual, double expected) const
{
    const double leakage = static_cast<double>(m_project->terminalIds().size()) * Gmin * 230;
    if (std::abs(actual - expected) <= 1e-9 * std::abs(expected) + leakage)
        return true;
    qWarning("%.12g, expected %.12g", actual, expected);
    return false;
}

bool TestSimulation::allResultsFinite() const
{
    QList<double> results;
    for (const Wire &w : m_project->wires())
        results << m_simulation->wireCurrent(w.id);
    for (const PlacedDevice &d : m_project->devices())
        results << m_simulation->deviceCurrent(d.id);
    for (const PlacedLoad &l : m_project->loads())
        results << m_simulation->loadCurrent(l.id) << m_simulation->loadVoltage(l.id);
    return std::ranges::all_of(results, [](double value) { return std::isfinite(value); });
}

void TestSimulation::init()
{
    m_project = std::make_unique<Project>(embeddedCatalog());
    m_simulation = std::make_unique<Simulation>(*m_project);
}

void TestSimulation::cleanup()
{
    m_simulation.reset();
    m_project.reset();
}

void TestSimulation::everythingStartsOff()
{
    addBaseCircuit();
    QVERIFY(!m_simulation->isOn(u"d1"));
    QVERIFY(!m_simulation->isOn(u"l1"));
    // only Gmin leaks along a wire
    for (const Wire &w : m_project->wires())
        QVERIFY(m_simulation->wireCurrent(w.id) < 1e-6);
    QCOMPARE(m_simulation->deviceCurrent(u"d1"), 0.0);
    QCOMPARE(m_simulation->loadCurrent(u"l1"), 0.0);
    QVERIFY(!m_simulation->isEnergized(u"l1"));
}

void TestSimulation::lampOnClosedBreaker()
{
    addBaseCircuit();
    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QVERIFY(m_simulation->setOn(u"l1"_s, true));
    QVERIFY(m_simulation->isOn(u"d1"));
    QVERIFY(m_simulation->isOn(u"l1"));

    const double current = seriesCurrent(baseResistance(), 10);
    QVERIFY(near(m_simulation->loadCurrent(u"l1"), current));
    QVERIFY(near(m_simulation->deviceCurrent(u"d1"), current));
    for (const Wire &w : m_project->wires())
        QVERIFY(near(m_simulation->wireCurrent(w.id), current));
    QVERIFY(near(m_simulation->loadVoltage(u"l1") / (230.0 * 230.0 / 10), current));
    QVERIFY(m_simulation->isEnergized(u"l1"));
}

void TestSimulation::wireResistanceFollowsLengthAndSection_data()
{
    QTest::addColumn<double>("length");
    QTest::addColumn<double>("section");
    QTest::newRow("15 m of 1.5 mm²") << 15.0 << 1.5;
    QTest::newRow("100 m of 1.5 mm²") << 100.0 << 1.5;
    QTest::newRow("15 m of 4 mm²") << 15.0 << 4.0;
}

void TestSimulation::wireResistanceFollowsLengthAndSection()
{
    QFETCH(double, length);
    QFETCH(double, section);
    addBaseCircuit("ceiling-lamp", 2000, length, section);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"l1"_s, true);
    QVERIFY(near(m_simulation->loadCurrent(u"l1"),
                 seriesCurrent(baseResistance(length, section), 2000)));
}

void TestSimulation::parallelLampsAddCurrents()
{
    addBaseCircuit();
    QCOMPARE(addLoad("ceiling-lamp", QPointF(45, 270)), u"l2"_s);
    addWire(u"l1:L"_s, u"l2:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"l2:N"_s, 15, 1.5);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"l1"_s, true);
    m_simulation->setOn(u"l2"_s, true);

    const double i1 = m_simulation->loadCurrent(u"l1");
    const double i2 = m_simulation->loadCurrent(u"l2");
    QVERIFY(i1 > 0.99 * seriesCurrent(baseResistance(), 10));
    QVERIFY(i2 > 0.99 * seriesCurrent(baseResistance(), 10));
    QVERIFY(near(m_simulation->deviceCurrent(u"d1"), i1 + i2));
}

void TestSimulation::sharedNeutralCarriesTheSum()
{
    addBaseCircuit();
    QCOMPARE(addDevice("breaker-1p-c10", 0, 1), u"d2"_s);
    QCOMPARE(addLoad("ceiling-lamp", QPointF(45, 270)), u"l2"_s);
    addWire(u"supply:L"_s, u"d2:1"_s, 1, 2.5);
    addWire(u"d2:2"_s, u"l2:L"_s, 15, 1.5);
    addWire(u"l2:N"_s, u"nbar:3"_s, 15, 1.5);
    for (const QString &id : { u"d1"_s, u"d2"_s, u"l1"_s, u"l2"_s })
        m_simulation->setOn(id, true);

    const double i1 = m_simulation->loadCurrent(u"l1");
    const double i2 = m_simulation->loadCurrent(u"l2");
    QVERIFY(i1 > 0.99 * seriesCurrent(baseResistance(), 10));
    QVERIFY(i2 > 0.99 * seriesCurrent(baseResistance(), 10));
    QVERIFY(near(m_simulation->wireCurrent(wireBetween(u"nbar:2", u"supply:N")), i1 + i2));
}

void TestSimulation::openContactStopsCurrent()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"l1"_s, true);
    QVERIFY(m_simulation->isEnergized(u"l1"));

    QVERIFY(m_simulation->setOn(u"d1"_s, false));
    for (const Wire &w : m_project->wires())
        QVERIFY(m_simulation->wireCurrent(w.id) < 1e-6);
    QVERIFY(m_simulation->deviceCurrent(u"d1") < 1e-6);
    QVERIFY(m_simulation->loadCurrent(u"l1") < 1e-6);
    QVERIFY(!m_simulation->isEnergized(u"l1"));
    QVERIFY(m_simulation->loadVoltage(u"l1") < 1);
}

void TestSimulation::twoPoleDeviceSwitchesBothPoles()
{
    QCOMPARE(addDevice("breaker-2p-c40", 0, 0), u"d1"_s);
    QCOMPARE(addLoad("ceiling-lamp", QPointF(0, 270)), u"l1"_s);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"d1:4"_s, 15, 1.5);
    addWire(u"d1:3"_s, u"supply:N"_s, 1, 2.5);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"l1"_s, true);

    // both contacts and the N tie are in series
    const double current = seriesCurrent(
            2 * wireResistance(1, 2.5) + 2 * wireResistance(15, 1.5) + 3 * Contact, 10);
    QVERIFY(near(m_simulation->wireCurrent(wireBetween(u"supply:L", u"d1:1")), current));
    QVERIFY(near(m_simulation->wireCurrent(wireBetween(u"d1:3", u"supply:N")), current));
    QVERIFY(near(m_simulation->deviceCurrent(u"d1"), current));

    m_simulation->setOn(u"d1"_s, false);
    for (const Wire &w : m_project->wires())
        QVERIFY(m_simulation->wireCurrent(w.id) < 1e-6);
    QVERIFY(m_simulation->deviceCurrent(u"d1") < 1e-6);
    QVERIFY(m_simulation->loadCurrent(u"l1") < 1e-6);
}

void TestSimulation::floatingPartsSolve()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"l1"_s, true);
    const double before = m_simulation->loadCurrent(u"l1");

    QCOMPARE(addDevice("rcd-2p-40-30", 0, 2), u"d2"_s);
    QCOMPARE(addLoad("ceiling-lamp", QPointF(45, 270)), u"l2"_s);
    addWire(u"d1:2"_s, u"l2:L"_s, 15, 1.5);
    m_simulation->setOn(u"d2"_s, true);
    m_simulation->setOn(u"l2"_s, true);

    QVERIFY(allResultsFinite());
    QVERIFY(m_simulation->loadCurrent(u"l2") < 1e-6);
    QVERIFY(near(m_simulation->loadCurrent(u"l1"), before));
}

void TestSimulation::pluggedApplianceDrawsThroughItsSocket()
{
    addBaseCircuit("socket");
    const QString kettle = plugIn("kettle", u"l1"_s, 1);
    m_simulation->setOn(u"d1"_s, true);
    QVERIFY(m_simulation->setOn(kettle, true));

    QVERIFY(near(m_simulation->loadCurrent(kettle), seriesCurrent(baseResistance(), 2000)));
    QVERIFY(m_simulation->isEnergized(kettle));

    m_simulation->setOn(kettle, false);
    QCOMPARE(m_simulation->loadCurrent(kettle), 0.0);
    QVERIFY(near(m_simulation->deviceCurrent(u"d1"), 0));
    QVERIFY(!m_simulation->isEnergized(kettle));

    QVERIFY(!m_simulation->setOn(u"l1"_s, true));
    QVERIFY(!m_simulation->isOn(u"l1"));
}

void TestSimulation::unpluggedApplianceDrawsNothing()
{
    addBaseCircuit("socket");
    const QString kettle = addLoad("kettle", QPointF(200, 400));
    m_simulation->setOn(u"d1"_s, true);
    QVERIFY(m_simulation->setOn(kettle, true));

    QCOMPARE(m_simulation->loadCurrent(kettle), 0.0);
    QVERIFY(!m_simulation->isEnergized(kettle));
    QCOMPARE(m_simulation->loadVoltage(kettle), 0.0);
}

void TestSimulation::lampWiredToPeLights()
{
    addDevice("breaker-1p-c10", 0, 0);
    addLoad("ceiling-lamp", QPointF(0, 270));
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"pebar:1"_s, 15, 1.5);
    addWire(u"pebar:2"_s, u"supply:PE"_s, 1, 2.5);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"l1"_s, true);

    QVERIFY(m_simulation->isEnergized(u"l1"));
    // the PE tie is as strong as the N tie
    const double current = seriesCurrent(baseResistance(), 10);
    QVERIFY(near(m_simulation->wireCurrent(wireBetween(u"l1:N", u"pebar:1")), current));
    QVERIFY(near(m_simulation->wireCurrent(wireBetween(u"pebar:2", u"supply:PE")), current));
}

void TestSimulation::loadBeforeBreakerStaysEnergized()
{
    addDevice("breaker-1p-c10", 0, 0);
    addLoad("ceiling-lamp", QPointF(0, 270));
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"supply:L"_s, u"l1:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"nbar:1"_s, 15, 1.5);
    addWire(u"nbar:2"_s, u"supply:N"_s, 1, 2.5);
    m_simulation->setOn(u"l1"_s, true);

    QVERIFY(!m_simulation->isOn(u"d1"));
    QVERIFY(m_simulation->isEnergized(u"l1"));
}

void TestSimulation::shortIsLimitedBySupplyAndWires()
{
    addDevice("breaker-1p-c10", 0, 0);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"nbar:1"_s, 0.3, 1.5);
    addWire(u"nbar:2"_s, u"supply:N"_s, 1, 2.5);
    m_simulation->setOn(u"d1"_s, true);

    const double current
            = 230 / (0.4 + 2 * wireResistance(1, 2.5) + wireResistance(0.3, 1.5) + 2 * Contact);
    QVERIFY(allResultsFinite());
    QVERIFY(near(m_simulation->deviceCurrent(u"d1"), current));
    for (const Wire &w : m_project->wires())
        QVERIFY(near(m_simulation->wireCurrent(w.id), current));
    QVERIFY(m_simulation->deviceCurrent(u"d1") > 100);
}

void TestSimulation::energizedNeedsMoreThanHalfTheVoltage()
{
    addDevice("breaker-1p-c10", 0, 0);
    addLoad("ceiling-lamp", QPointF(0, 270));
    addLoad("ceiling-lamp", QPointF(45, 270), 100);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"l2:L"_s, 15, 1.5);
    addWire(u"l2:N"_s, u"nbar:1"_s, 15, 1.5);
    addWire(u"nbar:2"_s, u"supply:N"_s, 1, 2.5);
    for (const QString &id : { u"d1"_s, u"l1"_s, u"l2"_s })
        m_simulation->setOn(id, true);

    // l2 is a resistance in l1's series circuit
    const double z1 = 230.0 * 230.0 / 10;
    const double z2 = 230.0 * 230.0 / 100;
    const double current = seriesCurrent(
            2 * wireResistance(1, 2.5) + 3 * wireResistance(15, 1.5) + 2 * Contact + z2, 10);
    QVERIFY(near(m_simulation->loadVoltage(u"l1") / z1, current)); // about 209 V
    QVERIFY(near(m_simulation->loadVoltage(u"l2") / z2, current)); // about 21 V
    QVERIFY(m_simulation->isEnergized(u"l1"));
    QVERIFY(!m_simulation->isEnergized(u"l2"));
}

void TestSimulation::settingsSurviveEdits()
{
    addBaseCircuit();
    QVERIFY(m_simulation->setOn(u"d1"_s, true));

    PlacedDevice moved = *m_project->device(u"d1");
    moved.module = 3;
    QVERIFY(m_project->updateDevice(moved));
    QVERIFY(m_simulation->isOn(u"d1"));

    QVERIFY(m_project->removeDevice(u"d1"));
    QCOMPARE(m_project->addDevice(moved), u"d1"_s);
    QVERIFY(m_simulation->isOn(u"d1"));

    const QString added = addDevice("breaker-1p-c10", 0, 5);
    QVERIFY(!m_simulation->isOn(added));
    QVERIFY(m_simulation->isOn(u"d1"));

    const QString socket = addLoad("socket", QPointF(100, 270));
    const QString kettle = plugIn("kettle", socket, 1);
    QVERIFY(m_simulation->setOn(kettle, true));
    PlacedLoad unplugged = *m_project->load(kettle);
    unplugged.socket.clear();
    unplugged.place = 0;
    unplugged.position = QPointF(200, 400);
    QVERIFY(m_project->updateLoad(unplugged));
    QVERIFY(m_simulation->isOn(kettle));
    PlacedLoad replugged = unplugged;
    replugged.socket = socket;
    replugged.place = 1;
    QVERIFY(m_project->updateLoad(replugged));
    QVERIFY(m_simulation->isOn(kettle));

    QVERIFY(!m_simulation->setOn(u"d99"_s, true));
    QVERIFY(!m_simulation->isOn(u"d99"));
}

void TestSimulation::solvesOncePerChange()
{
    QSignalSpy solved(m_simulation.get(), &Simulation::solved);
    addDevice("breaker-1p-c10", 0, 0);
    QCOMPARE(solved.count(), 1);
    addLoad("ceiling-lamp", QPointF(0, 270));
    QCOMPARE(solved.count(), 2);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    QCOMPARE(solved.count(), 3);
    QVERIFY(m_project->setEnclosure(u"enclosure-24"_s));
    QCOMPARE(solved.count(), 4);

    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QCOMPARE(solved.count(), 5);
    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QVERIFY(m_simulation->setOn(u"l1"_s, false));
    QCOMPARE(solved.count(), 5);
    QVERIFY(m_simulation->setOn(u"d1"_s, false));
    QCOMPARE(solved.count(), 6);
}

void TestSimulation::largePanelSolvesQuickly()
{
    addBaseCircuit();
    QVERIFY(m_project->setEnclosure(u"enclosure-36"_s));
    for (int row = 0; row < 3; ++row) {
        for (int module = 0; module < 12; ++module) {
            if (row > 0 || module > 0)
                addDevice("breaker-1p-c10", row, module);
        }
    }
    for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 10; ++column)
            addLoad("ceiling-lamp", QPointF(45 * column, 630 + 54 * row));
    }
    QCOMPARE(m_project->devices().size(), 36);
    QCOMPARE(m_project->loads().size(), 61);
    for (const PlacedDevice &d : m_project->devices())
        m_simulation->setOn(d.id, true);
    for (const PlacedLoad &l : m_project->loads()) {
        if (l.id != u"l1")
            m_simulation->setOn(l.id, true);
    }

    QElapsedTimer timer;
    timer.start();
    QVERIFY(m_simulation->setOn(u"l1"_s, true));
    const double milliseconds = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    QTest::setBenchmarkResult(milliseconds, QTest::WalltimeMilliseconds);
    QVERIFY(milliseconds < 100);
    QVERIFY(m_simulation->isEnergized(u"l1"));
}

QTEST_APPLESS_MAIN(TestSimulation)
#include "tst_simulation.moc"
