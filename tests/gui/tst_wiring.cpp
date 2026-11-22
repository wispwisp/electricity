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

#include <QTest>

#include <algorithm>
#include <functional>
#include <memory>

namespace {
bool orthogonal(const QPolygonF &route)
{
    for (qsizetype i = 1; i < route.size(); ++i) {
        if (route[i].x() != route[i - 1].x() && route[i].y() != route[i - 1].y())
            return false;
    }
    return route.size() >= 2;
}

// For orthogonal routes, where a segment's bounding rect is the segment itself. The first and the
// last segment are the stubs into the route's own terminals and are skipped.
bool crossesInside(const QPolygonF &route, const QRectF &rect)
{
    const QRectF inside = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    for (qsizetype i = 2; i + 1 < route.size(); ++i) {
        const QRectF segment = QRectF(route[i - 1], route[i]).normalized();
        if (segment.left() < inside.right() && segment.right() > inside.left()
            && segment.top() < inside.bottom() && segment.bottom() > inside.top())
            return true;
    }
    return false;
}

// Any segment, stubs included.
bool touches(const QPolygonF &route, const QRectF &rect)
{
    for (qsizetype i = 1; i < route.size(); ++i) {
        const QRectF segment = QRectF(route[i - 1], route[i]).normalized();
        if (segment.left() <= rect.right() && segment.right() >= rect.left()
            && segment.top() <= rect.bottom() && segment.bottom() >= rect.top())
            return true;
    }
    return false;
}

QList<QGraphicsItem *> itemsWithId(QGraphicsScene *scene, const QString &id)
{
    QList<QGraphicsItem *> result;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(0).toString() == id)
            result << item;
    }
    return result;
}

// Centred over the 9-unit band above the device body.
bool overBand(const QGraphicsItem *designation, const QRectF &body)
{
    const QRectF rect = designation->sceneBoundingRect();
    return qAbs(rect.center().x() - body.center().x()) < 0.01 && rect.top() >= body.top() - 9
            && rect.bottom() <= body.top();
}
} // namespace

class TestWiring : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void drawsWiresFromTheModel();
    void wiresAvoidDeviceBodies();
    void wiresFollowMovedDevice();
    void wiresFollowEnclosureResize();
    void barBridgeGoesAroundTheBars();
    void wireSelectionSurvivesResync();
    void clickingEmptyCanvasClearsWireSelection();
    void supplyShowsConductorLetters();
    void designationsDrawOverWires();
    void wiredPanelInLightTheme();

private:
    [[nodiscard]] WireItem *wireItem(const QString &id) const;
    [[nodiscard]] DeviceItem *deviceItem(const QString &id) const;
    void wireTutorialPanel();
    void clickWire(const QString &id);

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
};

WireItem *TestWiring::wireItem(const QString &id) const
{
    return dynamic_cast<WireItem *>(sceneItem(scene, id));
}

DeviceItem *TestWiring::deviceItem(const QString &id) const
{
    return dynamic_cast<DeviceItem *>(sceneItem(scene, id));
}

void TestWiring::wireTutorialPanel()
{
    addTutorialDevices(window->project());
    addTutorialWires(window->project());
    QCOMPARE(window->project().devices().size(), 5);
    QCOMPARE(window->project().wires().size(), 9);
}

// Clicks the midpoint of the wire's longest segment that no other wire covers, so that the click
// can't land on a wire crossing it (Ruling 2).
void TestWiring::clickWire(const QString &id)
{
    WireItem *item = wireItem(id);
    QVERIFY(item);
    const QPolygonF route = item->route();
    QList<QLineF> segments;
    for (qsizetype i = 1; i < route.size(); ++i)
        segments << QLineF(route[i - 1], route[i]);
    std::ranges::stable_sort(segments, std::ranges::greater{}, &QLineF::length);
    for (const QLineF &segment : segments) {
        const QPoint pos = viewportPoint(canvas, segment.center());
        if (!canvas->viewport()->rect().contains(pos))
            continue;
        // The pixel the view hit-tests a click with.
        const QRectF pixel = canvas->mapToScene(QRect(pos, QSize(1, 1))).boundingRect();
        QStringList wires;
        for (QGraphicsItem *hit : scene->items(pixel)) {
            if (dynamic_cast<WireItem *>(hit))
                wires << hit->data(0).toString();
        }
        if (wires == QStringList{ id }) {
            QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, pos);
            return;
        }
    }
    QFAIL(qPrintable(id + " has no segment midpoint free of other wires"));
}

