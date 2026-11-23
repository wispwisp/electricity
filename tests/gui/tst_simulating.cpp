// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/currenttext.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "core/project.h"
#include "core/simulation.h"
#include "editorhelpers.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSvgRenderer>
#include <QTest>
#include <QToolBar>
#include <QUndoStack>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

namespace {
// Distance from the point to the nearest point of the route.
qreal distanceToRoute(QPointF point, const QPolygonF &route)
{
    qreal nearest = std::numeric_limits<qreal>::max();
    for (qsizetype i = 1; i < route.size(); ++i) {
        const QPointF start = route[i - 1];
        const QPointF along = route[i] - start;
        const qreal lengthSquared = QPointF::dotProduct(along, along);
        const qreal t = lengthSquared > 0
                ? std::clamp(QPointF::dotProduct(point - start, along) / lengthSquared, 0.0, 1.0)
                : 0.0;
        nearest = std::min(nearest, QLineF(point, start + along * t).length());
    }
    return nearest;
}

struct WireEnds
{
    const char *from;
    const char *to;
};

// With QF1, QD1, QF2 and Lamp 1 on: the wires that carry the lamp's current, and some that don't.
constexpr auto kLampWires = std::to_array<WireEnds>({ { "supply:L", "d1:1" },
                                                      { "supply:N", "d1:3" },
                                                      { "d1:2", "d2:1" },
                                                      { "d1:4", "d2:N1" },
                                                      { "d2:2", "d3:1" },
                                                      { "d2:N2", "nbar:1" },
                                                      { "d3:2", "l1:L" },
                                                      { "nbar:2", "l1:N" } });
constexpr auto kIdleWires = std::to_array<WireEnds>({ { "pebar:2", "l1:PE" },
                                                      { "l1:L", "l2:L" },
                                                      { "l1:N", "l2:N" },
                                                      { "l1:PE", "l2:PE" },
                                                      { "d3:1", "d4:1" },
                                                      { "d4:1", "d5:1" },
                                                      { "supply:PE", "pebar:1" } });
} // namespace

class TestSimulating : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void devicesStartOffWithHandleDown();
    void clickOnHandleSwitches();
    void clickOnBodySelectsWithoutSwitching();
    void dragFromHandleMovesWithoutSwitching();
    void escapedPressOnHandleDoesNotSwitch();
    void doubleClickOnHandleFlipsTwice();
    void wireToolClicksDoNotSwitch();
    void switchingIsNotAnEdit();
    void switchSurvivesDeleteAndUndo();
    void tutorialLoadsFixture();
    void switchableLoadsShowAButton();
    void clickOnButtonSwitchesLoad();
    void clickElsewhereOnLoadSelectsWithoutSwitching();
    void dragFromButtonMovesLoad();
    void doubleClickOnButtonFlipsTwice();
    void lampGlowsOnlyWhenOnAndPowered();
    void applianceRunsThroughItsSocket();
    void deletingSocketStopsItsAppliancesUntilUndo();
    void wireToolClicksDoNotSwitchLoads();
    void currentTextFormats_data();
    void currentTextFormats();
    void showsCurrentFromFiveMilliamperes();
    void labelsWhereCurrentFlows();
    void labelsDrawAboveWiresAndLetClicksThrough();
    void wireLabelsKeepClear_data();
    void wireLabelsKeepClear();
    void wireLabelStepsOutOfCoveredRoute();
    void deviceLabelKeepsHandleRatingAndTerminalsClear();
    void showCurrentsTogglesAndPersists();
    void labelsFollowLanguage();
    void labelsInLightTheme();
    void shortShowsWholeAmperes();
    void roadmapScenario();

private:
    [[nodiscard]] LoadItem *loadItem(const QString &id) const;
    // The viewport point over the middle of the load's button / of its picture.
    [[nodiscard]] QPoint onButton(const QString &id) const;
    [[nodiscard]] QPoint onPicture(const QString &id) const;
    // The zoomed-out view that shows the loads.
    void zoomOut();
    [[nodiscard]] DeviceItem *deviceItem(const QString &id) const;
    // Bounds of an element of the device's picture, in item coordinates.
    [[nodiscard]] QRectF pictureElement(const QString &id, const QString &element) const;
    // The viewport point over the middle of the device's handle where it is drawn now.
    [[nodiscard]] QPoint onHandle(const QString &id) const;
    // The viewport point over the middle of the device's rating label.
    [[nodiscard]] QPoint onLabel(const QString &id) const;
    void clickHandle(const QString &id);
    // The viewport point a device-body point would be at with the device shifted by modules.
    [[nodiscard]] QPoint shifted(QPoint viewportPos, int modules) const;
    // The device is off in the simulation and on its item, with its handle drawn down.
    void checkOff(const QString &id) const;
    // The device is on in the simulation and on its item, with its handle drawn up.
    void checkOn(const QString &id) const;
    // The wire from one terminal to another, or nullptr.
    [[nodiscard]] WireItem *wireItem(const WireEnds &ends) const;
    // The tutorial loads in the zoomed-out view, with the devices and loads switched on by clicks.
    void switchOn(const QStringList &ids);
    // The same with QF1, QD1, QF2 and Lamp 1.
    void lightLamp1();
    // QF1, QD1, QF2 and Lamp 1 on, everything else off.
    void checkLamp1Switches() const;
    // A view at the scale centred on the scene point, to read the labels.
    void closeUp(qreal scale, QPointF centre);
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
};

