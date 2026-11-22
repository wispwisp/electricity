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
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStatusBar>
#include <QTest>
#include <QToolBar>
#include <QUndoStack>

#include <algorithm>
#include <memory>

class TestProperties : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void showsHintWhenNothingSelected();
    void editsBreakerRating();
    void refusesPoleChangeWithoutRoom();
    void showsRcdAndRcbo();
    void changesEnclosureSize();
    void panelFollowsUndoOfSelectedDevice();
    void deleteKeyInComboKeepsDevice();
    void choosingTheShownValueAddsNoUndoStep();
    void retranslatesAndFollowsTheme();
    void showsAndEditsWireSection();
    void refusesPoleChangeThatOrphansWires();
    void refusesShrinkThatOrphansBarWires();
    void wirePropertiesTranslated();
    void showsLampPowerAndEditsIt();
    void showsApplianceAndWhereItIsPlugged();
    void showsWhatASocketHas();
    void wireToLoadShowsLengthAndEditsCable();
    void editFinishedByClickingAnotherObject();
    void directSwitchCommitsTypedValueToItsObject();
    void refusesShrinkWhenLoadsWouldCollide();
    void panelFollowsUndoOfSelectedLoad();
    void undoAndRedoKeysWorkInTheFields();
    void loadPropertiesTranslated();

private:
    [[nodiscard]] QPoint at(int row, int module) const;
    void drop(const QString &presetId, int row, int module, int modules = 1);
    void select(int row, int module);
    // The layout settles after the event loop runs; every shown caption must then fit its text.
    void settleAndCheckFit();
    void selectEnclosure();
    void selectWire(const QString &id);
    [[nodiscard]] static QStringList items(const QComboBox *combo);
    // The area around the enclosure, where loads go.
    void zoomOut();
    QString addLoad(const QString &type, QPointF position);
    // Lamp 1 (0, 270) and Socket 1 (90, 270) with a kettle in place 1 and a microwave in place 2;
    // zooms out to them.
    void addLoads();
    // The tutorial panel, Lamp 1's cable (QF2 2, N bar 2, PE bar 2) and QF3 2 → Socket 1 L.
    void addLoadWires();
    // The centre of a load's picture, or the midpoint of a wire's longest segment.
    [[nodiscard]] QPointF pointOn(const QString &id) const;
    void clickLoad(const QString &id);
    // Clicks the midpoint of the wire's longest segment, which must select it.
    void clickWire(const QString &id);
    // A click sent to the scene alone: the field keeps the focus, and the selection switches
    // straight to the clicked object in one selectionChanged.
    void clickInScene(const QString &id);
    // Clicks into the field and replaces its number with text, without Enter.
    void typeInto(QAbstractSpinBox *spin, const QString &text);
    void clickUndo();
    [[nodiscard]] bool showsLabel(const QString &text) const;
    void chooseLanguage(const char *code);
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
    PaletteWidget *palette = nullptr;
    QString lamp, socket, kettle, microwave;
    QStringList lampWires; // L, N, PE
    QString socketWire;
};

QPoint TestProperties::at(int row, int module) const
{
    return viewportPoint(canvas, scene->slotRect(row, module, 1).center());
}

void TestProperties::drop(const QString &presetId, int row, int module, int modules)
{
    dropPreset(canvas, palette, presetId,
               viewportPoint(canvas, scene->slotRect(row, module, modules).center()));
}

void TestProperties::select(int row, int module)
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(row, module));
}

void TestProperties::settleAndCheckFit()
{
    QTest::qWait(50);
    for (const QLabel *label : named<QWidget>("propertiesPanel")->findChildren<QLabel *>()) {
        if (label->isVisible() && !label->wordWrap())
            QVERIFY2(label->width() >= label->sizeHint().width(), qPrintable(label->text()));
    }
}

// An empty rail slot belongs to the enclosure.
void TestProperties::selectEnclosure()
{
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 8));
    QVERIFY(scene->isEnclosureSelected());
}

QStringList TestProperties::items(const QComboBox *combo)
{
    QStringList result;
    for (int i = 0; i < combo->count(); ++i)
        result << combo->itemText(i);
    return result;
}

void TestProperties::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestProperties::init()
{
    window = std::make_unique<MainWindow>();
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    canvas = named<CanvasView>("canvasView");
    QVERIFY(canvas);
    scene = qobject_cast<EditorScene *>(canvas->scene());
    QVERIFY(scene);
    palette = named<PaletteWidget>("paletteTree");
    QVERIFY(palette);
}

void TestProperties::cleanup()
{
    window.reset();
}

