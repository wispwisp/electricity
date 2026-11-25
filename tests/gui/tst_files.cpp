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
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
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
// With a name, only a modal of that object name counts, so that nested calls for different boxes
// don't take each other's.
template<typename T>
bool onModal(const std::function<void(T &)> &callback, const std::function<void()> &trigger,
             const char *name = nullptr)
{
    bool found = false;
    QTimer timer;
    const QDeadlineTimer deadline(kModalTimeout);
    QObject::connect(&timer, &QTimer::timeout, &timer, [&] {
        QWidget *modal = QApplication::activeModalWidget();
        auto *dialog = qobject_cast<T *>(modal);
        if (dialog && (!name || dialog->objectName() == QLatin1String(name))) {
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

// Runs trigger, and clicks the button of that object name on the "Save changes?" question that
// shows meanwhile; text, if given, gets the question's text. Returns whether it showed.
bool answerQuestion(const char *button, const std::function<void()> &trigger,
                    QString *text = nullptr)
{
    return onModal<QMessageBox>(
            [&](QMessageBox &box) {
                if (text)
                    *text = box.text();
                if (auto *b = box.findChild<QAbstractButton *>(QString::fromLatin1(button)))
                    QTest::mouseClick(b, Qt::LeftButton);
            },
            trigger, "saveChangesBox");
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
    void saveAsAddsSuffixToNameWithDot();
    void saveAsAsksBeforeReplacingDottedName();
    void cyrillicFileName();
    void openRefusesDamagedAndNewerFiles();
    void openedFileTripsAtOnce();
    void failedSaveKeepsUntitled();
    void newDropsOldConditionsAndUndo();
    void quitClosesWindow();
    void dialogsStartInLastFolder();
    void editMarksWindowModified();
    void switchingIsNotAChange();
    void closeAsksAboutUnsavedChanges();
    void saveFromQuestionOnUntitledOpensSaveAs();
    void saveFromQuestionSavesThenContinues();
    void failedOpenAfterDontSaveKeepsEdits();
    void failedSaveCancelsClose();
    void recentListsOpenedAndSavedFiles();
    void openRecentOpensTheFile();
    void missingRecentFileIsRemoved();
    void damagedRecentFileStays();
    void fileTextsFollowLanguage();
    void fileDialogIsTranslated_data();
    void fileDialogIsTranslated();
    void roadmapScenario();

private:
    // An edit that leaves the window modified: the last tutorial device is removed.
    void editProject();
    // Opens the File menu and clicks the action.
    void clickFileAction(const char *name);
    // Clicks the action and types path into the file dialog it opens; whether the dialog showed.
    bool chooseFile(const char *action, const QString &path);
    [[nodiscard]] QString tempPath(const QString &name) const;
    // Opens File > Open Recent and clicks the entry at index; the submenu stays open if keepOpen.
    bool clickRecent(int index, bool keepOpen = false);
    // Switches the language through the Language menu.
    void clickLanguage(const char *action);
    // The text of each Open Recent entry.
    [[nodiscard]] QStringList recentTexts() const;
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

void TestFiles::editProject()
{
    window->undoStack().push(new RemoveDeviceCommand(window->project(), QStringLiteral("d5")));
    QVERIFY(window->isWindowModified());
}

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

bool TestFiles::clickRecent(int index, bool keepOpen)
{
    auto *menu = named<QMenu>("fileMenu");
    auto *recent = named<QMenu>("recentMenu");
    if (!menu || !recent)
        return false;
    QMenuBar *bar = window->menuBar();
    QTest::mouseMove(bar, bar->actionGeometry(menu->menuAction()).center());
    clickMenuBarItem(*window, menu);
    openSubmenu(menu, recent);
    if (!keepOpen)
        clickMenuItem(recent, recent->actions().at(index));
    return true;
}

void TestFiles::clickLanguage(const char *action)
{
    auto *menu = named<QMenu>("languageMenu");
    auto *item = named<QAction>(action);
    QVERIFY2(menu && item, action);
    QMenuBar *bar = window->menuBar();
    QTest::mouseMove(bar, bar->actionGeometry(menu->menuAction()).center());
    clickMenuBarItem(*window, menu);
    clickMenuItem(menu, item);
    QVERIFY2(item->isChecked(), action);
}

QStringList TestFiles::recentTexts() const
{
    QStringList texts;
    for (QAction *action : named<QMenu>("recentMenu")->actions())
        texts << action->text();
    return texts;
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
    Settings::saveRecentFiles({});
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

    QStringList items;
    for (QAction *action : menu->actions()) {
        if (action->menu())
            items << action->menu()->objectName();
        else
            items << (action->isSeparator() ? QStringLiteral("-") : action->objectName());
    }
    QCOMPARE(items,
             (QStringList{ "newAction", "openAction", "recentMenu", "-", "saveAction",
                           "saveAsAction", "-", "quitAction" }));
    QCOMPARE(named<QMenu>("recentMenu")->title(), QStringLiteral("Open Recent"));
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

// Qt's dialog adds the default suffix only to names without a dot; addresses often have one.
void TestFiles::saveAsAddsSuffixToNameWithDot()
{
    const QString path = tempPath(QStringLiteral("ул. Абая 10.panel"));
    QVERIFY(chooseFile("saveAsAction", tempPath(QStringLiteral("ул. Абая 10"))));
    QVERIFY(QFile::exists(path));
    QVERIFY(!QFile::exists(tempPath(QStringLiteral("ул. Абая 10"))));
    Project saved(window->project().catalog());
    QCOMPARE(openProject(path, saved).status, OpenResult::Status::Opened);
    compareDesigns(saved, window->project());
    QCOMPARE(window->windowTitle(), QStringLiteral("ул. Абая 10.panel[*] — Electricity"));
    QCOMPARE(window->filePath(), path);
}

void TestFiles::saveAsAsksBeforeReplacingDottedName()
{
    const QString path = tempPath(QStringLiteral("flat.v2.panel"));
    const QByteArray before = "someone else's panel";
    QVERIFY(writeAll(path, before));
    // Save As typing the name without .panel, answering the question with the button.
    const auto saveAs = [this](QMessageBox::StandardButton answer, QString &name, QString &title,
                               QString &text) {
        return onModal<QMessageBox>(
                [&](QMessageBox &box) {
                    name = box.objectName();
                    title = box.windowTitle();
                    text = box.text();
                    QTest::mouseClick(box.button(answer), Qt::LeftButton);
                },
                [&] { QVERIFY(chooseFile("saveAsAction", tempPath(QStringLiteral("flat.v2")))); });
    };

    QString name;
    QString title;
    QString text;
    QVERIFY(saveAs(QMessageBox::No, name, title, text));
    QCOMPARE(name, QStringLiteral("replaceBox"));
    QCOMPARE(title, QStringLiteral("Electricity"));
    QVERIFY2(text.contains(QStringLiteral("flat.v2.panel")), qPrintable(text));
    QCOMPARE(readAll(path), before);
    QVERIFY(!QFile::exists(tempPath(QStringLiteral("flat.v2"))));
    QCOMPARE(window->windowTitle(), QStringLiteral("Untitled[*] — Electricity"));
    QCOMPARE(window->filePath(), QString());

    QVERIFY(saveAs(QMessageBox::Yes, name, title, text));
    QCOMPARE(name, QStringLiteral("replaceBox"));
    Project saved(window->project().catalog());
    QCOMPARE(openProject(path, saved).status, OpenResult::Status::Opened);
    compareDesigns(saved, window->project());
    QVERIFY(!QFile::exists(tempPath(QStringLiteral("flat.v2"))));
    QCOMPARE(window->windowTitle(), QStringLiteral("flat.v2.panel[*] — Electricity"));
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

void TestFiles::editMarksWindowModified()
{
    const QString qf = window->project().addDevice(
            deviceFromPreset(preset(QStringLiteral("breaker-1p-c16")), 0, 8));
    QVERIFY(!window->isWindowModified());
    const auto at = [this](int module) {
        return viewportPoint(canvas, scene->slotRect(0, module, 1).center());
    };
    mouseDrag(canvas->viewport(), at(8), at(9));
    QCOMPARE(window->project().device(qf)->module, 9);
    QVERIFY(window->isWindowModified());
    QCOMPARE(window->windowTitle(), QStringLiteral("Untitled[*] — Electricity"));
    named<QAction>("undoAction")->trigger();
    QCOMPARE(window->project().device(qf)->module, 8);
    QVERIFY(!window->isWindowModified());
}

void TestFiles::switchingIsNotAChange()
{
    const QString path = tempPath(QStringLiteral("switching.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    canvas->setTransform(QTransform::fromScale(0.6, 0.6));
    canvas->centerOn(QPointF(140, 300));
    clickHandle(QStringLiteral("d1"));
    QVERIFY(window->simulation().isOn(u"d1"));
    QVERIFY(!window->isWindowModified());

    clickFileAction("saveAction");
    Project saved(window->project().catalog());
    const OpenResult result = openProject(path, saved);
    QCOMPARE(result.status, OpenResult::Status::Opened);
    QVERIFY(result.on.contains(QStringLiteral("d1")));

    clickHandle(QStringLiteral("d1"));
    QVERIFY(!window->simulation().isOn(u"d1"));
    bool closed = false;
    const bool asked = answerQuestion("saveChangesCancel", [&] { closed = window->close(); });
    QVERIFY(!asked);
    QVERIFY(closed);
}

void TestFiles::closeAsksAboutUnsavedChanges()
{
    editProject();
    QString text;
    QString name;
    bool closed = true;
    bool screenshot = false;
    bool asked = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                name = box.objectName();
                text = box.text();
                screenshot = saveScreenshot(box, QStringLiteral("f10"), QStringLiteral("question"));
                QTest::mouseClick(
                        box.findChild<QAbstractButton *>(QStringLiteral("saveChangesCancel")),
                        Qt::LeftButton);
            },
            [&] { closed = window->close(); });
    QVERIFY(asked);
    QCOMPARE(name, QStringLiteral("saveChangesBox"));
    QCOMPARE(text, QStringLiteral("Save changes to the new project?"));
    QVERIFY(screenshot);
    QVERIFY(!closed);
    QVERIFY(window->isVisible());
    QVERIFY(window->isWindowModified());

    asked = answerQuestion("saveChangesDiscard", [&] { closed = window->close(); });
    QVERIFY(asked);
    QVERIFY(closed);
    QVERIFY(!window->isVisible());
}

void TestFiles::saveFromQuestionOnUntitledOpensSaveAs()
{
    editProject();
    const QList<PlacedDevice> devices = window->project().devices();
    bool dialogShown = onModal<QFileDialog>(
            [](QFileDialog &dialog) {
                QVERIFY(QTest::qWaitForWindowActive(&dialog));
                QTest::keyClick(&dialog, Qt::Key_Escape);
            },
            [&] {
                QVERIFY(answerQuestion("saveChangesSave", [&] { clickFileAction("newAction"); }));
            });
    QVERIFY(dialogShown);
    QCOMPARE(window->project().devices(), devices);
    QVERIFY(window->isWindowModified());
    QCOMPARE(window->filePath(), QString());
}

void TestFiles::saveFromQuestionSavesThenContinues()
{
    const QString path = tempPath(QStringLiteral("then.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    editProject();
    const QList<PlacedDevice> edited = window->project().devices();
    QVERIFY(answerQuestion("saveChangesSave", [&] { clickFileAction("newAction"); }));

    Project saved(window->project().catalog());
    QCOMPARE(openProject(path, saved).status, OpenResult::Status::Opened);
    QCOMPARE(saved.devices(), edited);
    QVERIFY(window->project().devices().isEmpty());
    QCOMPARE(window->filePath(), QString());
    QCOMPARE(window->windowTitle(), QStringLiteral("Untitled[*] — Electricity"));
    QVERIFY(!window->isWindowModified());
}

void TestFiles::failedOpenAfterDontSaveKeepsEdits()
{
    const QString damaged = tempPath(QStringLiteral("bad.panel"));
    QVERIFY(writeAll(damaged, "{"));
    editProject();
    const QList<PlacedDevice> edited = window->project().devices();

    QString error;
    const bool shown = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                error = box.text();
                QTest::mouseClick(box.button(QMessageBox::Ok), Qt::LeftButton);
            },
            [&] {
                QVERIFY(answerQuestion("saveChangesDiscard",
                                       [&] { QVERIFY(chooseFile("openAction", damaged)); }));
            },
            "fileErrorBox");
    QVERIFY(shown);
    QVERIFY(error.contains(QStringLiteral("bad.panel")));
    QCOMPARE(window->project().devices(), edited);
    QVERIFY(window->isWindowModified());
    auto *undo = named<QAction>("undoAction");
    QVERIFY(undo->isEnabled());
    undo->trigger();
    QCOMPARE(window->project().devices().size(), edited.size() + 1);
    QVERIFY(!window->isWindowModified());
}

void TestFiles::failedSaveCancelsClose()
{
#ifdef Q_OS_WIN
    QSKIP("Folder permissions are not POSIX on Windows");
#endif
    const QString readOnly = tempPath(QStringLiteral("ro"));
    QVERIFY(QDir().mkpath(readOnly));
    const QString path = readOnly + QStringLiteral("/p.panel");
    QVERIFY(chooseFile("saveAsAction", path));
    editProject();
    QVERIFY(QFile::setPermissions(readOnly, QFile::ReadOwner | QFile::ExeOwner));
    const auto restore = qScopeGuard([&readOnly] {
        QFile::setPermissions(readOnly, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    });
    if (QFile probe(readOnly + QStringLiteral("/probe")); probe.open(QIODevice::WriteOnly)) {
        probe.remove();
        QSKIP("The folder stays writable (running as root)");
    }

    QString text;
    bool closed = true;
    const bool shown = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                text = box.text();
                QTest::mouseClick(box.button(QMessageBox::Ok), Qt::LeftButton);
            },
            [&] { QVERIFY(answerQuestion("saveChangesSave", [&] { closed = window->close(); })); },
            "fileErrorBox");
    QVERIFY(shown);
    QVERIFY2(text.startsWith(QStringLiteral("Can't save p.panel: ")), qPrintable(text));
    QVERIFY(!closed);
    QVERIFY(window->isVisible());
    QVERIFY(window->isWindowModified());
}

void TestFiles::recentListsOpenedAndSavedFiles()
{
    auto *recent = named<QMenu>("recentMenu");
    QVERIFY(recent);
    QVERIFY(!recent->isEnabled());
    const QString a = tempPath(QStringLiteral("a.panel"));
    const QString b = tempPath(QStringLiteral("b.panel"));
    QVERIFY(chooseFile("saveAsAction", a));
    QVERIFY(chooseFile("saveAsAction", b));
    QVERIFY(chooseFile("openAction", a));
    QVERIFY(recent->isEnabled());
    QCOMPARE(recentTexts(), (QStringList{ "a.panel", "b.panel" }));
    QCOMPARE(recent->actions().at(0)->statusTip(), QDir::toNativeSeparators(a));
    QCOMPARE(recent->actions().at(1)->statusTip(), QDir::toNativeSeparators(b));

    QStringList expected;
    for (int i = 0; i < 9; ++i) {
        const QString name = QStringLiteral("p%1.panel").arg(i);
        QVERIFY(chooseFile("saveAsAction", tempPath(name)));
        expected.prepend(name);
    }
    expected.removeLast();
    QCOMPARE(recentTexts(), expected);
    QCOMPARE(expected.size(), 8);

    MainWindow another;
    auto *anotherRecent = another.findChild<QMenu *>(QStringLiteral("recentMenu"));
    QVERIFY(anotherRecent);
    QStringList anotherTexts;
    for (QAction *action : anotherRecent->actions())
        anotherTexts << action->text();
    QCOMPARE(anotherTexts, expected);

    Settings::saveRecentFiles({});
    MainWindow fresh;
    auto *freshRecent = fresh.findChild<QMenu *>(QStringLiteral("recentMenu"));
    QVERIFY(freshRecent);
    QVERIFY(!freshRecent->isEnabled());
}

void TestFiles::openRecentOpensTheFile()
{
    lightLamp1();
    const QString path = tempPath(QStringLiteral("tutorial.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    Project saved(window->project().catalog());
    QCOMPARE(openProject(path, saved).status, OpenResult::Status::Opened);
    clickFileAction("newAction");
    QVERIFY(window->project().devices().isEmpty());

    QVERIFY(clickRecent(0, true));
    auto *recent = named<QMenu>("recentMenu");
    // A menu is a window of its own, so the window's grab would not show it.
    QVERIFY(saveScreenshot(*recent, QStringLiteral("f10"), QStringLiteral("recent")));
    clickMenuItem(recent, recent->actions().at(0));
    compareDesigns(window->project(), saved);
    QCOMPARE(window->filePath(), path);
    QVERIFY(window->simulation().isOn(u"d1"));
}

void TestFiles::missingRecentFileIsRemoved()
{
    const QString path = tempPath(QStringLiteral("x.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    QVERIFY(QFile::remove(path));
    const QList<PlacedDevice> devices = window->project().devices();
    QString name;
    QString text;
    const bool shown = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                name = box.objectName();
                text = box.text();
                QTest::mouseClick(box.button(QMessageBox::Ok), Qt::LeftButton);
            },
            [&] { QVERIFY(clickRecent(0)); }, "fileErrorBox");
    QVERIFY(shown);
    QCOMPARE(text, QStringLiteral("Can't find x.panel; it was moved or deleted."));
    QVERIFY(recentTexts().isEmpty());
    QVERIFY(!named<QMenu>("recentMenu")->isEnabled());
    QCOMPARE(window->project().devices(), devices);
    QCOMPARE(window->filePath(), path);
}

void TestFiles::damagedRecentFileStays()
{
    const QString path = tempPath(QStringLiteral("d.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    clickFileAction("newAction");
    QVERIFY(writeAll(path, "{"));
    QString text;
    const bool shown = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                text = box.text();
                QTest::mouseClick(box.button(QMessageBox::Ok), Qt::LeftButton);
            },
            [&] { QVERIFY(clickRecent(0)); }, "fileErrorBox");
    QVERIFY(shown);
    QCOMPARE(text, QStringLiteral("d.panel is not an Electricity panel file, or it is damaged."));
    QCOMPARE(recentTexts(), (QStringList{ "d.panel" }));
}

void TestFiles::fileTextsFollowLanguage()
{
    clickLanguage("languageAction_ru");
    QCOMPARE(named<QMenu>("fileMenu")->title(), QStringLiteral("Файл"));
    QCOMPARE(window->windowTitle(), QStringLiteral("Без имени[*] — Electricity"));
    editProject();
    QString text;
    QVERIFY(answerQuestion("saveChangesCancel", [&] { window->close(); }, &text));
    QCOMPARE(text, QStringLiteral("Сохранить изменения в новом проекте?"));
}

void TestFiles::fileDialogIsTranslated_data()
{
    QTest::addColumn<QByteArray>("action");
    QTest::newRow("ru") << QByteArray("languageAction_ru");
    QTest::newRow("kk") << QByteArray("languageAction_kk");
}

// Qt's own dialog, whose strings the app translates itself.
void TestFiles::fileDialogIsTranslated()
{
    QFETCH(QByteArray, action);
    clickLanguage(action.constData());

    QStringList labels;
    QString open;
    QString cancel;
    bool screenshot = false;
    const bool shown = onModal<QFileDialog>(
            [&](QFileDialog &dialog) {
                QVERIFY(QTest::qWaitForWindowActive(&dialog));
                for (const char *name : { "lookInLabel", "fileNameLabel", "fileTypeLabel" }) {
                    auto *label = dialog.findChild<QLabel *>(QString::fromLatin1(name));
                    QVERIFY2(label, name);
                    labels << label->text();
                }
                auto *box = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
                QVERIFY(box && box->button(QDialogButtonBox::Open)
                        && box->button(QDialogButtonBox::Cancel));
                open = box->button(QDialogButtonBox::Open)->text();
                cancel = box->button(QDialogButtonBox::Cancel)->text();
                const QString step
                        = QStringLiteral("dialog-") + QString::fromLatin1(QTest::currentDataTag());
                screenshot = saveScreenshot(dialog, QStringLiteral("f10"), step);
                QTest::keyClick(&dialog, Qt::Key_Escape);
            },
            [&] { clickFileAction("openAction"); });
    QVERIFY(shown);
    QCOMPARE(labels.size(), 3);
    QCOMPARE_NE(labels.at(0), QStringLiteral("&Look in:"));
    QCOMPARE_NE(labels.at(1), QStringLiteral("File &name:"));
    QCOMPARE_NE(labels.at(2), QStringLiteral("Files of &type:"));
    QCOMPARE_NE(open, QStringLiteral("&Open"));
    QCOMPARE_NE(cancel, QStringLiteral("Cancel"));
    QVERIFY(screenshot);
}

void TestFiles::roadmapScenario()
{
    clickPause();
    lightLamp1();
    const QStringList expectedOn{ "d1", "d2", "d3", "l1" };
    QCOMPARE(window->simulation().switchedOn(), expectedOn);
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("roadmap-1")));

    const QString path = tempPath(QStringLiteral("roadmap.panel"));
    QVERIFY(chooseFile("saveAsAction", path));
    Project snapshot(window->project().catalog());
    snapshot.replace(window->project());
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("roadmap-2")));

    clickFileAction("newAction");
    QVERIFY(window->project().devices().isEmpty());
    QVERIFY(window->simulation().switchedOn().isEmpty());
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("roadmap-3")));

    QVERIFY(chooseFile("openAction", path));
    compareDesigns(snapshot, window->project());
    QCOMPARE(window->simulation().switchedOn(), expectedOn);
    QVERIFY(!window->isWindowModified());
    QVERIFY(saveScreenshot(*window, QStringLiteral("f10"), QStringLiteral("roadmap-4")));

    const auto at = [this](int module) {
        const QRectF slot = scene->slotRect(0, module, 1);
        return viewportPoint(canvas, slot.center() + QPointF(0, slot.height() * 0.15));
    };
    const int module = window->project().device(u"d5")->module;
    mouseDrag(canvas->viewport(), at(module),
              at(module + 2)); // the view is zoomed out: one module is under the drag distance
    QCOMPARE(window->project().device(u"d5")->module, module + 2);
    QVERIFY(window->isWindowModified());
    bool closed = true;
    bool asked = onModal<QMessageBox>(
            [&](QMessageBox &box) {
                QCOMPARE(box.objectName(), QStringLiteral("saveChangesBox"));
                QVERIFY(saveScreenshot(box, QStringLiteral("f10"), QStringLiteral("roadmap-5")));
                QTest::mouseClick(
                        box.findChild<QAbstractButton *>(QStringLiteral("saveChangesDiscard")),
                        Qt::LeftButton);
            },
            [&] { closed = window->close(); });
    QVERIFY(asked);
    QVERIFY(closed);
    // Don't Save left the file as it was.
    Project reread(window->project().catalog());
    QCOMPARE(openProject(path, reread).status, OpenResult::Status::Opened);
    compareDesigns(reread, snapshot);
}

QTEST_MAIN(TestFiles)
#include "tst_files.moc"