DeviceItem *TestSimulating::deviceItem(const QString &id) const
{
    return dynamic_cast<DeviceItem *>(sceneItem(scene, id));
}

LoadItem *TestSimulating::loadItem(const QString &id) const
{
    return dynamic_cast<LoadItem *>(sceneItem(scene, id));
}

QPoint TestSimulating::onButton(const QString &id) const
{
    const LoadItem *item = loadItem(id);
    return viewportPoint(canvas, item->mapToScene(item->buttonRect().center()));
}

QPoint TestSimulating::onPicture(const QString &id) const
{
    const LoadItem *item = loadItem(id);
    return viewportPoint(canvas, item->mapToScene(item->shape().boundingRect().center()));
}

void TestSimulating::zoomOut()
{
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
}

QRectF TestSimulating::pictureElement(const QString &id, const QString &element) const
{
    const Project &project = window->project();
    const PlacedDevice *device = project.device(id);
    const DeviceType *type = device ? project.catalog().deviceType(device->type) : nullptr;
    const DeviceVariant *variant = type ? type->variant(device->poles) : nullptr;
    if (!variant)
        return {};
    const QString path = picturePath(variant->picture);
    const QSvgRenderer &renderer = svgRenderer(path);
    return renderer.transformForElement(element).mapRect(renderer.boundsOnElement(element));
}

QPoint TestSimulating::onHandle(const QString &id) const
{
    const DeviceItem *item = deviceItem(id);
    return viewportPoint(canvas, item->mapToScene(item->handleRect().center()));
}

QPoint TestSimulating::onLabel(const QString &id) const
{
    const DeviceItem *item = deviceItem(id);
    return viewportPoint(canvas,
                         item->mapToScene(pictureElement(id, QStringLiteral("label")).center()));
}

void TestSimulating::clickHandle(const QString &id)
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onHandle(id));
}

QPoint TestSimulating::shifted(QPoint viewportPos, int modules) const
{
    const QPointF shift
            = scene->slotRect(0, modules, 1).topLeft() - scene->slotRect(0, 0, 1).topLeft();
    return viewportPoint(canvas, canvas->mapToScene(viewportPos) + shift);
}

void TestSimulating::checkOff(const QString &id) const
{
    QVERIFY2(!window->simulation().isOn(id), qPrintable(id));
    const DeviceItem *item = deviceItem(id);
    QVERIFY2(item, qPrintable(id));
    QVERIFY2(!item->isOn(), qPrintable(id));
    QCOMPARE(item->handleRect(), pictureElement(id, QStringLiteral("handle")).translated(0, 12.5));
}

void TestSimulating::checkOn(const QString &id) const
{
    QVERIFY2(window->simulation().isOn(id), qPrintable(id));
    const DeviceItem *item = deviceItem(id);
    QVERIFY2(item, qPrintable(id));
    QVERIFY2(item->isOn(), qPrintable(id));
    QCOMPARE(item->handleRect(), pictureElement(id, QStringLiteral("handle")));
}

WireItem *TestSimulating::wireItem(const WireEnds &ends) const
{
    const QList<Wire> &wires = window->project().wires();
    const auto it = std::ranges::find_if(wires, [&ends](const Wire &wire) {
        return wire.from == QLatin1StringView(ends.from) && wire.to == QLatin1StringView(ends.to);
    });
    return it == wires.end() ? nullptr : dynamic_cast<WireItem *>(sceneItem(scene, it->id));
}

void TestSimulating::switchOn(const QStringList &ids)
{
    addTutorialLoads(window->project());
    zoomOut();
    for (const QString &id : ids) {
        if (window->project().device(id))
            clickHandle(id);
        else
            QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton(id));
        QVERIFY2(window->simulation().isOn(id), qPrintable(id));
    }
}

void TestSimulating::lightLamp1()
{
    switchOn({ QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d3"),
               QStringLiteral("l1") });
    QVERIFY(loadItem("l1")->isRunning());
}

void TestSimulating::checkLamp1Switches() const
{
    for (const char *id : { "d1", "d2", "d3", "l1" })
        QVERIFY2(window->simulation().isOn(QString::fromLatin1(id)), id);
    for (const char *id : { "d4", "d5", "l2" })
        QVERIFY2(!window->simulation().isOn(QString::fromLatin1(id)), id);
}

void TestSimulating::closeUp(qreal scale, QPointF centre)
{
    canvas->setTransform(QTransform::fromScale(scale, scale));
    canvas->centerOn(centre);
}

void TestSimulating::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestSimulating::init()
{
    window = std::make_unique<MainWindow>();
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    canvas = window->findChild<CanvasView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas);
    scene = qobject_cast<EditorScene *>(canvas->scene());
    QVERIFY(scene);
    QCOMPARE(&window->simulation(), &scene->simulation());
    addTutorialDevices(window->project());
    addTutorialWires(window->project());
}

// Also when a test fails midway, the next one starts in English and in the dark theme.
void TestSimulating::cleanup()
{
    window.reset();
    installLanguage(Language::English);
    Theme::apply(Theme::Mode::Dark);
    Settings::saveShowCurrents(true);
}

