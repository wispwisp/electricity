// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/language.h"
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

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGraphicsLineItem>
#include <QHash>
#include <QMenu>
#include <QStatusBar>
#include <QTest>
#include <QToolBar>
#include <QUndoStack>
#include <QWindow>

#include <algorithm>
#include <memory>

class TestLoading : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void drawsLoadsFromTheModel();
    void loadWiresEnterFromAboveAndAvoidLoads();
    void wiresIntoALoadUnderAnotherRunBetweenThem();
    void selectsAndDeletesLoadWithUndo();
    void captionsFollowLanguage();
    void fitAndRoutingWithFarLoads();
    void loadsInLightTheme();
    void dropsLoadsOutsideTheEnclosure();
    void refusesDropsOnEnclosureSupplyAndLoads();
    void dropsApplianceOntoSocket();
    void dragPreviewShowsRefusal();
    void loadDropKeepsWireToolOn();
    void movesLoadByDragging();
    void refusedMoveGoesBack();
    void unplugsAndReplugsByDragging();
    void dropOnOwnSocketIsNoEdit();
    void movingSocketCarriesAppliances();
    void escCancelsLoadMove();
    void nextPressEndsALoadMoveThatLostItsRelease();
    void noLoadMovesUnderWireTool();
    void drawnLoadWiresStartAt15mAndJoinTheirCable();
    void undoCancelsWireFromRemovedLoad();
    void roadmapScenario();

private:
    [[nodiscard]] LoadItem *loadItem(const QString &id) const;
    [[nodiscard]] WireItem *wireItem(const QString &id) const;
    // A load of that type through the model; returns its id.
    QString addLoad(const QString &type, QPointF position);
    QString addWire(const QString &from, const QString &to, double section);
    // A ceiling lamp at (0, 270), a socket at (90, 270) with a kettle in place 1 and an unplugged
    // TV at (300, 300), through the model.
    void addLoads();
    // d3:2→lamp L, nbar:2→lamp N, pebar:2→lamp PE, d4:2→socket L.
    void addLoadWires();
    // The viewport point over the middle of the load's picture.
    [[nodiscard]] QPoint onPicture(const QString &id) const;
    // Saves the canvas zoomed in on the lamp and the socket, then restores the view.
    void saveCloseUp(const QString &step);
    // Every caption, as painted, lies within its load's outline, which is all that wires keep
    // clear of.
    void checkCaptionsFitOutlines();
    // Drags a load type from the palette onto the canvas and drops it at the scene point.
    void drop(const QString &type, QPointF scenePos);
    // The status bar text; cleared by clearMessage, so a test sees the hint of its last drop.
    [[nodiscard]] QString message() const;
    void clearMessage();
    void clickWireButton();
    // Chooses the section of new wires in the toolbar combo.
    void chooseSection(double section);
    void clickTerminal(const QString &id);
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
    QString lamp;
    QString socket;
    QString kettle;
    QString tv;
    QStringList loadWires; // in the order of addLoadWires
};

LoadItem *TestLoading::loadItem(const QString &id) const
{
    return dynamic_cast<LoadItem *>(sceneItem(scene, id));
}

WireItem *TestLoading::wireItem(const QString &id) const
{
    return dynamic_cast<WireItem *>(sceneItem(scene, id));
}

QString TestLoading::addLoad(const QString &type, QPointF position)
{
    const LoadType *loadType = window->project().catalog().loadType(type);
    if (!loadType)
        return {};
    return window->project().addLoad(loadFromType(*loadType, position));
}

QString TestLoading::addWire(const QString &from, const QString &to, double section)
{
    Project &project = window->project();
    Wire wire{ {}, from, to, section, project.conductorFor(from, to) };
    wire.length = project.newWireLength(from, to);
    return project.addWire(wire);
}

void TestLoading::addLoads()
{
    Project &project = window->project();
    lamp = addLoad("ceiling-lamp", QPointF(0, 270));
    socket = addLoad("socket", QPointF(90, 270));
    PlacedLoad plugged = loadFromType(*project.catalog().loadType(u"kettle"), QPointF());
    plugged.socket = socket;
    plugged.place = 1;
    kettle = project.addLoad(plugged);
    tv = addLoad("tv", QPointF(300, 300));
    QVERIFY(!lamp.isEmpty());
    QVERIFY(!socket.isEmpty());
    QVERIFY(!kettle.isEmpty());
    QVERIFY(!tv.isEmpty());
}

void TestLoading::addLoadWires()
{
    loadWires = { addWire("d3:2", lamp + ":L", 1.5), addWire("nbar:2", lamp + ":N", 1.5),
                  addWire("pebar:2", lamp + ":PE", 1.5), addWire("d4:2", socket + ":L", 2.5) };
    QVERIFY(!loadWires.contains(QString()));
}

QPoint TestLoading::onPicture(const QString &id) const
{
    const PlacedLoad *load = window->project().load(id);
    const LoadType *type = window->project().catalog().loadType(load->type);
    return viewportPoint(canvas,
                         QRectF(window->project().loadPosition(*load), type->size).center());
}

void TestLoading::saveCloseUp(const QString &step)
{
    const QTransform transform = canvas->transform();
    const QPointF centre = canvas->mapToScene(canvas->viewport()->rect().center());
    canvas->setTransform(QTransform::fromScale(3, 3));
    canvas->centerOn(QPointF(110, 260));
    QVERIFY(saveScreenshot(*canvas, "f07", step));
    canvas->setTransform(transform);
    canvas->centerOn(centre);
}

