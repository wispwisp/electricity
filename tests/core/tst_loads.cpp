// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalog.h"
#include "core/project.h"

#include <QSignalSpy>
#include <QtTest>

using namespace Qt::StringLiterals;

namespace {

PlacedLoad load(const char *type, QPointF pos)
{
    const LoadType *t = embeddedCatalog().loadType(QString::fromLatin1(type));
    if (!t)
        qFatal("unknown load type %s", type);
    return loadFromType(*t, pos);
}

PlacedLoad plugged(const char *type, const QString &socket, int place)
{
    PlacedLoad l = load(type, {});
    l.socket = socket;
    l.place = place;
    return l;
}

PlacedDevice preset(const char *id)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == QLatin1StringView(id))
            return deviceFromPreset(p, 0, 0);
    }
    qFatal("unknown preset %s", id);
}

} // namespace

Q_DECLARE_METATYPE(LoadPlacement)
Q_DECLARE_METATYPE(PlacedLoad)

class TestLoads : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void addsLoadWithStableIdAndSignal();
    void restoresGivenIdAndNeverReusesIds();
    void numbersPerTypeInPlacementOrder();
    void outlineCoversPictureCaptionAndPlaces();
    void placement_data();
    void placement();
    void plugsAppliancesIntoSocketPlaces();
    void movingSocketCarriesItsAppliances();
    void loadTerminalsAreWireTerminals();
    void removingLoadRemovesItsWires();
    void removingSocketUnplugsItsAppliances();
    void updateLoadRules();
    void removeUnknownLoadIsRefused();
    void growingEnclosureMovesLoadsBelow();
    void shrinkingMovesThemBackUp();
    void shrinkRefusedWhenLoadsWouldCollide();
    void newWireLengthDependsOnEnds();
    void cableGroupsWiresBySides();
    void setCableLengthSetsEveryWire();
    void newWireJoinsItsCableLength();

private:
    void addCableFixture();
    QString addWire(const QString &from, const QString &to);
    std::unique_ptr<Project> m_project;
};

void TestLoads::addCableFixture()
{
    PlacedDevice qf1 = preset("breaker-2p-c40");
    PlacedDevice qf2 = preset("breaker-1p-c10");
    qf2.module = 2;
    m_project->addDevice(qf1);
    m_project->addDevice(qf2);
    m_project->addLoad(load("ceiling-lamp", { 300, 0 }));
    m_project->addLoad(load("ceiling-lamp", { 360, 0 }));
    m_project->addLoad(load("ceiling-lamp", { 420, 0 }));
}

QString TestLoads::addWire(const QString &from, const QString &to)
{
    const QString id = m_project->addWire(Wire{ {},
                                                from,
                                                to,
                                                2.5,
                                                m_project->conductorFor(from, to),
                                                m_project->newWireLength(from, to) });
    if (id.isEmpty())
        qFatal("wire refused: %s -> %s", qPrintable(from), qPrintable(to));
    return id;
}

void TestLoads::init()
{
    m_project = std::make_unique<Project>(embeddedCatalog());
}

void TestLoads::addsLoadWithStableIdAndSignal()
{
    QSignalSpy spy(m_project.get(), &Project::changed);
    QCOMPARE(m_project->addLoad(load("ceiling-lamp", { 300, 0 })), u"l1"_s);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_project->load(u"l1")->power, 10);
    QCOMPARE(m_project->loads().size(), 1);
}

void TestLoads::restoresGivenIdAndNeverReusesIds()
{
    PlacedLoad l = load("ceiling-lamp", { 300, 0 });
    l.id = u"l5"_s;
    QCOMPARE(m_project->addLoad(l), u"l5"_s);
    QCOMPARE(m_project->addLoad(l), QString()); // duplicate id
    QCOMPARE(m_project->addLoad(load("ceiling-lamp", { 400, 0 })), u"l6"_s);
    QVERIFY(m_project->removeLoad(u"l6"));
    QCOMPARE(m_project->addLoad(load("ceiling-lamp", { 400, 0 })), u"l7"_s);
    PlacedLoad first = load("ceiling-lamp", { 500, 0 });
    first.id = u"l2"_s;
    QCOMPARE(m_project->addLoad(first, 0), u"l2"_s);
    QCOMPARE(m_project->loads().first().id, u"l2"_s);
    QCOMPARE(m_project->addLoad(load("ceiling-lamp", { 600, 0 }), 99), QString());
}

