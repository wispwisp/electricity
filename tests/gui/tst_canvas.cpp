// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/mainwindow.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "app/theme.h"
#include "core/project.h"
#include "editorhelpers.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QPainter>
#include <QTest>

namespace {
CanvasView *canvasOf(MainWindow &window)
{
    return window.findChild<CanvasView *>(QStringLiteral("canvasView"));
}

EditorScene *sceneOf(MainWindow &window)
{
    CanvasView *canvas = canvasOf(window);
    return canvas ? qobject_cast<EditorScene *>(canvas->scene()) : nullptr;
}

DeviceItem *deviceItem(EditorScene *scene, const QString &id)
{
    return dynamic_cast<DeviceItem *>(sceneItem(scene, id));
}

QString addPreset(Project &project, const QString &presetId, int row, int module)
{
    return project.addDevice(deviceFromPreset(preset(presetId), row, module));
}

// A point on the enclosure interior below the rail, right of the bars, away from any device.
const QPointF kEnclosureInterior(240, 190);
} // namespace

class TestCanvas : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void startsWithSupplyAndEmptyEnclosure();
    void showsDevicesAtTheirSlots();
    void followsDeviceUpdatesAndRemovals();
    void enclosureSizeChangeRelayoutsAndRefits();
    void staysFittedOnResizeUntilTheUserZooms();
    void zoomsAndPans();
    void quickSecondPanStillPans();
    void selectsOneThingAtATime();
    void followsLightTheme();
};

void TestCanvas::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestCanvas::startsWithSupplyAndEmptyEnclosure()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);

    QCOMPARE(countItems(scene, "supply"), 1);
    QCOMPARE(countItems(scene, "enclosure"), 1);
    QCOMPARE(countItems(scene, "nbar"), 8);
    QCOMPARE(countItems(scene, "pebar"), 8);
    QVERIFY(window.project().devices().isEmpty());
    for (QGraphicsItem *item : scene->items())
        QVERIFY(!dynamic_cast<DeviceItem *>(item));

    const QRectF enclosure = sceneItem(scene, "enclosure")->sceneBoundingRect();
    const QRectF supply = sceneItem(scene, "supply")->sceneBoundingRect();
    QCOMPARE(supply.bottom(), enclosure.top() - 12);
    QCOMPARE(supply.left(), scene->slotRect(0, 0, 1).left());
    QVERIFY(scene->panelRect().contains(enclosure.united(supply)));

    QVERIFY(fullyVisible(canvas, scene->panelRect()));
    const QRect shown = canvas->mapFromScene(enclosure).boundingRect();
    const QSize viewport = canvas->viewport()->size();
    QVERIFY(shown.width() >= 0.4 * viewport.width() || shown.height() >= 0.6 * viewport.height());
    QVERIFY(saveScreenshot(window, "f05", "start"));
}

void TestCanvas::showsDevicesAtTheirSlots()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);

    Project &project = window.project();
    const QString qf1 = addPreset(project, "breaker-2p-c40", 0, 0);
    const QString qd1 = addPreset(project, "rcd-2p-40-30", 0, 2);
    const QString qf2 = addPreset(project, "breaker-1p-c10", 0, 4);
    struct Expected
    {
        QString id;
        int module, width;
        QString designation, rating;
    };
    for (const Expected &e :
         { Expected{ qf1, 0, 2, "QF1", "C40" }, Expected{ qd1, 2, 2, "QD1", "40 A" },
           Expected{ qf2, 4, 1, "QF2", "C10" } }) {
        DeviceItem *item = deviceItem(scene, e.id);
        QVERIFY2(item, qPrintable(e.id));
        QCOMPARE(item->pos(), scene->slotRect(0, e.module, e.width).topLeft());
        QCOMPARE(item->designation(), e.designation);
        QCOMPARE(item->rating(), e.rating);
    }
    // enclosure-12.svg: "Row 1: devices span x 32-248, y 48-138"
    QCOMPARE(scene->slotRect(0, 0, 1), QRectF(32, 48, 18, 90));
    QVERIFY(saveScreenshot(window, "f05", "devices"));

    const QRectF panel = scene->panelRect();
    QPixmap closeup((panel.size() * 4).toSize());
    closeup.fill(canvas->palette().color(QPalette::Base));
    QPainter painter(&closeup);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    scene->render(&painter, QRectF(), panel);
    painter.end();
    QVERIFY(saveScreenshot(closeup, "f05", "devices-closeup"));
}

