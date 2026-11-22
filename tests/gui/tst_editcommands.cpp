// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/editcommands.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QSignalSpy>
#include <QUndoStack>
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

PlacedLoad loadOf(const char *type, QPointF position)
{
    const LoadType *loadType = embeddedCatalog().loadType(QString::fromLatin1(type));
    if (!loadType)
        qFatal("unknown load type %s", type);
    return loadFromType(*loadType, position);
}

} // namespace

class TestEditCommands : public QObject
{
    Q_OBJECT
private slots:
    void addUndoRedoKeepsId();
    void removeRestoresDeviceAndDesignations();
    void updateUndoRestoresPositionAndParameters();
    void setEnclosureUndo();
    void addWireUndoRedoKeepsId();
    void removeWireRestoresIdAndOrder();
    void updateWireUndoRestoresSection();
    void removeDeviceRestoresItsWires();
    void addLoadUndoRedoKeepsId();
    void removeSocketRestoresWiresAndPlugs();
    void updateLoadUndoRestoresPlugAndPower();
    void setCableLengthUndoRestoresEachWire();
    void setEnclosureUndoMovesLoadsBack();
};

void TestEditCommands::addUndoRedoKeepsId()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    const QList<PlacedDevice> before = project.devices();

    auto *add = new AddDeviceCommand(project, preset("breaker-2p-c40", 0, 0));
    stack.push(add);
    QCOMPARE(add->deviceId(), u"d1"_s);
    QVERIFY(project.device(u"d1"));
    const QList<PlacedDevice> after = project.devices();

    stack.undo();
    QCOMPARE(project.devices(), before);
    QVERIFY(project.devices().isEmpty());

    stack.redo();
    QCOMPARE(project.devices(), after);
    QVERIFY(project.device(u"d1"));
}

void TestEditCommands::removeRestoresDeviceAndDesignations()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    for (int module = 0; module < 3; ++module)
        stack.push(new AddDeviceCommand(project, preset("breaker-1p-c16", 0, module)));
    const QList<PlacedDevice> before = project.devices();
    const PlacedDevice removed = *project.device(u"d2");
    QCOMPARE(project.designation(u"d2"), u"QF2"_s);
    QCOMPARE(project.designation(u"d3"), u"QF3"_s);

    stack.push(new RemoveDeviceCommand(project, u"d2"_s));
    QVERIFY(!project.device(u"d2"));
    QCOMPARE(project.designation(u"d3"), u"QF2"_s);
    const QList<PlacedDevice> after = project.devices();

    QSignalSpy changed(&project, &Project::changed);
    stack.undo();
    QCOMPARE(changed.size(), 1);
    QCOMPARE(project.devices(), before);
    QCOMPARE(*project.device(u"d2"), removed);
    QCOMPARE(project.designation(u"d2"), u"QF2"_s);
    QCOMPARE(project.designation(u"d3"), u"QF3"_s);

    stack.redo();
    QCOMPARE(project.devices(), after);
}

void TestEditCommands::updateUndoRestoresPositionAndParameters()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddDeviceCommand(project, preset("breaker-1p-c16", 0, 0)));
    const PlacedDevice original = *project.device(u"d1");

    PlacedDevice moved = original;
    moved.module = 6;
    stack.push(new UpdateDeviceCommand(project, moved));
    QCOMPARE(*project.device(u"d1"), moved);

    PlacedDevice rated = moved;
    rated.parameters["ratedCurrent"] = 25;
    stack.push(new UpdateDeviceCommand(project, rated));
    QCOMPARE(*project.device(u"d1"), rated);

    PlacedDevice twoPole = rated;
    twoPole.poles = u"2P"_s;
    stack.push(new UpdateDeviceCommand(project, twoPole));
    QCOMPARE(*project.device(u"d1"), twoPole);

    stack.undo();
    QCOMPARE(*project.device(u"d1"), rated);
    stack.undo();
    QCOMPARE(*project.device(u"d1"), moved);
    stack.undo();
    QCOMPARE(*project.device(u"d1"), original);

    stack.redo();
    stack.redo();
    stack.redo();
    QCOMPARE(*project.device(u"d1"), twoPole);
}

void TestEditCommands::setEnclosureUndo()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    const QList<PlacedDevice> before = project.devices();
    QCOMPARE(project.enclosure().id, u"enclosure-12"_s);

    stack.push(new SetEnclosureCommand(project, u"enclosure-24"_s));
    QCOMPARE(project.enclosure().id, u"enclosure-24"_s);

    stack.undo();
    QCOMPARE(project.enclosure().id, u"enclosure-12"_s);
    QCOMPARE(project.devices(), before);

    stack.redo();
    QCOMPARE(project.enclosure().id, u"enclosure-24"_s);
}

