// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/eventtext.h"
#include "app/language.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "app/theme.h"
#include "core/project.h"
#include "core/simulation.h"
#include "editorhelpers.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>

#include <algorithm>
#include <memory>

namespace {
const QString kQf3Short = QStringLiteral("QF3 tripped: short circuit, 370 A on C16");
const QString kQd1Leakage = QStringLiteral("QD1 tripped: leakage 230 mA, trips at 30 mA");
} // namespace

Q_DECLARE_METATYPE(SimulationEvent::Kind)
Q_DECLARE_METATYPE(TripCause)

class TestProtecting : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void eventTextForEveryKind_data();
    void eventTextForEveryKind();
    void statusBarShowsTripsAndFallsBack();
    void statusBarFollowsLanguage();
    void dangerMessageWhileItLasts();
    void newerConditionShowsThenFallsBack();
    void messageOfADeletedObjectGoes();
    void tripMessageKeepsTheRatingAtTheTrip();
    void hintThenBackToTheEvent();
    void tripShowsOnTheDevice();
    void clickOnTrippedHandleResets();
    void handSwitchOffShowsNoMark();
    void tripMarkKeepsTerminalsAndHandleClear();
    void testButtonTripsRcd();
    void testButtonWithoutPowerHints();
    void dragFromTestButtonMoves();
    void wireToolTestButtonDoesNothing();
    void stairwellTripShowsOnSupplyAndClickResets();
    void supplyClickWhenNotTrippedDoesNothing();
    void loadMenuOffersFaults();
    void shortCircuitFromMenuTrips();
    void leakageOnKettleTripsRcd();
    void faultStaysWithUnpluggedAppliance();
    void dangerMarkIsRed();
    void leakageOutsideRcdIsMarkedDangerous();
    void faultActionsAreNotEdits();
    void faultSurvivesDeleteAndUndo();
    void faultMarkKeepsTerminalsAndButtonClear();
    void timeControlStartsAtTenTimes();
    void pauseStopsHeatingNotInstantTrips();
    void overloadTripsAtSixtyTimes();
    void wireTurnsOrangeThenRed();
    void overloadedRcdGlows();
    void rcdTripMessageOutlastsItsOverload();
    void rcdOverloadMessageOutlastsItsTrip();
    void quietTimerDoesNoSolves();
    void tripDuringDragKeepsTheDrag();
    void roadmapScenario();

private:
    void zoomOut();
    [[nodiscard]] DeviceItem *deviceItem(const QString &id) const;
    [[nodiscard]] QPoint onHandle(const QString &id) const;
    void clickHandle(const QString &id);
    [[nodiscard]] QPoint onTestButton(const QString &id) const;
    [[nodiscard]] QPoint onSupply() const;
    // Where the item's handle is drawn when the device is off.
    [[nodiscard]] QRectF offHandle(const QString &id) const;
    [[nodiscard]] QString message() const;
    [[nodiscard]] LoadItem *loadItem(const QString &id) const;
    [[nodiscard]] QPoint onPicture(const QString &id) const;
    // Right-clicks the load or device and returns the open menu.
    QMenu *openMenu(const QPoint &pos);
    // Chooses the named fault action of the open menu of the load.
    void chooseFault(const QString &loadId, const char *action);
    // The tutorial loads, zoomed out, with QF1, QD1 and QF3 on and a short circuit on Socket 1:
    // QF3 trips.
    void tripQf3();
    // The wire with these ends, from "d3:2>l1:L"; empty if none.
    [[nodiscard]] QString wireId(const QString &ends) const;
    [[nodiscard]] WireItem *wireItem(const QString &ends) const;
    // The viewport point over the middle of the load's button.
    [[nodiscard]] QPoint onButton(const QString &id) const;
    // Switches the devices and loads on by clicks.
    void switchOn(const QStringList &ids);
    // Clicks the toolbar button of the checkable action, which is then checked.
    void clickToolButton(const char *name);
    // A washing machine dropped into Socket 4, then QF1, QD1, QF4, the kettle, the microwave and
    // the machine switched on by clicks: QF4 carries 22.0 A.
    void overloadQf4();
    // Through the model: d6, a 1P C32 at (0, 7) fed from d5:1 by 0.3 m of 6 mm², and l11, a 9 kW
    // cooktop wired to d6 and the bars by 15 m of 1.5 mm².
    void addCooktopCircuit();
    // Paused, addCooktopCircuit(), then QF1, QD1, QF4, QF5, the cooktop, the kettle and the
    // microwave switched on by clicks: QD1 carries 46.5 A, over its In of 40 A.
    void overloadQd1();
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
};

DeviceItem *TestProtecting::deviceItem(const QString &id) const
{
    return dynamic_cast<DeviceItem *>(sceneItem(scene, id));
}

void TestProtecting::zoomOut()
{
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
}

QPoint TestProtecting::onHandle(const QString &id) const
{
    const DeviceItem *item = deviceItem(id);
    return viewportPoint(canvas, item->mapToScene(item->handleRect().center()));
}

void TestProtecting::clickHandle(const QString &id)
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onHandle(id));
}

QPoint TestProtecting::onTestButton(const QString &id) const
{
    const DeviceItem *item = deviceItem(id);
    return viewportPoint(canvas, item->mapToScene(item->testButtonRect().center()));
}

QPoint TestProtecting::onSupply() const
{
    const QGraphicsItem *supply = sceneItem(scene, QStringLiteral("supply"));
    return viewportPoint(canvas, supply->mapToScene(supply->boundingRect().center()));
}

QRectF TestProtecting::offHandle(const QString &id) const
{
    const DeviceItem *item = deviceItem(id);
    const QRectF travel = item->switchRect();
    const QSizeF size = item->handleRect().size();
    return { travel.left(), travel.bottom() - size.height(), size.width(), size.height() };
}

QString TestProtecting::message() const
{
    return window->statusBar()->currentMessage();
}

LoadItem *TestProtecting::loadItem(const QString &id) const
{
    return dynamic_cast<LoadItem *>(sceneItem(scene, id));
}

QPoint TestProtecting::onPicture(const QString &id) const
{
    const LoadItem *item = loadItem(id);
    return viewportPoint(canvas, item->mapToScene(item->shape().boundingRect().center()));
}

QMenu *TestProtecting::openMenu(const QPoint &pos)
{
    QTest::mouseClick(canvas->viewport(), Qt::RightButton, {}, pos);
    auto *menu = named<QMenu>("deviceContextMenu");
    if (menu)
        static_cast<void>(QTest::qWaitFor([menu] { return menu->isVisible(); }));
    return menu;
}

