// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/settings.h"
#include "core/catalog.h"
#include "core/project.h"
#include "editorhelpers.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAction>
#include <QApplication>
#include <QGraphicsItem>
#include <QMenu>
#include <QMenuBar>
#include <QTest>
#include <QToolBar>
#include <QUndoStack>

#include <memory>

class TestEditing : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void menusAndActionsExist();
    void undoRedoThroughToolbarAndShortcuts();
    void deletesWithKeyAndContextMenu();
    void deleteNeedsASelectedDevice();
    void deleteKeyRightAfterADrop();
    void refusedEditLeavesNoUndoEntry();
    void fitActionRefits();
    void sceneTeardownEmitsNoSelectionChange();

private:
    [[nodiscard]] QPoint at(int row, int module, int modules) const;
    void drop(const QString &presetId, int row, int module, int modules = 1);
    [[nodiscard]] QString designation(int row, int module) const;
    [[nodiscard]] QStringList ids() const;
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

QPoint TestEditing::at(int row, int module, int modules) const
{
    return viewportPoint(canvas, scene->slotRect(row, module, modules).center());
}

void TestEditing::drop(const QString &presetId, int row, int module, int modules)
{
    dropPreset(canvas, palette, presetId, at(row, module, modules));
}

QString TestEditing::designation(int row, int module) const
{
    for (const PlacedDevice &device : window->project().devices())
        if (device.row == row && device.module == module)
            return window->project().designation(device.id);
    return {};
}

QStringList TestEditing::ids() const
{
    QStringList result;
    for (const PlacedDevice &device : window->project().devices())
        result << device.id;
    return result;
}

void TestEditing::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestEditing::init()
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

void TestEditing::cleanup()
{
    window.reset();
}

void TestEditing::menusAndActionsExist()
{
    QStringList titles;
    for (QAction *action : window->menuBar()->actions())
        titles << action->text();
    QCOMPARE(titles, (QStringList{ "Edit", "View", "Settings", "Language" }));
    QVERIFY(named<QMenu>("editMenu"));
    QVERIFY(named<QMenu>("viewMenu"));
    QVERIFY(named<QToolBar>("editToolBar"));
    auto *undo = named<QAction>("undoAction");
    auto *redo = named<QAction>("redoAction");
    auto *del = named<QAction>("deleteAction");
    QVERIFY(undo && redo && del && named<QAction>("fitAction"));
    QVERIFY(!undo->isEnabled());
    QVERIFY(!redo->isEnabled());
    QVERIFY(redo->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)));
    QVERIFY(redo->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_Y)));
    QCOMPARE(del->shortcutContext(), Qt::WidgetShortcut);
    drop("breaker-1p-c16", 0, 0);
    QCOMPARE(undo->text(), QStringLiteral("Undo"));
    QCOMPARE(redo->text(), QStringLiteral("Redo"));
}