void TestLoading::checkCaptionsFitOutlines()
{
    constexpr qreal tolerance = 0.01;
    for (const PlacedLoad &load : window->project().loads()) {
        const LoadItem *item = loadItem(load.id);
        QVERIFY2(item, qPrintable(load.id));
        if (item->caption().isEmpty())
            continue;
        const QRectF caption = item->mapRectToScene(item->captionRect());
        const QRectF outline = window->project().loadOutline(load);
        QVERIFY2(caption.width() > 0 && caption.left() >= outline.left() - tolerance
                         && caption.right() <= outline.right() + tolerance,
                 qPrintable(QStringLiteral("%1: \"%2\" is %3 wide, its outline %4")
                                    .arg(load.id, item->caption())
                                    .arg(caption.width())
                                    .arg(outline.width())));
    }
}

void TestLoading::drop(const QString &type, QPointF scenePos)
{
    dropLoad(canvas, named<PaletteWidget>("paletteTree"), type, viewportPoint(canvas, scenePos));
}

QString TestLoading::message() const
{
    return window->statusBar()->currentMessage();
}

void TestLoading::clearMessage()
{
    window->statusBar()->clearMessage();
}

void TestLoading::clickWireButton()
{
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(named<QAction>("wireAction")),
                      Qt::LeftButton);
}

void TestLoading::chooseSection(double section)
{
    auto *combo = named<QComboBox>("wireSectionCombo");
    QVERIFY(combo);
    chooseComboItem(combo, combo->itemText(combo->findData(section)));
    QCOMPARE(scene->wireSection(), section);
}

void TestLoading::clickTerminal(const QString &id)
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, scene->terminalPos(id)));
}

void TestLoading::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestLoading::init()
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
    // The area around the enclosure, where loads go.
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
}

// Also when a test fails midway, the next one starts in English and in the dark theme.
void TestLoading::cleanup()
{
    window.reset();
    installLanguage(Language::English);
    Theme::apply(Theme::Mode::Dark);
}

void TestLoading::drawsLoadsFromTheModel()
{
    addLoads();
    const Project &project = window->project();
    for (const PlacedLoad &load : project.loads()) {
        QCOMPARE(countItems(scene, load.id), 1);
        LoadItem *item = loadItem(load.id);
        QVERIFY2(item, qPrintable(load.id));
        QCOMPARE(item->pos(), project.loadPosition(load));
        QCOMPARE(item->zValue(), LoadItem::PlacedZ);
    }
    // The kettle hangs from the socket's first place.
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 310.5));

    QCOMPARE(loadItem(lamp)->caption(), QStringLiteral("Lamp 1"));
    QCOMPARE(loadItem(socket)->caption(), QStringLiteral("Socket 1"));
    QCOMPARE(loadItem(kettle)->caption(), QString());
    QCOMPARE(loadItem(tv)->caption(), QString());

    // In item coordinates: the free second place, and a cord from the socket's bottom edge down
    // to the first.
    QCOMPARE(loadItem(socket)->emptyPlaces(), QList<QRectF>{ QRectF(31.5, 40.5, 36, 36) });
    QCOMPARE(loadItem(socket)->cords(), QList<QLineF>{ QLineF(4.5, 27, 4.5, 40.5) });
    for (const QString &id : { lamp, kettle, tv }) {
        QVERIFY2(loadItem(id)->emptyPlaces().isEmpty(), qPrintable(id));
        QVERIFY2(loadItem(id)->cords().isEmpty(), qPrintable(id));
    }
    QVERIFY(saveScreenshot(*window, "f07", "loads"));
    saveCloseUp("loads-closeup");
}

void TestLoading::loadWiresEnterFromAboveAndAvoidLoads()
{
    addLoads();
    addLoadWires();
    const Project &project = window->project();
    QCOMPARE(scene->terminalPos(lamp + ":L"), QPointF(9, 274.5));
    QCOMPARE(scene->terminalPos(socket + ":L"), QPointF(103.5, 274.5));
    for (const QString &id : std::as_const(loadWires)) {
        const QPolygonF route = wireItem(id)->route();
        QVERIFY2(route.size() >= 2, qPrintable(id));
        QCOMPARE(route.last(), scene->terminalPos(project.wire(id)->to));
        const QPointF before = route[route.size() - 2];
        QVERIFY2(before.x() == route.last().x() && before.y() < route.last().y(), qPrintable(id));
    }
    for (const Wire &wire : project.wires()) {
        const QPolygonF route = wireItem(wire.id)->route();
        for (const PlacedLoad &load : project.loads())
            QVERIFY2(!crossesInside(route, project.loadOutline(load)),
                     qPrintable(wire.id + " crosses " + load.id));
    }
    QVERIFY(saveScreenshot(*window, "f07", "wired-loads"));
    saveCloseUp("wired-loads-closeup");
}

// A lamp dragged down from over another one is refused until there is room above it for its wires
// to come in, so the wire dropped there doesn't run through the lamp above.
void TestLoading::wiresIntoALoadUnderAnotherRunBetweenThem()
{
    lamp = addLoad("ceiling-lamp", QPointF(0, 270));
    QVERIFY(!lamp.isEmpty());
    const Project &project = window->project();
    auto *palette = named<PaletteWidget>("paletteTree");
    const QPointF half(18, 18);
    QPointF topLeft(0, 306); // over the lamp's caption
    for (;; topLeft.ry() += 4.5) {
        QVERIFY(topLeft.y() < 360);
        dragLoadOver(canvas, palette, "ceiling-lamp", viewportPoint(canvas, topLeft + half));
        QVERIFY(loadItem("loadPreview"));
        if (!loadItem("loadPreview")->isRefused())
            break;
    }
    drop("ceiling-lamp", topLeft + half);
    QCOMPARE(project.loads().size(), 2);
    const QString under = project.loads().last().id;

    clickWireButton();
    clickTerminal("d3:2");
    clickTerminal(under + ":L");
    const Wire wire = project.wires().last();
    QCOMPARE(wire.to, under + ":L");
    const QPolygonF route = wireItem(wire.id)->route();
    QCOMPARE(route.last(), scene->terminalPos(under + ":L"));
    const QRectF above = project.loadOutline(*project.load(lamp));
    QVERIFY(!stubCrossesInside(route, above));
    QVERIFY(!crossesInside(route, above));
    QCOMPARE(project.load(under)->position, QPointF(0, 324));
    QVERIFY(saveScreenshot(*window, "f07", "entry-lane"));
    saveCloseUp("entry-lane-closeup");
}

