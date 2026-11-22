// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "app/theme.h"
#include "core/project.h"
#include "editorhelpers.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QMenu>
#include <QStatusBar>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>

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

bool samePoint(QPointF a, QPointF b)
{
    return QLineF(a, b).length() < 0.01;
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
    void drawsWiresWithTheTool();
    void refusesFullTerminalsAndDuplicates();
    void escCancelsThenTurnsToolOff();
    void escAfterToolbarSection();
    void escAfterPropertiesSection();
    void escAfterWireButton();
    void highlightOnlyUnderTheCursor();
    void secondClickOnStartAndRightClickCancel();
    void doubleClickStartsAWire();
    void doubleClickFinishesAWire();
    void clicksElsewhereDoNothing();
    void paletteDropKeepsToolOn();
    void undoCancelsWireFromRemovedTerminal();
    void statusBarFollowsTheTool();
    void statusBarAfterUndoCancel();
    void sectionChoiceSurvivesLanguageSwitch();
    void rightClickOnWireShowsMenu();
    void deletesWireAndUndoes();
    void deletingWiredDeviceRemovesItsWires();

private:
    [[nodiscard]] WireItem *wireItem(const QString &id) const;
    [[nodiscard]] DeviceItem *deviceItem(const QString &id) const;
    void wireTutorialPanel();
    void clickWire(const QString &id, Qt::MouseButton button = Qt::LeftButton);
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }
    // d1 QF1 2P C40 (0,0) and d2 QD1 (0,2), through the model.
    void addToolDevices();
    // Clicks the toolbar's Wire button.
    void clickWireButton();
    void clickTerminal(const QString &id);
    // What Qt delivers for a double-click: press, release, double-click, release.
    void doubleClickTerminal(const QString &id);
    void hover(QPointF scenePos);
    [[nodiscard]] QPoint at(int row, int module, int modules) const;
    [[nodiscard]] QString message() const;
    [[nodiscard]] QGraphicsEllipseItem *highlight() const;
    [[nodiscard]] QGraphicsLineItem *preview() const;
    // Sends Esc where the user's key press goes: to the focus widget.
    void pressEscape();

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
void TestWiring::clickWire(const QString &id, Qt::MouseButton button)
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
            QTest::mouseClick(canvas->viewport(), button, {}, pos);
            return;
        }
    }
    QFAIL(qPrintable(id + " has no segment midpoint free of other wires"));
}

void TestWiring::addToolDevices()
{
    Project &project = window->project();
    QVERIFY(!project.addDevice(deviceFromPreset(preset("breaker-2p-c40"), 0, 0)).isEmpty());
    QVERIFY(!project.addDevice(deviceFromPreset(preset("rcd-2p-40-30"), 0, 2)).isEmpty());
    QCOMPARE(project.designation(u"d1"), QStringLiteral("QF1"));
    QCOMPARE(project.designation(u"d2"), QStringLiteral("QD1"));
}

void TestWiring::clickWireButton()
{
    auto *toolbar = named<QToolBar>("editToolBar");
    QVERIFY(toolbar);
    auto *action = named<QAction>("wireAction");
    QVERIFY(action);
    QWidget *button = toolbar->widgetForAction(action);
    QVERIFY(button);
    QTest::mouseClick(button, Qt::LeftButton);
}

void TestWiring::clickTerminal(const QString &id)
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, scene->terminalPos(id)));
}

void TestWiring::doubleClickTerminal(const QString &id)
{
    const QPoint pos = viewportPoint(canvas, scene->terminalPos(id));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, pos);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, pos);
    QTest::mouseDClick(canvas->viewport(), Qt::LeftButton, {}, pos);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, pos);
}

// A move to where the pointer already is would not arrive, so the pointer steps aside first.
void TestWiring::hover(QPointF scenePos)
{
    const QPoint pos = viewportPoint(canvas, scenePos);
    QTest::mouseMove(canvas->viewport(), pos + QPoint(1, 0));
    QTest::mouseMove(canvas->viewport(), pos);
}

QPoint TestWiring::at(int row, int module, int modules) const
{
    return viewportPoint(canvas, scene->slotRect(row, module, modules).center());
}

QString TestWiring::message() const
{
    return window->statusBar()->currentMessage();
}

QGraphicsEllipseItem *TestWiring::highlight() const
{
    return qgraphicsitem_cast<QGraphicsEllipseItem *>(sceneItem(scene, "terminalHighlight"));
}