void TestLoads::numbersPerTypeInPlacementOrder()
{
    m_project->addLoad(load("ceiling-lamp", { 300, 0 }));
    m_project->addLoad(load("socket", { 0, 300 }));
    m_project->addLoad(load("ceiling-lamp", { 400, 0 }));
    QCOMPARE(m_project->loadNumber(u"l1"), 1);
    QCOMPARE(m_project->loadNumber(u"l2"), 1);
    QCOMPARE(m_project->loadNumber(u"l3"), 2);
    QCOMPARE(m_project->loadNumber(u"l9"), 0);
    m_project->removeLoad(u"l1");
    QCOMPARE(m_project->loadNumber(u"l3"), 1);
}

void TestLoads::outlineCoversPictureCaptionAndPlaces()
{
    QCOMPARE(m_project->loadOutline(load("ceiling-lamp", { 300, 0 })), QRectF(300, 0, 36, 45));
    const PlacedLoad socket = load("socket", { 0, 300 });
    QCOMPARE(m_project->loadOutline(socket), QRectF(-13.5, 300, 81, 76.5));
    QCOMPARE(m_project->loadOutline(load("kettle", { 400, 0 })), QRectF(400, 0, 36, 36));
    QCOMPARE(m_project->plugPlaceRect(socket, 2), QRectF(31.5, 340.5, 36, 36));
}

void TestLoads::placement_data()
{
    QTest::addColumn<PlacedLoad>("load");
    QTest::addColumn<LoadPlacement>("expected");

    PlacedLoad same = load("ceiling-lamp", { 300, 0 });
    same.id = u"l1"_s; // the stored lamp itself is ignored
    QTest::newRow("fits") << same << LoadPlacement::Fits;
    QTest::newRow("on enclosure") << load("ceiling-lamp", { 100, 100 })
                                  << LoadPlacement::OnEnclosure;
    QTest::newRow("partly on enclosure")
            << load("ceiling-lamp", { 270, 230 }) << LoadPlacement::OnEnclosure;
    QTest::newRow("onto lamp") << load("ceiling-lamp", { 310, 9 }) << LoadPlacement::Overlaps;
    QTest::newRow("onto plug place")
            << load("ceiling-lamp", { -9, 345 }) << LoadPlacement::Overlaps;
    PlacedLoad unknown = load("ceiling-lamp", { 400, 0 });
    unknown.type = u"nope"_s;
    QTest::newRow("unknown type") << unknown << LoadPlacement::Invalid;
    PlacedLoad zero = load("ceiling-lamp", { 400, 0 });
    zero.power = 0;
    QTest::newRow("power 0") << zero << LoadPlacement::Invalid;
    PlacedLoad big = load("ceiling-lamp", { 400, 0 });
    big.power = 15001;
    QTest::newRow("power too big") << big << LoadPlacement::Invalid;
    PlacedLoad powered = load("socket", { 0, 450 });
    powered.power = 10;
    QTest::newRow("socket with power") << powered << LoadPlacement::Invalid;
    QTest::newRow("lamp with socket")
            << plugged("ceiling-lamp", u"l2"_s, 1) << LoadPlacement::Invalid;
    QTest::newRow("kettle into lamp") << plugged("kettle", u"l1"_s, 1) << LoadPlacement::Invalid;
    QTest::newRow("place 3") << plugged("kettle", u"l2"_s, 3) << LoadPlacement::Invalid;
    QTest::newRow("place taken") << plugged("kettle", u"l2"_s, 1) << LoadPlacement::PlaceTaken;
}