void TestLoading::selectsAndDeletesLoadWithUndo()
{
    addLoads();
    addLoadWires();
    Project &project = window->project();
    const QList<PlacedLoad> loads = project.loads();
    const QList<Wire> wires = project.wires();

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onPicture(lamp));
    QCOMPARE(scene->selectedLoadId(), lamp);
    QCOMPARE(scene->selectedItems().size(), 1);
    QVERIFY(named<QAction>("deleteAction")->isEnabled());

    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!project.load(lamp));
    QVERIFY(!sceneItem(scene, lamp));
    for (const QString &id : loadWires.first(3))
        QVERIFY2(!project.wire(id), qPrintable(id));
    QCOMPARE(project.wires().size(), wires.size() - 3);

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.loads(), loads);
    QCOMPARE(project.wires(), wires);
    QVERIFY(loadItem(lamp));
    for (const QString &id : loadWires.first(3))
        QVERIFY2(wireItem(id), qPrintable(id));

    // An empty plug place doesn't take clicks: one there selects nothing.
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onPicture(socket));
    QCOMPARE(scene->selectedLoadId(), socket);
    const QRectF place = project.plugPlaceRect(*project.load(socket), 2);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, place.center()));
    QCOMPARE(scene->selectedLoadId(), QString());

    QTest::mouseClick(canvas->viewport(), Qt::RightButton, {}, onPicture(socket));
    QCOMPARE(scene->selectedLoadId(), socket);
    auto *menu = named<QMenu>("deviceContextMenu");
    QVERIFY(menu);
    QTRY_VERIFY(menu->isVisible());
    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());
}

void TestLoading::captionsFollowLanguage()
{
    addLoads();
    QHash<QString, QPointF> positions;
    for (const PlacedLoad &load : window->project().loads())
        positions.insert(load.id, loadItem(load.id)->pos());
    checkCaptionsFitOutlines();

    auto *languageMenu = named<QMenu>("languageMenu");
    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_ru"));
    QCOMPARE(loadItem(lamp)->caption(), QStringLiteral("Светильник 1"));
    QCOMPARE(loadItem(socket)->caption(), QStringLiteral("Розетка 1"));
    QCOMPARE(loadItem(kettle)->caption(), QString());
    checkCaptionsFitOutlines();
    QVERIFY(saveScreenshot(*window, "f07", "captions-ru"));
    saveCloseUp("captions-ru-closeup");

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_kk"));
    QCOMPARE(loadItem(lamp)->caption(), QStringLiteral("Шам 1"));
    checkCaptionsFitOutlines();
    QVERIFY(saveScreenshot(*window, "f07", "captions-kk"));
    saveCloseUp("captions-kk-closeup");

    for (const PlacedLoad &load : window->project().loads())
        QCOMPARE(loadItem(load.id)->pos(), positions.value(load.id));
}

void TestLoading::fitAndRoutingWithFarLoads()
{
    addLoads();
    Project &project = window->project();
    const QString far = addLoad("ceiling-lamp", QPointF(1200, 900));
    QVERIFY(!far.isEmpty());
    const QStringList wires{ addWire("d3:2", far + ":L", 1.5), addWire("nbar:2", far + ":N", 1.5),
                             addWire("pebar:2", far + ":PE", 1.5) };
    QVERIFY(!wires.contains(QString()));

    PlacedLoad moved = *project.load(far);
    moved.position = QPointF(1200, 945);
    QElapsedTimer timer;
    timer.start();
    QVERIFY(project.updateLoad(moved));
    const qint64 elapsed = timer.elapsed();
    QVERIFY2(elapsed < 1000, qPrintable(QStringLiteral("%1 ms").arg(elapsed)));
    QCOMPARE(loadItem(far)->pos(), QPointF(1200, 945));
    QCOMPARE(wireItem(wires.last())->route().last(), scene->terminalPos(far + ":PE"));
    QCOMPARE(scene->terminalPos(far + ":PE"), QPointF(1227, 949.5));

    QVERIFY(!fullyVisible(canvas, project.loadOutline(*project.load(far))));
    named<QAction>("fitAction")->trigger();
    for (const PlacedLoad &load : project.loads())
        QVERIFY2(fullyVisible(canvas, project.loadOutline(load)), qPrintable(load.id));
}

void TestLoading::loadsInLightTheme()
{
    addLoads();
    addLoadWires();
    Theme::apply(Theme::Mode::Light);
    QCoreApplication::processEvents(); // the palette reaches widgets through posted events
    QVERIFY(saveScreenshot(*window, "f07", "light"));
    saveCloseUp("light-closeup");
}

