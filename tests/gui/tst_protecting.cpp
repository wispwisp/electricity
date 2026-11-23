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
#include <QMenu>
#include <QStatusBar>
#include <QTest>
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
    void hintThenBackToTheEvent();

private:
    void zoomOut();
    [[nodiscard]] DeviceItem *deviceItem(const QString &id) const;
    [[nodiscard]] QPoint onHandle(const QString &id) const;
    void clickHandle(const QString &id);
    [[nodiscard]] QString message() const;
    // The tutorial loads, zoomed out, with QF1, QD1 and QF3 on and a short circuit on Socket 1:
    // QF3 trips.
    void tripQf3();
    // The wire with these ends, from "d3:2>l1:L"; empty if none.
    [[nodiscard]] QString wireId(const QString &ends) const;
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

QString TestProtecting::message() const
{
    return window->statusBar()->currentMessage();
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

void TestProtecting::hintThenBackToTheEvent()
{
    tripQf3();
    dropPreset(canvas, named<PaletteWidget>("paletteTree"), "breaker-1p-c16",
               viewportPoint(canvas, scene->slotRect(0, 1, 1).center()));
    QCOMPARE(message(), QStringLiteral("No room here: another device is in the way."));
    QTRY_COMPARE_WITH_TIMEOUT(message(), kQf3Short, 7000);
}

QTEST_MAIN(TestProtecting)
#include "tst_protecting.moc"
