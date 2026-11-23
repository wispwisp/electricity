// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/nodalsolver.h"
#include "core/project.h"
#include "core/simulation.h"

#include <QSignalSpy>
#include <QtTest>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace {

// The model worked out by hand (ADR 0007, 0008, 0030), with its constants written out.
constexpr double Contact = 0.001;   // Ω, a closed contact, also the supply N and PE ties
constexpr double FaultOhms = 0.001; // Ω, a short circuit or an earth fault
constexpr double PersonOhms = 1000; // Ω, leakage from L to earth

double wireResistance(double length, double section)
{
    return 0.0175 * length / section;
}

// The current through these resistances in series with the supply impedance.
double seriesCurrent(double resistance)
{
    return 230 / (0.4 + resistance);
}

// The base circuit from supply L to the N tie, without its load: two 1 m feeder wires, the two
// load wires, d1's contact and the N tie.
double baseResistance(double length = 15)
{
    return 2 * wireResistance(1, 2.5) + 2 * wireResistance(length, 1.5) + 2 * Contact;
}

// The length of the base circuit's load wires that makes a short circuit at the load draw this
// current: 230 / current = 0.4 + baseResistance(length) + FaultOhms.
double lengthForShort(double current)
{
    const double rest = 230 / current - 0.4 - baseResistance(0) - FaultOhms;
    return rest / (2 * wireResistance(1, 1.5));
}

// V(l1:L) and the current through a person there, behind one contact and its two wires.
double leakageCurrent()
{
    return seriesCurrent(wireResistance(1, 2.5) + Contact + wireResistance(15, 1.5) + PersonOhms);
}

} // namespace

class TestProtection : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void shortTripsBreakerInstantly();
    void magneticTripAtUpperBand_data();
    void magneticTripAtUpperBand();
    void smallestBreakerTripsFirst();
    void shortWithNoBreakerTripsTheStairwellBreaker();
    void leakageTripsTheRcd();
    void neutralBypassingTheRcdTrips_data();
    void neutralBypassingTheRcdTrips();
    void earthFaultTripsBreakerAndRcd();
    void rcboTripsOnShortLeakageAndEarthFault_data();
    void rcboTripsOnShortLeakageAndEarthFault();
    void faultOnUnpluggedApplianceWaitsForItsSocket();
    void resetWithFaultStillThereTripsAgain();
    void tripOpensBothPolesOfA2PDevice();
    void handSwitchOffClearsTheTrip();
    void leakageFlowsToEarthWithoutPe();
    void earthFaultWithoutPeLeavesTheCaseLive();
    void unprotectedLeakageIsDangerous();
    void protectedLeakageIsNotDangerous();
    void testButtonTripsOnlyWithPower();
    void tripsAndFaultsSurviveRemoveAndRestore();
    void projectEditsKeepTripsAndFaults();
    void setFaultRefusesUnknownLoadsAndRepeats();
    void eventsOncePerChange();

private:
    // ratedCurrent 0 keeps the preset's
    QString addDevice(const char *presetId, int row, int module, int ratedCurrent = 0);
    // power 0 keeps the type's default
    QString addLoad(const char *type, QPointF position, int power = 0);
    QString addWire(const QString &from, const QString &to, double length, double section);
    // d1, a 1P C10 at (0, 0), feeding the load l1 at (0, 270): supply:L → d1:1 (1 m, 2.5 mm²),
    // d1:2 → l1:L and l1:N → nbar:1 (the load wires, 1.5 mm²), nbar:2 → supply:N (1 m, 2.5 mm²).
    void addBaseCircuit(const char *loadType = "ceiling-lamp", double length = 15);
    // d1, an RCD or RCBO at (0, 0), feeding the lamp l1 at (0, 270): supply:L → d1:1 and
    // supply:N → d1:N1 (1 m, 2.5 mm²), d1:2 → l1:L and l1:N → d1:N2 (15 m, 1.5 mm²); with pe,
    // l1:PE → pebar:1 (15 m, 1.5 mm²) and pebar:2 → supply:PE (1 m, 2.5 mm²).
    void addRcdCircuit(const char *presetId = "rcd-2p-40-30", bool pe = true);
    [[nodiscard]] bool near(double actual, double expected) const;
    // only Gmin leaks through every wire, device and load
    [[nodiscard]] bool everyCurrentGone() const;
    [[nodiscard]] static QList<SimulationEvent> events(const QSignalSpy &spy);

    std::unique_ptr<Project> m_project;
    std::unique_ptr<Simulation> m_simulation;
};