// Each drop point is the intended top-left of the picture plus half its size, so the snapped
// top-left is exact. Top-lefts lie on the 4.5 grid: the TV goes to (301.5, 301.5), not (300, 300).
void TestLoading::dropsLoadsOutsideTheEnclosure()
{
    const Project &project = window->project();
    drop("ceiling-lamp", QPointF(18, 288));
    QCOMPARE(project.loads().size(), 1);
    const PlacedLoad first = project.loads()[0];
    QCOMPARE(first.id, QStringLiteral("l1"));
    QCOMPARE(first.type, QStringLiteral("ceiling-lamp"));
    QCOMPARE(first.position, QPointF(0, 270));
    QCOMPARE(first.power, 10);
    QCOMPARE(scene->selectedLoadId(), QStringLiteral("l1"));
    QCOMPARE(scene->selectedItems().size(), 1);
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(countItems(scene, "loadPreview"), 0);

    drop("socket", QPointF(117, 283.5));
    QCOMPARE(project.loads().size(), 2);
    QCOMPARE(project.loads()[1].type, QStringLiteral("socket"));
    QCOMPARE(project.loads()[1].position, QPointF(90, 270));
    QCOMPARE(scene->selectedLoadId(), project.loads()[1].id);

    drop("tv", QPointF(319.5, 319.5));
    QCOMPARE(project.loads().size(), 3);
    const PlacedLoad tvLoad = project.loads()[2];
    QCOMPARE(tvLoad.type, QStringLiteral("tv"));
    QCOMPARE(tvLoad.position, QPointF(301.5, 301.5));
    QCOMPARE(tvLoad.socket, QString());
    QCOMPARE(tvLoad.place, 0);
    QCOMPARE(scene->selectedLoadId(), tvLoad.id);
    QCOMPARE(window->undoStack().count(), 3);
    QCOMPARE(countItems(scene, "loadPreview"), 0);
    QVERIFY(saveScreenshot(*window, "f07", "dropped"));
}

void TestLoading::refusesDropsOnEnclosureSupplyAndLoads()
{
    lamp = addLoad("ceiling-lamp", QPointF(0, 270));
    QVERIFY(!lamp.isEmpty());
    const Project &project = window->project();
    const QList<PlacedLoad> loads = project.loads();
    const int commands = window->undoStack().count();
    const QString noRoom = QStringLiteral("No room here: something is in the way.");

    clearMessage();
    drop("ceiling-lamp", QRectF(QPointF(0, 0), project.enclosure().size).center());
    QCOMPARE(message(), QStringLiteral("Loads go outside the enclosure."));

    clearMessage();
    drop("ceiling-lamp", sceneItem(scene, "supply")->sceneBoundingRect().center());
    QCOMPARE(message(), noRoom);

    // The "Supply" text belongs to the supply area too. A lamp centred on it, snapped by up to
    // half a grid step, stays right of the supply block.
    const QList<QGraphicsItem *> items = scene->items();
    const auto text = std::ranges::find_if(items, [](QGraphicsItem *item) {
        const auto *textItem = dynamic_cast<TextItem *>(item);
        return textItem && textItem->text() == QStringLiteral("Supply");
    });
    QVERIFY(text != items.end());
    const QPointF onText = (*text)->sceneBoundingRect().center();
    QVERIFY(onText.x() - 18 - 2.25 >= sceneItem(scene, "supply")->sceneBoundingRect().right());
    clearMessage();
    drop("ceiling-lamp", onText);
    QCOMPARE(message(), noRoom);

    clearMessage();
    drop("ceiling-lamp", QPointF(18, 288));
    QCOMPARE(message(), noRoom);

    QCOMPARE(project.loads(), loads);
    QCOMPARE(window->undoStack().count(), commands);
    QCOMPARE(countItems(scene, "loadPreview"), 0);
}

void TestLoading::dropsApplianceOntoSocket()
{
    socket = addLoad("socket", QPointF(90, 270));
    QVERIFY(!socket.isEmpty());
    const Project &project = window->project();
    const PlacedLoad socketLoad = *project.load(socket);
    const QPointF onSocketPicture(117, 283.5);

    drop("kettle", project.plugPlaceRect(socketLoad, 1).center());
    QCOMPARE(project.loads().size(), 2);
    const PlacedLoad kettleLoad = project.loads()[1];
    QCOMPARE(kettleLoad.type, QStringLiteral("kettle"));
    QCOMPARE(kettleLoad.socket, socket);
    QCOMPARE(kettleLoad.place, 1);
    QCOMPARE(scene->selectedLoadId(), kettleLoad.id);
    QCOMPARE(loadItem(kettleLoad.id)->pos(), QPointF(76.5, 310.5));

    // Over the picture, the free place is the second one.
    drop("microwave", onSocketPicture);
    QCOMPARE(project.loads().size(), 3);
    const PlacedLoad microwave = project.loads()[2];
    QCOMPARE(microwave.type, QStringLiteral("microwave"));
    QCOMPARE(microwave.socket, socket);
    QCOMPARE(microwave.place, 2);
    QCOMPARE(project.pluggedInto(socket), (QStringList{ kettleLoad.id, microwave.id }));
    QCOMPARE(loadItem(microwave.id)->pos(), QPointF(121.5, 310.5));
    QCOMPARE(window->undoStack().count(), 2);
    QVERIFY(saveScreenshot(*window, "f07", "dropped-plugged"));

    const QList<PlacedLoad> loads = project.loads();
    clearMessage();
    drop("tv", onSocketPicture);
    QCOMPARE(message(), QStringLiteral("This socket already has two appliances."));
    clearMessage();
    drop("ceiling-lamp", project.plugPlaceRect(socketLoad, 1).center());
    QCOMPARE(message(), QStringLiteral("No room here: something is in the way."));
    QCOMPARE(project.loads(), loads);
    QCOMPARE(window->undoStack().count(), 2);
}

