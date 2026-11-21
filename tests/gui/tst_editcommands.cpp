// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/editcommands.h"
#include "core/catalog.h"
#include "core/project.h"

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

} // namespace

class TestEditCommands : public QObject
{
    Q_OBJECT
private slots:
    void addUndoRedoKeepsId();
    void removeRestoresDeviceAndDesignations();
    void updateUndoRestoresPositionAndParameters();
    void setEnclosureUndo();
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

    stack.undo();
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

QTEST_MAIN(TestEditCommands)
#include "tst_editcommands.moc"