void TestCanvas::followsDeviceUpdatesAndRemovals()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);

    Project &project = window.project();
    const QString first = addPreset(project, "breaker-1p-c10", 0, 4);
    const QString second = addPreset(project, "breaker-1p-c16", 0, 6);
    QCOMPARE(deviceItem(scene, second)->designation(), QStringLiteral("QF2"));

    PlacedDevice moved = *project.device(first);
    moved.module = 8;
    moved.parameters[QStringLiteral("ratedCurrent")] = 25;
    QVERIFY(project.updateDevice(moved));
    QCOMPARE(countItems(scene, first), 1);
    QCOMPARE(deviceItem(scene, first)->pos(), scene->slotRect(0, 8, 1).topLeft());
    QCOMPARE(deviceItem(scene, first)->rating(), QStringLiteral("C25"));
    QCOMPARE(deviceItem(scene, first)->designation(), QStringLiteral("QF2"));
    QCOMPARE(deviceItem(scene, second)->designation(), QStringLiteral("QF1"));

    PlacedDevice wider = *project.device(second);
    wider.poles = QStringLiteral("2P");
    QVERIFY(project.updateDevice(wider));
    QCOMPARE(deviceItem(scene, second)->boundingRect().width(), 36.0);

    deviceItem(scene, second)->setSelected(true);
    QVERIFY(project.removeDevice(second));
    QCOMPARE(countItems(scene, second), 0);
    QCOMPARE(scene->selectedDeviceId(), QString());
    QCOMPARE(deviceItem(scene, first)->designation(), QStringLiteral("QF1"));
}

void TestCanvas::enclosureSizeChangeRelayoutsAndRefits()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);
    QWidget *viewport = canvas->viewport();

    QTest::mouseClick(viewport, Qt::LeftButton, {}, viewportPoint(canvas, kEnclosureInterior));
    QVERIFY(scene->isEnclosureSelected());
    for (int i = 0; i < 3; ++i)
        wheel(viewport, viewport->rect().center(), 120);
    QVERIFY(!fullyVisible(canvas, scene->panelRect()));

    QVERIFY(window.project().setEnclosure(QStringLiteral("enclosure-24")));
    QCOMPARE(countItems(scene, "enclosure"), 1);
    QCOMPARE(countItems(scene, "nbar"), 12);
    QCOMPARE(countItems(scene, "pebar"), 12);
    QVERIFY(scene->slotRect(1, 0, 1).top() > scene->slotRect(0, 0, 1).bottom());
    QVERIFY(fullyVisible(canvas, scene->panelRect()));
    QVERIFY(scene->isEnclosureSelected());
    QVERIFY(saveScreenshot(window, "f05", "enclosure-24"));
}

void TestCanvas::staysFittedOnResizeUntilTheUserZooms()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);
    const qreal fitted = canvas->transform().m11();

    // A small screen shows the window at its default size, then maximizes it.
    window.resize(900, 560);
    QTRY_COMPARE(window.size(), QSize(900, 560));
    QVERIFY(fullyVisible(canvas, scene->panelRect()));
    QVERIFY(canvas->transform().m11() < fitted);

    wheel(canvas->viewport(), canvas->viewport()->rect().center(), 120);
    const qreal zoomed = canvas->transform().m11();
    window.resize(1280, 800);
    QTRY_COMPARE(window.size(), QSize(1280, 800));
    QCOMPARE(canvas->transform().m11(), zoomed);
}

void TestCanvas::zoomsAndPans()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);
    QWidget *viewport = canvas->viewport();
    const QPoint centre = viewport->rect().center();

    // One notch zooms around the cursor: the scene point under it stays put.
    const qreal fitted = canvas->transform().m11();
    const QPoint cursor = centre + QPoint(120, 60);
    const QPointF underCursor = canvas->mapToScene(cursor);
    wheel(viewport, cursor, 120);
    QCOMPARE(canvas->transform().m11(), fitted * 1.15);
    QVERIFY(QLineF(canvas->mapToScene(cursor), underCursor).length() < 1);

    for (int i = 0; i < 40; ++i)
        wheel(viewport, centre, 120);
    QVERIFY(canvas->transform().m11() <= 12);
    for (int i = 0; i < 40; ++i)
        wheel(viewport, centre, -120);
    QVERIFY(canvas->transform().m11() >= 0.5);

    canvas->fitToPanel(); // room to pan in every direction
    const qreal scale = canvas->transform().m11();
    QPointF before = canvas->mapToScene(centre);
    mouseDrag(viewport, centre, centre + QPoint(100, 0), Qt::MiddleButton);
    QVERIFY(qAbs(canvas->mapToScene(centre).x() - (before.x() - 100 / scale)) < 1);
    QVERIFY(qAbs(canvas->mapToScene(centre).y() - before.y()) < 1);

    // The view centre is now over the enclosure; a Space drag pans instead of selecting it.
    before = canvas->mapToScene(centre);
    QTest::keyPress(canvas, Qt::Key_Space);
    mouseDrag(viewport, centre, centre + QPoint(100, 0));
    QTest::keyRelease(canvas, Qt::Key_Space);
    QVERIFY(qAbs(canvas->mapToScene(centre).x() - (before.x() - 100 / scale)) < 1);
    QVERIFY(scene->selectedItems().isEmpty());

    for (int i = 0; i < 5; ++i)
        wheel(viewport, centre, 120);
    QVERIFY(!fullyVisible(canvas, scene->panelRect()));
    canvas->fitToPanel();
    QVERIFY(fullyVisible(canvas, scene->panelRect()));
}