void TestLoading::dragPreviewShowsRefusal()
{
    socket = addLoad("socket", QPointF(90, 270));
    QVERIFY(!socket.isEmpty());
    const Project &project = window->project();
    auto *palette = named<PaletteWidget>("paletteTree");

    dragLoadOver(canvas, palette, "ceiling-lamp", viewportPoint(canvas, QPointF(18, 288)));
    QCOMPARE(countItems(scene, "loadPreview"), 1);
    LoadItem *preview = loadItem("loadPreview");
    QVERIFY(preview);
    QVERIFY(preview->isVisible());
    QCOMPARE(preview->opacity(), 0.6);
    QCOMPARE(preview->zValue(), LoadItem::LiftedZ);
    QCOMPARE(preview->pos(), QPointF(0, 270));
    QVERIFY(!preview->isRefused());
    QVERIFY(saveScreenshot(*window, "f07", "drag-preview"));

    dragLoadOver(canvas, palette, "ceiling-lamp",
                 viewportPoint(canvas, QRectF(QPointF(0, 0), project.enclosure().size).center()));
    QCOMPARE(countItems(scene, "loadPreview"), 1);
    preview = loadItem("loadPreview");
    QVERIFY(preview);
    QVERIFY(preview->isVisible());
    QVERIFY(preview->isRefused());
    QVERIFY(saveScreenshot(*window, "f07", "drag-refused"));

    // Over the socket, an appliance hangs in the place it would go into.
    dragLoadOver(canvas, palette, "kettle",
                 viewportPoint(canvas, project.plugPlaceRect(*project.load(socket), 1).center()));
    QCOMPARE(countItems(scene, "loadPreview"), 1);
    preview = loadItem("loadPreview");
    QVERIFY(preview);
    QCOMPARE(preview->pos(), QPointF(76.5, 310.5));
    QVERIFY(!preview->isRefused());
    saveCloseUp("drag-plug-closeup");

    leaveDrag(canvas);
    QCOMPARE(countItems(scene, "loadPreview"), 0);
    QCOMPARE(project.loads().size(), 1);
    QCOMPARE(window->undoStack().count(), 0);
}

void TestLoading::loadDropKeepsWireToolOn()
{
    auto *wireAction = named<QAction>("wireAction");
    QVERIFY(wireAction);
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(wireAction), Qt::LeftButton);
    QVERIFY(scene->isWireMode());
    drop("ceiling-lamp", QPointF(18, 288));
    QCOMPARE(window->project().loads().size(), 1);
    QVERIFY(scene->isWireMode());
    QVERIFY(wireAction->isChecked());
}

// Drags press on the middle of a picture and release where the middle of the picture should go,
// so the snapped top-left is exact.
void TestLoading::movesLoadByDragging()
{
    addLoads();
    const Project &project = window->project();
    mouseDrag(canvas->viewport(), onPicture(lamp), viewportPoint(canvas, QPointF(18, 378)));
    QCOMPARE(project.load(lamp)->position, QPointF(0, 360));
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(loadItem(lamp)->pos(), QPointF(0, 360));
    QCOMPARE(loadItem(lamp)->zValue(), LoadItem::PlacedZ);
    QCOMPARE(scene->selectedLoadId(), lamp);
    QVERIFY(saveScreenshot(*window, "f07", "moved"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.load(lamp)->position, QPointF(0, 270));
    QCOMPARE(loadItem(lamp)->pos(), QPointF(0, 270));
}

void TestLoading::refusedMoveGoesBack()
{
    addLoads();
    const Project &project = window->project();
    LoadItem *item = loadItem(lamp);
    QWidget *viewport = canvas->viewport();
    const QPoint onEnclosure
            = viewportPoint(canvas, QRectF(QPointF(0, 0), project.enclosure().size).center());
    QTest::mousePress(viewport, Qt::LeftButton, {}, onPicture(lamp));
    QTest::mouseMove(viewport, onEnclosure);
    QVERIFY(item->isRefused());
    QCOMPARE(item->zValue(), LoadItem::LiftedZ);
    QVERIFY(QRectF(QPointF(0, 0), project.enclosure().size)
                    .contains(item->sceneBoundingRect().center()));
    QVERIFY(saveScreenshot(*window, "f07", "moving-refused"));

    clearMessage();
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, onEnclosure);
    QCOMPARE(project.load(lamp)->position, QPointF(0, 270));
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(message(), QStringLiteral("Loads go outside the enclosure."));
    QCOMPARE(item->pos(), QPointF(0, 270));
    QVERIFY(!item->isRefused());
    QCOMPARE(item->zValue(), LoadItem::PlacedZ);
}

