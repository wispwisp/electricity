// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/canvasview.h"
#include "app/editcommands.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/sceneitems.h"
#include "app/settings.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "core/project.h"
#include "core/projectfile.h"
#include "core/simulation.h"
#include "editorhelpers.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>

#include <algorithm>
#include <functional>
#include <memory>

namespace {
constexpr int kModalTimeout = 5000; // ms

// Runs trigger, and callback on the first modal T that shows meanwhile (checked every 10 ms).
// Returns whether one showed. A modal the callback leaves open is closed; if none shows within
// kModalTimeout, whatever modal is open is closed instead, so a failed check cannot hang the test.
template<typename T>
bool onModal(const std::function<void(T &)> &callback, const std::function<void()> &trigger)
{
    bool found = false;
    QTimer timer;
    const QDeadlineTimer deadline(kModalTimeout);
    QObject::connect(&timer, &QTimer::timeout, &timer, [&] {
        QWidget *modal = QApplication::activeModalWidget();
        if (auto *dialog = qobject_cast<T *>(modal)) {
            timer.stop();
            found = true;
            callback(*dialog);
            if (dialog->isVisible())
                dialog->close();
        } else if (deadline.hasExpired()) {
            timer.stop();
            if (modal)
                modal->close();
        }
    });
    timer.start(10);
    trigger();
    return found;
}

// QTest::keyClicks handles ASCII; other characters go as key events carrying their text.
void typeText(QWidget *widget, const QString &text)
{
    for (const QChar c : text) {
        if (c.unicode() < 0x80)
            QTest::keyClicks(widget, QString(c));
        else
            QTest::sendKeyEvent(QTest::Click, widget, Qt::Key_unknown, QString(c), Qt::NoModifier);
    }
}

// The file name field once the dialog is the active window and the field has the focus, as for a
// user; nullptr if that doesn't happen.
QLineEdit *fileNameField(QFileDialog &dialog)
{
    if (!QTest::qWaitForWindowActive(&dialog))
        return nullptr;
    auto *edit = dialog.findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
    return edit && edit->hasFocus() ? edit : nullptr;
}

// Types path into the file dialog's file name field and presses Return.
void typeInFileDialog(QFileDialog &dialog, const QString &path)
{
    QLineEdit *edit = fileNameField(dialog);
    QVERIFY(edit);
    typeText(edit, path);
    QTest::keyClick(edit, Qt::Key_Return);
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeAll(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

// The same enclosure, devices, wires and loads.
void compareDesigns(const Project &a, const Project &b)
{
    QCOMPARE(a.enclosure().id, b.enclosure().id);
    QCOMPARE(a.devices(), b.devices());
    QCOMPARE(a.wires(), b.wires());
    QCOMPARE(a.loads(), b.loads());
}
} // namespace

class TestFiles : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void fileMenuComesFirst();
    void saveAsWritesFileAndNamesWindow();
    void newStartsEmpty();
    void openRestoresDesignAndSwitches();
    void saveAsAddsSuffixBeforeAskingToReplace();
    void cyrillicFileName();
    void openRefusesDamagedAndNewerFiles();
    void openedFileTripsAtOnce();
    void failedSaveKeepsUntitled();
    void newDropsOldConditionsAndUndo();
    void quitClosesWindow();
    void dialogsStartInLastFolder();

private:
    // Opens the File menu and clicks the action.
    void clickFileAction(const char *name);
    // Clicks the action and types path into the file dialog it opens; whether the dialog showed.
    bool chooseFile(const char *action, const QString &path);
    [[nodiscard]] QString tempPath(const QString &name) const;
    [[nodiscard]] DeviceItem *deviceItem(const QString &id) const;
    [[nodiscard]] LoadItem *loadItem(const QString &id) const;
    // Bounds of an element of the device's picture, in item coordinates.
    [[nodiscard]] QRectF pictureElement(const QString &id, const QString &element) const;
    void clickHandle(const QString &id);
    void clickButton(const QString &id);
    void clickPause();
    // The tutorial loads in a zoomed-out view, with QF1, QD1, QF2 and Lamp 1 switched on by clicks.
    void lightLamp1();
    template<typename T>
    T *named(const char *name) const
    {
        return window->findChild<T *>(QString::fromLatin1(name));
    }

    std::unique_ptr<MainWindow> window;
    CanvasView *canvas = nullptr;
    EditorScene *scene = nullptr;
    std::unique_ptr<QTemporaryDir> folder; // a new one per test
    QTemporaryDir qtSettings;
};

void TestFiles::clickFileAction(const char *name)
{
    auto *menu = named<QMenu>("fileMenu");
    auto *action = named<QAction>(name);
    QVERIFY2(menu && action, name);
    // The pointer comes from the menu's title, as a menu ignores a click where the pointer was when
    // it opened: on the same item as last time, say.
    QMenuBar *bar = window->menuBar();
    QTest::mouseMove(bar, bar->actionGeometry(menu->menuAction()).center());
    clickMenuBarItem(*window, menu);
    clickMenuItem(menu, action);
}

bool TestFiles::chooseFile(const char *action, const QString &path)
{
    return onModal<QFileDialog>([&path](QFileDialog &dialog) { typeInFileDialog(dialog, path); },
                                [&] { clickFileAction(action); });
}

QString TestFiles::tempPath(const QString &name) const
{
    return folder->filePath(name);
}

DeviceItem *TestFiles::deviceItem(const QString &id) const
{
    return dynamic_cast<DeviceItem *>(sceneItem(scene, id));
}

LoadItem *TestFiles::loadItem(const QString &id) const
{
    return dynamic_cast<LoadItem *>(sceneItem(scene, id));
}

QRectF TestFiles::pictureElement(const QString &id, const QString &element) const
{
    const Project &project = window->project();
    const PlacedDevice *device = project.device(id);
    const DeviceType *type = device ? project.catalog().deviceType(device->type) : nullptr;
    const DeviceVariant *variant = type ? type->variant(device->poles) : nullptr;
    if (!variant)
        return {};
    const QString path = picturePath(variant->picture);
    const QSvgRenderer &renderer = svgRenderer(path);
    return renderer.transformForElement(element).mapRect(renderer.boundsOnElement(element));
}

void TestFiles::clickHandle(const QString &id)
{
    const DeviceItem *item = deviceItem(id);
    QVERIFY2(item, qPrintable(id));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, item->mapToScene(item->handleRect().center())));
}