QString TestProtection::addDevice(const char *presetId, int row, int module, int ratedCurrent)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == QLatin1StringView(presetId)) {
            PlacedDevice device = deviceFromPreset(p, row, module);
            if (ratedCurrent > 0)
                device.parameters[u"ratedCurrent"_s] = ratedCurrent;
            const QString id = m_project->addDevice(device);
            if (id.isEmpty())
                qFatal("device refused: %s at %d, %d", presetId, row, module);
            return id;
        }
    }
    qFatal("unknown preset %s", presetId);
}

QString TestProtection::addLoad(const char *type, QPointF position, int power)
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

QString TestProtection::addWire(const QString &from, const QString &to, double length,
                                double section)
{
    const QString id = m_project->addWire(
            Wire{ {}, from, to, section, m_project->conductorFor(from, to), length });
    if (id.isEmpty())
        qFatal("wire refused: %s -> %s", qPrintable(from), qPrintable(to));
    return id;
}

void TestProtection::addBaseCircuit(const char *loadType, double length)
{
    addDevice("breaker-1p-c10", 0, 0);
    addLoad(loadType, QPointF(0, 270));
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, length, 1.5);
    addWire(u"l1:N"_s, u"nbar:1"_s, length, 1.5);
    addWire(u"nbar:2"_s, u"supply:N"_s, 1, 2.5);
}

void TestProtection::addRcdCircuit(const char *presetId, bool pe)
{
    addDevice(presetId, 0, 0);
    addLoad("ceiling-lamp", QPointF(0, 270));
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"supply:N"_s, u"d1:N1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"d1:N2"_s, 15, 1.5);
    if (pe) {
        addWire(u"l1:PE"_s, u"pebar:1"_s, 15, 1.5);
        addWire(u"pebar:2"_s, u"supply:PE"_s, 1, 2.5);
    }
}

// Gmin draws at most Gmin · 230 V from each terminal, so a solved current can be off the hand
// worked series value by up to that much per terminal, besides rounding.
bool TestProtection::near(double actual, double expected) const
{
    const double leakage = static_cast<double>(m_project->terminalIds().size()) * Gmin * 230;
    if (std::abs(actual - expected) <= 1e-9 * std::abs(expected) + leakage)
        return true;
    qWarning("%.12g, expected %.12g", actual, expected);
    return false;
}

bool TestProtection::everyCurrentGone() const
{
    QList<double> currents;
    for (const Wire &w : m_project->wires())
        currents << m_simulation->wireCurrent(w.id);
    for (const PlacedDevice &d : m_project->devices())
        currents << m_simulation->deviceCurrent(d.id);
    for (const PlacedLoad &l : m_project->loads())
        currents << m_simulation->loadCurrent(l.id);
    return std::ranges::all_of(currents, [](double current) { return current < 1e-6; });
}

QList<SimulationEvent> TestProtection::events(const QSignalSpy &spy)
{
    QList<SimulationEvent> list;
    for (const QList<QVariant> &arguments : spy)
        list << arguments.at(0).value<SimulationEvent>();
    return list;
}

void TestProtection::init()
{
    m_project = std::make_unique<Project>(embeddedCatalog());
    m_simulation = std::make_unique<Simulation>(*m_project);
}

void TestProtection::cleanup()
{
    m_simulation.reset();
    m_project.reset();
}

void TestProtection::shortTripsBreakerInstantly()
{
    addBaseCircuit();
    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::ShortCircuit));

    QCOMPARE(m_simulation->fault(u"l1"), Fault::ShortCircuit);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QVERIFY(!m_simulation->isOn(u"d1"));
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Tripped);
    QCOMPARE(list[0].id, u"d1"_s);
    QCOMPARE(list[0].cause, TripCause::ShortCircuit);
    QVERIFY(near(list[0].current, seriesCurrent(baseResistance() + FaultOhms))); // about 299.9 A
    QCOMPARE(list[0].limit, 10.0);
    QCOMPARE(list[0].time, 0.0);
    QVERIFY(everyCurrentGone());
}