QGraphicsLineItem *TestWiring::preview() const
{
    return qgraphicsitem_cast<QGraphicsLineItem *>(sceneItem(scene, "wirePreview"));
}

void TestWiring::pressEscape()
{
    QWidget *focus = QApplication::focusWidget();
    QVERIFY(focus);
    QTest::keyClick(focus, Qt::Key_Escape);
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

// The roadmap scenario: the tutorial's first wires, drawn with the tool.
void TestWiring::drawsWiresWithTheTool()
{
    addToolDevices();
    Project &project = window->project();
    auto *wireAction = named<QAction>("wireAction");
    QVERIFY(wireAction);
    QVERIFY(wireAction->isCheckable());
    QCOMPARE(wireAction->text(), QStringLiteral("Wire"));
    QCOMPARE(wireAction->toolTip(),
             QStringLiteral("Draw wires: click one terminal, then another."));
    auto *button = qobject_cast<QToolButton *>(
            named<QToolBar>("editToolBar")->widgetForAction(wireAction));
    QVERIFY(button);
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    QVERIFY(!scene->isWireMode());
    clickWireButton();
    QVERIFY(scene->isWireMode());
    QVERIFY(wireAction->isChecked());
    QCOMPARE(message(), QStringLiteral("Click a terminal to start a wire."));

    auto *combo = named<QComboBox>("wireSectionCombo");
    QVERIFY(combo);
    QCOMPARE(combo->toolTip(), QStringLiteral("Section of new wires"));
    QStringList texts;
    for (int i = 0; i < combo->count(); ++i)
        texts << combo->itemText(i);
    QCOMPARE(texts,
             (QStringList{ QStringLiteral("1.5 mm² — lighting, C10"),
                           QStringLiteral("2.5 mm² — sockets, C16"), QStringLiteral("4 mm²"),
                           QStringLiteral("6 mm² — cooktop, C32"),
                           QStringLiteral("10 mm² — apartment main, C40"),
                           QStringLiteral("16 mm²") }));
    QCOMPARE(combo->currentData().toDouble(), 2.5);
    QCOMPARE(scene->wireSection(), 2.5);
    chooseComboItem(combo, combo->itemText(combo->findData(10.0)));
    QCOMPARE(scene->wireSection(), 10.0);

    hover(scene->terminalPos("supply:L"));
    QGraphicsEllipseItem *ring = highlight();
    QVERIFY(ring);
    QVERIFY(ring->isVisible());
    QCOMPARE(ring->zValue(), 3.0);
    QVERIFY(samePoint(ring->sceneBoundingRect().center(), scene->terminalPos("supply:L")));
    QCOMPARE(ring->pen().color(), Theme::selectionColor());
    QVERIFY(saveScreenshot(*window, "f06", "terminal-highlight"));

    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    QCOMPARE(message(), QStringLiteral("Click the second terminal. Esc cancels."));
    hover((scene->terminalPos("supply:L") + scene->terminalPos("d1:1")) / 2);
    QVERIFY(!ring->isVisible()); // away from terminals
    QGraphicsLineItem *line = preview();
    QVERIFY(line);
    QVERIFY(line->isVisible());
    QCOMPARE(line->zValue(), 3.0);
    QCOMPARE(line->pen().style(), Qt::DashLine);
    QCOMPARE(line->pen().color(), Theme::conductorColor(Conductor::L));
    QVERIFY(samePoint(line->line().p1(), scene->terminalPos("supply:L")));
    QVERIFY(saveScreenshot(*window, "f06", "wire-in-progress"));

    clickTerminal("d1:1");
    QCOMPARE(project.wires(), (QList<Wire>{ Wire{ "w1", "supply:L", "d1:1", 10, Conductor::L } }));
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w1"));
    QCOMPARE(scene->selectedItems().size(), 1);
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(!line->isVisible());
    QVERIFY(scene->isWireMode());
    QCOMPARE(window->undoStack().count(), 1);

    for (const char *terminal : { "d1:2", "d2:1", "supply:N", "d1:3", "d1:4", "d2:N1" })
        clickTerminal(QString::fromLatin1(terminal));
    QCOMPARE(project.wires(),
             (QList<Wire>{ Wire{ "w1", "supply:L", "d1:1", 10, Conductor::L },
                           Wire{ "w2", "d1:2", "d2:1", 10, Conductor::L },
                           Wire{ "w3", "supply:N", "d1:3", 10, Conductor::N },
                           Wire{ "w4", "d1:4", "d2:N1", 10, Conductor::N } }));
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w4"));
    QCOMPARE(window->undoStack().count(), 4);
    QVERIFY(scene->isWireMode());
    QVERIFY(saveScreenshot(*window, "f06", "wired-with-tool"));
}

void TestWiring::refusesFullTerminalsAndDuplicates()
{
    addToolDevices();
    Project &project = window->project();
    for (const auto &[from, to] : { std::pair("supply:L", "d1:1"), std::pair("d1:1", "d1:3"),
                                    std::pair("supply:PE", "pebar:1"), std::pair("d1:2", "d2:1") })
        QVERIFY(!project.addWire(Wire{ {}, from, to, 2.5, project.conductorFor(from, to) })
                         .isEmpty());
    const QList<Wire> wires = project.wires();
    const auto unchanged
            = [&] { return project.wires() == wires && window->undoStack().count() == 0; };
    clickWireButton();
    QVERIFY(scene->isWireMode());

    // Two wires on a device terminal, whose wire stubs cover it.
    clickTerminal("d1:1");
    QCOMPARE(message(), QStringLiteral("This terminal already has two wires."));
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(unchanged());
    QGraphicsEllipseItem *ring = highlight();
    QVERIFY(ring);
    QVERIFY(ring->isVisible());
    QVERIFY(samePoint(ring->sceneBoundingRect().center(), scene->terminalPos("d1:1")));
    QCOMPARE(ring->pen().color(), Theme::refusedColor());
    QVERIFY(saveScreenshot(*window, "f06", "refused-full"));

    clickTerminal("pebar:1");
    QCOMPARE(message(), QStringLiteral("This bar hole already has a wire."));
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(unchanged());

    clickTerminal("d2:1");
    QCOMPARE(scene->wireStart(), QStringLiteral("d2:1"));
    clickTerminal("d1:2");
    QCOMPARE(message(), QStringLiteral("These terminals are already joined by a wire."));
    QCOMPARE(scene->wireStart(), QStringLiteral("d2:1"));
    QVERIFY(unchanged());

    // A full terminal is refused as the end too, and the wire stays started.
    clickTerminal("d1:1");
    QCOMPARE(message(), QStringLiteral("This terminal already has two wires."));
    QCOMPARE(scene->wireStart(), QStringLiteral("d2:1"));
    QVERIFY(unchanged());
}

void TestWiring::escCancelsThenTurnsToolOff()
{
    addToolDevices();
    auto *wireAction = named<QAction>("wireAction");
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    hover(scene->terminalPos("d1:1"));
    QVERIFY(highlight() && preview());
    QVERIFY(highlight()->isVisible());
    QVERIFY(preview()->isVisible());

    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Escape);
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(scene->isWireMode());
    QVERIFY(!preview()->isVisible());

    QTest::keyClick(canvas, Qt::Key_Escape);
    QVERIFY(!scene->isWireMode());
    QVERIFY(!wireAction->isChecked());
    QVERIFY(!highlight()->isVisible());
    QVERIFY(!preview()->isVisible());

    // The Wire button turns the tool off too, cancelling a wire in progress.
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    clickWireButton();
    QVERIFY(!scene->isWireMode());
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(!preview()->isVisible());
    QVERIFY(window->project().wires().isEmpty());
}