void TestProtecting::chooseFault(const QString &loadId, const char *action)
{
    QMenu *menu = openMenu(onPicture(loadId));
    QVERIFY(menu && menu->isVisible());
    clickMenuItem(menu, named<QAction>(action));
}

void TestProtecting::tripQf3()
{
    for (const char *id : { "d1", "d2", "d4" })
        clickHandle(QString::fromLatin1(id));
    QVERIFY(window->simulation().setFault(QStringLiteral("l3"), Fault::ShortCircuit));
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);
    QCOMPARE(message(), kQf3Short);
}

QString TestProtecting::wireId(const QString &ends) const
{
    const QStringList parts = ends.split(QLatin1Char('>'));
    const QList<Wire> &wires = window->project().wires();
    const auto it = std::ranges::find_if(wires, [&parts](const Wire &wire) {
        return wire.from == parts[0] && wire.to == parts[1];
    });
    return it == wires.end() ? QString() : it->id;
}

WireItem *TestProtecting::wireItem(const QString &ends) const
{
    return dynamic_cast<WireItem *>(sceneItem(scene, wireId(ends)));
}

QPoint TestProtecting::onButton(const QString &id) const
{
    const LoadItem *item = loadItem(id);
    return viewportPoint(canvas, item->mapToScene(item->buttonRect().center()));
}

void TestProtecting::switchOn(const QStringList &ids)
{
    for (const QString &id : ids) {
        if (window->project().device(id))
            clickHandle(id);
        else
            QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton(id));
        QVERIFY2(window->simulation().isOn(id), qPrintable(id));
    }
}

void TestProtecting::clickToolButton(const char *name)
{
    auto *action = named<QAction>(name);
    QVERIFY2(action, name);
    QWidget *button = named<QToolBar>("editToolBar")->widgetForAction(action);
    QVERIFY2(button, name);
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY2(action->isChecked(), name);
}

void TestProtecting::overloadQf4()
{
    dropLoad(canvas, named<PaletteWidget>("paletteTree"), "washing-machine",
             viewportPoint(canvas, loadItem(QStringLiteral("l6"))->mapToScene(QPointF(18, 13.5))));
    const QString machine = window->project().loads().last().id;
    QCOMPARE(window->project().load(machine)->socket, QStringLiteral("l6"));
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d5"),
               QStringLiteral("l9"), QStringLiteral("l10"), machine });
}

void TestProtecting::addCooktopCircuit()
{
    Project &project = window->project();
    QCOMPARE(project.addDevice(deviceFromPreset(preset(QStringLiteral("breaker-1p-c32")), 0, 7)),
             QStringLiteral("d6"));
    PlacedLoad cooktop = loadFromType(*project.catalog().loadType(QStringLiteral("cooktop")),
                                      QPointF(432, 270));
    cooktop.power = 9000;
    QCOMPARE(project.addLoad(cooktop), QStringLiteral("l11"));
    struct Ends
    {
        const char *from;
        const char *to;
        double section;
        double length;
    };
    for (const Ends &ends :
         { Ends{ "d5:1", "d6:1", 6, 0.3 }, Ends{ "d6:2", "l11:L", 1.5, 15 },
           Ends{ "nbar:5", "l11:N", 1.5, 15 }, Ends{ "pebar:5", "l11:PE", 1.5, 15 } }) {
        Wire wire{ {}, QString::fromLatin1(ends.from), QString::fromLatin1(ends.to), ends.section };
        wire.conductor = project.conductorFor(wire.from, wire.to);
        wire.length = ends.length;
        QVERIFY2(!project.addWire(wire).isEmpty(), ends.from);
    }
}

void TestProtecting::overloadQd1()
{
    clickToolButton("pauseAction");
    addCooktopCircuit();
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d5"),
               QStringLiteral("d6"), QStringLiteral("l11"), QStringLiteral("l9"),
               QStringLiteral("l10") });
}

void TestProtecting::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestProtecting::init()
{
    window = std::make_unique<MainWindow>();
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    canvas = window->findChild<CanvasView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas);
    scene = qobject_cast<EditorScene *>(canvas->scene());
    QVERIFY(scene);
    addTutorialDevices(window->project());
    addTutorialWires(window->project());
    addTutorialLoads(window->project());
    zoomOut();
}

void TestProtecting::cleanup()
{
    window.reset();
    installLanguage(Language::English);
    Theme::apply(Theme::Mode::Dark);
}

void TestProtecting::eventTextForEveryKind_data()
{
    QTest::addColumn<SimulationEvent::Kind>("kind");
    QTest::addColumn<QString>("id"); // or the ends of a wire, "from>to"
    QTest::addColumn<TripCause>("cause");
    QTest::addColumn<double>("current");
    QTest::addColumn<double>("voltage");
    QTest::addColumn<double>("limit");
    QTest::addColumn<QString>("text");
    using Kind = SimulationEvent::Kind;

    QTest::newRow("overload") << Kind::Tripped << "d5" << TripCause::Overload << 22.005 << 0.0
                              << 16.0 << "QF4 tripped: overload, 22.0 A on C16";
    QTest::newRow("leakage") << Kind::Tripped << "d2" << TripCause::Leakage << 0.2299 << 0.0 << 0.03
                             << kQd1Leakage;
    QTest::newRow("big leakage") << Kind::Tripped << "d2" << TripCause::Leakage << 370.1 << 0.0
                                 << 0.03 << "QD1 tripped: leakage 370 A, trips at 30 mA";
    QTest::newRow("test") << Kind::Tripped << "d2" << TripCause::Test << 0.0 << 0.0 << 0.03
                          << "QD1 tripped: test button";
    QTest::newRow("stairwell") << Kind::Tripped << "supply" << TripCause::ShortCircuit << 570.6
                               << 0.0 << 50.0
                               << "Stairwell breaker tripped: short circuit, 571 A on C50";
    QTest::newRow("rcd overloaded")
            << Kind::Overloaded << "d2" << TripCause::None << 47.3 << 0.0 << 40.0
            << "QD1 overloaded: 47.3 A on 40 A, an RCD doesn't protect against overload";
    QTest::newRow("wire overloaded")
            << Kind::Overloaded << "d3:2>l1:L" << TripCause::None << 20.5 << 0.0 << 19.0
            << "Wire QF2 → Lamp 1 overloaded: 20.5 A on 1.5 mm², allowed 19 A";
    QTest::newRow("wire overheating")
            << Kind::Overheating << "d2:N2>nbar:1" << TripCause::None << 75.0 << 0.0 << 70.0
            << "Wire QD1 → N bar overheating: 75.0 A on 10 mm², allowed 70 A";
    QTest::newRow("through a person")
            << Kind::Dangerous << "l9" << TripCause::None << 0.2299 << 229.9 << 0.0
            << "Kettle: 230 mA through a person, no RCD turned it off";
    QTest::newRow("live case") << Kind::Dangerous << "l1" << TripCause::None << 0.0 << 229.6 << 0.0
                               << "Lamp 1: its case is live, 230 V";
    QTest::newRow("cleared") << Kind::Cleared << "d2" << TripCause::None << 0.0 << 0.0 << 0.0 << "";
}