// Top-lefts lie on the 4.5 grid: the kettle goes to (301.5, 301.5), not (300, 300).
void TestLoading::unplugsAndReplugsByDragging()
{
    Project &project = window->project();
    socket = addLoad("socket", QPointF(90, 270));
    QVERIFY(!socket.isEmpty());
    PlacedLoad plugged = loadFromType(*project.catalog().loadType(u"kettle"), QPointF());
    plugged.socket = socket;
    plugged.place = 1;
    kettle = project.addLoad(plugged);
    QVERIFY(!kettle.isEmpty());
    const QString second = addLoad("socket", QPointF(189, 270));
    QVERIFY(!second.isEmpty());
    const PlacedLoad before = *project.load(kettle);
    QWidget *viewport = canvas->viewport();

    mouseDrag(viewport, onPicture(kettle), viewportPoint(canvas, QPointF(319.5, 319.5)));
    QCOMPARE(project.load(kettle)->socket, QString());
    QCOMPARE(project.load(kettle)->place, 0);
    QCOMPARE(project.load(kettle)->position, QPointF(301.5, 301.5));
    QCOMPARE(loadItem(kettle)->pos(), QPointF(301.5, 301.5));
    QCOMPARE(window->undoStack().count(), 1);
    QVERIFY(project.pluggedInto(socket).isEmpty());

    // Over the second socket it hangs in the place it would go into.
    const QPoint onPlace
            = viewportPoint(canvas, project.plugPlaceRect(*project.load(second), 2).center());
    QTest::mousePress(viewport, Qt::LeftButton, {}, onPicture(kettle));
    QTest::mouseMove(viewport, onPlace);
    QCOMPARE(loadItem(kettle)->pos(), QPointF(220.5, 310.5));
    QVERIFY(!loadItem(kettle)->isRefused());
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, onPlace);
    QCOMPARE(project.load(kettle)->socket, second);
    QCOMPARE(project.load(kettle)->place, 2);
    QCOMPARE(project.pluggedInto(second), QStringList{ kettle });
    QCOMPARE(loadItem(kettle)->pos(), QPointF(220.5, 310.5));
    QCOMPARE(window->undoStack().count(), 2);
    QVERIFY(saveScreenshot(*window, "f07", "replugged"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(*project.load(kettle), before);
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 310.5));
}

// Over its own socket a plugged appliance stays in its place, also while it moves.
void TestLoading::dropOnOwnSocketIsNoEdit()
{
    addLoads();
    const Project &project = window->project();
    const QList<PlacedLoad> loads = project.loads();
    const int commands = window->undoStack().count();
    QWidget *viewport = canvas->viewport();
    const QPoint onPlace
            = viewportPoint(canvas, project.plugPlaceRect(*project.load(socket), 2).center());
    QVERIFY((onPlace - onPicture(kettle)).manhattanLength() > QApplication::startDragDistance());

    QTest::mousePress(viewport, Qt::LeftButton, {}, onPicture(kettle));
    QTest::mouseMove(viewport, onPlace);
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 310.5));
    QVERIFY(!loadItem(kettle)->isRefused());
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, onPlace);
    QCOMPARE(window->undoStack().count(), commands);
    QCOMPARE(project.load(kettle)->place, 1);
    QCOMPARE(project.loads(), loads);
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 310.5));
}

void TestLoading::movingSocketCarriesAppliances()
{
    addLoads();
    addLoadWires();
    const Project &project = window->project();
    QWidget *viewport = canvas->viewport();
    const QPoint below = viewportPoint(canvas, QPointF(117, 373.5));

    QTest::mousePress(viewport, Qt::LeftButton, {}, onPicture(socket));
    QTest::mouseMove(viewport, below);
    QCOMPARE(loadItem(socket)->pos(), QPointF(90, 360));
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 400.5));
    QCOMPARE(loadItem(kettle)->zValue(), LoadItem::LiftedZ);
    QVERIFY(saveScreenshot(*window, "f07", "socket-moving"));
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, below);

    QCOMPARE(project.load(socket)->position, QPointF(90, 360));
    QCOMPARE(project.load(kettle)->socket, socket);
    QCOMPARE(project.load(kettle)->place, 1);
    QCOMPARE(window->undoStack().count(), 1);
    QCOMPARE(loadItem(socket)->pos(), QPointF(90, 360));
    QCOMPARE(loadItem(kettle)->pos(), project.loadPosition(*project.load(kettle)));
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 400.5));
    QCOMPARE(loadItem(kettle)->zValue(), LoadItem::PlacedZ);
    QCOMPARE(scene->terminalPos(socket + ":L"), QPointF(103.5, 364.5));
    QCOMPARE(wireItem(loadWires.last())->route().last(), scene->terminalPos(socket + ":L"));
    QVERIFY(saveScreenshot(*window, "f07", "socket-moved"));
}

void TestLoading::escCancelsLoadMove()
{
    addLoads();
    const Project &project = window->project();
    const QList<PlacedLoad> loads = project.loads();
    QWidget *viewport = canvas->viewport();
    QTest::mousePress(viewport, Qt::LeftButton, {}, onPicture(socket));
    QTest::mouseMove(viewport, viewportPoint(canvas, QPointF(117, 373.5)));
    QCOMPARE(loadItem(socket)->pos(), QPointF(90, 360));
    QCOMPARE(QApplication::focusWidget(), canvas);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    for (const PlacedLoad &load : loads) {
        QCOMPARE(loadItem(load.id)->pos(), project.loadPosition(load));
        QCOMPARE(loadItem(load.id)->zValue(), LoadItem::PlacedZ);
        QVERIFY(!loadItem(load.id)->isRefused());
    }

    // The rest of the drag no longer moves the socket.
    const QPoint further = viewportPoint(canvas, QPointF(117, 418.5));
    QTest::mouseMove(viewport, further);
    QCOMPARE(loadItem(socket)->pos(), QPointF(90, 270));
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, further);
    QCOMPARE(project.loads(), loads);
    QCOMPARE(window->undoStack().count(), 0);
    QCOMPARE(loadItem(socket)->pos(), QPointF(90, 270));
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 310.5));
}