void TestProperties::showsHintWhenNothingSelected()
{
    auto *hint = named<QLabel>("propertiesHint");
    QVERIFY(hint);
    QVERIFY(hint->isVisible());
    QCOMPARE(hint->text(), QStringLiteral("Select a device on the panel to see its properties."));
    QVERIFY(!named<QComboBox>("ratingCombo")->isVisible());
    QVERIFY(!named<QComboBox>("enclosureSizeCombo")->isVisible());
    QVERIFY(saveScreenshot(*window, "f05", "properties-empty"));
}

void TestProperties::editsBreakerRating()
{
    drop("breaker-1p-c16", 0, 4);
    QCOMPARE(named<QLabel>("designationLabel")->text(), QStringLiteral("QF1"));
    QCOMPARE(named<QLabel>("typeNameLabel")->text(), QStringLiteral("Circuit breaker"));
    auto *rating = named<QComboBox>("ratingCombo");
    QCOMPARE(
            items(rating),
            (QStringList{ "6 A", "10 A", "16 A", "20 A", "25 A", "32 A", "40 A", "50 A", "63 A" }));
    QCOMPARE(rating->currentText(), QStringLiteral("16 A"));
    auto *poles = named<QComboBox>("polesCombo");
    QCOMPARE(items(poles), (QStringList{ "1P", "2P" }));
    QCOMPARE(poles->currentText(), QStringLiteral("1P"));
    QCOMPARE(named<QLabel>("curveValueLabel")->text(), QStringLiteral("C"));
    QVERIFY(!named<QLabel>("residualValueLabel")->isVisible());

    chooseComboItem(rating, "25 A");
    QCOMPARE(window->project().device(QStringLiteral("d1"))->parameters["ratedCurrent"].toInt(),
             25);
    auto *item = static_cast<DeviceItem *>(sceneItem(scene, "d1"));
    QCOMPARE(item->rating(), QStringLiteral("C25"));
    QVERIFY(saveScreenshot(*window, "f05", "properties-breaker"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(window->project().device(QStringLiteral("d1"))->parameters["ratedCurrent"].toInt(),
             16);
    QCOMPARE(rating->currentText(), QStringLiteral("16 A"));
}

void TestProperties::refusesPoleChangeWithoutRoom()
{
    const QString refusal = QStringLiteral("Not enough room next to the device for more poles.");
    drop("breaker-1p-c16", 0, 4);
    drop("breaker-1p-c16", 0, 5);
    select(0, 4);
    auto *poles = named<QComboBox>("polesCombo");
    const int undoCount = window->undoStack().count();
    chooseComboItem(poles, "2P");
    QCOMPARE(window->project().device(QStringLiteral("d1"))->poles, QStringLiteral("1P"));
    QCOMPARE(poles->currentText(), QStringLiteral("1P"));
    QCOMPARE(window->statusBar()->currentMessage(), refusal);
    QCOMPARE(window->undoStack().count(), undoCount);

    select(0, 5);
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QCOMPARE(window->project().devices().size(), 1);
    select(0, 4);
    chooseComboItem(poles, "2P");
    QCOMPARE(window->project().device(QStringLiteral("d1"))->poles, QStringLiteral("2P"));
    QCOMPARE(sceneItem(scene, "d1")->boundingRect().width(), 36.0);
    QCOMPARE(named<QLabel>("designationLabel")->text(), QStringLiteral("QF1"));

    drop("breaker-1p-c16", 0, 11);
    select(0, 11);
    window->statusBar()->clearMessage();
    chooseComboItem(poles, "2P");
    QCOMPARE(window->project().device(QStringLiteral("d3"))->poles, QStringLiteral("1P"));
    QCOMPARE(window->statusBar()->currentMessage(), refusal);
}

void TestProperties::showsRcdAndRcbo()
{
    drop("rcd-2p-40-30", 0, 0, 2);
    QCOMPARE(named<QLabel>("polesValueLabel")->text(), QStringLiteral("2P"));
    QVERIFY(!named<QComboBox>("polesCombo")->isVisible());
    QCOMPARE(named<QLabel>("residualValueLabel")->text(), QStringLiteral("30 mA"));
    QVERIFY(!named<QLabel>("curveValueLabel")->isVisible());
    QCOMPARE(items(named<QComboBox>("ratingCombo")),
             (QStringList{ "25 A", "32 A", "40 A", "63 A" }));

    drop("rcbo-c16-30", 0, 4);
    QCOMPARE(named<QLabel>("polesValueLabel")->text(), QStringLiteral("1P+N"));
    QCOMPARE(named<QLabel>("curveValueLabel")->text(), QStringLiteral("C"));
    QCOMPARE(named<QLabel>("residualValueLabel")->text(), QStringLiteral("30 mA"));
}

void TestProperties::changesEnclosureSize()
{
    const QString refusal
            = QStringLiteral("Move or delete the devices on the rows that would be removed first.");
    selectEnclosure();
    auto *size = named<QComboBox>("enclosureSizeCombo");
    QVERIFY(size->isVisible());
    QCOMPARE(items(size), (QStringList{ "12", "24", "36" }));
    QCOMPARE(size->currentText(), QStringLiteral("12"));

    chooseComboItem(size, "24");
    QCOMPARE(window->project().enclosure().id, QStringLiteral("enclosure-24"));
    QCOMPARE(countItems(scene, "nbar"), 12);
    QVERIFY(size->isVisible());
    QCOMPARE(size->currentText(), QStringLiteral("24"));

    drop("breaker-1p-c16", 1, 0);
    selectEnclosure();
    chooseComboItem(size, "12");
    QCOMPARE(window->project().enclosure().id, QStringLiteral("enclosure-24"));
    QCOMPARE(window->statusBar()->currentMessage(), refusal);
    QCOMPARE(size->currentText(), QStringLiteral("24"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    selectEnclosure();
    chooseComboItem(size, "12");
    QCOMPARE(window->project().enclosure().id, QStringLiteral("enclosure-12"));
    QVERIFY(saveScreenshot(*window, "f05", "properties-enclosure"));
}

void TestProperties::panelFollowsUndoOfSelectedDevice()
{
    drop("breaker-1p-c16", 0, 0);
    QCOMPARE(named<QLabel>("designationLabel")->text(), QStringLiteral("QF1"));
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(named<QLabel>("propertiesHint")->isVisible());
    QVERIFY(!named<QComboBox>("ratingCombo")->isVisible());
    QTest::keyClick(window.get(), Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(window->project().devices().size(), 1);
    QVERIFY(named<QLabel>("propertiesHint")->isVisible());
    QVERIFY(!named<QComboBox>("ratingCombo")->isVisible());
}

void TestProperties::deleteKeyInComboKeepsDevice()
{
    drop("breaker-1p-c16", 0, 0);
    auto *rating = named<QComboBox>("ratingCombo");
    rating->setFocus();
    QTest::keyClick(rating, Qt::Key_Delete);
    QCOMPARE(window->project().devices().size(), 1);
}

// A drop-down reports a choice even when it is the value already shown; that is no edit, so Ctrl+Z
// must not first undo an invisible step.
void TestProperties::choosingTheShownValueAddsNoUndoStep()
{
    drop("breaker-1p-c16", 0, 4);
    const int undoCount = window->undoStack().count();
    chooseComboItem(named<QComboBox>("ratingCombo"), "16 A");
    QCOMPARE(window->undoStack().count(), undoCount);
    chooseComboItem(named<QComboBox>("polesCombo"), "1P");
    QCOMPARE(window->undoStack().count(), undoCount);
    selectEnclosure();
    chooseComboItem(named<QComboBox>("enclosureSizeCombo"), "12");
    QCOMPARE(window->undoStack().count(), undoCount);
}

void TestProperties::retranslatesAndFollowsTheme()
{
    drop("breaker-1p-c16", 0, 4);
    auto *languageMenu = named<QMenu>("languageMenu");
    auto *dock = named<QDockWidget>("propertiesDock");
    QVERIFY(dock);
    QCOMPARE(dock->windowTitle(), QStringLiteral("Properties"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_ru"));
    QCOMPARE(dock->windowTitle(), QStringLiteral("Свойства"));
    QCOMPARE(named<QMenu>("editMenu")->title(), QStringLiteral("Правка"));
    bool found = false;
    for (const QLabel *label :
         window->findChild<QWidget *>("propertiesPanel")->findChildren<QLabel *>())
        found = found || label->text() == QStringLiteral("Номинал");
    QVERIFY(found);
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f05", "properties-ru"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_kk"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f05", "properties-kk"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_en"));
    QCOMPARE(dock->windowTitle(), QStringLiteral("Properties"));

    auto *settingsMenu = named<QMenu>("settingsMenu");
    auto *themeMenu = named<QMenu>("themeMenu");
    clickMenuBarItem(*window, settingsMenu);
    openSubmenu(settingsMenu, themeMenu);
    clickMenuItem(themeMenu, named<QAction>("themeAction_light"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f05", "properties-light"));
    clickMenuBarItem(*window, settingsMenu);
    openSubmenu(settingsMenu, themeMenu);
    clickMenuItem(themeMenu, named<QAction>("themeAction_dark"));
}

void TestProperties::selectWire(const QString &id)
{
    auto *item = sceneItem(scene, id);
    QVERIFY(item);
    scene->clearSelection();
    item->setSelected(true);
    QCOMPARE(scene->selectedWireId(), id);
}

void TestProperties::showsAndEditsWireSection()
{
    addTutorialDevices(window->project());
    addTutorialWires(window->project());
    selectWire("w1");
    auto *box = named<QWidget>("wireBox");
    QVERIFY(box);
    QVERIFY(box->isVisible());
    QVERIFY(!named<QComboBox>("ratingCombo")->isVisible());
    QVERIFY(!named<QComboBox>("enclosureSizeCombo")->isVisible());
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("Supply L \u2192 QF1 1"));
    auto *section = named<QComboBox>("sectionCombo");
    QCOMPARE(section->currentText(), QStringLiteral("10 mm\u00b2"));
    QCOMPARE(items(section),
             (QStringList{ "1.5 mm\u00b2", "2.5 mm\u00b2", "4 mm\u00b2", "6 mm\u00b2",
                           "10 mm\u00b2", "16 mm\u00b2" }));

    const int undoCount = window->undoStack().count();
    chooseComboItem(section, "16 mm\u00b2");
    QCOMPARE(window->project().wire(QStringLiteral("w1"))->section, 16.0);
    QCOMPARE(window->undoStack().count(), undoCount + 1);
    QVERIFY(saveScreenshot(*window, "f06", "properties-wire"));
    chooseComboItem(section, "16 mm\u00b2");
    QCOMPARE(window->undoStack().count(), undoCount + 1);
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(window->project().wire(QStringLiteral("w1"))->section, 10.0);
    QCOMPARE(section->currentText(), QStringLiteral("10 mm\u00b2"));

    selectWire("w8");
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("QD1 N2 \u2192 N bar 1"));
    selectWire("w9");
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("Supply PE \u2192 PE bar 1"));
}

void TestProperties::refusesPoleChangeThatOrphansWires()
{
    drop("breaker-2p-c40", 0, 6, 2);
    QCOMPARE(window->project().device(QStringLiteral("d1"))->poles, QStringLiteral("2P"));
    Wire wire;
    wire.from = QStringLiteral("supply:L");
    wire.to = QStringLiteral("d1:3");
    QVERIFY(!window->project().addWire(wire).isEmpty());
    select(0, 6);
    auto *poles = named<QComboBox>("polesCombo");
    const int undoCount = window->undoStack().count();
    chooseComboItem(poles, "1P");
    QCOMPARE(window->statusBar()->currentMessage(),
             QStringLiteral("Remove the wires from terminals 3 first."));
    QCOMPARE(window->project().device(QStringLiteral("d1"))->poles, QStringLiteral("2P"));
    QCOMPARE(poles->currentText(), QStringLiteral("2P"));
    QCOMPARE(window->undoStack().count(), undoCount);
}

void TestProperties::refusesShrinkThatOrphansBarWires()
{
    QVERIFY(window->project().setEnclosure(QStringLiteral("enclosure-24")));
    Wire wire;
    wire.from = QStringLiteral("supply:N");
    wire.to = QStringLiteral("nbar:12");
    QVERIFY(!window->project().addWire(wire).isEmpty());
    selectEnclosure();
    auto *size = named<QComboBox>("enclosureSizeCombo");
    chooseComboItem(size, "12");
    QCOMPARE(window->statusBar()->currentMessage(),
             QStringLiteral("Remove the wires from the bar holes that would be removed first."));
    QCOMPARE(window->project().enclosure().id, QStringLiteral("enclosure-24"));
    QCOMPARE(size->currentText(), QStringLiteral("24"));
}

void TestProperties::wirePropertiesTranslated()
{
    addTutorialDevices(window->project());
    addTutorialWires(window->project());
    selectWire("w1");
    auto *languageMenu = named<QMenu>("languageMenu");
    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_ru"));
    bool title = false;
    for (const QLabel *label : named<QWidget>("wireBox")->findChildren<QLabel *>())
        title = title || label->text() == QStringLiteral("Провод");
    QVERIFY(title);
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("Ввод L \u2192 QF1 1"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f06", "properties-wire-ru"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_kk"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f06", "properties-wire-kk"));

    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu, named<QAction>("languageAction_en"));
}

void TestProperties::zoomOut()
{
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
}

QString TestProperties::addLoad(const QString &type, QPointF position)
{
    return window->project().addLoad(
            loadFromType(*window->project().catalog().loadType(type), position));
}

void TestProperties::addLoads()
{
    Project &project = window->project();
    lamp = addLoad("ceiling-lamp", QPointF(0, 270));
    socket = addLoad("socket", QPointF(90, 270));
    const auto plugIn = [&](const QString &type, int place) {
        PlacedLoad appliance = loadFromType(*project.catalog().loadType(type), QPointF());
        appliance.socket = socket;
        appliance.place = place;
        return project.addLoad(appliance);
    };
    kettle = plugIn("kettle", 1);
    microwave = plugIn("microwave", 2);
    QVERIFY(!lamp.isEmpty() && !socket.isEmpty() && !kettle.isEmpty() && !microwave.isEmpty());
    zoomOut();
}

void TestProperties::addLoadWires()
{
    Project &project = window->project();
    addTutorialDevices(project);
    addTutorialWires(project);
    const auto addWire = [&](const QString &from, const QString &to) {
        Wire wire{ {}, from, to, 1.5, project.conductorFor(from, to) };
        wire.length = project.newWireLength(from, to);
        return project.addWire(wire);
    };
    lampWires = { addWire("d3:2", lamp + ":L"), addWire("nbar:2", lamp + ":N"),
                  addWire("pebar:2", lamp + ":PE") };
    socketWire = addWire("d4:2", socket + ":L");
    QVERIFY(!lampWires.contains(QString()) && !socketWire.isEmpty());
}

QPointF TestProperties::pointOn(const QString &id) const
{
    if (const PlacedLoad *load = window->project().load(id)) {
        const QSizeF size = window->project().catalog().loadType(load->type)->size;
        return QRectF(window->project().loadPosition(*load), size).center();
    }
    const QPolygonF route = dynamic_cast<WireItem *>(sceneItem(scene, id))->route();
    QLineF longest;
    for (qsizetype i = 1; i < route.size(); ++i) {
        if (QLineF(route[i - 1], route[i]).length() > longest.length())
            longest = QLineF(route[i - 1], route[i]);
    }
    return longest.center();
}

void TestProperties::clickLoad(const QString &id)
{
    QVERIFY(window->project().load(id));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, viewportPoint(canvas, pointOn(id)));
    QCOMPARE(scene->selectedLoadId(), id);
}

void TestProperties::clickWire(const QString &id)
{
    QVERIFY(dynamic_cast<WireItem *>(sceneItem(scene, id)));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, viewportPoint(canvas, pointOn(id)));
    QCOMPARE(scene->selectedWireId(), id);
}

void TestProperties::clickInScene(const QString &id)
{
    const QPointF pos = pointOn(id);
    for (const QEvent::Type type :
         { QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease }) {
        QGraphicsSceneMouseEvent event(type);
        event.setScenePos(pos);
        event.setButtonDownScenePos(Qt::LeftButton, pos);
        event.setButton(Qt::LeftButton);
        event.setButtons(type == QEvent::GraphicsSceneMousePress ? Qt::LeftButton : Qt::NoButton);
        QCoreApplication::sendEvent(scene, &event);
    }
}

void TestProperties::typeInto(QAbstractSpinBox *spin, const QString &text)
{
    QTest::mouseClick(spin, Qt::LeftButton, {}, QPoint(spin->width() / 4, spin->height() / 2));
    QVERIFY(spin->hasFocus());
    QTest::keyClick(spin, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(spin, text);
}

// Undoes from the toolbar, like a user would with the mouse.
void TestProperties::clickUndo()
{
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(named<QAction>("undoAction")),
                      Qt::LeftButton);
}

bool TestProperties::showsLabel(const QString &text) const
{
    const QList<QLabel *> labels = named<QWidget>("propertiesPanel")->findChildren<QLabel *>();
    return std::ranges::any_of(labels, [&](const QLabel *label) {
        return label->isVisible() && label->text() == text;
    });
}

void TestProperties::chooseLanguage(const char *code)
{
    auto *languageMenu = named<QMenu>("languageMenu");
    clickMenuBarItem(*window, languageMenu);
    clickMenuItem(languageMenu,
                  window->findChild<QAction *>(QStringLiteral("languageAction_%1").arg(code)));
}

void TestProperties::showsLampPowerAndEditsIt()
{
    addLoads();
    clickLoad(lamp);
    QVERIFY(named<QWidget>("loadBox")->isVisible());
    QVERIFY(!named<QLabel>("propertiesHint")->isVisible());
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Lamp 1"));
    QCOMPARE(named<QLabel>("loadTypeLabel")->text(), QStringLiteral("Ceiling lamp"));
    QVERIFY(showsLabel("Power"));
    auto *power = named<QSpinBox>("powerSpin");
    QVERIFY(power->isVisible());
    QCOMPARE(power->value(), 10);
    QCOMPARE(power->text(), QStringLiteral("10 W"));
    QCOMPARE(power->minimum(), 1);
    QCOMPARE(power->maximum(), MaxLoadPower);
    QVERIFY(!named<QLabel>("plugStatusLabel")->isVisible());
    QVERIFY(!named<QLabel>("socketContentsLabel")->isVisible());

    const int undoCount = window->undoStack().count();
    typeInto(power, "15");
    QCOMPARE(window->project().load(lamp)->power, 10); // no keyboard tracking
    QTest::keyClick(power, Qt::Key_Return);
    QCOMPARE(window->project().load(lamp)->power, 15);
    QCOMPARE(window->undoStack().count(), undoCount + 1);
    typeInto(power, "15");
    QTest::keyClick(power, Qt::Key_Return);
    QCOMPARE(window->undoStack().count(), undoCount + 1);

    clickUndo();
    QCOMPARE(window->project().load(lamp)->power, 10);
    QCOMPARE(power->value(), 10);
}

void TestProperties::showsApplianceAndWhereItIsPlugged()
{
    addLoads();
    clickLoad(kettle);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Kettle"));
    QVERIFY(!named<QLabel>("loadTypeLabel")->isVisible()); // it would repeat the name
    QCOMPARE(named<QSpinBox>("powerSpin")->value(), 2000);
    auto *status = named<QLabel>("plugStatusLabel");
    QVERIFY(status->isVisible());
    QCOMPARE(status->text(), QStringLiteral("Plugged into Socket 1"));
    QVERIFY(!named<QLabel>("socketContentsLabel")->isVisible());

    Project &project = window->project();
    PlacedLoad unplugged = *project.load(kettle);
    unplugged.position = QPointF(198, 270);
    unplugged.socket.clear();
    unplugged.place = 0;
    QVERIFY(project.updateLoad(unplugged));
    QCOMPARE(scene->selectedLoadId(), kettle);
    QCOMPARE(status->text(), QStringLiteral("Not plugged in"));
}

void TestProperties::showsWhatASocketHas()
{
    addLoads();
    clickLoad(socket);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Socket 1"));
    QCOMPARE(named<QLabel>("loadTypeLabel")->text(), QStringLiteral("Double socket"));
    QVERIFY(showsLabel("Plugged in"));
    auto *contents = named<QLabel>("socketContentsLabel");
    QVERIFY(contents->isVisible());
    QCOMPARE(contents->text(), QStringLiteral("Kettle, 2000 W\nMicrowave, 1200 W"));
    QVERIFY(!named<QSpinBox>("powerSpin")->isVisible());
    QVERIFY(!named<QLabel>("plugStatusLabel")->isVisible());

    const QString empty = addLoad("socket", QPointF(198, 270));
    QVERIFY(!empty.isEmpty());
    clickLoad(empty);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Socket 2"));
    QCOMPARE(contents->text(), QStringLiteral("Nothing plugged in"));
}

void TestProperties::wireToLoadShowsLengthAndEditsCable()
{
    addLoads();
    addLoadWires();
    selectWire(lampWires[0]);
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("QF2 2 \u2192 Lamp 1 L"));
    QVERIFY(showsLabel("Length"));
    auto *length = named<QDoubleSpinBox>("lengthSpin");
    QVERIFY(length->isVisible());
    QCOMPARE(length->value(), LoadWireLength);
    QCOMPARE(length->text(), QStringLiteral("15.0 m"));
    QCOMPARE(length->minimum(), 0.5);
    QCOMPARE(length->maximum(), MaxWireLength);
    QCOMPARE(length->singleStep(), 0.5);
    auto *note = named<QLabel>("cableNoteLabel");
    QVERIFY(note->isVisible());
    QCOMPARE(note->text(),
             QStringLiteral("Applies to the whole cable: every wire between the same two places."));

    Project &project = window->project();
    const int undoCount = window->undoStack().count();
    typeInto(length, "8");
    QTest::keyClick(length, Qt::Key_Return);
    for (const QString &id : lampWires)
        QCOMPARE(project.wire(id)->length, 8.0);
    QCOMPARE(project.wire(socketWire)->length, LoadWireLength);
    QCOMPARE(window->undoStack().count(), undoCount + 1);
    typeInto(length, "8");
    QTest::keyClick(length, Qt::Key_Return);
    QCOMPARE(window->undoStack().count(), undoCount + 1);

    clickUndo();
    for (const QString &id : lampWires)
        QCOMPARE(project.wire(id)->length, LoadWireLength);
    QCOMPARE(length->value(), LoadWireLength);

    selectWire("w1");
    QVERIFY(named<QComboBox>("sectionCombo")->isVisible());
    QVERIFY(!length->isVisible());
    QVERIFY(!note->isVisible());
}

// The field reports a typed value when it loses the focus to the canvas, around the click that
// selects another object; the value belongs to the object it was typed for.
void TestProperties::editFinishedByClickingAnotherObject()
{
    addLoads();
    addLoadWires();
    Project &project = window->project();
    clickLoad(kettle);
    auto *power = named<QSpinBox>("powerSpin");
    typeInto(power, "2200");
    clickLoad(lamp);
    QCOMPARE(project.load(kettle)->power, 2200);
    QCOMPARE(project.load(lamp)->power, 10);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Lamp 1"));
    QCOMPARE(power->value(), 10);

    selectWire(lampWires[0]);
    auto *length = named<QDoubleSpinBox>("lengthSpin");
    typeInto(length, "8");
    clickWire(socketWire);
    for (const QString &id : lampWires)
        QCOMPARE(project.wire(id)->length, 8.0);
    QCOMPARE(project.wire(socketWire)->length, LoadWireLength);
    QCOMPARE(length->value(), LoadWireLength);

    // The selection can also change first, without a click: hiding the field then takes its focus,
    // and the value still goes to the object it was typed for.
    clickLoad(kettle);
    typeInto(power, "1800");
    selectWire(socketWire);
    QCOMPARE(project.load(kettle)->power, 1800);
    typeInto(length, "6");
    scene->clearSelection();
    sceneItem(scene, kettle)->setSelected(true);
    QCOMPARE(project.wire(socketWire)->length, 6.0);
    for (const QString &id : lampWires)
        QCOMPARE(project.wire(id)->length, 8.0);
    QCOMPARE(power->value(), 1800);
}

// With a value typed but not sent, the selection switches straight from one object to another:
// kettle to socket hides the power row, lamp to kettle keeps it, and a cable to a panel wire hides
// the length row. The value goes to the object it was typed for, and nothing to the new one.
void TestProperties::directSwitchCommitsTypedValueToItsObject()
{
    addLoads();
    addLoadWires();
    Project &project = window->project();
    auto *power = named<QSpinBox>("powerSpin");
    auto *length = named<QDoubleSpinBox>("lengthSpin");
    QSignalSpy selectionChanged(scene, &QGraphicsScene::selectionChanged);
    const int undoCount = window->undoStack().count();

    clickLoad(kettle);
    typeInto(power, "1900");
    selectionChanged.clear();
    clickInScene(socket);
    QCOMPARE(selectionChanged.count(), 1);
    QCOMPARE(scene->selectedLoadId(), socket);
    QCOMPARE(project.load(kettle)->power, 1900);
    QCOMPARE(project.load(socket)->power, 0);

    clickLoad(lamp);
    typeInto(power, "25");
    selectionChanged.clear();
    clickInScene(kettle);
    QCOMPARE(selectionChanged.count(), 1);
    QCOMPARE(scene->selectedLoadId(), kettle);
    QCOMPARE(project.load(lamp)->power, 25);
    QCOMPARE(project.load(kettle)->power, 1900);
    QCOMPARE(power->value(), 1900);

    selectWire(lampWires[0]);
    typeInto(length, "7");
    selectionChanged.clear();
    clickInScene("w1");
    QCOMPARE(selectionChanged.count(), 1);
    QCOMPARE(scene->selectedWireId(), QStringLiteral("w1"));
    for (const QString &id : lampWires)
        QCOMPARE(project.wire(id)->length, 7.0);
    QCOMPARE(project.wire(QStringLiteral("w1"))->length, 0.3);
    QCOMPARE(window->undoStack().count(), undoCount + 3);
}

void TestProperties::refusesShrinkWhenLoadsWouldCollide()
{
    Project &project = window->project();
    QVERIFY(project.setEnclosure(QStringLiteral("enclosure-24")));
    QVERIFY(!addLoad("ceiling-lamp", QPointF(300, 300)).isEmpty());
    QVERIFY(!addLoad("ceiling-lamp", QPointF(300, 400)).isEmpty());
    selectEnclosure();
    auto *size = named<QComboBox>("enclosureSizeCombo");
    const int undoCount = window->undoStack().count();
    chooseComboItem(size, "12");
    QCOMPARE(window->statusBar()->currentMessage(),
             QStringLiteral("Move the loads below the enclosure first: they would run into other "
                            "loads."));
    QCOMPARE(project.enclosure().id, QStringLiteral("enclosure-24"));
    QCOMPARE(size->currentText(), QStringLiteral("24"));
    QCOMPARE(window->undoStack().count(), undoCount);
}

void TestProperties::panelFollowsUndoOfSelectedLoad()
{
    zoomOut();
    dropLoad(canvas, palette, "ceiling-lamp", viewportPoint(canvas, QPointF(60, 300)));
    QCOMPARE(window->project().loads().size(), 1);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Lamp 1"));
    QVERIFY(named<QWidget>("loadBox")->isVisible());
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(window->project().loads().isEmpty());
    auto *hint = named<QLabel>("propertiesHint");
    QVERIFY(hint->isVisible());
    QCOMPARE(hint->text(), QStringLiteral("Select a device on the panel to see its properties."));
    QVERIFY(!named<QWidget>("loadBox")->isVisible());
}

// The fields keep the focus after Enter; the undo and redo keys still reach the window's Undo and
// Redo (ADR 0026), not the field's own text history.
void TestProperties::undoAndRedoKeysWorkInTheFields()
{
    addLoads();
    addLoadWires();
    Project &project = window->project();
    clickLoad(lamp);
    auto *power = named<QSpinBox>("powerSpin");
    typeInto(power, "15");
    QTest::keyClick(power, Qt::Key_Return);
    QCOMPARE(project.load(lamp)->power, 15);
    QVERIFY(power->hasFocus());
    QTest::keyClick(power, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.load(lamp)->power, 10);
    QCOMPARE(power->value(), 10);
    QTest::keyClick(power, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(project.load(lamp)->power, 15);
    QTest::keyClick(power, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.load(lamp)->power, 10);
    QTest::keyClick(power, Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(project.load(lamp)->power, 15);
    QCOMPARE(power->value(), 15);

    selectWire(lampWires[0]);
    auto *length = named<QDoubleSpinBox>("lengthSpin");
    typeInto(length, "8");
    QTest::keyClick(length, Qt::Key_Return);
    QCOMPARE(project.wire(lampWires[0])->length, 8.0);
    QVERIFY(length->hasFocus());
    QTest::keyClick(length, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.wire(lampWires[0])->length, LoadWireLength);
    QCOMPARE(length->value(), LoadWireLength);
    QTest::keyClick(length, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(project.wire(lampWires[0])->length, 8.0);
    QTest::keyClick(length, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.wire(lampWires[0])->length, LoadWireLength);
    QTest::keyClick(length, Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(project.wire(lampWires[0])->length, 8.0);
    QCOMPARE(length->value(), 8.0);

    // A value typed but not sent is dropped: the field shows the model's value after the undo, and
    // leaving the field sends nothing.
    const int undoCount = window->undoStack().count();
    typeInto(length, "6");
    QTest::keyClick(length, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project.wire(lampWires[0])->length, LoadWireLength);
    QCOMPARE(length->text(), QStringLiteral("15.0 m"));
    clickLoad(lamp);
    QCOMPARE(project.wire(lampWires[0])->length, LoadWireLength);
    QCOMPARE(window->undoStack().count(), undoCount);
    QCOMPARE(window->undoStack().index(), undoCount - 1);
}

void TestProperties::loadPropertiesTranslated()
{
    addLoads();
    addLoadWires();
    clickLoad(kettle);
    chooseLanguage("ru");
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Чайник"));
    QVERIFY(showsLabel("Мощность"));
    QCOMPARE(named<QSpinBox>("powerSpin")->text(), QStringLiteral("2000 Вт"));
    QCOMPARE(named<QLabel>("plugStatusLabel")->text(), QStringLiteral("Подключён к: Розетка 1"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f07", "properties-load-ru"));

    clickLoad(socket);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Розетка 1"));
    QCOMPARE(named<QLabel>("loadTypeLabel")->text(), QStringLiteral("Двойная розетка"));
    QVERIFY(showsLabel("Включено"));
    QCOMPARE(named<QLabel>("socketContentsLabel")->text(),
             QStringLiteral("Чайник, 2000 Вт\nМикроволновая печь, 1200 Вт"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f07", "properties-socket"));

    selectWire(lampWires[0]);
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("QF2 2 \u2192 Светильник 1 L"));
    QVERIFY(showsLabel("Длина"));
    QCOMPARE(named<QDoubleSpinBox>("lengthSpin")->text(), QStringLiteral("15,0 м"));
    QCOMPARE(named<QLabel>("cableNoteLabel")->text(),
             QStringLiteral("Применяется ко всему кабелю: ко всем проводам между теми же двумя "
                            "местами."));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f07", "properties-wire-length"));

    chooseLanguage("kk");
    QCOMPARE(named<QLabel>("wireEndsLabel")->text(), QStringLiteral("QF2 2 \u2192 Шам 1 L"));
    QVERIFY(showsLabel("Ұзындық"));
    QCOMPARE(named<QDoubleSpinBox>("lengthSpin")->text(), QStringLiteral("15,0 м"));
    QCOMPARE(named<QLabel>("cableNoteLabel")->text(),
             QStringLiteral("Бүкіл кабельге қолданылады: сол екі орын арасындағы әр сымға."));
    settleAndCheckFit();
    clickLoad(socket);
    QVERIFY(showsLabel("Қосылғандар"));
    QCOMPARE(named<QLabel>("loadTypeLabel")->text(), QStringLiteral("Қос розетка"));
    QCOMPARE(named<QLabel>("socketContentsLabel")->text(),
             QStringLiteral("Шәйнек, 2000 Вт\nМикротолқынды пеш, 1200 Вт"));
    settleAndCheckFit();
    clickLoad(kettle);
    QCOMPARE(named<QLabel>("loadNameLabel")->text(), QStringLiteral("Шәйнек"));
    QVERIFY(showsLabel("Қуат"));
    QCOMPARE(named<QLabel>("plugStatusLabel")->text(), QStringLiteral("Қосылған жері: Розетка 1"));
    settleAndCheckFit();
    QVERIFY(saveScreenshot(*window, "f07", "properties-load-kk"));

    chooseLanguage("en");
}

QTEST_MAIN(TestProperties)
#include "tst_properties.moc"