void TestProtecting::eventTextForEveryKind()
{
    QFETCH(SimulationEvent::Kind, kind);
    QFETCH(QString, id);
    QFETCH(TripCause, cause);
    QFETCH(double, current);
    QFETCH(double, voltage);
    QFETCH(double, limit);
    QFETCH(QString, text);
    SimulationEvent event;
    event.kind = kind;
    event.id = id.contains(QLatin1Char('>')) ? wireId(id) : id;
    QVERIFY(!event.id.isEmpty());
    event.cause = cause;
    event.current = current;
    event.voltage = voltage;
    event.limit = limit;
    QCOMPARE(eventText(window->project(), event), text);
}

void TestProtecting::statusBarShowsTripsAndFallsBack()
{
    QCOMPARE(message(), QStringLiteral("Ready"));
    tripQf3();
    QVERIFY(!window->simulation().isOn(QStringLiteral("d4")));
    QVERIFY(saveScreenshot(*window, "f09", "status-trip"));
    QVERIFY(window->simulation().setFault(QStringLiteral("l3"), Fault::None));
    clickHandle("d4");
    QVERIFY(window->simulation().isOn(QStringLiteral("d4")));
    QCOMPARE(message(), QStringLiteral("Ready"));
}

void TestProtecting::statusBarFollowsLanguage()
{
    tripQf3();
    const Simulation &simulation = window->simulation();
    const auto switchLanguage = [this](const char *action) {
        auto *languageMenu = named<QMenu>("languageMenu");
        clickMenuBarItem(*window, languageMenu);
        clickMenuItem(languageMenu, named<QAction>(action));
    };

    switchLanguage("languageAction_ru");
    QCOMPARE(currentLanguage(), Language::Russian);
    QCOMPARE(message(), QStringLiteral("QF3: срабатывание от короткого замыкания, 370 А на C16"));
    QCOMPARE(simulation.tripCause(u"d4"), TripCause::ShortCircuit);
    QVERIFY(saveScreenshot(*window, "f09", "status-ru"));

    switchLanguage("languageAction_kk");
    QCOMPARE(currentLanguage(), Language::Kazakh);
    QCOMPARE(message(), QStringLiteral("QF3: қысқа тұйықталудан ажыратылды, 370 А, номиналы C16"));
    QVERIFY(saveScreenshot(*window, "f09", "status-kk"));

    for (const char *id : { "d1", "d2" })
        QVERIFY(simulation.isOn(QString::fromLatin1(id)));
    QVERIFY(!simulation.isOn(QStringLiteral("d4")));
    QCOMPARE(simulation.tripCause(u"d4"), TripCause::ShortCircuit);
}

void TestProtecting::dangerMessageWhileItLasts()
{
    Project &project = window->project();
    QVERIFY(project.removeWire(wireId(QStringLiteral("pebar:4>l5:PE"))));
    for (const char *id : { "d1", "d2", "d5" })
        clickHandle(QString::fromLatin1(id));
    QVERIFY(window->simulation().setFault(QStringLiteral("l9"), Fault::EarthFault));
    for (const char *id : { "d1", "d2", "d5" })
        QVERIFY(window->simulation().isOn(QString::fromLatin1(id)));
    QCOMPARE(message(), QStringLiteral("Kettle: its case is live, 230 V"));
    QVERIFY(saveScreenshot(*window, "f09", "status-danger"));
    clickHandle("d5");
    QCOMPARE(message(), QStringLiteral("Ready"));
}

void TestProtecting::newerConditionShowsThenFallsBack()
{
    tripQf3();
    clickHandle("d5");
    QVERIFY(window->simulation().setFault(QStringLiteral("l9"), Fault::Leakage));
    QCOMPARE(window->simulation().tripCause(u"d2"), TripCause::Leakage);
    QCOMPARE(message(), kQd1Leakage);
    // Ruling R2: with the fault still there, the reset would trip QD1 again
    QVERIFY(window->simulation().setFault(QStringLiteral("l9"), Fault::None));
    clickHandle("d2");
    QVERIFY(window->simulation().isOn(QStringLiteral("d2")));
    QCOMPARE(message(), kQf3Short);
}

void TestProtecting::messageOfADeletedObjectGoes()
{
    tripQf3();
    deviceItem(QStringLiteral("d4"))->setSelected(true);
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!window->project().device(u"d4"));
    QCOMPARE(message(), QStringLiteral("Ready"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(window->project().device(u"d4"));
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);
    QCOMPARE(message(), kQf3Short);
}

void TestProtecting::tripMessageKeepsTheRatingAtTheTrip()
{
    tripQf3();
    PlacedDevice device = *window->project().device(u"d4");
    device.parameters[QStringLiteral("ratedCurrent")] = 25;
    QVERIFY(window->project().updateDevice(device));
    QCOMPARE(ratingText(*window->project().device(u"d4")), QStringLiteral("C25"));
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);
    QCOMPARE(message(), kQf3Short);
}

void TestProtecting::hintThenBackToTheEvent()
{
    tripQf3();
    dropPreset(canvas, named<PaletteWidget>("paletteTree"), "breaker-1p-c16",
               viewportPoint(canvas, scene->slotRect(0, 1, 1).center()));
    QCOMPARE(message(), QStringLiteral("No room here: another device is in the way."));
    QTRY_COMPARE_WITH_TIMEOUT(message(), kQf3Short, 7000);
}

void TestProtecting::tripShowsOnTheDevice()
{
    tripQf3();
    const DeviceItem *item = deviceItem(QStringLiteral("d4"));
    QVERIFY(item->isTripped());
    QVERIFY(!item->isOn());
    QTRY_COMPARE(item->handleRect(), offHandle(QStringLiteral("d4")));
    QVERIFY(!item->tripMarkRect().isEmpty());
    QVERIFY(saveScreenshot(*window, "f09", "tripped"));
}