void TestEditing::undoRedoThroughToolbarAndShortcuts()
{
    drop("breaker-2p-c40", 0, 0, 2);
    drop("rcd-2p-40-30", 0, 2, 2);
    QCOMPARE(window->project().devices().size(), 2);
    auto *undo = named<QAction>("undoAction");
    auto *toolbar = named<QToolBar>("editToolBar");
    QVERIFY(toolbar);
    QWidget *undoButton = toolbar->widgetForAction(undo);
    QVERIFY(undoButton);
    QTest::mouseClick(undoButton, Qt::LeftButton);
    QCOMPARE(ids(), QStringList{ "d1" });
    QTest::keyClick(window.get(), Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(ids(), (QStringList{ "d1", "d2" }));
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(window->project().devices().isEmpty());
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(ids(), QStringList{ "d1" });
    QCOMPARE(designation(0, 0), QStringLiteral("QF1"));

    auto *editMenu = named<QMenu>("editMenu");
    QTest::keyClick(window.get(), Qt::Key_Y, Qt::ControlModifier);
    clickMenuBarItem(*window, editMenu);
    QVERIFY(saveScreenshot(*editMenu, "f05", "edit-menu"));
    clickMenuItem(editMenu, undo);
    clickMenuItem(editMenu, undo);
    QVERIFY(window->project().devices().isEmpty());
}

void TestEditing::deletesWithKeyAndContextMenu()
{
    drop("breaker-2p-c40", 0, 0, 2);
    drop("rcd-2p-40-30", 0, 2, 2);
    drop("breaker-1p-c10", 0, 4);
    drop("breaker-1p-c16", 0, 5);
    drop("breaker-1p-c16", 0, 6);
    QCOMPARE(designation(0, 4), QStringLiteral("QF2"));
    QCOMPARE(designation(0, 6), QStringLiteral("QF4"));

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 4, 1));
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Delete);
    QCOMPARE(window->project().devices().size(), 4);
    QCOMPARE(designation(0, 4), QString());
    QCOMPARE(designation(0, 5), QStringLiteral("QF2"));
    QCOMPARE(designation(0, 6), QStringLiteral("QF3"));
    QVERIFY(saveScreenshot(*window, "f05", "deleted"));

    QTest::mouseClick(canvas->viewport(), Qt::RightButton, {}, at(0, 5, 1));
    auto *menu = named<QMenu>("deviceContextMenu");
    QVERIFY(menu);
    QTRY_VERIFY(menu->isVisible());
    clickMenuItem(menu, named<QAction>("deleteAction"));
    QCOMPARE(window->project().devices().size(), 3);
    QCOMPARE(designation(0, 6), QStringLiteral("QF2"));

    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(designation(0, 5), QStringLiteral("QF2"));
    QCOMPARE(designation(0, 6), QStringLiteral("QF3"));
    QCOMPARE(window->project().devices().size(), 4);

    const QPointF interior(240, 190);
    QTest::mouseClick(canvas->viewport(), Qt::RightButton, {}, viewportPoint(canvas, interior));
    QVERIFY(!menu->isVisible());
}

void TestEditing::deleteNeedsASelectedDevice()
{
    auto *del = named<QAction>("deleteAction");
    QVERIFY(!del->isEnabled());
    drop("breaker-1p-c16", 0, 0); // a dropped device is selected
    QVERIFY(del->isEnabled());
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, viewportPoint(canvas, { 240, 190 }));
    QVERIFY(scene->isEnclosureSelected());
    QVERIFY(!del->isEnabled());
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {}, at(0, 0, 1));
    QVERIFY(del->isEnabled());

    auto *editMenu = named<QMenu>("editMenu");
    clickMenuBarItem(*window, editMenu);
    clickMenuItem(editMenu, del);
    QVERIFY(window->project().devices().isEmpty());
    QVERIFY(!del->isEnabled());
}

// Pressing a palette entry gives the palette focus, and a drag never moves it; after the drop the
// canvas has focus, so the Delete key (and Space for panning) reach it.
void TestEditing::deleteKeyRightAfterADrop()
{
    palette->setFocus();
    QVERIFY(palette->hasFocus());
    drop("breaker-1p-c16", 0, 0);
    QCOMPARE(window->project().devices().size(), 1);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Delete);
    QVERIFY(window->project().devices().isEmpty());
    QCOMPARE(QApplication::focusWidget(), canvas);
}

void TestEditing::refusedEditLeavesNoUndoEntry()
{
    drop("breaker-2p-c40", 0, 0, 2);
    dropPreset(canvas, palette, "breaker-1p-c16", at(0, 1, 1));
    QCOMPARE(window->project().devices().size(), 1);
    QTest::keyClick(window.get(), Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(window->project().devices().isEmpty());
    QVERIFY(!named<QAction>("undoAction")->isEnabled());
}

void TestEditing::fitActionRefits()
{
    for (int i = 0; i < 5; ++i)
        wheel(canvas->viewport(), canvas->viewport()->rect().center(), 120);
    QVERIFY(!fullyVisible(canvas, scene->panelRect()));
    QTest::keyClick(window.get(), Qt::Key_0, Qt::ControlModifier);
    QVERIFY(fullyVisible(canvas, scene->panelRect()));
}

void TestEditing::sceneTeardownEmitsNoSelectionChange()
{
    Project project(embeddedCatalog());
    QUndoStack undoStack;
    auto *standalone = new EditorScene(project, undoStack);
    const QString id = project.addDevice(deviceFromPreset(preset("breaker-1p-c16"), 0, 0));
    sceneItem(standalone, id)->setSelected(true);
    int changes = 0;
    connect(standalone, &QGraphicsScene::selectionChanged, this, [&] { ++changes; });
    delete standalone;
    QCOMPARE(changes, 0);
}

QTEST_MAIN(TestEditing)
#include "tst_editing.moc"