// A popup opened mid-move takes the left release. The next press ends that move: the socket and
// its appliance go back, and the press doesn't drag them along.
void TestLoading::nextPressEndsALoadMoveThatLostItsRelease()
{
    addLoads();
    const Project &project = window->project();
    const QList<PlacedLoad> loads = project.loads();
    QWindow *handle = window->windowHandle();
    const auto windowPoint = [this](QPoint viewportPos) {
        return canvas->viewport()->mapTo(window.get(), viewportPos);
    };
    const QPoint below = windowPoint(viewportPoint(canvas, QPointF(117, 373.5)));
    QTest::mousePress(handle, Qt::LeftButton, {}, windowPoint(onPicture(socket)));
    QTest::mouseMove(handle, below);
    QCOMPARE(loadItem(socket)->pos(), QPointF(90, 360));
    auto *menu = named<QMenu>("editMenu");
    QVERIFY(menu);
    menu->popup(window->mapToGlobal(QPoint(0, 0)));
    QTRY_VERIFY(menu->isVisible());
    QTest::mouseRelease(handle, Qt::LeftButton, {}, below);
    QTest::keyClick(handle, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());

    const QPoint empty = viewportPoint(canvas, QPointF(200, 450));
    mouseDrag(canvas->viewport(), empty, empty + QPoint(30, 0));
    for (const PlacedLoad &load : loads) {
        QCOMPARE(loadItem(load.id)->pos(), project.loadPosition(load));
        QCOMPARE(loadItem(load.id)->zValue(), LoadItem::PlacedZ);
        QVERIFY(!loadItem(load.id)->isRefused());
    }
    QCOMPARE(project.loads(), loads);
    QCOMPARE(window->undoStack().count(), 0);
}

void TestLoading::noLoadMovesUnderWireTool()
{
    addLoads();
    auto *wireAction = named<QAction>("wireAction");
    QVERIFY(wireAction);
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(wireAction), Qt::LeftButton);
    QVERIFY(scene->isWireMode());
    mouseDrag(canvas->viewport(), onPicture(lamp), viewportPoint(canvas, QPointF(18, 378)));
    QCOMPARE(window->project().load(lamp)->position, QPointF(0, 270));
    QCOMPARE(loadItem(lamp)->pos(), QPointF(0, 270));
    QCOMPARE(loadItem(lamp)->zValue(), LoadItem::PlacedZ);
    QCOMPARE(window->undoStack().count(), 0);
}