void TestProtecting::clickOnTrippedHandleResets()
{
    QList<SimulationEvent> events;
    connect(&window->simulation(), &Simulation::occurred, this,
            [&events](const SimulationEvent &event) { events << event; });
    const auto tripped = [&events] {
        return std::ranges::count_if(events, [](const SimulationEvent &event) {
            return event.kind == SimulationEvent::Kind::Tripped && event.id == QStringLiteral("d4");
        });
    };
    tripQf3();
    const DeviceItem *item = deviceItem(QStringLiteral("d4"));
    QCOMPARE(tripped(), 1);

    clickHandle("d4"); // the fault is still there
    QVERIFY(item->isTripped());
    QVERIFY(!item->isOn());
    QCOMPARE(tripped(), 2);

    QVERIFY(window->simulation().setFault(QStringLiteral("l3"), Fault::None));
    clickHandle("d4");
    QVERIFY(item->isOn());
    QVERIFY(!item->isTripped());
    QVERIFY(item->tripMarkRect().isEmpty());
    QCOMPARE(tripped(), 2);
}

void TestProtecting::handSwitchOffShowsNoMark()
{
    clickHandle("d1");
    clickHandle("d1");
    const DeviceItem *item = deviceItem(QStringLiteral("d1"));
    QVERIFY(!item->isOn());
    QVERIFY(!item->isTripped());
    QVERIFY(item->tripMarkRect().isEmpty());
}

void TestProtecting::tripMarkKeepsTerminalsAndHandleClear()
{
    const Catalog &catalog = window->project().catalog();
    for (const char *name : { "d1", "d2", "d3" }) {
        const QString id = QString::fromLatin1(name);
        DeviceItem *item = deviceItem(id);
        item->setTripped(true);
        const QRectF mark = item->tripMarkRect();
        QVERIFY2(!mark.isEmpty(), name);
        QVERIFY2(!mark.intersects(item->handleRect()), name);
        const PlacedDevice *device = window->project().device(id);
        const DeviceVariant *variant = catalog.deviceType(device->type)->variant(device->poles);
        for (const TerminalSpec &terminal : variant->terminals) {
            const QRectF around(terminal.position - QPointF(3.6, 3.6), QSizeF(7.2, 7.2));
            QVERIFY2(!mark.intersects(around), qPrintable(id + QLatin1Char(':') + terminal.name));
        }
        item->setTripped(false);
    }
}

void TestProtecting::testButtonTripsRcd()
{
    clickHandle("d1");
    clickHandle("d2");
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onTestButton("d2"));
    const DeviceItem *item = deviceItem(QStringLiteral("d2"));
    QCOMPARE(window->simulation().tripCause(u"d2"), TripCause::Test);
    QVERIFY(item->isTripped());
    QCOMPARE(message(), QStringLiteral("QD1 tripped: test button"));
    clickHandle("d2");
    QVERIFY(item->isOn());
    QVERIFY(!item->isTripped());
}

void TestProtecting::testButtonWithoutPowerHints()
{
    clickHandle("d2");
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onTestButton("d2"));
    QVERIFY(!deviceItem(QStringLiteral("d2"))->isTripped());
    QCOMPARE(message(),
             QStringLiteral("The test button works only when the RCD is on and has power"));
}

void TestProtecting::dragFromTestButtonMoves()
{
    clickHandle("d1");
    clickHandle("d2");
    // The device follows the cursor by the offset of the press from its centre.
    const QPoint from = onTestButton("d2");
    const QPoint offset = from - viewportPoint(canvas, scene->slotRect(0, 2, 2).center());
    mouseDrag(canvas->viewport(), from,
              viewportPoint(canvas, scene->slotRect(0, 8, 2).center()) + offset);
    QCOMPARE(window->project().device(u"d2")->module, 8);
    QVERIFY(!deviceItem(QStringLiteral("d2"))->isTripped());
}

void TestProtecting::wireToolTestButtonDoesNothing()
{
    clickHandle("d1");
    clickHandle("d2");
    window->findChild<QAction *>(QStringLiteral("wireAction"))->setChecked(true);
    QVERIFY(scene->isWireMode());
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onTestButton("d2"));
    QVERIFY(!deviceItem(QStringLiteral("d2"))->isTripped());
    QVERIFY(window->simulation().isOn(QStringLiteral("d2")));
}

void TestProtecting::stairwellTripShowsOnSupplyAndClickResets()
{
    Project &project = window->project();
    const QGraphicsItem *mark = sceneItem(scene, QStringLiteral("supplyTripMark"));
    QVERIFY(mark);
    QVERIFY(!mark->isVisible());
    Wire wire{ {}, QStringLiteral("supply:L"), QStringLiteral("supply:N"), 2.5, Conductor::L };
    wire.conductor = project.conductorFor(wire.from, wire.to);
    wire.length = 0.3;
    const QString id = project.addWire(wire);
    QVERIFY(!id.isEmpty());
    QCOMPARE(window->simulation().tripCause(SupplyBreakerId), TripCause::ShortCircuit);
    QVERIFY(mark->isVisible());
    QCOMPARE(message(), QStringLiteral("Stairwell breaker tripped: short circuit, 571 A on C50"));
    QVERIFY(saveScreenshot(*window, "f09", "stairwell"));

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onSupply());
    QCOMPARE(window->simulation().tripCause(SupplyBreakerId), TripCause::ShortCircuit);
    QVERIFY(mark->isVisible());

    sceneItem(scene, id)->setSelected(true);
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!project.wire(id));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onSupply());
    QCOMPARE(window->simulation().tripCause(SupplyBreakerId), TripCause::None);
    QVERIFY(!mark->isVisible());
    QCOMPARE(message(), QStringLiteral("Ready"));
}

void TestProtecting::supplyClickWhenNotTrippedDoesNothing()
{
    clickHandle("d1");
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onSupply());
    QCOMPARE(window->simulation().tripCause(SupplyBreakerId), TripCause::None);
    QVERIFY(window->simulation().isOn(QStringLiteral("d1")));
    QCOMPARE(message(), QStringLiteral("Ready"));
}

void TestProtecting::loadMenuOffersFaults()
{
    QMenu *menu = openMenu(onPicture(QStringLiteral("l3")));
    QVERIFY(menu && menu->isVisible());
    const QList<QAction *> actions = menu->actions();
    QCOMPARE(actions.size(), 6);
    QCOMPARE(actions[0], named<QAction>("shortCircuitAction"));
    QCOMPARE(actions[1], named<QAction>("leakageAction"));
    QCOMPARE(actions[2], named<QAction>("earthFaultAction"));
    QCOMPARE(actions[3], named<QAction>("removeFaultAction"));
    QVERIFY(actions[4]->isSeparator());
    QCOMPARE(actions[5], named<QAction>("deleteAction"));
    QCOMPARE(actions[0]->text(), QStringLiteral("Short circuit"));
    QCOMPARE(actions[1]->text(), QStringLiteral("Leakage"));
    QCOMPARE(actions[2]->text(), QStringLiteral("Earth fault"));
    QCOMPARE(actions[3]->text(), QStringLiteral("Remove fault"));
    for (int i = 0; i < 3; ++i)
        QVERIFY(!actions[i]->isChecked());
    QVERIFY(!actions[3]->isEnabled());
    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());

    menu = openMenu(
            viewportPoint(canvas, deviceItem(QStringLiteral("d4"))->sceneBoundingRect().center()));
    QVERIFY(menu && menu->isVisible());
    QCOMPARE(menu->actions(), QList<QAction *>{ named<QAction>("deleteAction") });
    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());
}