void TestProtection::magneticTripAtUpperBand_data()
{
    QTest::addColumn<double>("current");
    QTest::addColumn<bool>("trips");
    QTest::newRow("99 A, under 10 · In") << 99.0 << false;
    QTest::newRow("101 A") << 101.0 << true;
}

void TestProtection::magneticTripAtUpperBand()
{
    QFETCH(double, current);
    QFETCH(bool, trips);
    addBaseCircuit("ceiling-lamp", lengthForShort(current));
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    m_simulation->setFault(u"l1"_s, Fault::ShortCircuit);

    QCOMPARE(m_simulation->tripCause(u"d1"), trips ? TripCause::ShortCircuit : TripCause::None);
    QCOMPARE(m_simulation->isOn(u"d1"), !trips);
    if (trips) {
        QCOMPARE(occurred.count(), 1);
        QVERIFY(near(events(occurred)[0].current, current));
    } else {
        QCOMPARE(occurred.count(), 0);
        QVERIFY(near(m_simulation->deviceCurrent(u"d1"), current));
    }
}

void TestProtection::smallestBreakerTripsFirst()
{
    QCOMPARE(addDevice("breaker-1p-c10", 0, 0, 40), u"d1"_s);
    QCOMPARE(addDevice("breaker-1p-c10", 0, 1, 16), u"d2"_s);
    QCOMPARE(addLoad("socket", QPointF(0, 270)), u"l1"_s);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"d2:1"_s, 0.3, 2.5);
    addWire(u"d2:2"_s, u"l1:L"_s, 0.3, 2.5);
    addWire(u"l1:N"_s, u"nbar:1"_s, 0.3, 2.5);
    addWire(u"nbar:2"_s, u"supply:N"_s, 1, 2.5);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"d2"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::ShortCircuit));

    // about 542 A: over d2's 160 A, d1's 400 A and the stairwell breaker's 500 A
    const double current = seriesCurrent(2 * wireResistance(1, 2.5) + 3 * wireResistance(0.3, 2.5)
                                         + 3 * Contact + FaultOhms);
    QVERIFY(current > 500);
    QCOMPARE(m_simulation->tripCause(u"d2"), TripCause::ShortCircuit);
    QVERIFY(!m_simulation->isOn(u"d2"));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);
    QVERIFY(m_simulation->isOn(u"d1"));
    QCOMPARE(m_simulation->tripCause(SupplyBreakerId), TripCause::None);
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].id, u"d2"_s);
    QVERIFY(near(list[0].current, current));
    QCOMPARE(list[0].limit, 16.0);
}

void TestProtection::shortWithNoBreakerTripsTheStairwellBreaker()
{
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    const QString bare = addWire(u"supply:L"_s, u"supply:N"_s, 0.3, 2.5);

    QCOMPARE(m_simulation->tripCause(SupplyBreakerId), TripCause::ShortCircuit);
    QVERIFY(m_simulation->wireCurrent(bare) < 1e-6);
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Tripped);
    QCOMPARE(list[0].id, u"supply"_s);
    QCOMPARE(list[0].cause, TripCause::ShortCircuit);
    // about 570.6 A over the C50's 500 A
    QVERIFY(near(list[0].current, seriesCurrent(wireResistance(0.3, 2.5) + Contact)));
    QCOMPARE(list[0].limit, 50.0);

    QVERIFY(m_simulation->resetSupplyBreaker());
    QCOMPARE(m_simulation->tripCause(SupplyBreakerId), TripCause::ShortCircuit);

    QVERIFY(m_project->removeWire(bare));
    QCOMPARE(m_simulation->tripCause(SupplyBreakerId), TripCause::ShortCircuit);
    QVERIFY(m_simulation->resetSupplyBreaker());
    QCOMPARE(m_simulation->tripCause(SupplyBreakerId), TripCause::None);
    QVERIFY(!m_simulation->resetSupplyBreaker());
}