void TestSimulating::devicesStartOffWithHandleDown()
{
    for (const char *id : { "d1", "d2", "d3", "d4", "d5" })
        checkOff(id);

    dropPreset(canvas, named<PaletteWidget>("paletteTree"), "breaker-1p-c16",
               viewportPoint(canvas, scene->slotRect(0, 8, 1).center()));
    const QList<PlacedDevice> &devices = window->project().devices();
    QCOMPARE(devices.size(), 6);
    QCOMPARE(devices.last().module, 8);
    checkOff(devices.last().id);
}

void TestSimulating::clickOnHandleSwitches()
{
    clickHandle("d1");
    checkOn("d1");
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d1"));

    clickHandle("d1");
    checkOff("d1");

    clickHandle("d1");
    clickHandle("d2");
    checkOn("d1");
    checkOn("d2");
    for (const char *id : { "d3", "d4", "d5" })
        checkOff(id);
    QVERIFY(saveScreenshot(*window, "f08", "handles"));
}

void TestSimulating::clickOnBodySelectsWithoutSwitching()
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onLabel("d3"));
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d3"));
    checkOff("d3");
}

void TestSimulating::dragFromHandleMovesWithoutSwitching()
{
    const QPoint handle = onHandle("d5");
    mouseDrag(canvas->viewport(), handle, shifted(handle, 2));
    QCOMPARE(window->project().device(u"d5")->module, 8);
    QCOMPARE(window->undoStack().count(), 1);
    checkOff("d5");
}

void TestSimulating::escapedPressOnHandleDoesNotSwitch()
{
    QWidget *viewport = canvas->viewport();
    const QPoint handle = onHandle("d3");
    QTest::mousePress(viewport, Qt::LeftButton, {}, handle);
    QCOMPARE(QApplication::focusWidget(), canvas);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, handle);
    checkOff("d3");
    QCOMPARE(window->project().device(u"d3")->module, 4);
    QCOMPARE(deviceItem("d3")->pos(), scene->slotRect(0, 4, 1).topLeft());
    QCOMPARE(window->undoStack().count(), 0);
}

// What a quick double click sends the canvas: press, release, double click, release.
void TestSimulating::doubleClickOnHandleFlipsTwice()
{
    QWidget *viewport = canvas->viewport();
    const QPoint handle = onHandle("d3");
    QTest::mousePress(viewport, Qt::LeftButton, {}, handle);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, handle);
    QTest::mouseDClick(viewport, Qt::LeftButton, {}, handle);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, handle);
    checkOff("d3");
    QCOMPARE(window->project().device(u"d3")->module, 4);
    QCOMPARE(deviceItem("d3")->pos(), scene->slotRect(0, 4, 1).topLeft());
    QCOMPARE(deviceItem("d3")->zValue(), DeviceItem::PlacedZ);
    QVERIFY(!scene->mouseGrabberItem());
    QCOMPARE(window->undoStack().count(), 0);

    clickHandle("d3");
    checkOn("d3");
}

void TestSimulating::wireToolClicksDoNotSwitch()
{
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(named<QAction>("wireAction")),
                      Qt::LeftButton);
    QVERIFY(scene->isWireMode());
    clickHandle("d1");
    checkOff("d1");
}

void TestSimulating::switchingIsNotAnEdit()
{
    clickHandle("d1");
    checkOn("d1");
    QCOMPARE(window->undoStack().count(), 0);

    const QPoint label = onLabel("d3");
    mouseDrag(canvas->viewport(), label, shifted(label, 4));
    QCOMPARE(window->project().device(u"d3")->module, 8);
    QCOMPARE(window->undoStack().count(), 1);
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(window->project().device(u"d3")->module, 4);
    checkOn("d1");
}

void TestSimulating::switchSurvivesDeleteAndUndo()
{
    clickHandle("d3");
    checkOn("d3");
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d3"));
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!window->project().device(u"d3"));
    QVERIFY(!deviceItem("d3"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(window->project().device(u"d3"));
    checkOn("d3");
}

void TestSimulating::tutorialLoadsFixture()
{
    Project &project = window->project();
    const qsizetype wires = project.wires().size();
    addTutorialLoads(project);
    QCOMPARE(project.loads().size(), 10);
    QCOMPARE(project.wires().size(), wires + 18);
    for (const Wire &wire : project.wires()) {
        if (project.load(terminalObject(wire.from)) || project.load(terminalObject(wire.to)))
            QCOMPARE(wire.length, 15.0);
    }
}

void TestSimulating::switchableLoadsShowAButton()
{
    addTutorialLoads(window->project());
    zoomOut();
    dropLoad(canvas, named<PaletteWidget>("paletteTree"), "cooktop",
             viewportPoint(canvas, QPointF(400, 400)));
    const QString cooktop = window->project().loads().last().id;
    for (const QString &id : { QStringLiteral("l1"), QStringLiteral("l9"), cooktop }) {
        const PlacedLoad *load = window->project().load(id);
        const LoadItem *item = loadItem(id);
        QVERIFY2(item, qPrintable(id));
        const QRectF button = item->buttonRect();
        const LoadType *type = window->project().catalog().loadType(load->type);
        QVERIFY2(QRectF(QPointF(0, 0), type->size).contains(button), qPrintable(id));
        for (const TerminalSpec &terminal : type->terminals) {
            const QRectF keepOut(terminal.position - QPointF(3.6, 3.6), QSizeF(7.2, 7.2));
            QVERIFY2(!button.intersects(keepOut), qPrintable(id));
        }
    }
    for (const char *id : { "l3", "l4", "l5", "l6" })
        QVERIFY2(loadItem(id)->buttonRect().isEmpty(), id);
}

void TestSimulating::clickOnButtonSwitchesLoad()
{
    addTutorialLoads(window->project());
    zoomOut();
    QVERIFY(!window->simulation().isOn(u"l1"));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton("l1"));
    QVERIFY(window->simulation().isOn(u"l1"));
    QVERIFY(loadItem("l1")->isOn());
    QVERIFY(loadItem("l1")->isSelected());
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton("l1"));
    QVERIFY(!window->simulation().isOn(u"l1"));
    QVERIFY(!loadItem("l1")->isOn());
    QCOMPARE(window->undoStack().count(), 0);
}