void TestProtecting::shortCircuitFromMenuTrips()
{
    for (const char *id : { "d1", "d2", "d4" })
        clickHandle(QString::fromLatin1(id));
    chooseFault(QStringLiteral("l3"), "shortCircuitAction");
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);
    QCOMPARE(window->simulation().fault(u"l3"), Fault::ShortCircuit);
    QVERIFY(!loadItem(QStringLiteral("l3"))->faultMarkRect().isEmpty());

    QMenu *menu = openMenu(onPicture(QStringLiteral("l3")));
    QVERIFY(menu && menu->isVisible());
    QVERIFY(named<QAction>("shortCircuitAction")->isChecked());
    QVERIFY(!named<QAction>("leakageAction")->isChecked());
    QVERIFY(named<QAction>("removeFaultAction")->isEnabled());
    clickMenuItem(menu, named<QAction>("removeFaultAction"));
    QCOMPARE(window->simulation().fault(u"l3"), Fault::None);
    QVERIFY(loadItem(QStringLiteral("l3"))->faultMarkRect().isEmpty());
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);

    clickHandle("d4");
    QVERIFY(window->simulation().isOn(u"d4"));
}

void TestProtecting::leakageOnKettleTripsRcd()
{
    for (const char *id : { "d1", "d2", "d5" })
        clickHandle(QString::fromLatin1(id));
    chooseFault(QStringLiteral("l9"), "leakageAction");
    QCOMPARE(window->simulation().tripCause(u"d2"), TripCause::Leakage);
    QCOMPARE(message(), kQd1Leakage);
    QVERIFY(saveScreenshot(*window, "f09", "leakage"));
}

void TestProtecting::faultStaysWithUnpluggedAppliance()
{
    chooseFault(QStringLiteral("l9"), "shortCircuitAction");
    mouseDrag(canvas->viewport(), onPicture("l9"), viewportPoint(canvas, QPointF(500, 300)));
    QVERIFY(window->project().load(u"l9")->socket.isEmpty());
    for (const char *id : { "d1", "d2", "d5" })
        clickHandle(QString::fromLatin1(id));
    for (const char *id : { "d1", "d2", "d3", "d4", "d5" })
        QCOMPARE(window->simulation().tripCause(QString::fromLatin1(id)), TripCause::None);
    mouseDrag(canvas->viewport(), onPicture("l9"),
              viewportPoint(canvas, loadItem(QStringLiteral("l5"))->mapToScene(QPointF(18, 13.5))));
    QCOMPARE(window->project().load(u"l9")->socket, QStringLiteral("l5"));
    QCOMPARE(window->simulation().tripCause(u"d5"), TripCause::ShortCircuit);
}

void TestProtecting::dangerMarkIsRed()
{
    window->project().removeWire(wireId(QStringLiteral("pebar:4>l5:PE")));
    for (const char *id : { "d1", "d2", "d5" })
        clickHandle(QString::fromLatin1(id));
    chooseFault(QStringLiteral("l9"), "earthFaultAction");
    QVERIFY(window->simulation().isDangerous(u"l9"));
    QVERIFY(loadItem(QStringLiteral("l9"))->isDangerous());
    for (const char *id : { "d1", "d2", "d3", "d4", "d5" })
        QCOMPARE(window->simulation().tripCause(QString::fromLatin1(id)), TripCause::None);
    QVERIFY(saveScreenshot(*window, "f09", "danger"));
}

void TestProtecting::leakageOutsideRcdIsMarkedDangerous()
{
    // The lighting line bypasses QD1 on both poles: QF2 is fed from QF1's bottom, and Lamp 1's N
    // goes back through QF1's N pole instead of the N bar.
    Project &project = window->project();
    QVERIFY(project.removeWire(wireId(QStringLiteral("d2:2>d3:1"))));
    QVERIFY(project.removeWire(wireId(QStringLiteral("nbar:2>l1:N"))));
    struct Ends
    {
        const char *from;
        const char *to;
        double section;
        double length;
    };
    for (const Ends &ends : { Ends{ "d1:2", "d3:1", 6, 0.3 }, Ends{ "l1:N", "d1:4", 1.5, 15 } }) {
        Wire wire{ {}, QString::fromLatin1(ends.from), QString::fromLatin1(ends.to), ends.section };
        wire.conductor = project.conductorFor(wire.from, wire.to);
        wire.length = ends.length;
        QVERIFY2(!project.addWire(wire).isEmpty(), ends.from);
    }
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d3"),
               QStringLiteral("l1") });
    chooseFault(QStringLiteral("l1"), "leakageAction");

    const Simulation &simulation = window->simulation();
    QCOMPARE(simulation.fault(u"l1"), Fault::Leakage);
    for (const char *id : { "d1", "d2", "d3", "d4", "d5" })
        QCOMPARE(simulation.tripCause(QString::fromLatin1(id)), TripCause::None);
    for (const char *id : { "d1", "d2", "d3", "l1" })
        QVERIFY2(simulation.isOn(QString::fromLatin1(id)), id);
    QVERIFY(simulation.isDangerous(u"l1"));
    QVERIFY(loadItem(QStringLiteral("l1"))->isDangerous());
    // By hand: 0.5784 Ω from the source to l1:L (the supply, QF1, the 6 mm² jumper, QF2, 15 m of
    // 1.5 mm²), then the person's 1000 Ω ∥ the lamp's 5290 Ω and its 0.1765 Ω way back: 0.27329 A,
    // V(l1:L) = 230 − 0.27329 · 0.5784 = 229.84 V, so 229.84 mA through the person.
    QCOMPARE(message(), QStringLiteral("Lamp 1: 230 mA through a person, no RCD turned it off"));
    QVERIFY(saveScreenshot(*window, "f09", "danger-leakage"));
}