void TestLoading::drawnLoadWiresStartAt15mAndJoinTheirCable()
{
    lamp = addLoad("ceiling-lamp", QPointF(0, 270));
    QVERIFY(!lamp.isEmpty());
    Project &project = window->project();
    clickWireButton();
    QVERIFY(scene->isWireMode());

    clickTerminal("d3:2");
    clickTerminal(lamp + ":L");
    const Wire first = project.wires().last();
    QCOMPARE(first.from, QStringLiteral("d3:2"));
    QCOMPARE(first.to, lamp + ":L");
    QCOMPARE(first.length, LoadWireLength);
    QCOMPARE(scene->selectedWireId(), first.id);

    auto *length = named<QDoubleSpinBox>("lengthSpin");
    QVERIFY(length);
    QVERIFY(length->isVisible());
    QCOMPARE(length->value(), LoadWireLength);
    QTest::mouseClick(length, Qt::LeftButton, {},
                      QPoint(length->width() / 4, length->height() / 2));
    QVERIFY(length->hasFocus());
    QTest::keyClick(length, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(length, "8");
    QTest::keyClick(length, Qt::Key_Return);
    QCOMPARE(project.wire(first.id)->length, 8.0);

    clickTerminal("nbar:2");
    clickTerminal(lamp + ":N");
    QCOMPARE(project.wires().last().from, QStringLiteral("nbar:2"));
    QCOMPARE(project.wires().last().to, lamp + ":N");
    QCOMPARE(project.wires().last().length, 8.0);

    // The tutorial already joins QF1 2 to QD1 1: that wire is removed, then drawn again.
    const auto tutorialWire = std::ranges::find_if(project.wires(), [](const Wire &wire) {
        return wire.from == QStringLiteral("d1:2") && wire.to == QStringLiteral("d2:1");
    });
    QVERIFY(tutorialWire != project.wires().end());
    const QString tutorialWireId = tutorialWire->id;
    QVERIFY(project.removeWire(tutorialWireId));
    clickTerminal("d1:2");
    clickTerminal("d2:1");
    QCOMPARE(project.wires().last().from, QStringLiteral("d1:2"));
    QCOMPARE(project.wires().last().to, QStringLiteral("d2:1"));
    QCOMPARE(project.wires().last().length, 0.3);
}

void TestLoading::undoCancelsWireFromRemovedLoad()
{
    const Project &project = window->project();
    drop("ceiling-lamp", QPointF(18, 288));
    QCOMPARE(project.loads().size(), 1);
    const QString dropped = project.loads().last().id;
    clickWireButton();
    QVERIFY(scene->isWireMode());
    clickTerminal(dropped + ":L");
    QCOMPARE(scene->wireStart(), dropped + ":L");
    auto *preview = qgraphicsitem_cast<QGraphicsLineItem *>(sceneItem(scene, "wirePreview"));
    QVERIFY(preview);
    QVERIFY(preview->isVisible());

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(project.loads().isEmpty());
    QVERIFY(scene->wireStart().isEmpty());
    QVERIFY(!preview->isVisible());
    QVERIFY(scene->isWireMode());

    clickTerminal("d3:2");
    QCOMPARE(scene->wireStart(), QStringLiteral("d3:2"));
}

// The F07 roadmap scenario on the tutorial panel, all through the UI.
void TestLoading::roadmapScenario()
{
    Project &project = window->project();
    const qsizetype panelWires = project.wires().size(); // w1 to w9

    // A lamp and a socket below the enclosure, and a kettle plugged into the socket.
    drop("ceiling-lamp", QPointF(18, 288));
    drop("socket", QPointF(117, 283.5));
    QCOMPARE(project.loads().size(), 2);
    lamp = project.loads()[0].id;
    socket = project.loads()[1].id;
    QCOMPARE(project.load(lamp)->position, QPointF(0, 270));
    QCOMPARE(project.load(socket)->position, QPointF(90, 270));
    QVERIFY(saveScreenshot(*window, "f07", "scenario-placed"));
    drop("kettle", project.plugPlaceRect(*project.load(socket), 1).center());
    QCOMPARE(project.loads().size(), 3);
    kettle = project.loads()[2].id;
    QCOMPARE(project.load(kettle)->type, QStringLiteral("kettle"));
    QVERIFY(saveScreenshot(*window, "f07", "scenario-plugged"));

    // The lighting line in 1.5 mm², the socket line in 2.5 mm².
    clickWireButton();
    QVERIFY(scene->isWireMode());
    chooseSection(1.5);
    clickTerminal("d3:2");
    clickTerminal(lamp + ":L");
    clickTerminal("nbar:2");
    clickTerminal(lamp + ":N");
    clickTerminal("pebar:2");
    clickTerminal(lamp + ":PE");
    chooseSection(2.5);
    clickTerminal("d4:2");
    clickTerminal(socket + ":L");
    clickTerminal("nbar:3");
    clickTerminal(socket + ":N");
    clickTerminal("pebar:3");
    clickTerminal(socket + ":PE");
    QVERIFY(saveScreenshot(*window, "f07", "scenario-wired"));

    QCOMPARE(project.loads().size(), 3);
    QCOMPARE(project.pluggedInto(socket), QStringList{ kettle });
    QCOMPARE(project.load(kettle)->place, 1);
    QCOMPARE(project.wires().size(), panelWires + 6);
    QCOMPARE(project.wires().mid(panelWires),
             (QList<Wire>{
                     Wire{ "w10", "d3:2", lamp + ":L", 1.5, Conductor::L, LoadWireLength },
                     Wire{ "w11", "nbar:2", lamp + ":N", 1.5, Conductor::N, LoadWireLength },
                     Wire{ "w12", "pebar:2", lamp + ":PE", 1.5, Conductor::PE, LoadWireLength },
                     Wire{ "w13", "d4:2", socket + ":L", 2.5, Conductor::L, LoadWireLength },
                     Wire{ "w14", "nbar:3", socket + ":N", 2.5, Conductor::N, LoadWireLength },
                     Wire{ "w15", "pebar:3", socket + ":PE", 2.5, Conductor::PE,
                           LoadWireLength } }));

    // Deleting the socket takes its wires, and the kettle stays where it hung, unplugged.
    clickWireButton();
    QVERIFY(!scene->isWireMode());
    const QList<PlacedLoad> loads = project.loads();
    const QList<Wire> wires = project.wires();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, onPicture(socket));
    QCOMPARE(scene->selectedLoadId(), socket);
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QVERIFY(!project.load(socket));
    QVERIFY(!sceneItem(scene, socket));
    QCOMPARE(project.wires(), wires.first(panelWires + 3));
    QCOMPARE(project.load(kettle)->socket, QString());
    QCOMPARE(project.load(kettle)->place, 0);
    QCOMPARE(project.load(kettle)->position, QPointF(76.5, 310.5));
    QCOMPARE(loadItem(kettle)->pos(), QPointF(76.5, 310.5));
    QVERIFY(saveScreenshot(*window, "f07", "scenario-socket-deleted"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.loads(), loads);
    QCOMPARE(project.wires(), wires);
    for (const PlacedLoad &load : loads)
        QCOMPARE(loadItem(load.id)->pos(), project.loadPosition(load));
    for (const Wire &wire : wires)
        QVERIFY2(wireItem(wire.id), qPrintable(wire.id));
    QVERIFY(saveScreenshot(*window, "f07", "scenario-undone"));

    // A 24-module enclosure is 124 taller: the loads below it move down, and their wires follow.
    QHash<QString, QPointF> positions;
    for (const PlacedLoad &load : loads)
        positions.insert(load.id, project.loadPosition(load));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, scene->slotRect(0, 9, 1).center()));
    QVERIFY(scene->isEnclosureSelected());
    chooseComboItem(named<QComboBox>("enclosureSizeCombo"), "24");
    QCOMPARE(project.enclosure().id, QStringLiteral("enclosure-24"));
    QCOMPARE(project.load(lamp)->position, QPointF(0, 394));
    QCOMPARE(project.load(socket)->position, QPointF(90, 394));
    QCOMPARE(project.pluggedInto(socket), QStringList{ kettle });
    for (const PlacedLoad &load : project.loads()) {
        QCOMPARE(project.loadPosition(load), positions.value(load.id) + QPointF(0, 124));
        QCOMPARE(loadItem(load.id)->pos(), project.loadPosition(load));
    }
    QCOMPARE(project.wires(), wires);
    QCOMPARE(scene->terminalPos(lamp + ":L"), QPointF(9, 398.5));
    QCOMPARE(scene->terminalPos(socket + ":L"), QPointF(103.5, 398.5));
    for (const Wire &wire : wires) {
        const QPolygonF route = wireItem(wire.id)->route();
        QVERIFY2(route.size() >= 2, qPrintable(wire.id));
        QCOMPARE(route.first(), scene->terminalPos(wire.from));
        QCOMPARE(route.last(), scene->terminalPos(wire.to));
    }
    QVERIFY(saveScreenshot(*window, "f07", "scenario-enclosure-24"));
}

QTEST_MAIN(TestLoading)
#include "tst_loading.moc"
