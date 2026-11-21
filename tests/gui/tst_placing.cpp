// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "core/project.h"
#include "editorhelpers.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QApplication>
#include <QDrag>
#include <QJsonObject>
#include <QMimeData>
#include <QStatusBar>
#include <QTest>
#include <QUndoStack>

#include <memory>
#include <utility>

namespace {
const QString kOverlapHint = QStringLiteral("No room here: another device is in the way.");
const QString kRailHint = QStringLiteral("Devices go on a DIN rail inside the enclosure.");

QString addPreset(Project &project, const QString &presetId, int row, int module)
{
    return project.addDevice(deviceFromPreset(preset(presetId), row, module));
}

DeviceItem *deviceItem(EditorScene *scene, const QString &id)
{
    return dynamic_cast<DeviceItem *>(sceneItem(scene, id));
}

// A point on the enclosure interior below the rail, right of the bars, away from any device.
const QPointF kEnclosureInterior(240, 190);
} // namespace

class TestPlacing : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void paletteProvidesPresetDragData();
    void dropsPresetsOntoTheRail();
    void previewShowsWhereTheDeviceLands();
    void refusesOverlappingDrop();
    void refusesDropAwayFromTheRail();
    void snapsToRailEnds();
    void movesDeviceByMouse();
    void clickSelectsWithoutMoving();
    void spaceDragPansInsteadOfMoving();
    void quickSecondDragStillMoves();
    void otherButtonsDontBreakAMove();
    void closesWhileSelectedOrDragging();

private:
    // the viewport point over the centre of a device body at that slot
    [[nodiscard]] QPoint at(int row, int module, int modules) const;
    [[nodiscard]] QString message() const;
    [[nodiscard]] std::pair<int, int> position(const QString &id) const;

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
    PaletteWidget *palette = nullptr;
};

QPoint TestPlacing::at(int row, int module, int modules) const
{
    return viewportPoint(canvas, scene->slotRect(row, module, modules).center());
}

QString TestPlacing::message() const
{
    return window->statusBar()->currentMessage();
}

std::pair<int, int> TestPlacing::position(const QString &id) const
{
    const PlacedDevice *device = window->project().device(id);
    return device ? std::pair(device->row, device->module) : std::pair(-1, -1);
}

void TestPlacing::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestPlacing::init()
{
    window = std::make_unique<MainWindow>();
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    canvas = window->findChild<CanvasView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas);
    scene = qobject_cast<EditorScene *>(canvas->scene());
    QVERIFY(scene);
    palette = window->findChild<PaletteWidget *>(QStringLiteral("paletteTree"));
    QVERIFY(palette);
}

void TestPlacing::cleanup()
{
    window.reset();
}

void TestPlacing::paletteProvidesPresetDragData()
{
    QVERIFY(palette->mimeTypes().contains(PaletteWidget::PresetMimeType));
    QTreeWidgetItem *item = presetItem(palette, "breaker-2p-c40");
    QVERIFY(item);
    const std::unique_ptr<QMimeData> data(palette->mimeData({ item }));
    QVERIFY(data);
    QVERIFY(data->hasFormat(PaletteWidget::PresetMimeType));
    QCOMPARE(data->data(PaletteWidget::PresetMimeType), QByteArray("breaker-2p-c40"));

    // A press and a short move don't start a drag; moving beyond the start distance does. The
    // offscreen platform ends the drag at once, and Qt deletes the QDrag later.
    QWidget *viewport = palette->viewport();
    const QPoint start = palette->visualItemRect(item).center();
    QTest::mousePress(viewport, Qt::LeftButton, {}, start);
    QTest::mouseMove(viewport, start + QPoint(QApplication::startDragDistance() - 2, 0));
    QVERIFY(!palette->findChild<QDrag *>());
    QTest::mouseMove(viewport, start + QPoint(QApplication::startDragDistance() + 10, 0));
    auto *drag = palette->findChild<QDrag *>();
    QVERIFY(drag);
    QCOMPARE(drag->mimeData()->data(PaletteWidget::PresetMimeType), QByteArray("breaker-2p-c40"));
    QCOMPARE(drag->supportedActions(), Qt::CopyAction);
    QVERIFY(!drag->pixmap().isNull());
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, start + QPoint(30, 0));
    QVERIFY(window->project().devices().isEmpty());
}