void TestProtection::leakageTripsTheRcd()
{
    addRcdCircuit();
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::Leakage));

    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::Leakage);
    QVERIFY(!m_simulation->isOn(u"d1"));
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Tripped);
    QCOMPARE(list[0].id, u"d1"_s);
    QCOMPARE(list[0].cause, TripCause::Leakage);
    // V(l1:L) / 1 kΩ, about 0.2299 A
    const double residual = leakageCurrent();
    QVERIFY(std::abs(list[0].current - residual) < 1e-3 * residual);
    QCOMPARE(list[0].limit, 0.03);
}

void TestProtection::neutralBypassingTheRcdTrips_data()
{
    QTest::addColumn<int>("power");
    QTest::addColumn<bool>("trips");
    QTest::newRow("6 W, about 26 mA") << 6 << false;
    QTest::newRow("10 W, about 43 mA") << 10 << true;
}

void TestProtection::neutralBypassingTheRcdTrips()
{
    QFETCH(int, power);
    QFETCH(bool, trips);
    addDevice("rcd-2p-40-30", 0, 0);
    addLoad("ceiling-lamp", QPointF(0, 270), power);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"supply:N"_s, u"d1:N1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, 15, 1.5);
    // the lamp's N goes back past the RCD, so all of its current is residual
    addWire(u"l1:N"_s, u"supply:N"_s, 15, 1.5);
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    m_simulation->setOn(u"l1"_s, true);

    const double current = seriesCurrent(wireResistance(1, 2.5) + 2 * wireResistance(15, 1.5)
                                         + 2 * Contact + 230.0 * 230.0 / power);
    QCOMPARE(current >= 0.03, trips);
    QCOMPARE(m_simulation->tripCause(u"d1"), trips ? TripCause::Leakage : TripCause::None);
    QCOMPARE(occurred.count(), trips ? 1 : 0);
    if (trips)
        QVERIFY(near(events(occurred)[0].current, current));
    else
        QVERIFY(near(m_simulation->loadCurrent(u"l1"), current));
}

void TestProtection::earthFaultTripsBreakerAndRcd()
{
    QCOMPARE(addDevice("breaker-1p-c16", 0, 0), u"d1"_s);
    QCOMPARE(addDevice("rcd-2p-40-30", 0, 1), u"d2"_s);
    QCOMPARE(addLoad("socket", QPointF(0, 270)), u"l1"_s);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"d2:1"_s, 0.3, 2.5);
    addWire(u"supply:N"_s, u"d2:N1"_s, 1, 2.5);
    addWire(u"d2:2"_s, u"l1:L"_s, 15, 2.5);
    addWire(u"l1:N"_s, u"d2:N2"_s, 15, 2.5);
    addWire(u"l1:PE"_s, u"pebar:1"_s, 15, 2.5);
    addWire(u"pebar:2"_s, u"supply:PE"_s, 1, 2.5);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"d2"_s, true);
    QSignalSpy solved(m_simulation.get(), &Simulation::solved);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::EarthFault));

    QCOMPARE(solved.count(), 1);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QCOMPARE(m_simulation->tripCause(u"d2"), TripCause::Leakage);
    // about 365 A from L through the fault to PE, over the C16's 160 A; all of it is residual
    const double current = seriesCurrent(2 * wireResistance(1, 2.5) + wireResistance(0.3, 2.5)
                                         + 2 * wireResistance(15, 2.5) + 3 * Contact + FaultOhms);
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 2);
    for (const SimulationEvent &event : list) {
        QCOMPARE(event.kind, SimulationEvent::Kind::Tripped);
        QVERIFY(near(event.current, current));
        if (event.id == u"d1") {
            QCOMPARE(event.cause, TripCause::ShortCircuit);
            QCOMPARE(event.limit, 16.0);
        } else {
            QCOMPARE(event.id, u"d2"_s);
            QCOMPARE(event.cause, TripCause::Leakage);
            QCOMPARE(event.limit, 0.03);
        }
    }
}