void TestSimulating::clickElsewhereOnLoadSelectsWithoutSwitching()
{
    addTutorialLoads(window->project());
    zoomOut();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onPicture("l1"));
    QVERIFY(loadItem("l1")->isSelected());
    QVERIFY(!window->simulation().isOn(u"l1"));
    QVERIFY(!loadItem("l1")->isOn());
}

void TestSimulating::dragFromButtonMovesLoad()
{
    addTutorialLoads(window->project());
    zoomOut();
    const QPointF before = window->project().loadPosition(*window->project().load(u"l2"));
    const QPoint button = onButton("l2");
    mouseDrag(canvas->viewport(), button, button + QPoint(0, 90));
    QVERIFY(window->project().loadPosition(*window->project().load(u"l2")) != before);
    QVERIFY(!window->simulation().isOn(u"l2"));
}

// What a quick double click sends the canvas: press, release, double click, release.
void TestSimulating::doubleClickOnButtonFlipsTwice()
{
    addTutorialLoads(window->project());
    zoomOut();
    QWidget *viewport = canvas->viewport();
    const QPoint button = onButton("l1");
    const QPointF before = loadItem("l1")->pos();
    QTest::mousePress(viewport, Qt::LeftButton, {}, button);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, button);
    QVERIFY(window->simulation().isOn(u"l1"));
    QTest::mouseDClick(viewport, Qt::LeftButton, {}, button);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, button);
    QVERIFY(!window->simulation().isOn(u"l1"));
    QVERIFY(!loadItem("l1")->isOn());
    QCOMPARE(loadItem("l1")->pos(), before);
    QCOMPARE(loadItem("l1")->zValue(), LoadItem::PlacedZ);
    QVERIFY(!scene->mouseGrabberItem());
    QCOMPARE(window->undoStack().count(), 0);
}

void TestSimulating::lampGlowsOnlyWhenOnAndPowered()
{
    addTutorialLoads(window->project());
    zoomOut();
    QVERIFY(!loadItem("l1")->isRunning());
    window->simulation().setOn(QStringLiteral("l1"), true);
    QVERIFY(!loadItem("l1")->isRunning());
    for (const char *id : { "d1", "d2", "d3" })
        clickHandle(id);
    QVERIFY(loadItem("l1")->isOn());
    QVERIFY(loadItem("l1")->isRunning());
    QVERIFY(!loadItem("l2")->isRunning());
    QVERIFY(saveScreenshot(*window, "f08", "lamp-glow"));
    canvas->setTransform(QTransform::fromScale(5, 5));
    canvas->centerOn(loadItem("l1")->sceneBoundingRect().center() + QPointF(20, 20));
    QVERIFY(saveScreenshot(*window, "f08", "lamp-glow-closeup"));
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QVERIFY(saveScreenshot(*window, "f08", "lamp-glow-light-closeup"));
}

void TestSimulating::applianceRunsThroughItsSocket()
{
    addTutorialLoads(window->project());
    zoomOut();
    for (const char *id : { "d1", "d2", "d5" })
        clickHandle(id);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton("l9"));
    QVERIFY(loadItem("l9")->isOn());
    QVERIFY(loadItem("l9")->isRunning());
    QVERIFY(saveScreenshot(*window, "f08", "appliance-halo"));
    canvas->setTransform(QTransform::fromScale(5, 5));
    canvas->centerOn(loadItem("l9")->sceneBoundingRect().center() + QPointF(20, 0));
    QVERIFY(saveScreenshot(*window, "f08", "appliance-halo-closeup"));
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QVERIFY(saveScreenshot(*window, "f08", "appliance-halo-light-closeup"));
    Theme::apply(Theme::Mode::Dark);
    zoomOut();

    mouseDrag(canvas->viewport(), onPicture("l9"), viewportPoint(canvas, QPointF(500, 300)));
    QVERIFY(window->project().load(u"l9")->socket.isEmpty());
    QVERIFY(loadItem("l9")->isOn());
    QVERIFY(!loadItem("l9")->isRunning());

    mouseDrag(canvas->viewport(), onPicture("l9"),
              viewportPoint(canvas, loadItem("l5")->mapToScene(QPointF(18, 13.5))));
    QCOMPARE(window->project().load(u"l9")->socket, QStringLiteral("l5"));
    QVERIFY(loadItem("l9")->isRunning());
}