// Esc reaches the tool with the focus left on the drop-down, without a click on the canvas first.
// In the open drop-down, Esc only closes it.
void TestWiring::escAfterToolbarSection()
{
    addToolDevices();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    auto *combo = named<QComboBox>("wireSectionCombo");
    QVERIFY(combo);
    QTest::mouseClick(combo, Qt::LeftButton);
    QTRY_VERIFY(combo->view()->isVisible());
    pressEscape();
    QTRY_VERIFY(!combo->view()->isVisible());
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));

    chooseComboItem(combo, combo->itemText(combo->findData(10.0)));
    QCOMPARE(scene->wireSection(), 10.0);
    QTRY_VERIFY(!combo->view()->isVisible());
    QCOMPARE(QApplication::focusWidget(), combo);
    pressEscape();
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(scene->isWireMode());
    pressEscape();
    QVERIFY(!scene->isWireMode());
    QVERIFY(!named<QAction>("wireAction")->isChecked());
    QCOMPARE(scene->wireSection(), 10.0);
}

// Fixing a new wire's section in the properties panel, with the next wire already started.
void TestWiring::escAfterPropertiesSection()
{
    addToolDevices();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    clickTerminal("d1:1");
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w1"));
    clickTerminal("supply:N");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:N"));
    auto *section = named<QComboBox>("sectionCombo");
    QVERIFY(section);
    QVERIFY(section->isVisible());
    chooseComboItem(section, QStringLiteral("10 mm\u00b2"));
    QCOMPARE(window->project().wire(QStringLiteral("w1"))->section, 10.0);
    QTRY_VERIFY(!section->view()->isVisible());
    QCOMPARE(QApplication::focusWidget(), section);
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:N"));
    pressEscape();
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(scene->isWireMode());
    pressEscape();
    QVERIFY(!scene->isWireMode());
    QCOMPARE(window->project().wires().size(), 1);
}