void TestLoads::placement()
{
    QFETCH(PlacedLoad, load);
    QFETCH(LoadPlacement, expected);
    m_project->addLoad(::load("ceiling-lamp", { 300, 0 }));
    m_project->addLoad(::load("socket", { 0, 300 }));
    QCOMPARE(m_project->addLoad(plugged("microwave", u"l2"_s, 1)), u"l3"_s);
    QCOMPARE(m_project->loadPlacement(load), expected);
}

void TestLoads::plugsAppliancesIntoSocketPlaces()
{
    m_project->addLoad(load("socket", { 0, 300 }));
    QCOMPARE(m_project->addLoad(plugged("kettle", u"l1"_s, 1)), u"l2"_s);
    QCOMPARE(m_project->addLoad(plugged("microwave", u"l1"_s, 2)), u"l3"_s);
    QCOMPARE(m_project->loadPosition(*m_project->load(u"l2")), QPointF(-13.5, 340.5));
    QCOMPARE(m_project->loadPosition(*m_project->load(u"l3")), QPointF(31.5, 340.5));
    QCOMPARE(m_project->pluggedInto(u"l1"), QStringList({ u"l2"_s, u"l3"_s }));
    QCOMPARE(m_project->addLoad(plugged("tv", u"l1"_s, 1)), QString());
    QCOMPARE(m_project->addLoad(plugged("tv", u"l1"_s, 2)), QString());
    PlacedLoad kettle = *m_project->load(u"l2");
    kettle.socket.clear();
    kettle.place = 0;
    kettle.position = { 400, 0 };
    QVERIFY(m_project->updateLoad(kettle));
    QCOMPARE(m_project->pluggedInto(u"l1"), QStringList({ u"l3"_s }));
}

void TestLoads::movingSocketCarriesItsAppliances()
{
    m_project->addLoad(load("socket", { 0, 300 }));
    m_project->addLoad(plugged("kettle", u"l1"_s, 1));
    PlacedLoad socket = *m_project->load(u"l1");
    socket.position = { 0, 450 };
    QCOMPARE(m_project->loadPlacement(socket), LoadPlacement::Fits);
    QVERIFY(m_project->updateLoad(socket));
    QCOMPARE(m_project->loadPosition(*m_project->load(u"l2")), QPointF(-13.5, 490.5));

    m_project->addLoad(load("ceiling-lamp", { 300, 600 }));
    socket.position = { 280, 590 };
    QCOMPARE(m_project->loadPlacement(socket), LoadPlacement::Overlaps);
}

void TestLoads::loadTerminalsAreWireTerminals()
{
    m_project->addDevice(preset("breaker-1p-c10"));
    m_project->addLoad(load("ceiling-lamp", { 300, 0 }));
    m_project->addLoad(load("kettle", { 400, 0 }));
    const QStringList ids = m_project->terminalIds();
    QCOMPARE(ids.last(3), QStringList({ u"l1:L"_s, u"l1:N"_s, u"l1:PE"_s }));
    QVERIFY(!ids.contains(u"l2:L"_s));
    QCOMPARE(m_project->terminalRole(u"l1:L"_s), Conductor::L);
    QCOMPARE(m_project->terminalRole(u"l1:N"_s), Conductor::N);
    QCOMPARE(m_project->terminalRole(u"l1:PE"_s), Conductor::PE);
    QCOMPARE(m_project->wireCapacity(u"l1:PE"_s), 2);
}

