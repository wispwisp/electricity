// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
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
#include "screenshot.h"
#include "testsettings.h"

#include <QAction>
#include <QApplication>
#include <QSvgRenderer>
#include <QTest>
#include <QToolBar>
#include <QUndoStack>

#include <memory>

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

QTEST_MAIN(TestSimulating)
#include "tst_simulating.moc"