// The Wire button doesn't move the focus, which stays where it was, here on the drop-down.
void TestWiring::escAfterWireButton()
{
    addToolDevices();
    auto *combo = named<QComboBox>("wireSectionCombo");
    QVERIFY(combo);
    chooseComboItem(combo, combo->itemText(combo->findData(6.0)));
    QTRY_VERIFY(!combo->view()->isVisible());
    clickWireButton();
    QVERIFY(scene->isWireMode());
    QVERIFY(QApplication::focusWidget() != canvas);
    pressEscape();
    QVERIFY(!scene->isWireMode());
    QVERIFY(!named<QAction>("wireAction")->isChecked());
}

// The ring shows where the cursor is on the canvas: not where it was when the tool was last on, and
// not after the cursor has left the canvas, also through an edit made from the toolbar.
void TestWiring::highlightOnlyUnderTheCursor()
{
    addToolDevices();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    hover(scene->terminalPos("d1:1"));
    QVERIFY(highlight()->isVisible());
    pressEscape();
    pressEscape();
    QVERIFY(!scene->isWireMode());
    clickWireButton();
    QVERIFY(scene->isWireMode());
    QVERIFY(!highlight()->isVisible());
    hover(scene->terminalPos("d1:1"));
    QVERIFY(highlight()->isVisible());

    clickTerminal("supply:L");
    clickTerminal("d1:1");
    clickTerminal("supply:N");
    hover(scene->terminalPos("d1:3"));
    QVERIFY(highlight()->isVisible());
    QWidget *undoButton
            = named<QToolBar>("editToolBar")->widgetForAction(named<QAction>("undoAction"));
    QVERIFY(undoButton);
    QTest::mouseMove(undoButton);
    QVERIFY(!highlight()->isVisible());
    QVERIFY(preview()->isVisible()); // the wire in progress still shows
    QTest::mouseClick(undoButton, Qt::LeftButton);
    QVERIFY(window->project().wires().isEmpty());
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:N"));
    QVERIFY(!highlight()->isVisible());
    hover(scene->terminalPos("d1:3"));
    QVERIFY(highlight()->isVisible());
}

void TestWiring::secondClickOnStartAndRightClickCancel()
{
    addToolDevices();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    clickTerminal("supply:L");
    QVERIFY(scene->wireStart().isEmpty());

    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    QTest::mouseClick(canvas->viewport(), Qt::RightButton, {}, at(0, 2, 2));
    QVERIFY(scene->wireStart().isEmpty());
    auto *menu = named<QMenu>("deviceContextMenu");
    QVERIFY(!menu || !menu->isVisible());
    QVERIFY(scene->isWireMode());
    QVERIFY(window->project().wires().isEmpty());
    QCOMPARE(window->undoStack().count(), 0);
}

// A double-click acts as one click.
void TestWiring::doubleClickStartsAWire()
{
    addToolDevices();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    doubleClickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("supply:L"));
    QCOMPARE(message(), QStringLiteral("Click the second terminal. Esc cancels."));
    QVERIFY(window->project().wires().isEmpty());
    QCOMPARE(window->undoStack().count(), 0);
}

void TestWiring::doubleClickFinishesAWire()
{
    addToolDevices();
    Project &project = window->project();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal("supply:L");
    doubleClickTerminal("d1:1");
    QCOMPARE(project.wires(), (QList<Wire>{ Wire{ "w1", "supply:L", "d1:1", 2.5, Conductor::L } }));
    QVERIFY(scene->wireStart().isEmpty());
    clickTerminal("d2:1");
    QCOMPARE(scene->wireStart(), QStringLiteral("d2:1"));
    QCOMPARE(project.wires().size(), 1);
    clickTerminal("d2:1");
    QVERIFY(scene->wireStart().isEmpty());

    // On a bar hole, which takes one wire, the double-click doesn't report the new wire as full.
    clickTerminal("supply:PE");
    doubleClickTerminal("pebar:1");
    QCOMPARE(project.wires().size(), 2);
    QCOMPARE(project.wires().last().to, QStringLiteral("pebar:1"));
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(message() != QStringLiteral("This bar hole already has a wire."));
    QCOMPARE(window->undoStack().count(), 2);
}