void TestPlacing::dropsPresetsOntoTheRail()
{
    // roadmap scenario: "Breaker 2P C40" and "RCD 40/30" into the enclosure
    dropPreset(canvas, palette, "breaker-2p-c40", at(0, 0, 2));
    dropPreset(canvas, palette, "rcd-2p-40-30", at(0, 2, 2));
    const QList<PlacedDevice> &devices = window->project().devices();
    QCOMPARE(devices.size(), 2);
    QCOMPARE(devices[0].type, QStringLiteral("breaker"));
    QCOMPARE(devices[0].poles, QStringLiteral("2P"));
    QCOMPARE(devices[0].parameters, (QJsonObject{ { "curve", "C" }, { "ratedCurrent", 40 } }));
    QCOMPARE(std::pair(devices[0].row, devices[0].module), std::pair(0, 0));
    QCOMPARE(std::pair(devices[1].row, devices[1].module), std::pair(0, 2));
    QCOMPARE(window->project().designation(devices[0].id), QStringLiteral("QF1"));
    QCOMPARE(window->project().designation(devices[1].id), QStringLiteral("QD1"));
    QCOMPARE(scene->selectedDeviceId(), devices[1].id);
    QCOMPARE(window->undoStack().count(), 2);
    QCOMPARE(countItems(scene, "preview"), 0);
    QVERIFY(saveScreenshot(*window, "f05", "placed"));
}

void TestPlacing::previewShowsWhereTheDeviceLands()
{
    addPreset(window->project(), "breaker-2p-c40", 0, 0);
    addPreset(window->project(), "rcd-2p-40-30", 0, 2);

    dragPresetOver(canvas, palette, "breaker-1p-c16", at(0, 4, 1));
    QCOMPARE(countItems(scene, "preview"), 1);
    DeviceItem *preview = deviceItem(scene, "preview");
    QVERIFY(preview);
    QVERIFY(preview->isVisible());
    QCOMPARE(preview->opacity(), 0.6);
    QCOMPARE(preview->pos(), scene->slotRect(0, 4, 1).topLeft());
    QCOMPARE(preview->rating(), QStringLiteral("C16"));
    QVERIFY(!preview->isRefused());
    QVERIFY(saveScreenshot(*window, "f05", "preview"));

    dragPresetOver(canvas, palette, "breaker-1p-c16", at(0, 1, 1));
    QCOMPARE(countItems(scene, "preview"), 1);
    preview = deviceItem(scene, "preview");
    QVERIFY(preview->isVisible());
    QCOMPARE(preview->pos(), scene->slotRect(0, 1, 1).topLeft());
    QVERIFY(preview->isRefused());
    QVERIFY(saveScreenshot(*window, "f05", "preview-refused"));

    dragPresetOver(canvas, palette, "breaker-1p-c16",
                   viewportPoint(canvas, sceneItem(scene, "supply")->sceneBoundingRect().center()));
    preview = deviceItem(scene, "preview");
    QVERIFY(preview);
    QVERIFY(!preview->isVisible());

    leaveDrag(canvas);
    QCOMPARE(countItems(scene, "preview"), 0);
    QCOMPARE(window->project().devices().size(), 2);
}

void TestPlacing::refusesOverlappingDrop()
{
    addPreset(window->project(), "breaker-2p-c40", 0, 0);
    dropPreset(canvas, palette, "breaker-1p-c16", at(0, 1, 1));
    QCOMPARE(window->project().devices().size(), 1);
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(countItems(scene, "preview"), 0);
    QCOMPARE(message(), kOverlapHint);
    QTRY_COMPARE_WITH_TIMEOUT(message(), QStringLiteral("Ready"), 7000);
}

void TestPlacing::refusesDropAwayFromTheRail()
{
    dropPreset(canvas, palette, "breaker-1p-c16",
               viewportPoint(canvas, sceneItem(scene, "supply")->sceneBoundingRect().center()));
    QVERIFY(window->project().devices().isEmpty());
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(countItems(scene, "preview"), 0);
    QCOMPARE(message(), kRailHint);
}

void TestPlacing::snapsToRailEnds()
{
    dropPreset(canvas, palette, "breaker-2p-c40", at(0, 0, 1));
    QCOMPARE(window->project().devices().size(), 1);
    QCOMPARE(window->project().devices()[0].module, 0);
    dropPreset(canvas, palette, "breaker-2p-c40", at(0, 11, 1));
    QCOMPARE(window->project().devices().size(), 2);
    QCOMPARE(window->project().devices()[1].module, 10);
}