void TestProtection::rcboTripsOnShortLeakageAndEarthFault_data()
{
    QTest::addColumn<Fault>("fault");
    QTest::addColumn<TripCause>("cause");
    QTest::newRow("short circuit") << Fault::ShortCircuit << TripCause::ShortCircuit;
    QTest::newRow("leakage") << Fault::Leakage << TripCause::Leakage;
    QTest::newRow("earth fault, both ways") << Fault::EarthFault << TripCause::ShortCircuit;
}

void TestProtection::rcboTripsOnShortLeakageAndEarthFault()
{
    QFETCH(Fault, fault);
    QFETCH(TripCause, cause);
    addRcdCircuit("rcbo-c16-30");
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, fault));

    QCOMPARE(m_simulation->tripCause(u"d1"), cause);
    QVERIFY(!m_simulation->isOn(u"d1"));
    QCOMPARE(occurred.count(), 1);
    QCOMPARE(events(occurred)[0].limit, cause == TripCause::Leakage ? 0.03 : 16.0);
}

void TestProtection::faultOnUnpluggedApplianceWaitsForItsSocket()
{
    addBaseCircuit("socket");
    const QString kettle = addLoad("kettle", QPointF(200, 400));
    m_simulation->setOn(u"d1"_s, true);
    QVERIFY(m_simulation->setFault(kettle, Fault::ShortCircuit));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);
    QVERIFY(m_simulation->isOn(u"d1"));

    PlacedLoad plugged = *m_project->load(kettle);
    plugged.socket = u"l1"_s;
    plugged.place = 1;
    QVERIFY(m_project->updateLoad(plugged));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);

    PlacedLoad unplugged = plugged;
    unplugged.socket.clear();
    unplugged.place = 0;
    QVERIFY(m_project->updateLoad(unplugged));
    QCOMPARE(m_simulation->fault(kettle), Fault::ShortCircuit);
}

void TestProtection::resetWithFaultStillThereTripsAgain()
{
    addBaseCircuit();
    m_simulation->setOn(u"l1"_s, true);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setFault(u"l1"_s, Fault::ShortCircuit);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);

    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QVERIFY(!m_simulation->isOn(u"d1"));
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Cleared);
    QCOMPARE(list[0].id, u"d1"_s);
    QCOMPARE(list[1].kind, SimulationEvent::Kind::Tripped);
    QCOMPARE(list[1].id, u"d1"_s);
    QCOMPARE(list[1].cause, TripCause::ShortCircuit);

    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::None));
    QCOMPARE(m_simulation->fault(u"l1"), Fault::None);
    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QVERIFY(m_simulation->isOn(u"d1"));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);
    QVERIFY(m_simulation->isEnergized(u"l1"));
}

void TestProtection::tripOpensBothPolesOfA2PDevice()
{
    QCOMPARE(addDevice("breaker-2p-c40", 0, 0), u"d1"_s);
    QCOMPARE(addLoad("ceiling-lamp", QPointF(0, 270)), u"l1"_s);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"l1:L"_s, 3, 1.5);
    addWire(u"l1:N"_s, u"d1:4"_s, 3, 1.5);
    addWire(u"d1:3"_s, u"supply:N"_s, 1, 2.5);
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::ShortCircuit));

    // about 471 A through both contacts in series, over the C40's 400 A
    const double current = seriesCurrent(2 * wireResistance(1, 2.5) + 2 * wireResistance(3, 1.5)
                                         + 3 * Contact + FaultOhms);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QVERIFY(!m_simulation->isOn(u"d1"));
    QCOMPARE(occurred.count(), 1);
    QVERIFY(near(events(occurred)[0].current, current));
    QCOMPARE(m_simulation->deviceCurrent(u"d1"), 0.0);
    QVERIFY(everyCurrentGone());
}

void TestProtection::handSwitchOffClearsTheTrip()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setFault(u"l1"_s, Fault::ShortCircuit);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);

    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setOn(u"d1"_s, false));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);
    QVERIFY(!m_simulation->isOn(u"d1"));
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Cleared);
    QCOMPARE(list[0].id, u"d1"_s);
}

void TestProtection::leakageFlowsToEarthWithoutPe()
{
    addRcdCircuit("rcd-2p-40-30", false);
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::Leakage));

    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::Leakage);
    QCOMPARE(occurred.count(), 1);
    const double residual = leakageCurrent();
    QVERIFY(std::abs(events(occurred)[0].current - residual) < 1e-3 * residual);
}