void TestWiring::clicksElsewhereDoNothing()
{
    addToolDevices();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 0, 2));
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d1"));
    clickWireButton();
    QVERIFY(scene->isWireMode());

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 2, 2));
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d1"));
    QCOMPARE(scene->selectedItems().size(), 1);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, QPointF(-60, 120)));
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d1"));

    mouseDrag(canvas->viewport(), at(0, 2, 2), at(0, 4, 2));
    QCOMPARE(window->project().device(u"d2")->module, 2);
    QCOMPARE(deviceItem("d2")->pos(), scene->slotRect(0, 2, 2).topLeft());
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d1"));
    QVERIFY(scene->isWireMode());
}

void TestWiring::paletteDropKeepsToolOn()
{
    addToolDevices();
    clickWireButton();
    QVERIFY(scene->isWireMode());
    dropPreset(canvas, named<PaletteWidget>("paletteTree"), "breaker-1p-c16", at(0, 6, 1));
    QCOMPARE(window->project().devices().size(), 3);
    QCOMPARE(window->project().devices().last().module, 6);
    QVERIFY(scene->isWireMode());
    QVERIFY(named<QAction>("wireAction")->isChecked());
}

void TestWiring::undoCancelsWireFromRemovedTerminal()
{
    addToolDevices();
    Project &project = window->project();
    dropPreset(canvas, named<PaletteWidget>("paletteTree"), "breaker-1p-c16", at(0, 6, 1));
    QCOMPARE(window->undoStack().count(), 1);
    const QString dropped = project.devices().last().id;
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal(dropped + ":1");
    QCOMPARE(scene->wireStart(), dropped + ":1");
    hover(scene->slotRect(0, 4, 2).center());
    QVERIFY(preview());
    QVERIFY(preview()->isVisible());

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(!project.device(dropped));
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(!preview()->isVisible());
    QVERIFY(scene->isWireMode());

    clickTerminal("d1:1");
    QCOMPARE(scene->wireStart(), QStringLiteral("d1:1"));
}

// Once a wire is finished or cancelled the bar asks for the next start, and once the tool is off it
// says "Ready". A refusal stays on the bar.
void TestWiring::statusBarFollowsTheTool()
{
    addToolDevices();
    const QString start = QStringLiteral("Click a terminal to start a wire.");
    const QString second = QStringLiteral("Click the second terminal. Esc cancels.");
    clickWireButton();
    QVERIFY(scene->isWireMode());
    QCOMPARE(message(), start);
    clickTerminal("supply:L");
    QCOMPARE(message(), second);
    clickTerminal("d1:1");
    QCOMPARE(window->project().wires().size(), 1);
    QCOMPARE(message(), start);
    QVERIFY(saveScreenshot(*window, "f06", "status-after-wire"));

    clickTerminal("d1:2");
    QCOMPARE(message(), second);
    pressEscape();
    QVERIFY(scene->wireStart().isEmpty());
    QCOMPARE(message(), start);

    clickTerminal("d1:2");
    QCOMPARE(message(), second);
    clickTerminal("d1:2");
    QVERIFY(scene->wireStart().isEmpty());
    QCOMPARE(message(), start);

    clickTerminal("d1:2");
    QCOMPARE(message(), second);
    QTest::mouseClick(canvas->viewport(), Qt::RightButton, {}, at(0, 2, 2));
    QVERIFY(scene->wireStart().isEmpty());
    QCOMPARE(message(), start);

    clickTerminal("d1:1");
    clickTerminal("supply:L");
    QCOMPARE(scene->wireStart(), QStringLiteral("d1:1"));
    QCOMPARE(message(), QStringLiteral("These terminals are already joined by a wire."));
    pressEscape();
    QCOMPARE(message(), start);

    pressEscape();
    QVERIFY(!scene->isWireMode());
    QCOMPARE(message(), QStringLiteral("Ready"));

    clickWireButton();
    QCOMPARE(message(), start);
    clickTerminal("d1:2");
    QCOMPARE(message(), second);
    clickWireButton();
    QVERIFY(!scene->isWireMode());
    QCOMPARE(message(), QStringLiteral("Ready"));
    QCOMPARE(window->project().wires().size(), 1);
}