void TestFiles::clickButton(const QString &id)
{
    const LoadItem *item = loadItem(id);
    QVERIFY2(item, qPrintable(id));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, {},
                      viewportPoint(canvas, item->mapToScene(item->buttonRect().center())));
}

void TestFiles::clickPause()
{
    auto *action = named<QAction>("pauseAction");
    QVERIFY(action);
    QTest::mouseClick(named<QToolBar>("editToolBar")->widgetForAction(action), Qt::LeftButton);
    QVERIFY(action->isChecked());
}

void TestFiles::lightLamp1()
{
    addTutorialLoads(window->project());
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
    for (const char *id : { "d1", "d2", "d3" })
        clickHandle(QString::fromLatin1(id));
    clickButton(QStringLiteral("l1"));
    for (const char *id : { "d1", "d2", "d3", "l1" })
        QVERIFY2(window->simulation().isOn(QString::fromLatin1(id)), id);
}

void TestFiles::initTestCase()
{
    useTemporarySettings();
    // Qt's file dialog keeps its own state in the native settings, which stay out of the user's.
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, qtSettings.path());
    // The dialogs start in Documents, which a fresh CI machine may lack; Qt would show its parent.
    QVERIFY(QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestFiles::init()
{
    folder = std::make_unique<QTemporaryDir>();
    QVERIFY(folder->isValid());
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
}

// Also when a test fails midway, the next one starts in English and in the dark theme.
void TestFiles::cleanup()
{
    window.reset();
    folder.reset();
    installLanguage(Language::English);
    Theme::apply(Theme::Mode::Dark);
}

void TestFiles::fileMenuComesFirst()
{
    QStringList titles;
    for (QAction *action : window->menuBar()->actions())
        titles << action->text();
    QCOMPARE(titles, (QStringList{ "File", "Edit", "View", "Settings", "Language" }));
    auto *menu = named<QMenu>("fileMenu");
    QVERIFY(menu);
    QCOMPARE(window->menuBar()->actions().constFirst(), menu->menuAction());

    // The actions themselves, submenus aside.
    QStringList items;
    for (QAction *action : menu->actions()) {
        if (!action->menu())
            items << (action->isSeparator() ? QStringLiteral("-") : action->objectName());
    }
    QCOMPARE(items,
             (QStringList{ "newAction", "openAction", "-", "saveAction", "saveAsAction", "-",
                           "quitAction" }));
    struct Item
    {
        const char *name;
        QString text;
        QKeySequence shortcut;
    };
    for (const Item &item :
         { Item{ "newAction", QStringLiteral("New"), QKeySequence(Qt::CTRL | Qt::Key_N) },
           Item{ "openAction", QStringLiteral("Open…"), QKeySequence(Qt::CTRL | Qt::Key_O) },
           Item{ "saveAction", QStringLiteral("Save"), QKeySequence(Qt::CTRL | Qt::Key_S) },
           Item{ "saveAsAction", QStringLiteral("Save As…"),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S) },
           Item{ "quitAction", QStringLiteral("Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q) } }) {
        auto *action = named<QAction>(item.name);
        QVERIFY2(action, item.name);
        QCOMPARE(action->text(), item.text);
        QCOMPARE(action->shortcut(), item.shortcut);
    }
    QVERIFY(named<QAction>("saveAction")->isEnabled());
}

void TestFiles::saveAsWritesFileAndNamesWindow()
{
    lightLamp1();
    const QString path = tempPath(QStringLiteral("tutorial.panel"));
    QVERIFY(chooseFile("saveAsAction", path));

    QVERIFY(QFile::exists(path));
    Project saved(window->project().catalog());
    const OpenResult result = openProject(path, saved);
    QCOMPARE(result.status, OpenResult::Status::Opened);
    compareDesigns(saved, window->project());
    QCOMPARE(result.on, (QStringList{ "d1", "d2", "d3", "l1" }));
    QCOMPARE(window->windowTitle(), QStringLiteral("tutorial.panel[*] — Electricity"));
    QVERIFY(!window->isWindowModified());
    QCOMPARE(window->filePath(), path);
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("saved")));
}

void TestFiles::newStartsEmpty()
{
    lightLamp1();
    QVERIFY(window->project().setEnclosure(QStringLiteral("enclosure-24")));
    QVERIFY(chooseFile("saveAsAction", tempPath(QStringLiteral("before.panel"))));

    clickFileAction("newAction");
    const Project &project = window->project();
    QVERIFY(project.devices().isEmpty());
    QVERIFY(project.loads().isEmpty());
    QVERIFY(project.wires().isEmpty());
    QCOMPARE(project.enclosure().id, QStringLiteral("enclosure-12"));
    QCOMPARE(window->windowTitle(), QStringLiteral("Untitled[*] — Electricity"));
    QCOMPARE(window->filePath(), QString());
    QVERIFY(window->simulation().switchedOn().isEmpty());
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("new")));
    // Ids start again, as at launch.
    QCOMPARE(window->project().addDevice(
                     deviceFromPreset(preset(QStringLiteral("breaker-1p-c16")), 0, 0)),
             QStringLiteral("d1"));
}