void TestPlacing::movesDeviceByMouse()
{
    Project &project = window->project();
    addPreset(project, "breaker-2p-c40", 0, 0);
    addPreset(project, "rcd-2p-40-30", 0, 2);
    const QString qf = addPreset(project, "breaker-1p-c10", 0, 6);
    QWidget *viewport = canvas->viewport();
    DeviceItem *item = deviceItem(scene, qf);
    QVERIFY(item);

    mouseDrag(viewport, at(0, 6, 1), at(0, 8, 1));
    QCOMPARE(position(qf), std::pair(0, 8));
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(item->pos(), scene->slotRect(0, 8, 1).topLeft());
    QCOMPARE(scene->selectedDeviceId(), qf);
    QCOMPARE(item->designation(), QStringLiteral("QF2"));
    QVERIFY(saveScreenshot(*window, "f05", "moved"));

    // Over another device it snaps, tinted; released there, it goes back.
    QTest::mousePress(viewport, Qt::LeftButton, {}, at(0, 8, 1));
    QTest::mouseMove(viewport, at(0, 3, 1));
    QCOMPARE(item->pos(), scene->slotRect(0, 3, 1).topLeft());
    QVERIFY(item->isRefused());
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, at(0, 3, 1));
    QCOMPARE(position(qf), std::pair(0, 8));
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(item->pos(), scene->slotRect(0, 8, 1).topLeft());
    QVERIFY(!item->isRefused());
    QCOMPARE(message(), kOverlapHint);

    // Released on its own slot: 7 units right of the centre is still inside the module, yet far
    // enough to start a move.
    const QPoint centre = at(0, 8, 1);
    const QPoint right = viewportPoint(canvas, scene->slotRect(0, 8, 1).center() + QPointF(7, 0));
    QVERIFY((right - centre).manhattanLength() > QApplication::startDragDistance());
    window->statusBar()->clearMessage();
    mouseDrag(viewport, centre, right);
    QCOMPARE(position(qf), std::pair(0, 8));
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(item->pos(), scene->slotRect(0, 8, 1).topLeft());
    QCOMPARE(message(), QStringLiteral("Ready"));

    // Away from the rails it follows the cursor freely, tinted, keeping the grab offset.
    const QRectF enclosure = sceneItem(scene, "enclosure")->sceneBoundingRect();
    const QPoint above
            = viewportPoint(canvas, QPointF(enclosure.center().x(), enclosure.top() - 6));
    const QPointF grab = canvas->mapToScene(at(0, 8, 1)) - scene->slotRect(0, 8, 1).center();
    QTest::mousePress(viewport, Qt::LeftButton, {}, at(0, 8, 1));
    QTest::mouseMove(viewport, above);
    const QPointF bodyCentre = item->pos() + QPointF(9, 45);
    QVERIFY(QLineF(bodyCentre, canvas->mapToScene(above) - grab).length() < 0.01);
    QVERIFY(item->isRefused());
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, above);
    QCOMPARE(position(qf), std::pair(0, 8));
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(item->pos(), scene->slotRect(0, 8, 1).topLeft());
    QVERIFY(!item->isRefused());
    QCOMPARE(message(), kRailHint);

    QVERIFY(project.setEnclosure(QStringLiteral("enclosure-24")));
    mouseDrag(viewport, at(0, 8, 1), at(1, 3, 1));
    QCOMPARE(position(qf), std::pair(1, 3));
    QCOMPARE(window->undoStack().count(), 2);
    QCOMPARE(item->pos(), scene->slotRect(1, 3, 1).topLeft());
    QCOMPARE(item->designation(), QStringLiteral("QF2"));
    QVERIFY(saveScreenshot(*window, "f05", "moved-row1"));
}

void TestPlacing::clickSelectsWithoutMoving()
{
    const QString qf = addPreset(window->project(), "breaker-1p-c10", 0, 6);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 6, 1));
    QCOMPARE(scene->selectedDeviceId(), qf);
    QCOMPARE(position(qf), std::pair(0, 6));
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(deviceItem(scene, qf)->pos(), scene->slotRect(0, 6, 1).topLeft());
}