void TestSimulating::deletingSocketStopsItsAppliancesUntilUndo()
{
    addTutorialLoads(window->project());
    zoomOut();
    for (const char *id : { "d1", "d2", "d5" })
        clickHandle(id);
    for (const char *id : { "l9", "l10" })
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton(id));
    QVERIFY(loadItem("l9")->isRunning());
    QVERIFY(loadItem("l10")->isRunning());

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, loadItem("l5")->mapToScene(QPointF(27, 20))));
    QVERIFY(loadItem("l5")->isSelected());
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!window->project().load(u"l5"));
    for (const char *id : { "l9", "l10" }) {
        QVERIFY2(loadItem(id), id);
        QVERIFY2(window->project().load(QString::fromLatin1(id))->socket.isEmpty(), id);
        QVERIFY2(loadItem(id)->isOn(), id);
        QVERIFY2(!loadItem(id)->isRunning(), id);
    }

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    for (const char *id : { "l9", "l10" }) {
        QCOMPARE(window->project().load(QString::fromLatin1(id))->socket, QStringLiteral("l5"));
        QVERIFY2(loadItem(id)->isRunning(), id);
    }
}

void TestSimulating::wireToolClicksDoNotSwitchLoads()
{
    addTutorialLoads(window->project());
    zoomOut();
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(named<QAction>("wireAction")),
                      Qt::LeftButton);
    QVERIFY(scene->isWireMode());
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton("l1"));
    QVERIFY(!window->simulation().isOn(u"l1"));
}

void TestSimulating::currentTextFormats_data()
{
    QTest::addColumn<double>("amperes");
    QTest::addColumn<QString>("text");
    QTest::newRow("lamp") << 0.043472 << QStringLiteral("0.04 A");
    QTest::newRow("just under 10 A") << 9.994 << QStringLiteral("9.99 A");
    QTest::newRow("rounds up to 10 A") << 9.996 << QStringLiteral("10.0 A");
    QTest::newRow("kettle and microwave") << 13.409 << QStringLiteral("13.4 A");
    QTest::newRow("rounds up to 100 A") << 99.96 << QStringLiteral("100 A");
    QTest::newRow("short") << 556.9 << QStringLiteral("557 A");
}

void TestSimulating::currentTextFormats()
{
    QFETCH(double, amperes);
    QFETCH(QString, text);
    QCOMPARE(currentText(amperes), text);
}

void TestSimulating::showsCurrentFromFiveMilliamperes()
{
    QVERIFY(!showsCurrent(0.0049));
    QVERIFY(showsCurrent(0.005));
}

void TestSimulating::labelsWhereCurrentFlows()
{
    lightLamp1();
    for (const char *id : { "d1", "d2", "d3" })
        QCOMPARE(deviceItem(id)->currentText(), QStringLiteral("0.04 A"));
    for (const char *id : { "d4", "d5" })
        QCOMPARE(deviceItem(id)->currentText(), QString());
    for (const WireEnds &ends : kLampWires) {
        const WireItem *wire = wireItem(ends);
        QVERIFY2(wire, ends.from);
        QCOMPARE(wire->currentText(), QStringLiteral("0.04 A"));
        QVERIFY2(wire->currentLabel()->isVisible(), ends.from);
    }
    for (const WireEnds &ends : kIdleWires) {
        const WireItem *wire = wireItem(ends);
        QVERIFY2(wire, ends.from);
        QCOMPARE(wire->currentText(), QString());
        QVERIFY2(!wire->currentLabel()->isVisible(), ends.from);
    }
    QVERIFY(saveScreenshot(*window, "f08", "labels"));
    closeUp(3, QPointF(70, 120));
    QVERIFY(saveScreenshot(*window, "f08", "labels-closeup"));
    closeUp(3, QPointF(60, 260));
    QVERIFY(saveScreenshot(*window, "f08", "labels-lamp-closeup"));
}

void TestSimulating::labelsDrawAboveWiresAndLetClicksThrough()
{
    lightLamp1();
    const WireItem *wire = wireItem({ "d3:2", "l1:L" });
    QVERIFY(wire);
    const CurrentLabelItem *label = wire->currentLabel();
    QVERIFY(label->isVisible());
    QCOMPARE(label->data(EditorScene::IdKey).toString(), QStringLiteral("currentLabel"));
    QCOMPARE(label->zValue(), CurrentLabelItem::LabelZ);
    QVERIFY(label->zValue() > WireItem::WireZ);
    QVERIFY(label->zValue() > DeviceItem::LabelZ);
    QCOMPARE(label->acceptedMouseButtons(), Qt::NoButton);

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, label->sceneBoundingRect().center()));
    QCOMPARE(scene->selectedWireId(), wire->data(EditorScene::IdKey).toString());
}

void TestSimulating::wireLabelsKeepClear_data()
{
    QTest::addColumn<QStringList>("switchedOn");
    QTest::addColumn<int>("shown");
    // Worked out from the fixture's wiring: the wires on the path of each running load; never PE,
    // and never the wires to a socket with nothing running in it.
    QTest::newRow("lamp-on") << QStringList{ "d1", "d2", "d3", "l1" } << 8;
    QTest::newRow("kettle-on") << QStringList{ "d1", "d2", "d5", "l9" } << 10;
    QTest::newRow("all-on") << QStringList{ "d1", "d2", "d3", "d4", "d5", "l1",
                                            "l2", "l7", "l8", "l9", "l10" }
                            << 18;
}