void TestProtection::earthFaultWithoutPeLeavesTheCaseLive()
{
    QCOMPARE(addDevice("breaker-1p-c10", 0, 0), u"d1"_s);
    QCOMPARE(addDevice("rcd-2p-40-30", 0, 1), u"d2"_s);
    QCOMPARE(addLoad("ceiling-lamp", QPointF(0, 270)), u"l1"_s);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"d2:1"_s, 0.3, 2.5);
    addWire(u"supply:N"_s, u"d2:N1"_s, 1, 2.5);
    addWire(u"d2:2"_s, u"l1:L"_s, 15, 1.5);
    addWire(u"l1:N"_s, u"d2:N2"_s, 15, 1.5);
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setOn(u"d2"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::EarthFault));

    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);
    QCOMPARE(m_simulation->tripCause(u"d2"), TripCause::None);
    const double touch = m_simulation->touchVoltage(u"l1");
    QVERIFY(touch > 200);
    QVERIFY(m_simulation->isDangerous(u"l1"));
    QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Dangerous);
    QCOMPARE(list[0].id, u"l1"_s);
    QCOMPARE(list[0].voltage, touch);
    QCOMPARE(list[0].current, 0.0);

    occurred.clear();
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::None));
    list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Cleared);
    QCOMPARE(list[0].id, u"l1"_s);
    QVERIFY(!m_simulation->isDangerous(u"l1"));
    QCOMPARE(m_simulation->touchVoltage(u"l1"), 0.0);
}

void TestProtection::unprotectedLeakageIsDangerous()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::Leakage));

    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);
    QVERIFY(m_simulation->isOn(u"d1"));
    // V(l1:L), about 229.9 V
    const double touch = leakageCurrent() * PersonOhms;
    QVERIFY(std::abs(m_simulation->touchVoltage(u"l1") - touch) < 1e-3 * touch);
    QVERIFY(m_simulation->isDangerous(u"l1"));
    QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Dangerous);
    QCOMPARE(list[0].id, u"l1"_s);
    QCOMPARE(list[0].voltage, m_simulation->touchVoltage(u"l1"));
    QCOMPARE(list[0].current, m_simulation->touchVoltage(u"l1") / PersonOhms);

    occurred.clear();
    QVERIFY(m_simulation->setOn(u"d1"_s, false));
    list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Cleared);
    QCOMPARE(list[0].id, u"l1"_s);
    QVERIFY(!m_simulation->isDangerous(u"l1"));
}

void TestProtection::protectedLeakageIsNotDangerous()
{
    addRcdCircuit();
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::Leakage));

    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::Leakage);
    QVERIFY(m_simulation->touchVoltage(u"l1") < 1);
    QVERIFY(!m_simulation->isDangerous(u"l1"));
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Tripped);
}

void TestProtection::testButtonTripsOnlyWithPower()
{
    QCOMPARE(addDevice("rcd-2p-40-30", 0, 0), u"d1"_s);
    QCOMPARE(addDevice("rcd-2p-40-30", 0, 2), u"d2"_s);
    QCOMPARE(addDevice("breaker-1p-c10", 0, 4), u"d3"_s);
    addWire(u"supply:L"_s, u"d1:1"_s, 1, 2.5);
    addWire(u"supply:N"_s, u"d1:N1"_s, 1, 2.5);
    addWire(u"d1:2"_s, u"d3:1"_s, 0.3, 2.5);
    for (const QString &id : { u"d1"_s, u"d2"_s, u"d3"_s })
        m_simulation->setOn(id, true);

    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    QVERIFY(m_simulation->pressTest(u"d1"_s));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::Test);
    QVERIFY(!m_simulation->isOn(u"d1"));
    const QList<SimulationEvent> list = events(occurred);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].kind, SimulationEvent::Kind::Tripped);
    QCOMPARE(list[0].id, u"d1"_s);
    QCOMPARE(list[0].cause, TripCause::Test);
    QCOMPARE(list[0].limit, 0.03);

    QVERIFY(m_simulation->setOn(u"d1"_s, false));
    QVERIFY(!m_simulation->pressTest(u"d1"_s));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::None);

    QVERIFY(!m_simulation->pressTest(u"d2"_s)); // on, but nothing feeds it
    QCOMPARE(m_simulation->tripCause(u"d2"), TripCause::None);

    QVERIFY(m_simulation->setOn(u"d1"_s, true));
    QVERIFY(!m_simulation->pressTest(u"d3"_s)); // a breaker, fed
    QCOMPARE(m_simulation->tripCause(u"d3"), TripCause::None);
    QVERIFY(!m_simulation->pressTest(u"d99"_s));
}

