// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/mainwindow.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "app/theme.h"
#include "core/project.h"
#include "editorhelpers.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAction>
#include <QElapsedTimer>
#include <QHash>
#include <QMenu>
#include <QTest>

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
    void selectsAndDeletesLoadWithUndo();
    void captionsFollowLanguage();
    void fitAndRoutingWithFarLoads();
    void loadsInLightTheme();

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

QTEST_MAIN(TestLoading)
#include "tst_loading.moc"
