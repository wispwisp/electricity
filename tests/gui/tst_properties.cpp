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
#include <QGraphicsItem>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>
#include <QTest>
#include <QUndoStack>

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

private:
    [[nodiscard]] QPoint at(int row, int module) const;
    void drop(const QString &presetId, int row, int module, int modules = 1);
    void select(int row, int module);
    // The layout settles after the event loop runs; every shown caption must then fit its text.
    void settleAndCheckFit();
    void selectEnclosure();
    void selectWire(const QString &id);
    [[nodiscard]] static QStringList items(const QComboBox *combo);
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
    PaletteWidget *palette = nullptr;
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

QTEST_MAIN(TestProperties)
#include "tst_properties.moc"