void TestProtection::tripsAndFaultsSurviveRemoveAndRestore()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setFault(u"l1"_s, Fault::ShortCircuit);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    const PlacedDevice device = *m_project->device(u"d1");
    const PlacedLoad load = *m_project->load(u"l1");

    QVERIFY(m_project->removeLoad(u"l1"));
    QVERIFY(m_project->removeDevice(u"d1"));
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QCOMPARE(m_simulation->fault(u"l1"), Fault::ShortCircuit);

    QCOMPARE(m_project->addDevice(device), u"d1"_s);
    QCOMPARE(m_project->addLoad(load), u"l1"_s);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QVERIFY(!m_simulation->isOn(u"d1"));
    QCOMPARE(m_simulation->fault(u"l1"), Fault::ShortCircuit);
}

void TestProtection::projectEditsKeepTripsAndFaults()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    m_simulation->setFault(u"l1"_s, Fault::ShortCircuit);
    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);

    PlacedDevice rated = *m_project->device(u"d1");
    rated.parameters[u"ratedCurrent"_s] = 16;
    QVERIFY(m_project->updateDevice(rated));
    addWire(u"supply:PE"_s, u"pebar:1"_s, 1, 2.5);

    QCOMPARE(m_simulation->tripCause(u"d1"), TripCause::ShortCircuit);
    QVERIFY(!m_simulation->isOn(u"d1"));
    QCOMPARE(m_simulation->fault(u"l1"), Fault::ShortCircuit);
}

void TestProtection::setFaultRefusesUnknownLoadsAndRepeats()
{
    addBaseCircuit();
    QVERIFY(!m_simulation->setFault(u"l99"_s, Fault::ShortCircuit));
    QCOMPARE(m_simulation->fault(u"l99"), Fault::None);
    QVERIFY(!m_simulation->setFault(u"d1"_s, Fault::ShortCircuit));
    QCOMPARE(m_simulation->fault(u"d1"), Fault::None);
    QCOMPARE(m_simulation->fault(u"l1"), Fault::None);

    QSignalSpy solved(m_simulation.get(), &Simulation::solved);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::Leakage));
    QCOMPARE(solved.count(), 1);
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::Leakage));
    QCOMPARE(solved.count(), 1);
    // one fault per load: a new one replaces it
    QVERIFY(m_simulation->setFault(u"l1"_s, Fault::EarthFault));
    QCOMPARE(solved.count(), 2);
    QCOMPARE(m_simulation->fault(u"l1"), Fault::EarthFault);
}

void TestProtection::eventsOncePerChange()
{
    addBaseCircuit();
    m_simulation->setOn(u"d1"_s, true);
    QSignalSpy occurred(m_simulation.get(), &Simulation::occurred);
    m_simulation->setFault(u"l1"_s, Fault::ShortCircuit);
    QCOMPARE(occurred.count(), 1);
    addDevice("breaker-1p-c10", 0, 3);
    addWire(u"supply:PE"_s, u"pebar:1"_s, 1, 2.5);
    QCOMPARE(occurred.count(), 1);

    // a danger that lasts is announced once too
    m_simulation->setFault(u"l1"_s, Fault::Leakage);
    m_simulation->setOn(u"d1"_s, true);
    QCOMPARE(occurred.count(), 3); // Cleared d1, Dangerous l1
    addDevice("breaker-1p-c10", 0, 4);
    QCOMPARE(occurred.count(), 3);
}

QTEST_APPLESS_MAIN(TestProtection)
#include "tst_protection.moc"