void TestPlacing::spaceDragPansInsteadOfMoving()
{
    const QString qf = addPreset(window->project(), "breaker-1p-c10", 0, 6);
    QWidget *viewport = canvas->viewport();
    const QPoint centre = viewport->rect().center();
    const QPointF before = canvas->mapToScene(centre);
    QTest::keyPress(canvas, Qt::Key_Space);
    mouseDrag(viewport, at(0, 6, 1), at(0, 9, 1));
    QTest::keyRelease(canvas, Qt::Key_Space);
    QCOMPARE(position(qf), std::pair(0, 6));
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(deviceItem(scene, qf)->pos(), scene->slotRect(0, 6, 1).topLeft());
    QVERIFY(canvas->mapToScene(centre).x() < before.x());
}

// The press of a drag right after a click on the device arrives as a double click.
void TestPlacing::quickSecondDragStillMoves()
{
    const QString qf = addPreset(window->project(), "breaker-1p-c10", 0, 6);
    QWidget *viewport = canvas->viewport();
    QTest::mouseClick(viewport, Qt::LeftButton, {}, at(0, 6, 1));
    QTest::mouseDClick(viewport, Qt::LeftButton, {}, at(0, 6, 1));
    QTest::mouseMove(viewport, at(0, 9, 1));
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, at(0, 9, 1));
    QCOMPARE(position(qf), std::pair(0, 9));
    QCOMPARE(window->undoStack().count(), 1);
}

void TestPlacing::otherButtonsDontBreakAMove()
{
    const QString qf = addPreset(window->project(), "breaker-1p-c10", 0, 6);
    QWidget *viewport = canvas->viewport();
    const QPoint p6 = at(0, 6, 1);
    const QPoint p7 = at(0, 7, 1);
    const QPoint p8 = at(0, 8, 1);
    const QPoint p9 = at(0, 9, 1);
    const QPoint interior = viewportPoint(canvas, kEnclosureInterior);

    // The middle button goes down and up mid-move: no pan, and the device keeps following.
    QTest::mousePress(viewport, Qt::LeftButton, {}, p6);
    QTest::mouseMove(viewport, p7);
    QTest::mousePress(viewport, Qt::MiddleButton, {}, p7);
    QTest::mouseMove(viewport, p8);
    QTest::mouseRelease(viewport, Qt::MiddleButton, {}, p8);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, p8);
    QCOMPARE(position(qf), std::pair(0, 8));
    QTest::mouseClick(viewport, Qt::LeftButton, {}, interior);
    QVERIFY(scene->isEnclosureSelected());

    // The left button goes up before the middle one: the move still ends.
    QTest::mousePress(viewport, Qt::LeftButton, {}, p8);
    QTest::mouseMove(viewport, p9);
    QTest::mousePress(viewport, Qt::MiddleButton, {}, p9);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, p9);
    QTest::mouseRelease(viewport, Qt::MiddleButton, {}, p9);
    QCOMPARE(position(qf), std::pair(0, 9));
    QTest::mouseClick(viewport, Qt::LeftButton, {}, interior);
    QVERIFY(scene->isEnclosureSelected());
    QCOMPARE(window->undoStack().count(), 2);
}

void TestPlacing::closesWhileSelectedOrDragging()
{
    const QString qf = addPreset(window->project(), "breaker-1p-c10", 0, 6);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 6, 1));
    QCOMPARE(scene->selectedDeviceId(), qf);
    // The scene refers to the project, so it goes first.
    QStringList destroyed;
    connect(scene, &QObject::destroyed, this, [&] { destroyed << QStringLiteral("scene"); });
    connect(&window->project(), &QObject::destroyed, this,
            [&] { destroyed << QStringLiteral("project"); });
    window.reset();
    QCOMPARE(destroyed, (QStringList{ "scene", "project" }));

    init();
    addPreset(window->project(), "breaker-1p-c10", 0, 6);
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, at(0, 6, 1));
    QTest::mouseMove(canvas->viewport(), at(0, 9, 1));
    window.reset();
    // QTest still counts the left button as held; a release on a fresh window balances it.
    init();
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, at(0, 9, 1));

    dragPresetOver(canvas, palette, "breaker-1p-c16", at(0, 4, 1));
    QCOMPARE(countItems(scene, "preview"), 1);
    window.reset();
}

QTEST_MAIN(TestPlacing)
#include "tst_placing.moc"