void TestEditCommands::addWireUndoRedoKeepsId()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddDeviceCommand(project, preset("breaker-2p-c40", 0, 0)));

    Wire wire;
    wire.from = u"supply:L"_s;
    wire.to = u"d1:1"_s;
    wire.section = 10;
    wire.conductor = Conductor::N;
    auto *add = new AddWireCommand(project, wire);
    stack.push(add);
    QCOMPARE(add->wireId(), u"w1"_s);
    const QList<Wire> after = project.wires();

    stack.undo();
    QVERIFY(project.wires().isEmpty());

    stack.redo();
    QCOMPARE(project.wires(), after);
    QCOMPARE(project.wire(u"w1")->section, 10.0);
    QCOMPARE(project.wire(u"w1")->conductor, Conductor::N);
}

void TestEditCommands::removeWireRestoresIdAndOrder()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddDeviceCommand(project, preset("breaker-2p-c40", 0, 0)));
    for (const auto &ends :
         { std::pair{ u"supply:L"_s, u"d1:1"_s }, std::pair{ u"supply:N"_s, u"d1:2"_s },
           std::pair{ u"supply:PE"_s, u"pebar:1"_s } }) {
        Wire wire;
        wire.from = ends.first;
        wire.to = ends.second;
        stack.push(new AddWireCommand(project, wire));
    }
    const QList<Wire> before = project.wires();
    QCOMPARE(before.size(), 3);

    stack.push(new RemoveWireCommand(project, before[1].id));
    QCOMPARE(project.wires().size(), 2);

    stack.undo();
    QCOMPARE(project.wires(), before);

    stack.redo();
    QCOMPARE(project.wires().size(), 2);
    QVERIFY(!project.wire(before[1].id));
}

void TestEditCommands::updateWireUndoRestoresSection()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    Wire wire;
    wire.from = u"supply:PE"_s;
    wire.to = u"pebar:1"_s;
    stack.push(new AddWireCommand(project, wire));
    QCOMPARE(project.wire(u"w1")->section, 2.5);

    Wire thick = *project.wire(u"w1");
    thick.section = 10;
    stack.push(new UpdateWireCommand(project, thick));
    QCOMPARE(project.wire(u"w1")->section, 10.0);

    stack.undo();
    QCOMPARE(project.wire(u"w1")->section, 2.5);
    stack.redo();
    QCOMPARE(project.wire(u"w1")->section, 10.0);
}

void TestEditCommands::removeDeviceRestoresItsWires()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddDeviceCommand(project, preset("breaker-2p-c40", 0, 0)));
    stack.push(new AddDeviceCommand(project, preset("rcd-2p-40-30", 0, 2)));
    for (const auto &ends :
         { std::pair{ u"supply:L"_s, u"d1:1"_s }, std::pair{ u"supply:PE"_s, u"pebar:1"_s },
           std::pair{ u"d1:2"_s, u"d2:1"_s } }) {
        Wire wire;
        wire.from = ends.first;
        wire.to = ends.second;
        stack.push(new AddWireCommand(project, wire));
    }
    const QList<Wire> before = project.wires();
    QCOMPARE(before.size(), 3);

    stack.push(new RemoveDeviceCommand(project, u"d1"_s));
    QCOMPARE(project.wires().size(), 1);
    QCOMPARE(project.wires()[0].id, u"w2"_s);

    stack.undo();
    QCOMPARE(project.wires(), before);

    stack.redo();
    QCOMPARE(project.wires().size(), 1);
    stack.undo();
    QCOMPARE(project.wires(), before);
}

void TestEditCommands::addLoadUndoRedoKeepsId()
{
    Project project(embeddedCatalog());
    QUndoStack stack;

    auto *add = new AddLoadCommand(project, loadOf("ceiling-lamp", { 0, 260 }));
    stack.push(add);
    QCOMPARE(add->loadId(), u"l1"_s);
    const QList<PlacedLoad> after = project.loads();
    QCOMPARE(after.size(), 1);

    stack.undo();
    QVERIFY(project.loads().isEmpty());

    stack.redo();
    QCOMPARE(project.loads(), after);
}