// Every wire that carries current shows its label, and no label covers another, a device's
// designation, rating, handle, terminals or current, or a load's terminals, button or caption.
void TestSimulating::wireLabelsKeepClear()
{
    QFETCH(QStringList, switchedOn);
    QFETCH(int, shown);
    switchOn(switchedOn);
    const Project &project = window->project();
    QList<QRectF> keepClear;
    for (const PlacedDevice &device : project.devices()) {
        const DeviceItem *item = deviceItem(device.id);
        const DeviceVariant *variant
                = project.catalog().deviceType(device.type)->variant(device.poles);
        keepClear << item->designationItem()->sceneBoundingRect()
                  << item->mapRectToScene(pictureElement(device.id, QStringLiteral("label")))
                  << item->mapRectToScene(item->handleRect())
                  << item->mapRectToScene(item->currentRect());
        for (const TerminalSpec &terminal : variant->terminals) {
            const QRectF keepOut(terminal.position - QPointF(3.6, 3.6), QSizeF(7.2, 7.2));
            keepClear << item->mapRectToScene(keepOut);
        }
    }
    for (const PlacedLoad &load : project.loads()) {
        const LoadItem *item = loadItem(load.id);
        keepClear << item->mapRectToScene(item->buttonRect())
                  << item->mapRectToScene(item->captionRect());
        for (const TerminalSpec &terminal : project.catalog().loadType(load.type)->terminals) {
            const QRectF keepOut(terminal.position - QPointF(3.6, 3.6), QSizeF(7.2, 7.2));
            keepClear << item->mapRectToScene(keepOut);
        }
    }
    int labels = 0;
    for (const Wire &wire : project.wires()) {
        const auto *item = dynamic_cast<WireItem *>(sceneItem(scene, wire.id));
        if (!item->currentLabel()->isVisible())
            continue;
        ++labels;
        const QRectF label = item->currentLabel()->sceneBoundingRect();
        for (const QRectF &rect : std::as_const(keepClear))
            QVERIFY2(!label.intersects(rect), qPrintable(wire.id));
        keepClear << label;
        // On its own wire, or beside it: the label reaches within 2.5 of the route.
        const QPolygonF route = item->route();
        bool reachesOwnWire = false;
        for (qsizetype i = 1; i < route.size(); ++i) {
            const QRectF segment = QRectF(route[i - 1], route[i])
                                           .normalized()
                                           .adjusted(-0.01, -0.01, 0.01, 0.01);
            reachesOwnWire |= segment.intersects(label.adjusted(-2.5, -2.5, 2.5, 2.5));
        }
        QVERIFY2(reachesOwnWire, qPrintable(wire.id));
        // Never centred on another wire, which would read as that wire's current, or as current
        // on a PE wire.
        for (const Wire &other : project.wires()) {
            if (other.id == wire.id)
                continue;
            const auto *otherItem = dynamic_cast<WireItem *>(sceneItem(scene, other.id));
            QVERIFY2(distanceToRoute(label.center(), otherItem->route()) >= 2.25,
                     qPrintable(wire.id + QStringLiteral(" on ") + other.id));
        }
    }
    QCOMPARE(labels, shown);

    const QString tag = QString::fromLatin1(QTest::currentDataTag());
    closeUp(1.8, QPointF(175, 130));
    QVERIFY(saveScreenshot(*window, "f08", QStringLiteral("labels-%1").arg(tag)));
    closeUp(3, QPointF(70, 120));
    QVERIFY(saveScreenshot(*window, "f08", QStringLiteral("labels-%1-closeup").arg(tag)));
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    closeUp(1.8, QPointF(175, 130));
    QVERIFY(saveScreenshot(*window, "f08", QStringLiteral("labels-%1-light").arg(tag)));
    closeUp(3, QPointF(70, 120));
    QVERIFY(saveScreenshot(*window, "f08", QStringLiteral("labels-%1-light-closeup").arg(tag)));
}

// With every spot on and beside its route taken by labels, a wire's label still covers none.
void TestSimulating::wireLabelStepsOutOfCoveredRoute()
{
    WireItem wire(QStringLiteral("w"));
    wire.setWire(QPolygonF({ QPointF(0, 0), QPointF(0, 40), QPointF(30, 40) }), Conductor::L, 1.5);
    wire.setCurrentText(QStringLiteral("0.04 A"));
    const QRectF taken(-30, -20, 90, 90);
    const QRectF placed = wire.placeCurrentLabel({ taken }, {}, {});
    QVERIFY(!placed.intersects(taken));
    QCOMPARE(wire.currentLabel()->sceneBoundingRect(), placed);
}

void TestSimulating::deviceLabelKeepsHandleRatingAndTerminalsClear()
{
    lightLamp1();
    const Project &project = window->project();
    QHash<QString, QRectF> shown;
    for (const char *id : { "d1", "d3" }) {
        const DeviceItem *item = deviceItem(id);
        const QRectF label = item->currentRect();
        QVERIFY2(!label.isEmpty(), id);
        const QRectF body = item->boundingRect().adjusted(0, DeviceItem::DesignationHeight, 0, 0);
        QVERIFY2(body.contains(label), id);
        QVERIFY2(!label.intersects(item->handleRect()), id);
        QVERIFY2(!label.intersects(pictureElement(id, QStringLiteral("label"))), id);
        const PlacedDevice *device = project.device(QString::fromLatin1(id));
        const DeviceVariant *variant
                = project.catalog().deviceType(device->type)->variant(device->poles);
        for (const TerminalSpec &terminal : variant->terminals) {
            const QRectF keepOut(terminal.position - QPointF(3.6, 3.6), QSizeF(7.2, 7.2));
            QVERIFY2(!label.intersects(keepOut), id);
        }
        shown.insert(QString::fromLatin1(id), label);
    }

    clickHandle("d3");
    clickHandle("d1");
    for (const char *id : { "d1", "d3" }) {
        checkOff(id);
        const DeviceItem *item = deviceItem(id);
        QVERIFY2(item->currentRect().isEmpty(), id); // no current through open contacts
        QVERIFY2(!shown.value(QString::fromLatin1(id)).intersects(item->handleRect()), id);
    }
}