void TestCanvas::quickSecondPanStillPans()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);
    QWidget *viewport = canvas->viewport();
    const QPoint centre = viewport->rect().center();
    const QPoint right = centre + QPoint(100, 0);
    const qreal scale = canvas->transform().m11();
    QTest::mouseClick(viewport, Qt::LeftButton, {}, viewportPoint(canvas, kEnclosureInterior));
    QVERIFY(scene->isEnclosureSelected());

    // The press of a second drag right after the first one arrives as a double click.
    for (Qt::MouseButton button : { Qt::MiddleButton, Qt::LeftButton }) {
        if (button == Qt::LeftButton)
            QTest::keyPress(canvas, Qt::Key_Space);
        mouseDrag(viewport, centre, right, button);
        const QPointF before = canvas->mapToScene(centre);
        QTest::mouseDClick(viewport, button, {}, centre);
        QTest::mouseMove(viewport, right);
        QTest::mouseRelease(viewport, button, {}, right);
        if (button == Qt::LeftButton)
            QTest::keyRelease(canvas, Qt::Key_Space);
        QVERIFY(qAbs(canvas->mapToScene(centre).x() - (before.x() - 100 / scale)) < 1);
        QVERIFY(scene->isEnclosureSelected());
    }
}

void TestCanvas::selectsOneThingAtATime()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    EditorScene *scene = sceneOf(window);
    QVERIFY(scene);
    const QString qf1 = addPreset(window.project(), "breaker-2p-c40", 0, 0);
    const QString qd1 = addPreset(window.project(), "rcd-2p-40-30", 0, 2);
    const auto click = [&](QPointF scenePos, Qt::KeyboardModifiers modifiers = {}) {
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, modifiers,
                          viewportPoint(canvas, scenePos));
    };

    click(scene->slotRect(0, 0, 2).center());
    QCOMPARE(scene->selectedDeviceId(), qf1);
    QVERIFY(saveScreenshot(window, "f05", "device-selected"));

    click(kEnclosureInterior);
    QVERIFY(scene->isEnclosureSelected());
    QCOMPARE(scene->selectedDeviceId(), QString());

    click(scene->slotRect(0, 2, 2).center(), Qt::ControlModifier);
    QCOMPARE(scene->selectedItems().size(), 1);
    QCOMPARE(scene->selectedItems().first(), sceneItem(scene, qd1));
    click(scene->slotRect(0, 0, 2).center(), Qt::ShiftModifier);
    QCOMPARE(scene->selectedItems().size(), 1);
    QCOMPARE(scene->selectedDeviceId(), qf1);

    click(sceneItem(scene, "nbar")->sceneBoundingRect().center());
    QVERIFY(scene->isEnclosureSelected());
    QCOMPARE(scene->selectedItems().size(), 1);

    click(QPointF(-60, 120)); // left of the enclosure
    QVERIFY(scene->selectedItems().isEmpty());

    click(scene->slotRect(0, 0, 2).center());
    QCOMPARE(scene->selectedDeviceId(), qf1);
    click(sceneItem(scene, "supply")->sceneBoundingRect().center());
    QVERIFY(scene->selectedItems().isEmpty());
}

void TestCanvas::followsLightTheme()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    CanvasView *canvas = canvasOf(window);
    QVERIFY(sceneOf(window));
    addPreset(window.project(), "breaker-2p-c40", 0, 0);
    addPreset(window.project(), "rcd-2p-40-30", 0, 2);
    addPreset(window.project(), "breaker-1p-c10", 0, 4);

    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QVERIFY(canvas->grab().toImage().pixelColor(5, 5).lightness() > 200);
    QVERIFY(saveScreenshot(window, "f05", "light"));
    Theme::apply(Theme::Mode::Dark);
}

QTEST_MAIN(TestCanvas)
#include "tst_canvas.moc"