void TestWiring::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestWiring::init()
{
    window = std::make_unique<MainWindow>();
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    canvas = window->findChild<CanvasView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas);
    scene = qobject_cast<EditorScene *>(canvas->scene());
    QVERIFY(scene);
}

void TestWiring::cleanup()
{
    window.reset();
}

void TestWiring::drawsWiresFromTheModel()
{
    wireTutorialPanel();
    // Terminal positions come from the catalog, at the model's slots.
    QCOMPARE(scene->terminalPos("supply:L"), sceneItem(scene, "supply")->pos() + QPointF(9, 28));
    QCOMPARE(scene->terminalPos("d1:1"), scene->slotRect(0, 0, 2).topLeft() + QPointF(9, 8));
    QCOMPARE(scene->terminalPos("d3:2"), scene->slotRect(0, 4, 1).topLeft() + QPointF(9, 82));

    for (const Wire &wire : window->project().wires()) {
        QCOMPARE(countItems(scene, wire.id), 1);
        WireItem *item = wireItem(wire.id);
        QVERIFY2(item, qPrintable(wire.id));
        const QPolygonF route = item->route();
        QVERIFY2(orthogonal(route), qPrintable(wire.id));
        QCOMPARE(route.first(), scene->terminalPos(wire.from));
        QCOMPARE(route.last(), scene->terminalPos(wire.to));
        QCOMPARE(item->conductor(), wire.conductor);
    }
    QCOMPARE(wireItem("w9")->conductor(), Conductor::PE);
    QVERIFY(saveScreenshot(*window, "f06", "wired"));

    canvas->scale(3, 3);
    canvas->centerOn(sceneItem(scene, "enclosure")->sceneBoundingRect().center());
    QVERIFY(saveScreenshot(*canvas, "f06", "wired-closeup"));
}

void TestWiring::wiresAvoidDeviceBodies()
{
    wireTutorialPanel();
    const Project &project = window->project();
    for (const Wire &wire : project.wires()) {
        const QPolygonF route = wireItem(wire.id)->route();
        for (const PlacedDevice &device : project.devices()) {
            const QRectF body
                    = scene->slotRect(device.row, device.module, project.moduleWidth(device));
            QVERIFY2(!crossesInside(route, body), qPrintable(wire.id + " crosses " + device.id));
        }
    }
}

void TestWiring::wiresFollowMovedDevice()
{
    Project &project = window->project();
    addTutorialDevices(project);
    const QString wire = project.addWire(
            Wire{ {}, "d5:2", "d4:2", 2.5, project.conductorFor("d5:2", "d4:2") });
    QVERIFY(!wire.isEmpty());

    mouseDrag(canvas->viewport(), viewportPoint(canvas, scene->slotRect(0, 6, 1).center()),
              viewportPoint(canvas, scene->slotRect(0, 8, 1).center()));
    QCOMPARE(project.device(u"d5")->module, 8);
    QCOMPARE(scene->terminalPos("d5:2"), scene->slotRect(0, 8, 1).topLeft() + QPointF(9, 82));
    const QPolygonF route = wireItem(wire)->route();
    QVERIFY(orthogonal(route));
    QCOMPARE(route.first(), scene->terminalPos("d5:2"));
    QCOMPARE(route.last(), scene->terminalPos("d4:2"));
    QVERIFY(overBand(deviceItem("d5")->designationItem(), scene->slotRect(0, 8, 1)));
    QVERIFY(saveScreenshot(*window, "f06", "moved"));
}

void TestWiring::wiresFollowEnclosureResize()
{
    wireTutorialPanel();
    Project &project = window->project();
    const QPointF before = scene->terminalPos("nbar:1");
    QCOMPARE(wireItem("w8")->route().last(), before);

    QVERIFY(project.setEnclosure(QStringLiteral("enclosure-24")));
    const QPointF after = scene->terminalPos("nbar:1");
    QVERIFY(after.y() > before.y());
    QCOMPARE(wireItem("w8")->route().last(), after);
    QVERIFY(saveScreenshot(*window, "f06", "enclosure-24"));

    QVERIFY(project.setEnclosure(QStringLiteral("enclosure-12")));
    QCOMPARE(scene->terminalPos("nbar:1"), before);
    QCOMPARE(wireItem("w8")->route().last(), before);
}