void TestFiles::openRestoresDesignAndSwitches()
{
    clickPause(); // simulated time moves only by step()
    lightLamp1();
    const QString path = tempPath(QStringLiteral("tutorial.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    Project saved(window->project().catalog());
    QCOMPARE(openProject(path, saved).status, OpenResult::Status::Opened);
    clickFileAction("newAction");
    // An edit saved to another file: Undo is enabled, nothing is unsaved.
    window->undoStack().push(new AddDeviceCommand(
            window->project(), deviceFromPreset(preset(QStringLiteral("breaker-1p-c16")), 0, 0)));
    QVERIFY(chooseFile("saveAsAction", tempPath(QStringLiteral("other.panel"))));
    auto *undo = named<QAction>("undoAction");
    QVERIFY(undo->isEnabled());
    QVERIFY(window->undoStack().isClean());
    window->simulation().step(1);
    QCOMPARE(window->simulation().elapsed(), 1.0);
    canvas->setTransform(QTransform::fromScale(4, 4));
    QVERIFY(!fullyVisible(canvas, scene->panelRect()));

    QVERIFY(chooseFile("openAction", path));
    compareDesigns(window->project(), saved);
    for (const char *id : { "d1", "d2", "d3" }) {
        const QString deviceId = QString::fromLatin1(id);
        const DeviceItem *item = deviceItem(deviceId);
        QVERIFY2(item && item->isOn(), id);
        QCOMPARE(item->handleRect(), pictureElement(deviceId, QStringLiteral("handle")));
    }
    QVERIFY(window->simulation().isEnergized(u"l1"));
    QVERIFY(fullyVisible(canvas, scene->panelRect()));
    QVERIFY(!undo->isEnabled());
    QVERIFY(!window->isWindowModified());
    QCOMPARE(window->simulation().elapsed(), 0.0);
    QCOMPARE(window->windowTitle(), QStringLiteral("tutorial.panel[*] — Electricity"));
    QCOMPARE(window->filePath(), path);
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("reopened")));
}