void TestSimulating::showCurrentsTogglesAndPersists()
{
    lightLamp1();
    auto *viewMenu = named<QMenu>("viewMenu");
    auto *action = named<QAction>("showCurrentsAction");
    QVERIFY(viewMenu && action);
    QCOMPARE(viewMenu->actions().indexOf(action),
             viewMenu->actions().indexOf(named<QAction>("fitAction")) + 1);
    QCOMPARE(action->text(), QStringLiteral("Show currents"));
    QVERIFY(action->isCheckable());
    QVERIFY(action->isChecked());
    QVERIFY(scene->currentsShown());
    QCOMPARE(deviceItem("d1")->currentText(), QStringLiteral("0.04 A"));

    clickMenuBarItem(*window, viewMenu);
    clickMenuItem(viewMenu, action);
    QVERIFY(!action->isChecked());
    QVERIFY(!scene->currentsShown());
    for (const PlacedDevice &device : window->project().devices()) {
        QVERIFY2(deviceItem(device.id)->currentText().isEmpty(), qPrintable(device.id));
        QVERIFY2(deviceItem(device.id)->currentRect().isEmpty(), qPrintable(device.id));
    }
    for (const Wire &wire : window->project().wires()) {
        const auto *item = dynamic_cast<WireItem *>(sceneItem(scene, wire.id));
        QVERIFY2(item, qPrintable(wire.id));
        QVERIFY2(item->currentText().isEmpty(), qPrintable(wire.id));
        QVERIFY2(!item->currentLabel()->isVisible(), qPrintable(wire.id));
    }
    QVERIFY(!Settings::savedShowCurrents());
    checkLamp1Switches();
    closeUp(3, QPointF(70, 120));
    QVERIFY(saveScreenshot(*window, "f08", "currents-hidden"));

    {
        MainWindow another;
        const auto *anotherAction
                = another.findChild<QAction *>(QStringLiteral("showCurrentsAction"));
        QVERIFY(anotherAction);
        QVERIFY(!anotherAction->isChecked());
        const auto *anotherCanvas = another.findChild<CanvasView *>(QStringLiteral("canvasView"));
        QVERIFY(!qobject_cast<EditorScene *>(anotherCanvas->scene())->currentsShown());
    }

    // A menu doesn't trigger an item released where the cursor was when it opened, and the first
    // click left the cursor on this item.
    QTest::mouseMove(canvas->viewport());
    clickMenuBarItem(*window, viewMenu);
    clickMenuItem(viewMenu, action);
    QVERIFY(action->isChecked());
    QVERIFY(scene->currentsShown());
    QCOMPARE(deviceItem("d1")->currentText(), QStringLiteral("0.04 A"));
    QCOMPARE(wireItem({ "d3:2", "l1:L" })->currentText(), QStringLiteral("0.04 A"));
    QVERIFY(Settings::savedShowCurrents());
}