void TestProtecting::faultActionsAreNotEdits()
{
    // A real edit first, so that undo and redo have something to do.
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onPicture("l2"));
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!window->project().load(u"l2"));
    const int edits = window->undoStack().count();
    QVERIFY(edits > 0);

    for (const char *id : { "d1", "d2", "d4" })
        clickHandle(QString::fromLatin1(id));
    chooseFault(QStringLiteral("l3"), "shortCircuitAction");
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);
    QCOMPARE(window->undoStack().count(), edits);

    const auto expectUnchanged = [this] {
        QCOMPARE(window->simulation().fault(u"l3"), Fault::ShortCircuit);
        QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::ShortCircuit);
        QVERIFY(window->simulation().isOn(u"d1"));
        QVERIFY(window->simulation().isOn(u"d2"));
    };
    window->undoStack().undo();
    QVERIFY(window->project().load(u"l2"));
    expectUnchanged();
    window->undoStack().redo();
    QVERIFY(!window->project().load(u"l2"));
    expectUnchanged();
}

void TestProtecting::faultSurvivesDeleteAndUndo()
{
    // Not the first row: the same row of the same menu, clicked by the previous test, would reach
    // the menu as a double click.
    chooseFault(QStringLiteral("l3"), "leakageAction");
    QCOMPARE(window->simulation().fault(u"l3"), Fault::Leakage);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onPicture("l3"));
    QCOMPARE(scene->selectedLoadId(), QStringLiteral("l3"));
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!window->project().load(u"l3"));
    QCOMPARE(window->simulation().fault(u"l3"), Fault::Leakage);
    window->undoStack().undo();
    QVERIFY(window->project().load(u"l3"));
    QCOMPARE(window->simulation().fault(u"l3"), Fault::Leakage);
    QVERIFY(!loadItem(QStringLiteral("l3"))->faultMarkRect().isEmpty());
}

void TestProtecting::faultMarkKeepsTerminalsAndButtonClear()
{
    dropLoad(canvas, named<PaletteWidget>("paletteTree"), "cooktop",
             viewportPoint(canvas, QPointF(400, 400)));
    const QString cooktop = window->project().loads().last().id;
    const Catalog &catalog = window->project().catalog();
    for (const QString &id :
         { QStringLiteral("l1"), QStringLiteral("l3"), cooktop, QStringLiteral("l9") }) {
        LoadItem *item = loadItem(id);
        QVERIFY2(item, qPrintable(id));
        item->setFault(true, false);
        const QRectF mark = item->faultMarkRect();
        QVERIFY2(!mark.isEmpty(), qPrintable(id));
        QVERIFY2(!mark.intersects(item->buttonRect()), qPrintable(id));
        const LoadType *type = catalog.loadType(window->project().load(id)->type);
        for (const TerminalSpec &terminal : type->terminals) {
            const QRectF around(terminal.position - QPointF(3.6, 3.6), QSizeF(7.2, 7.2));
            QVERIFY2(!mark.intersects(around), qPrintable(id + QLatin1Char(':') + terminal.name));
        }
        item->setFault(false, false);
        QVERIFY2(item->faultMarkRect().isEmpty(), qPrintable(id));
    }
}

void TestProtecting::timeControlStartsAtTenTimes()
{
    const auto startsAtTenTimes = [](const MainWindow &w) {
        const auto *speed10 = w.findChild<QAction *>(QStringLiteral("speed10Action"));
        QVERIFY(speed10);
        QVERIFY(speed10->isChecked());
        const auto *timer = w.findChild<QTimer *>(QStringLiteral("simulationTimer"));
        QVERIFY(timer);
        QVERIFY(timer->isActive());
        QCOMPARE(timer->interval(), 50);
    };
    startsAtTenTimes(*window);
    QCOMPARE(named<QLabel>("timeLabel")->text(), QStringLiteral("Time"));
    QCOMPARE(named<QAction>("pauseAction")->text(), QStringLiteral("Pause"));
    QCOMPARE(named<QAction>("speed1Action")->text(), QStringLiteral("×1"));
    QCOMPARE(named<QAction>("speed10Action")->text(), QStringLiteral("×10"));
    QCOMPARE(named<QAction>("speed60Action")->text(), QStringLiteral("×60"));

    clickToolButton("speed60Action");
    for (const char *name : { "pauseAction", "speed1Action", "speed10Action" })
        QVERIFY2(!named<QAction>(name)->isChecked(), name);
    QVERIFY(named<QTimer>("simulationTimer")->isActive());

    const MainWindow second;
    startsAtTenTimes(second);
}

void TestProtecting::pauseStopsHeatingNotInstantTrips()
{
    clickToolButton("pauseAction");
    QVERIFY(!named<QTimer>("simulationTimer")->isActive());
    overloadQf4();
    Simulation &simulation = window->simulation();
    const double elapsed = simulation.elapsed();
    QTest::qWait(500);
    QCOMPARE(simulation.elapsed(), elapsed);
    QVERIFY(simulation.isOn(u"d5"));

    clickHandle("d4");
    QVERIFY(simulation.setFault(QStringLiteral("l3"), Fault::ShortCircuit));
    QCOMPARE(simulation.tripCause(u"d4"), TripCause::ShortCircuit);
    QCOMPARE(simulation.elapsed(), elapsed);
}

void TestProtecting::overloadTripsAtSixtyTimes()
{
    clickToolButton("pauseAction");
    overloadQf4();
    const Simulation &simulation = window->simulation();
    const DeviceItem *item = deviceItem(QStringLiteral("d5"));
    QCOMPARE(item->heatLevel(), 0.0);
    clickToolButton("speed60Action");
    QTRY_VERIFY_WITH_TIMEOUT(item->heatLevel() > 0.5, 10000);
    QCOMPARE(simulation.tripCause(u"d5"), TripCause::None);
    named<QAction>("fitAction")->trigger(); // the default zoom
    QVERIFY(saveScreenshot(*window, "f09", "heating"));

    QTRY_VERIFY_WITH_TIMEOUT(simulation.tripCause(u"d5") != TripCause::None, 10000);
    QCOMPARE(simulation.tripCause(u"d5"), TripCause::Overload);
    QCOMPARE(message(), QStringLiteral("QF4 tripped: overload, 22.0 A on C16"));
    QVERIFY(saveScreenshot(*window, "f09", "overload"));
}

void TestProtecting::wireTurnsOrangeThenRed()
{
    clickToolButton("pauseAction");
    addCooktopCircuit();
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d6"),
               QStringLiteral("l11") });
    const Simulation &simulation = window->simulation();
    const WireItem *wire = wireItem(QStringLiteral("d6:2>l11:L"));
    QVERIFY(wire);
    QCOMPARE(wire->heat(), WireHeat::Normal);

    clickToolButton("speed60Action");
    QTRY_COMPARE_WITH_TIMEOUT(wire->heat(), WireHeat::Overloaded, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(wire->heat(), WireHeat::Overheating, 10000);
    QVERIFY2(message().contains(QStringLiteral("overheating: 34.6 A on 1.5 mm², allowed 19 A")),
             qPrintable(message()));
    named<QAction>("fitAction")->trigger(); // the default zoom
    QVERIFY(saveScreenshot(*window, "f09", "wire-overheating"));
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QVERIFY(saveScreenshot(*window, "f09", "wire-overheating-light"));
    QVERIFY(simulation.isOn(u"d6"));
    QCOMPARE(simulation.tripCause(u"d6"), TripCause::None);
}