void TestEditCommands::removeSocketRestoresWiresAndPlugs()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddLoadCommand(project, loadOf("ceiling-lamp", { 100, 260 })));
    stack.push(new AddLoadCommand(project, loadOf("socket", { 0, 260 })));
    for (int place = 1; place <= 2; ++place) {
        PlacedLoad kettle = loadOf("kettle", { 0, 0 });
        kettle.socket = u"l2"_s;
        kettle.place = place;
        stack.push(new AddLoadCommand(project, kettle));
    }
    for (const auto &ends :
         { std::pair{ u"supply:L"_s, u"l2:L"_s }, std::pair{ u"supply:N"_s, u"l2:N"_s },
           std::pair{ u"pebar:1"_s, u"l2:PE"_s } }) {
        Wire wire;
        wire.from = ends.first;
        wire.to = ends.second;
        wire.length = 8;
        stack.push(new AddWireCommand(project, wire));
    }
    const QList<PlacedLoad> loadsBefore = project.loads();
    const QList<Wire> wiresBefore = project.wires();
    QCOMPARE(wiresBefore.size(), 3);

    stack.push(new RemoveLoadCommand(project, u"l2"_s));
    QVERIFY(!project.load(u"l2"));
    QVERIFY(project.wires().isEmpty());

    stack.undo();
    QCOMPARE(project.loads(), loadsBefore);
    QCOMPARE(project.wires(), wiresBefore);
    for (const Wire &wire : project.wires())
        QCOMPARE(wire.length, 8.0);
    QCOMPARE(project.pluggedInto(u"l2"), (QStringList{ u"l3"_s, u"l4"_s }));

    stack.redo();
    QVERIFY(!project.load(u"l2"));
    QVERIFY(project.wires().isEmpty());
    QVERIFY(project.load(u"l3")->socket.isEmpty());
    QVERIFY(project.load(u"l4")->socket.isEmpty());

    stack.undo();
    QCOMPARE(project.loads(), loadsBefore);
    QCOMPARE(project.wires(), wiresBefore);
}

void TestEditCommands::updateLoadUndoRestoresPlugAndPower()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddLoadCommand(project, loadOf("socket", { 0, 260 })));
    stack.push(new AddLoadCommand(project, loadOf("kettle", { 200, 260 })));
    const PlacedLoad loose = *project.load(u"l2");

    PlacedLoad plugged = loose;
    plugged.socket = u"l1"_s;
    plugged.place = 1;
    stack.push(new UpdateLoadCommand(project, plugged));
    QCOMPARE(*project.load(u"l2"), plugged);

    PlacedLoad stronger = plugged;
    stronger.power = 2200;
    stack.push(new UpdateLoadCommand(project, stronger));
    QCOMPARE(project.load(u"l2")->power, 2200);

    PlacedLoad unplugged = stronger;
    unplugged.socket.clear();
    unplugged.place = 0;
    unplugged.position = { 200, 300 };
    stack.push(new UpdateLoadCommand(project, unplugged));
    QCOMPARE(*project.load(u"l2"), unplugged);

    stack.undo();
    QCOMPARE(*project.load(u"l2"), stronger);
    stack.undo();
    QCOMPARE(*project.load(u"l2"), plugged);
    stack.undo();
    QCOMPARE(*project.load(u"l2"), loose);
    stack.redo();
    QCOMPARE(*project.load(u"l2"), plugged);
}

void TestEditCommands::setCableLengthUndoRestoresEachWire()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddLoadCommand(project, loadOf("ceiling-lamp", { 0, 260 })));
    for (const auto &ends :
         { std::pair{ u"supply:L"_s, u"l1:L"_s }, std::pair{ u"supply:N"_s, u"l1:N"_s },
           std::pair{ u"pebar:1"_s, u"l1:PE"_s } }) {
        Wire wire;
        wire.from = ends.first;
        wire.to = ends.second;
        wire.length = 15;
        stack.push(new AddWireCommand(project, wire));
    }

    stack.push(new SetCableLengthCommand(project, u"w1"_s, 8));
    for (const Wire &wire : project.wires())
        QCOMPARE(wire.length, 8.0);

    stack.undo();
    for (const Wire &wire : project.wires())
        QCOMPARE(wire.length, 15.0);

    stack.redo();
    for (const Wire &wire : project.wires())
        QCOMPARE(wire.length, 8.0);
}

void TestEditCommands::setEnclosureUndoMovesLoadsBack()
{
    Project project(embeddedCatalog());
    QUndoStack stack;
    stack.push(new AddLoadCommand(project, loadOf("ceiling-lamp", { 0, 260 })));

    stack.push(new SetEnclosureCommand(project, u"enclosure-24"_s));
    QCOMPARE(project.load(u"l1")->position, QPointF(0, 384));

    stack.undo();
    QCOMPARE(project.load(u"l1")->position, QPointF(0, 260));
}

QTEST_MAIN(TestEditCommands)
#include "tst_editcommands.moc"