void TestSimulating::labelsFollowLanguage()
{
    lightLamp1();
    const WireEnds lampL{ "d3:2", "l1:L" };
    const QPointF deviceLabel = deviceItem("d1")->currentRect().center();
    const QPointF wireLabel = wireItem(lampL)->currentLabel()->pos();
    auto *languageMenu = named<QMenu>("languageMenu");

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_ru"));
    QCOMPARE(currentLanguage(), Language::Russian);
    QCOMPARE(deviceItem("d1")->currentText(), QStringLiteral("0,04 А"));
    QCOMPARE(wireItem(lampL)->currentText(), QStringLiteral("0,04 А"));
    QCOMPARE(named<QAction>("showCurrentsAction")->text(), QStringLiteral("Показывать токи"));
    checkLamp1Switches();
    QCOMPARE(deviceItem("d1")->currentRect().center(), deviceLabel);
    QCOMPARE(wireItem(lampL)->currentLabel()->pos(), wireLabel);
    closeUp(3, QPointF(70, 120));
    QCoreApplication::processEvents(); // the toolbar's relayout for the new texts is posted
    QVERIFY(saveScreenshot(*window, "f08", "labels-ru"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_kk"));
    QCOMPARE(currentLanguage(), Language::Kazakh);
    QCOMPARE(deviceItem("d1")->currentText(), QStringLiteral("0,04 А"));
    QCOMPARE(wireItem(lampL)->currentText(), QStringLiteral("0,04 А"));
    QCOMPARE(named<QAction>("showCurrentsAction")->text(), QStringLiteral("Токтарды көрсету"));
    checkLamp1Switches();
    QCOMPARE(deviceItem("d1")->currentRect().center(), deviceLabel);
    QCOMPARE(wireItem(lampL)->currentLabel()->pos(), wireLabel);
    closeUp(3, QPointF(70, 120));
    QCoreApplication::processEvents(); // the toolbar's relayout for the new texts is posted
    QVERIFY(saveScreenshot(*window, "f08", "labels-kk"));
}

void TestSimulating::labelsInLightTheme()
{
    lightLamp1();
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QCOMPARE(deviceItem("d1")->currentText(), QStringLiteral("0.04 A"));
    QVERIFY(wireItem({ "d3:2", "l1:L" })->currentLabel()->isVisible());
    QVERIFY(saveScreenshot(*window, "f08", "labels-light"));
    closeUp(3, QPointF(70, 120));
    QVERIFY(saveScreenshot(*window, "f08", "labels-light-closeup"));
    closeUp(3, QPointF(60, 260));
    QVERIFY(saveScreenshot(*window, "f08", "labels-light-lamp-closeup"));
}

void TestSimulating::shortShowsWholeAmperes()
{
    Project &project = window->project();
    const WireEnds shortEnds{ "d3:2", "nbar:5" };
    const QString from = QString::fromLatin1(shortEnds.from);
    const QString to = QString::fromLatin1(shortEnds.to);
    Wire wire{ {}, from, to, 1.5, project.conductorFor(from, to) };
    wire.length = 0.3;
    QVERIFY(!project.addWire(wire).isEmpty());
    for (const char *id : { "d1", "d2", "d3" })
        clickHandle(id);
    QCOMPARE(deviceItem("d3")->currentText(), QStringLiteral("557 A"));
    QCOMPARE(wireItem(shortEnds)->currentText(), QStringLiteral("557 A"));
    QVERIFY(saveScreenshot(*window, "f08", "labels-short"));
}

void TestSimulating::roadmapScenario()
{
    addTutorialLoads(window->project());
    zoomOut();
    const Simulation &simulation = window->simulation();
    const auto clickButton = [this](const QString &id) {
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onButton(id));
    };
    const auto switchLanguage = [this](const char *action) {
        auto *languageMenu = named<QMenu>("languageMenu");
        clickMenuBarItem(*window, languageMenu);
        clickMenuItem(languageMenu, named<QAction>(action));
    };

    for (const char *id : { "d1", "d2", "d3" })
        clickHandle(id);
    for (const char *id : { "d1", "d2", "d3" }) {
        checkOn(id);
        // only the solver's Gmin leakage flows, far below what a label shows
        QVERIFY2(!showsCurrent(simulation.deviceCurrent(QString::fromLatin1(id))), id);
        QCOMPARE(deviceItem(id)->currentText(), QString());
    }
    QVERIFY(saveScreenshot(*window, "f08", "scenario-1-breakers"));

    clickButton("l1");
    QVERIFY(qAbs(simulation.deviceCurrent(u"d3") - 0.043472) < 0.0001);
    QVERIFY(simulation.isEnergized(u"l1"));
    QVERIFY(loadItem("l1")->isRunning());
    for (const char *id : { "d1", "d2", "d3" })
        QCOMPARE(deviceItem(id)->currentText(), QStringLiteral("0.04 A"));
    for (const WireEnds &ends : kLampWires)
        QCOMPARE(wireItem(ends)->currentText(), QStringLiteral("0.04 A"));
    for (const WireEnds &ends : kIdleWires)
        QCOMPARE(wireItem(ends)->currentText(), QString());
    QVERIFY(saveScreenshot(*window, "f08", "scenario-2-lamp"));

    clickHandle("d3");
    checkOff("d3");
    QVERIFY(!simulation.isEnergized(u"l1"));
    QVERIFY(!loadItem("l1")->isRunning());
    QCOMPARE(deviceItem("d3")->currentText(), QString());
    for (const WireEnds &ends : { WireEnds{ "d3:2", "l1:L" }, WireEnds{ "nbar:2", "l1:N" } })
        QCOMPARE(wireItem(ends)->currentText(), QString());
    QVERIFY(saveScreenshot(*window, "f08", "scenario-3-off"));

    clickHandle("d5");
    clickButton("l9");
    clickButton("l10");
    QVERIFY(qAbs(simulation.deviceCurrent(u"d5") - 13.409) < 0.005);
    QVERIFY(loadItem("l9")->isRunning());
    QVERIFY(loadItem("l10")->isRunning());
    QCOMPARE(deviceItem("d5")->currentText(), QStringLiteral("13.4 A"));
    QVERIFY(saveScreenshot(*window, "f08", "scenario-4-kitchen"));

    mouseDrag(canvas->viewport(), onPicture("l9"), viewportPoint(canvas, QPointF(500, 300)));
    QVERIFY(window->project().load(u"l9")->socket.isEmpty());
    QVERIFY(!loadItem("l9")->isRunning());
    QVERIFY(loadItem("l10")->isRunning());
    QVERIFY(qAbs(simulation.deviceCurrent(u"d5") - 5.1449) < 0.005);
    QCOMPARE(deviceItem("d5")->currentText(), QStringLiteral("5.14 A"));
    QVERIFY(saveScreenshot(*window, "f08", "scenario-5-unplugged"));

    switchLanguage("languageAction_ru");
    QCOMPARE(deviceItem("d5")->currentText(), QStringLiteral("5,14 А"));
    QCoreApplication::processEvents(); // the toolbar's relayout for the new texts is posted
    QVERIFY(saveScreenshot(*window, "f08", "scenario-ru"));
    switchLanguage("languageAction_kk");
    QCOMPARE(deviceItem("d5")->currentText(), QStringLiteral("5,14 А"));
    QCoreApplication::processEvents(); // the toolbar's relayout for the new texts is posted
    QVERIFY(saveScreenshot(*window, "f08", "scenario-kk"));
}

QTEST_MAIN(TestSimulating)
#include "tst_simulating.moc"