void TestProtecting::overloadedRcdGlows()
{
    clickToolButton("pauseAction");
    addCooktopCircuit();
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d5"),
               QStringLiteral("d6"), QStringLiteral("l11"), QStringLiteral("l9"),
               QStringLiteral("l10") });
    const Simulation &simulation = window->simulation();
    QSignalSpy occurred(&simulation, &Simulation::occurred);
    const DeviceItem *item = deviceItem(QStringLiteral("d2"));
    QCOMPARE(item->heatLevel(), 0.0);

    clickToolButton("speed60Action");
    QTRY_VERIFY_WITH_TIMEOUT(simulation.isOverloaded(u"d2"), 10000);
    QTRY_COMPARE(item->heatLevel(), 1.0);
    named<QAction>("fitAction")->trigger(); // the default zoom
    QVERIFY(saveScreenshot(*window, "f09", "rcd-overloaded"));
    // Ruling R5: a wire's message may be newer than QD1's.
    const auto overloaded = std::ranges::find_if(occurred, [](const QList<QVariant> &arguments) {
        const auto event = arguments.at(0).value<SimulationEvent>();
        return event.kind == SimulationEvent::Kind::Overloaded && event.id == QStringLiteral("d2");
    });
    QVERIFY(overloaded != occurred.cend());
    const QString text = eventText(window->project(), overloaded->at(0).value<SimulationEvent>());
    QVERIFY2(text.startsWith(QStringLiteral("QD1 overloaded:")), qPrintable(text));
    QVERIFY(simulation.isOn(u"d1"));
    QCOMPARE(simulation.tripCause(u"d1"), TripCause::None);
}

void TestProtecting::rcdTripMessageOutlastsItsOverload()
{
    overloadQd1();
    Simulation &simulation = window->simulation();
    simulation.step(600);
    QVERIFY(simulation.isOverloaded(u"d2"));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onTestButton("d2"));
    QCOMPARE(simulation.tripCause(u"d2"), TripCause::Test);
    simulation.step(600); // tripped, it carries nothing and cools

    QVERIFY(!simulation.isOverloaded(u"d2"));
    QCOMPARE(simulation.tripCause(u"d2"), TripCause::Test);
    QVERIFY(deviceItem(QStringLiteral("d2"))->isTripped());
    QCOMPARE(message(), QStringLiteral("QD1 tripped: test button"));
    QVERIFY(saveScreenshot(*window, "f09", "rcd-tripped-cooled"));
}

void TestProtecting::rcdOverloadMessageOutlastsItsTrip()
{
    overloadQd1();
    Simulation &simulation = window->simulation();
    // By hand: 0.41025 Ω from the source to QF4's top and back from the N bar, in series with
    // the kitchen (QF4, 2 × 15 m of 2.5 mm², the kettle ∥ the microwave: 16.742 Ω) ∥ the cooktop
    // line (the jumper, QF5, 2 × 15 m of 1.5 mm², 9 kW: 6.2297 Ω) → 46.46 A through QD1, 33.86 A
    // through the cooktop wires. QD1 is overloaded after 73 · ln(I² / (I² − 40²)) = 98.7 s, the
    // cooktop wires after 45.4 s and overheating after 130.2 s, so QD1's message is the newest
    // after the second step.
    simulation.step(60);
    simulation.step(50);
    QVERIFY(simulation.isOverloaded(u"d2"));
    const QString overloaded = QStringLiteral(
            "QD1 overloaded: 46.5 A on 40 A, an RCD doesn't protect against overload");
    QCOMPARE(message(), overloaded);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onTestButton("d2"));
    QCOMPARE(message(), QStringLiteral("QD1 tripped: test button"));
    clickHandle("d2"); // still hot

    QVERIFY(simulation.isOn(u"d2"));
    QVERIFY(simulation.isOverloaded(u"d2"));
    QCOMPARE(message(), overloaded);
    QVERIFY(saveScreenshot(*window, "f09", "rcd-reset-hot"));
}

void TestProtecting::quietTimerDoesNoSolves()
{
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d3"),
               QStringLiteral("d4"), QStringLiteral("d5"), QStringLiteral("l1"),
               QStringLiteral("l2"), QStringLiteral("l7"), QStringLiteral("l8"),
               QStringLiteral("l9"), QStringLiteral("l10") });
    clickToolButton("speed60Action");
    // A label moved by hand stays where it is unless the label layout runs.
    CurrentLabelItem *label = wireItem(QStringLiteral("d3:2>l1:L"))->currentLabel();
    QVERIFY(label->isVisible());
    const QPointF moved = label->pos() + QPointF(0, 40);
    label->setPos(moved);
    const Simulation &simulation = window->simulation();
    QSignalSpy solved(&simulation, &Simulation::solved);
    QSignalSpy heated(&simulation, &Simulation::heated);
    QTest::qWait(1000);
    QCOMPARE(solved.count(), 0);
    QVERIFY2(heated.count() >= 10, qPrintable(QString::number(heated.count())));
    QCOMPARE(label->pos(), moved);
    for (const char *id : { "d1", "d2", "d3", "d4", "d5" })
        QVERIFY2(simulation.isOn(QString::fromLatin1(id)), id);
}