// A bridge between the bars, as for splitting PEN, can't run between them: it goes round their end,
// also after an enclosure change has moved them.
void TestWiring::barBridgeGoesAroundTheBars()
{
    Project &project = window->project();
    const QString bridge = project.addWire(
            Wire{ {}, "nbar:2", "pebar:2", 10, project.conductorFor("nbar:2", "pebar:2") });
    QVERIFY(!bridge.isEmpty());
    const auto goesAround = [&] {
        QRectF bars;
        for (const QGraphicsItem *cell : itemsWithId(scene, "nbar") + itemsWithId(scene, "pebar"))
            bars |= cell->sceneBoundingRect();
        const QRectF route = wireItem(bridge)->route().boundingRect();
        return route.left() < bars.left() || route.right() > bars.right();
    };
    QVERIFY(goesAround());
    QVERIFY(project.setEnclosure(QStringLiteral("enclosure-24")));
    QVERIFY(goesAround());
}

void TestWiring::wireSelectionSurvivesResync()
{
    wireTutorialPanel();
    clickWire("w5");
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w5"));
    QCOMPARE(scene->selectedItems().size(), 1);
    QCOMPARE(scene->selectedDeviceId(), QString());

    PlacedDevice moved = *window->project().device(u"d5");
    moved.module = 8;
    QVERIFY(window->project().updateDevice(moved));
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w5"));
    QVERIFY(wireItem("w5")->isSelected());
    QVERIFY(saveScreenshot(*window, "f06", "wire-selected"));

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, scene->slotRect(0, 0, 2).center()));
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d1"));
    QCOMPARE(scene->selectedWireId(), QString());
}

void TestWiring::clickingEmptyCanvasClearsWireSelection()
{
    wireTutorialPanel();
    clickWire("w5");
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w5"));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, QPointF(-60, 120)));
    QCOMPARE(scene->selectedWireId(), QString());
    QVERIFY(scene->selectedItems().isEmpty());
}

void TestWiring::supplyShowsConductorLetters()
{
    wireTutorialPanel();
    QList<QGraphicsItem *> letters = itemsWithId(scene, "supplyLetter");
    QCOMPARE(letters.size(), 3);
    std::ranges::sort(letters, std::less{},
                      [](const QGraphicsItem *item) { return item->sceneBoundingRect().left(); });
    const QRectF supply = sceneItem(scene, "supply")->sceneBoundingRect();
    QStringList texts;
    for (QGraphicsItem *letter : letters) {
        const auto *text = dynamic_cast<TextItem *>(letter);
        QVERIFY(text);
        texts << text->text();
        const qreal x = scene->terminalPos("supply:" + text->text()).x();
        const QRectF rect = letter->sceneBoundingRect();
        QVERIFY2(rect.left() > x && rect.left() <= x + 6, qPrintable(text->text()));
        QVERIFY2(rect.top() >= supply.bottom(), qPrintable(text->text()));
        // Placed where the wires leaving the terminals don't cover it.
        for (const Wire &wire : window->project().wires())
            QVERIFY2(!touches(wireItem(wire.id)->route(), rect),
                     qPrintable(wire.id + " covers " + text->text()));
    }
    QCOMPARE(texts, (QStringList{ "L", "N", "PE" }));
}

void TestWiring::designationsDrawOverWires()
{
    wireTutorialPanel();
    DeviceItem *device = deviceItem("d3");
    QVERIFY(device);
    QGraphicsItem *designation = device->designationItem();
    QVERIFY(designation);
    QCOMPARE(designation->scene(), scene);
    QCOMPARE(designation->data(0).toString(), QStringLiteral("designation"));
    QVERIFY(overBand(designation, scene->slotRect(0, 4, 1)));
    QVERIFY(designation->zValue() > WireItem::WireZ);
    const auto *text = dynamic_cast<TextItem *>(designation);
    QVERIFY(text);
    QCOMPARE(text->text(), QStringLiteral("QF2"));
    QVERIFY(designation->sceneBoundingRect().intersects(wireItem("w5")->route().boundingRect()));
}

void TestWiring::wiredPanelInLightTheme()
{
    wireTutorialPanel();
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QVERIFY(saveScreenshot(*window, "f06", "wired-light"));
    Theme::apply(Theme::Mode::Dark);
}

QTEST_MAIN(TestWiring)
#include "tst_wiring.moc"