void TestLoads::removingLoadRemovesItsWires()
{
    m_project->addDevice(preset("breaker-1p-c10"));
    m_project->addLoad(load("ceiling-lamp", { 300, 0 }));
    QVERIFY(!m_project->addWire(Wire{ {}, u"d1:2"_s, u"l1:L"_s, 2.5, Conductor::L }).isEmpty());
    QSignalSpy spy(m_project.get(), &Project::changed);
    QVERIFY(m_project->removeLoad(u"l1"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(m_project->wires().isEmpty());
}

void TestLoads::removingSocketUnplugsItsAppliances()
{
    m_project->addLoad(load("socket", { 0, 300 }));
    m_project->addLoad(plugged("kettle", u"l1"_s, 1));
    QVERIFY(m_project->removeLoad(u"l1"));
    const PlacedLoad *kettle = m_project->load(u"l2");
    QVERIFY(kettle);
    QVERIFY(kettle->socket.isEmpty());
    QCOMPARE(kettle->place, 0);
    QCOMPARE(kettle->position, QPointF(-13.5, 340.5));
}

void TestLoads::updateLoadRules()
{
    m_project->addLoad(load("ceiling-lamp", { 300, 0 }));
    m_project->addLoad(load("ceiling-lamp", { 400, 0 }));
    PlacedLoad l = *m_project->load(u"l1");
    l.id = u"l9"_s;
    QVERIFY(!m_project->updateLoad(l));
    l = *m_project->load(u"l1");
    l.type = u"kettle"_s;
    QVERIFY(!m_project->updateLoad(l));
    l = *m_project->load(u"l1");
    l.position = { 400, 0 };
    QVERIFY(!m_project->updateLoad(l));
    l.position = { 300, 100 };
    l.power = 25;
    QVERIFY(m_project->updateLoad(l));
    QCOMPARE(m_project->load(u"l1")->power, 25);
}

void TestLoads::removeUnknownLoadIsRefused()
{
    QSignalSpy spy(m_project.get(), &Project::changed);
    QVERIFY(!m_project->removeLoad(u"l1"));
    QCOMPARE(spy.count(), 0);
}

void TestLoads::growingEnclosureMovesLoadsBelow()
{
    m_project->addLoad(load("ceiling-lamp", { 0, 260 }));
    m_project->addLoad(load("ceiling-lamp", { 300, 100 }));
    m_project->addLoad(load("socket", { 90, 270 }));
    m_project->addLoad(plugged("kettle", u"l3"_s, 1));
    const QPointF kettleBefore = m_project->loadPosition(*m_project->load(u"l4"));
    QSignalSpy spy(m_project.get(), &Project::changed);
    QVERIFY(m_project->setEnclosure(u"enclosure-24"_s));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_project->load(u"l1")->position, QPointF(0, 384));
    QCOMPARE(m_project->load(u"l2")->position, QPointF(300, 100));
    QCOMPARE(m_project->load(u"l3")->position, QPointF(90, 394));
    QCOMPARE(m_project->loadPosition(*m_project->load(u"l4")), kettleBefore + QPointF(0, 124));
}

void TestLoads::shrinkingMovesThemBackUp()
{
    m_project->addLoad(load("ceiling-lamp", { 0, 260 }));
    m_project->addLoad(load("ceiling-lamp", { 300, 100 }));
    m_project->addLoad(load("socket", { 90, 270 }));
    m_project->addLoad(plugged("kettle", u"l3"_s, 1));
    QVERIFY(m_project->setEnclosure(u"enclosure-24"_s));
    QVERIFY(m_project->setEnclosure(u"enclosure-12"_s));
    QCOMPARE(m_project->load(u"l1")->position, QPointF(0, 260));
    QCOMPARE(m_project->load(u"l2")->position, QPointF(300, 100));
    QCOMPARE(m_project->load(u"l3")->position, QPointF(90, 270));
}

void TestLoads::shrinkRefusedWhenLoadsWouldCollide()
{
    QVERIFY(m_project->setEnclosure(u"enclosure-24"_s));
    m_project->addLoad(load("ceiling-lamp", { 300, 300 }));
    m_project->addLoad(load("ceiling-lamp", { 300, 400 }));
    QVERIFY(!m_project->keepsLoadsClear(u"enclosure-12"_s));
    QVERIFY(!m_project->fitsEnclosure(u"enclosure-12"_s));
    QSignalSpy spy(m_project.get(), &Project::changed);
    QVERIFY(!m_project->setEnclosure(u"enclosure-12"_s));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(m_project->load(u"l1")->position, QPointF(300, 300));
    QCOMPARE(m_project->load(u"l2")->position, QPointF(300, 400));
    QCOMPARE(m_project->enclosure().id, u"enclosure-24"_s);
}

void TestLoads::newWireLengthDependsOnEnds()
{
    addCableFixture();
    QCOMPARE(m_project->newWireLength(u"d1:2"_s, u"d2:1"_s), 0.3);
    QCOMPARE(m_project->newWireLength(u"d2:2"_s, u"l1:L"_s), 15.0);
    QCOMPARE(m_project->newWireLength(u"supply:L"_s, u"l1:L"_s), 15.0);
    QCOMPARE(m_project->newWireLength(u"l1:L"_s, u"l2:L"_s), 15.0);
}

void TestLoads::cableGroupsWiresBySides()
{
    addCableFixture();
    const QString wl = addWire(u"d2:2"_s, u"l1:L"_s);
    const QString wn = addWire(u"nbar:2"_s, u"l1:N"_s);
    const QString wp = addWire(u"pebar:2"_s, u"l1:PE"_s);
    const QString w12l = addWire(u"l1:L"_s, u"l2:L"_s);
    const QString w12n = addWire(u"l1:N"_s, u"l2:N"_s);
    const QString w3 = addWire(u"l3:L"_s, u"l3:N"_s);
    const QString wd = addWire(u"d1:2"_s, u"d2:1"_s);
    const QStringList panelCable{ wl, wn, wp };
    QCOMPARE(m_project->cableWires(u"nbar:2"_s, u"l1:N"_s), panelCable);
    QCOMPARE(m_project->cableWires(u"l1:PE"_s, u"pebar:2"_s), panelCable);
    QCOMPARE(m_project->cableWires(u"d2:2"_s, u"l1:L"_s), panelCable);
    QCOMPARE(m_project->cableWires(u"l1:L"_s, u"l2:L"_s), QStringList({ w12l, w12n }));
    QCOMPARE(m_project->cableWires(u"l3:L"_s, u"l3:N"_s), QStringList({ w3 }));
    QVERIFY(m_project->cableWires(u"d1:2"_s, u"d2:1"_s).isEmpty());
    QVERIFY(!wd.isEmpty());
}

void TestLoads::setCableLengthSetsEveryWire()
{
    addCableFixture();
    const QString wl = addWire(u"d2:2"_s, u"l1:L"_s);
    const QString wn = addWire(u"nbar:2"_s, u"l1:N"_s);
    const QString wp = addWire(u"pebar:2"_s, u"l1:PE"_s);
    const QString w12 = addWire(u"l1:L"_s, u"l2:L"_s);
    const QString wd = addWire(u"d1:2"_s, u"d2:1"_s);
    QSignalSpy spy(m_project.get(), &Project::changed);
    QVERIFY(m_project->setCableLength(wl, 8));
    QCOMPARE(spy.count(), 1);
    for (const QString &id : { wl, wn, wp })
        QCOMPARE(m_project->wire(id)->length, 8.0);
    QCOMPARE(m_project->wire(w12)->length, 15.0);
    QVERIFY(!m_project->setCableLength(wd, 8));
    QVERIFY(!m_project->setCableLength(u"w99", 8));
    QVERIFY(!m_project->setCableLength(wl, 0));
    QVERIFY(!m_project->setCableLength(wl, 500.5));
    QVERIFY(m_project->setCableLength(wl, 500));
    QCOMPARE(m_project->wire(wp)->length, 500.0);
    QCOMPARE(spy.count(), 2);
}

void TestLoads::newWireJoinsItsCableLength()
{
    addCableFixture();
    const QString wl = addWire(u"d2:2"_s, u"l1:L"_s);
    QVERIFY(m_project->setCableLength(wl, 8));
    QCOMPARE(m_project->newWireLength(u"nbar:2"_s, u"l1:N"_s), 8.0);
}

QTEST_APPLESS_MAIN(TestLoads)
#include "tst_loads.moc"