void TestProtecting::tripDuringDragKeepsTheDrag()
{
    clickToolButton("pauseAction");
    overloadQf4();
    Simulation &simulation = window->simulation();
    QWidget *viewport = canvas->viewport();
    // On the rating label, away from the handle.
    const QPointF grab(9, 28.5);
    const QPoint from = viewportPoint(canvas, deviceItem(QStringLiteral("d3"))->mapToScene(grab));
    const QPoint to = viewportPoint(canvas, scene->slotRect(0, 8, 1).topLeft() + grab);
    QTest::mousePress(viewport, Qt::LeftButton, {}, from);
    QTest::mouseMove(viewport, (from + to) / 2);
    QVERIFY(deviceItem(QStringLiteral("d3"))->pos() != scene->slotRect(0, 4, 1).topLeft());
    simulation.step(200);
    QCOMPARE(simulation.tripCause(u"d5"), TripCause::Overload);
    QTest::mouseMove(viewport, to);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, to);
    QCOMPARE(window->project().device(u"d3")->module, 8);
    QCOMPARE(deviceItem(QStringLiteral("d3"))->pos(), scene->slotRect(0, 8, 1).topLeft());

    QVERIFY(simulation.setOn(QStringLiteral("d5"), true));
    QVERIFY(simulation.isOn(u"d5"));
    clickToolButton("wireAction");
    QVERIFY(scene->isWireMode());
    const QPoint terminal = viewportPoint(canvas, scene->terminalPos(QStringLiteral("nbar:8")));
    QTest::mouseClick(viewport, Qt::LeftButton, {}, terminal);
    QTest::mouseMove(viewport, terminal + QPoint(40, 20));
    QCOMPARE(scene->wireStart(), QStringLiteral("nbar:8"));
    const auto *preview = dynamic_cast<QGraphicsLineItem *>(sceneItem(scene, "wirePreview"));
    QVERIFY(preview && preview->isVisible());
    const QLineF line = preview->line();
    const qsizetype wires = window->project().wires().size();
    simulation.step(200);
    QCOMPARE(simulation.tripCause(u"d5"), TripCause::Overload);
    QCOMPARE(scene->wireStart(), QStringLiteral("nbar:8"));
    QVERIFY(preview->isVisible());
    QCOMPARE(preview->line(), line);
    QCOMPARE(window->project().wires().size(), wires);
}

void TestProtecting::roadmapScenario()
{
    Simulation &simulation = window->simulation();
    QSignalSpy occurred(&simulation, &Simulation::occurred);
    named<QAction>("fitAction")->trigger(); // the default zoom
    addCooktopCircuit();                    // d6 and l11, switched off until the last case
    // Paused while switching on, so that QF4 starts cold as the hand value assumes.
    clickToolButton("pauseAction");
    dropLoad(canvas, named<PaletteWidget>("paletteTree"), "washing-machine",
             viewportPoint(canvas, loadItem(QStringLiteral("l6"))->mapToScene(QPointF(18, 13.5))));
    const QString machine = window->project().loads().last().id;
    QCOMPARE(window->project().load(machine)->socket, QStringLiteral("l6"));
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d5"),
               QStringLiteral("l9"), QStringLiteral("l10"), machine });
    const double start = simulation.elapsed();
    clickToolButton("speed60Action");
    QTRY_COMPARE_WITH_TIMEOUT(simulation.tripCause(u"d5"), TripCause::Overload, 10000);
    const auto tripped = [&occurred](const QString &id) {
        for (qsizetype i = occurred.size() - 1; i >= 0; --i) {
            const auto event = occurred.at(i).at(0).value<SimulationEvent>();
            if (event.kind == SimulationEvent::Kind::Tripped && event.id == id)
                return event;
        }
        return SimulationEvent{};
    };
    const SimulationEvent overload = tripped(QStringLiteral("d5"));
    QCOMPARE(overload.cause, TripCause::Overload);
    QVERIFY2(qAbs(overload.time - start - 163.48) < 0.05,
             qPrintable(QString::number(overload.time - start)));
    QCOMPARE(message(), QStringLiteral("QF4 tripped: overload, 22.0 A on C16"));
    QVERIFY(saveScreenshot(*window, "f09", "scenario-1-overload"));

    // To the empty canvas just above Socket 4.
    mouseDrag(canvas->viewport(), onPicture(machine),
              viewportPoint(canvas, loadItem(QStringLiteral("l6"))->mapToScene(QPointF(18, -45))));
    QVERIFY(window->project().load(machine)->socket.isEmpty());
    clickHandle("d5");
    QVERIFY(simulation.isOn(u"d5"));
    clickHandle("d4");
    QVERIFY(simulation.isOn(u"d4"));
    chooseFault(QStringLiteral("l3"), "shortCircuitAction");
    QCOMPARE(simulation.tripCause(u"d4"), TripCause::ShortCircuit);
    // Not 370 A: the kettle and the microwave are on, and their current sags the supply.
    QCOMPARE(message(), QStringLiteral("QF3 tripped: short circuit, 367 A on C16"));
    QVERIFY(saveScreenshot(*window, "f09", "scenario-2-short"));

    QMenu *menu = openMenu(onPicture(QStringLiteral("l3")));
    QVERIFY(menu && menu->isVisible());
    clickMenuItem(menu, named<QAction>("removeFaultAction"));
    QCOMPARE(simulation.fault(u"l3"), Fault::None);
    clickHandle("d4");
    QVERIFY(simulation.isOn(u"d4"));
    QCOMPARE(simulation.tripCause(u"d4"), TripCause::None);
    chooseFault(QStringLiteral("l9"), "leakageAction");
    QCOMPARE(simulation.tripCause(u"d2"), TripCause::Leakage);
    // Not 230 mA: the same sag lowers the kettle's voltage to 223 V.
    QCOMPARE(message(), QStringLiteral("QD1 tripped: leakage 223 mA, trips at 30 mA"));
    QVERIFY(saveScreenshot(*window, "f09", "scenario-3-leakage"));

    menu = openMenu(onPicture(QStringLiteral("l9")));
    QVERIFY(menu && menu->isVisible());
    clickMenuItem(menu, named<QAction>("removeFaultAction"));
    clickHandle("d2");
    QVERIFY(simulation.isOn(u"d2"));
    QVERIFY(simulation.isEnergized(u"l9"));
    QVERIFY(simulation.loadCurrent(u"l9") > 0);

    switchOn({ QStringLiteral("d6"), QStringLiteral("l11") });
    const WireItem *wire = wireItem(QStringLiteral("d6:2>l11:L"));
    QVERIFY(wire);
    const double cooking = simulation.elapsed();
    QTRY_COMPARE_WITH_TIMEOUT(wire->heat(), WireHeat::Overheating, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(simulation.elapsed() - cooking >= 600, 20000);
    QVERIFY(simulation.isOn(u"d6"));
    QCOMPARE(simulation.tripCause(u"d6"), TripCause::None);
    QVERIFY(saveScreenshot(*window, "f09", "scenario-4-overheating"));

    const auto switchLanguage = [this](const char *action) {
        auto *languageMenu = named<QMenu>("languageMenu");
        clickMenuBarItem(*window, languageMenu);
        clickMenuItem(languageMenu, named<QAction>(action));
    };
    switchLanguage("languageAction_ru");
    QCOMPARE(currentLanguage(), Language::Russian);
    QVERIFY(saveScreenshot(*window, "f09", "scenario-ru"));
    switchLanguage("languageAction_kk");
    QCOMPARE(currentLanguage(), Language::Kazakh);
    QVERIFY(saveScreenshot(*window, "f09", "scenario-kk"));
}

QTEST_MAIN(TestProtecting)
#include "tst_protecting.moc"