void TestFiles::saveAsAddsSuffixBeforeAskingToReplace()
{
    const QString path = tempPath(QStringLiteral("a.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    const QByteArray before = readAll(path);
    QVERIFY(!before.isEmpty());
    window->undoStack().push(new RemoveDeviceCommand(window->project(), QStringLiteral("d5")));

    bool asked = false;
    QString question;
    const bool shown = onModal<QFileDialog>(
            [&](QFileDialog &dialog) {
                QLineEdit *edit = fileNameField(dialog);
                QVERIFY(edit);
                typeText(edit, tempPath(QStringLiteral("a")));
                asked = onModal<QMessageBox>(
                        [&](QMessageBox &box) {
                            question = box.text();
                            QTest::mouseClick(box.button(QMessageBox::No), Qt::LeftButton);
                        },
                        [&] { QTest::keyClick(edit, Qt::Key_Return); });
                QTest::keyClick(&dialog, Qt::Key_Escape);
            },
            [&] { clickFileAction("saveAsAction"); });
    QVERIFY(shown);
    QVERIFY(asked);
    QCOMPARE(question, QStringLiteral("a.panel already exists.\nDo you want to replace it?"));
    QCOMPARE(readAll(path), before);
    QVERIFY(!QFile::exists(tempPath(QStringLiteral("a"))));
    QCOMPARE(window->filePath(), path);
}

void TestFiles::cyrillicFileName()
{
    const QString path = tempPath(QStringLiteral("щит кухня.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    QVERIFY(QFile::exists(path));
    QCOMPARE(window->windowTitle(), QStringLiteral("щит кухня.panel[*] — Electricity"));
    Project saved(window->project().catalog());
    QCOMPARE(openProject(path, saved).status, OpenResult::Status::Opened);

    clickFileAction("newAction");
    QVERIFY(window->project().devices().isEmpty());
    QVERIFY(chooseFile("openAction", path));
    compareDesigns(window->project(), saved);
    QCOMPARE(window->project().devices().size(), 5);
    QCOMPARE(window->filePath(), path);
}

void TestFiles::openRefusesDamagedAndNewerFiles()
{
    const QString damaged = tempPath(QStringLiteral("bad.panel"));
    QVERIFY(writeAll(damaged, "{"));
    QJsonObject json = QJsonDocument::fromJson(writeProject(window->project(), {})).object();
    json[QStringLiteral("version")] = 2;
    const QString newer = tempPath(QStringLiteral("new.panel"));
    QVERIFY(writeAll(newer, QJsonDocument(json).toJson()));
    const QList<PlacedDevice> devices = window->project().devices();
    const QList<Wire> wires = window->project().wires();
    const QString title = window->windowTitle();

    struct Case
    {
        QString path;
        QString message;
        QString step;
    };
    for (const Case &c :
         { Case{ damaged,
                 QStringLiteral("bad.panel is not an Electricity panel file, or it is "
                                "damaged."),
                 QStringLiteral("damaged") },
           Case{ newer,
                 QStringLiteral("new.panel was saved by a newer version of Electricity. "
                                "Update the app to open it."),
                 QStringLiteral("newer") } }) {
        QString name;
        QString boxTitle;
        QString text;
        bool screenshot = false;
        const bool shown = onModal<QMessageBox>(
                [&](QMessageBox &box) {
                    name = box.objectName();
                    boxTitle = box.windowTitle();
                    text = box.text();
                    screenshot = saveScreenshot(box, QStringLiteral("f10"), c.step);
                    QTest::mouseClick(box.button(QMessageBox::Ok), Qt::LeftButton);
                },
                [&] { QVERIFY(chooseFile("openAction", c.path)); });
        QVERIFY2(shown, qPrintable(c.step));
        QCOMPARE(name, QStringLiteral("fileErrorBox"));
        QCOMPARE(boxTitle, QStringLiteral("Electricity"));
        QCOMPARE(text, c.message);
        QVERIFY(screenshot);
        QCOMPARE(window->project().devices(), devices);
        QCOMPARE(window->project().wires(), wires);
        QCOMPARE(window->windowTitle(), title);
        QCOMPARE(window->filePath(), QString());
    }
}

// A lamp whose neutral bypasses the RCD, saved on: as on a click, the RCD trips as soon as the file
// opens, and the status bar says so.
void TestFiles::openedFileTripsAtOnce()
{
    Project project(window->project().catalog());
    addTutorialDevices(project);
    addTutorialWires(project);
    addTutorialLoads(project);
    const QList<Wire> &wires = project.wires();
    const auto lampNeutral = std::ranges::find_if(
            wires, [](const Wire &wire) { return wire.from == u"nbar:2" && wire.to == u"l1:N"; });
    QVERIFY(lampNeutral != wires.end());
    Wire bypass = *lampNeutral;
    bypass.from = QStringLiteral("supply:N");
    QVERIFY(project.updateWire(bypass));
    const QString path = tempPath(QStringLiteral("bypass.panel"));
    QVERIFY(writeAll(path, writeProject(project, { "d1", "d2", "d3", "l1" })));

    QVERIFY(chooseFile("openAction", path));
    QCOMPARE(window->simulation().tripCause(u"d2"), TripCause::Leakage);
    QVERIFY2(window->statusBar()->currentMessage().startsWith(QStringLiteral("QD1 tripped")),
             qPrintable(window->statusBar()->currentMessage()));
}

void TestFiles::failedSaveKeepsUntitled()
{
#ifdef Q_OS_WIN
    QSKIP("Folder permissions are not POSIX on Windows");
#endif
    const QString readOnly = tempPath(QStringLiteral("ro"));
    QVERIFY(QDir().mkpath(readOnly));
    QVERIFY(QFile::setPermissions(readOnly, QFile::ReadOwner | QFile::ExeOwner));
    const auto restore = qScopeGuard([&readOnly] {
        QFile::setPermissions(readOnly, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    });
    if (QFile probe(readOnly + QStringLiteral("/probe")); probe.open(QIODevice::WriteOnly)) {
        probe.remove();
        QSKIP("The folder stays writable (running as root)");
    }

    const QString path = readOnly + QStringLiteral("/p.panel");
    QString name;
    QString text;
    const bool shown = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                name = box.objectName();
                text = box.text();
                QTest::mouseClick(box.button(QMessageBox::Ok), Qt::LeftButton);
            },
            [&] { QVERIFY(chooseFile("saveAsAction", path)); });
    QVERIFY(shown);
    QCOMPARE(name, QStringLiteral("fileErrorBox"));
    QVERIFY2(text.startsWith(QStringLiteral("Can't save p.panel: ")), qPrintable(text));
    QVERIFY(!QFile::exists(path));
    QCOMPARE(window->windowTitle(), QStringLiteral("Untitled[*] — Electricity"));
    QCOMPARE(window->filePath(), QString());
}

void TestFiles::newDropsOldConditionsAndUndo()
{
    QStatusBar *bar = window->statusBar();
    dropPreset(canvas, window->findChild<PaletteWidget *>(QStringLiteral("paletteTree")),
               QStringLiteral("breaker-1p-c16"),
               viewportPoint(canvas, scene->slotRect(0, 8, 1).center()));
    QCOMPARE(window->project().devices().size(), 6);
    QVERIFY(chooseFile("saveAsAction", tempPath(QStringLiteral("edited.panel"))));
    auto *undo = named<QAction>("undoAction");
    QVERIFY(undo->isEnabled());
    QVERIFY(window->undoStack().isClean());
    addTutorialLoads(window->project());
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
    for (const char *id : { "d1", "d2", "d4" })
        clickHandle(QString::fromLatin1(id));
    QVERIFY(window->simulation().setFault(QStringLiteral("l3"), Fault::ShortCircuit));
    QCOMPARE(bar->currentMessage(), QStringLiteral("QF3 tripped: short circuit, 370 A on C16"));

    clickFileAction("newAction");
    QCOMPARE(bar->currentMessage(), QStringLiteral("Ready"));
    QVERIFY(!undo->isEnabled());

    addTutorialDevices(window->project());
    addTutorialWires(window->project());
    addTutorialLoads(window->project());
    QVERIFY(!window->simulation().isOn(u"d1"));
    QCOMPARE(window->simulation().tripCause(u"d4"), TripCause::None);
    QCOMPARE(window->simulation().fault(u"l3"), Fault::None);
    QCOMPARE(bar->currentMessage(), QStringLiteral("Ready"));
}

void TestFiles::quitClosesWindow()
{
    clickFileAction("quitAction");
    QVERIFY(!window->isVisible());
}

void TestFiles::dialogsStartInLastFolder()
{
    clearSettings();
    const auto shownFolder = [this](const char *action) {
        QString shown;
        const bool found = onModal<QFileDialog>(
                [&shown](QFileDialog &dialog) {
                    shown = dialog.directory().canonicalPath();
                    QTest::keyClick(&dialog, Qt::Key_Escape);
                },
                [&] { clickFileAction(action); });
        return found ? shown : QStringLiteral("(no dialog)");
    };
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QCOMPARE(shownFolder("openAction"), QDir(documents).canonicalPath());

    QVERIFY(chooseFile("saveAsAction", tempPath(QStringLiteral("here.panel"))));
    const QString last = QDir(folder->path()).canonicalPath();
    QCOMPARE(shownFolder("openAction"), last);
    QCOMPARE(shownFolder("saveAsAction"), last);

    const QString sub = tempPath(QStringLiteral("sub"));
    QVERIFY(QDir().mkpath(sub));
    QVERIFY(QFile::copy(tempPath(QStringLiteral("here.panel")),
                        sub + QStringLiteral("/there.panel")));
    QVERIFY(chooseFile("openAction", sub + QStringLiteral("/there.panel")));
    QCOMPARE(shownFolder("saveAsAction"), QDir(sub).canonicalPath());
}

QTEST_MAIN(TestFiles)
#include "tst_files.moc"