void TestWiring::statusBarAfterUndoCancel()
{
    addToolDevices();
    dropPreset(canvas, named<PaletteWidget>("paletteTree"), "breaker-1p-c16", at(0, 6, 1));
    const QString dropped = window->project().devices().last().id;
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal(dropped + ":1");
    QCOMPARE(message(), QStringLiteral("Click the second terminal. Esc cancels."));
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(scene->wireStart().isEmpty());
    QCOMPARE(message(), QStringLiteral("Click a terminal to start a wire."));
}

void TestWiring::sectionChoiceSurvivesLanguageSwitch()
{
    auto *combo = named<QComboBox>("wireSectionCombo");
    QVERIFY(combo);
    chooseComboItem(combo, combo->itemText(combo->findData(6.0)));
    QCOMPARE(scene->wireSection(), 6.0);

    auto *languageMenu = named<QMenu>("languageMenu");
    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_ru"));
    QCOMPARE(combo->currentData().toDouble(), 6.0);
    QVERIFY2(combo->currentText().startsWith(QStringLiteral("6 mm² — варочная панель")),
             qPrintable(combo->currentText()));
    QCOMPARE(combo->itemText(combo->findData(2.5)), QStringLiteral("2,5 mm² — розетки, C16"));
    QCOMPARE(scene->wireSection(), 6.0);
    auto *wireAction = named<QAction>("wireAction");
    QCOMPARE(wireAction->text(), QStringLiteral("Провод"));
    // The longer texts fit once the toolbar has laid itself out again.
    auto *toolbar = named<QToolBar>("editToolBar");
    QWidget *button = toolbar->widgetForAction(wireAction);
    QTRY_VERIFY(button->width() >= button->sizeHint().width());
    QTRY_VERIFY(combo->width() >= combo->sizeHint().width());
    QVERIFY(saveScreenshot(*toolbar, "f06", "toolbar-ru"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_en"));
    QCOMPARE(combo->currentText(), QStringLiteral("6 mm² — cooktop, C32"));
}

// The right-click menu also opens on wires, with the tool off and, with no wire in progress, on.
void TestWiring::rightClickOnWireShowsMenu()
{
    wireTutorialPanel();
    clickWire("w5", Qt::RightButton);
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w5"));
    auto *menu = named<QMenu>("deviceContextMenu");
    QVERIFY(menu);
    QTRY_VERIFY(menu->isVisible());
    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());

    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickWire("w6", Qt::RightButton);
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w6"));
    QCOMPARE(scene->selectedItems().size(), 1);
    QTRY_VERIFY(menu->isVisible());
    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());
    QVERIFY(scene->isWireMode());
}

void TestWiring::deletesWireAndUndoes()
{
    wireTutorialPanel();
    const auto ids = [this] {
        QStringList result;
        for (const Wire &wire : window->project().wires())
            result << wire.id;
        return result;
    };
    const QStringList all = ids();

    clickWire("w5");
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QStringList without = all;
    without.removeOne("w5");
    QCOMPARE(ids(), without);
    QVERIFY(saveScreenshot(*window, "f06", "wire-deleted"));
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(ids(), all);

    clickWire("w6", Qt::RightButton);
    auto *menu = named<QMenu>("deviceContextMenu");
    QVERIFY(menu);
    QTRY_VERIFY(menu->isVisible());
    clickMenuItem(menu, named<QAction>("deleteAction"));
    QVERIFY(!window->project().wire(QStringLiteral("w6")));

    clickWire("w7");
    auto *editMenu = named<QMenu>("editMenu");
    clickMenuBarItem(*window, editMenu);
    clickMenuItem(editMenu, named<QAction>("deleteAction"));
    QVERIFY(!window->project().wire(QStringLiteral("w7")));
    QCOMPARE(window->project().wires().size(), all.size() - 2);
}

void TestWiring::deletingWiredDeviceRemovesItsWires()
{
    wireTutorialPanel();
    const QList<Wire> original = window->project().wires();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, deviceItem("d3")->sceneBoundingRect().center()));
    QCOMPARE(scene->selectedDeviceId(), QStringLiteral("d3"));
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!window->project().device(QStringLiteral("d3")));
    QVERIFY(!window->project().wire(QStringLiteral("w5")));
    QVERIFY(!window->project().wire(QStringLiteral("w6")));
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(window->project().wires(), original);
}

QTEST_MAIN(TestWiring)
#include "tst_wiring.moc"
