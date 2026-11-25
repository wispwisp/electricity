// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/mainwindow.h"

#include "app/canvasview.h"
#include "app/editcommands.h"
#include "app/editorscene.h"
#include "app/eventtext.h"
#include "app/language.h"
#include "app/palettewidget.h"
#include "app/propertiespanel.h"
#include "app/sectiontext.h"
#include "app/settings.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "core/project.h"
#include "core/projectfile.h"
#include "core/simulation.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>

#include <ranges>
#include <utility>

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kFrameAllowance(20, 60);
constexpr int kPaletteWidth = 300;
constexpr int kPropertiesWidth = 230;
constexpr int kHintTimeout = 5000; // ms
constexpr int kTickInterval = 50;  // ms of real time between simulation steps (ADR 0008)
constexpr int kStartSpeed = 10;    // every launch, not saved
constexpr int kMaxRecentFiles = 8;

// The condition an event starts or ends: Tripped, Overloaded (any heat) or Dangerous.
SimulationEvent::Kind condition(const SimulationEvent &event)
{
    using Kind = SimulationEvent::Kind;
    if (event.kind == Kind::Cleared)
        return event.ends;
    return event.kind == Kind::Overheating ? Kind::Overloaded : event.kind;
}
} // namespace

bool startsMaximized(const QSize &availableScreenSize)
{
    const QSize needed = kDefaultSize + kFrameAllowance;
    return availableScreenSize.width() < needed.width()
            || availableScreenSize.height() < needed.height();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("mainWindow"));

    m_project = new Project(embeddedCatalog(), this);
    m_undoStack = new QUndoStack(this);
    m_scene = new EditorScene(*m_project, *m_undoStack, this);
    auto *canvas = new CanvasView(m_scene, this);
    canvas->setObjectName(QStringLiteral("canvasView"));
    setCentralWidget(canvas);
    // After the scene's own connection, so the fit sees the new enclosure.
    connect(m_project, &Project::enclosureChanged, canvas, &CanvasView::fitToPanel);
    QStatusBar *bar = statusBar();
    connect(m_scene, &EditorScene::hint, bar,
            [bar](const QString &text) { bar->showMessage(text, kHintTimeout); });
    // The wire tool's hints don't outlive it.
    connect(m_scene, &EditorScene::wireModeChanged, bar, [bar](bool on) {
        if (!on)
            bar->clearMessage();
    });
    connect(bar, &QStatusBar::messageChanged, this, [this](const QString &message) {
        if (message.isEmpty())
            showIdleText();
    });
    connect(&m_scene->simulation(), &Simulation::occurred, this, &MainWindow::onEvent);
    // A hint shown by the scene's reaction to the same change stays.
    connect(m_project, &Project::changed, this, [this] {
        if (statusBar()->currentMessage() == m_idleText)
            showIdleText();
    });

    m_dock = new QDockWidget(this);
    m_dock->setObjectName(QStringLiteral("paletteDock"));
    m_dock->setWidget(new PaletteWidget(embeddedCatalog(), m_dock));
    m_dock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, m_dock);
    resizeDocks({ m_dock }, { kPaletteWidth }, Qt::Horizontal);

    m_propertiesDock = new QDockWidget(this);
    m_propertiesDock->setObjectName(QStringLiteral("propertiesDock"));
    m_properties = new PropertiesPanel(*m_project, *m_undoStack, *m_scene, m_propertiesDock);
    m_propertiesDock->setWidget(m_properties);
    m_propertiesDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    resizeDocks({ m_propertiesDock }, { kPropertiesWidth }, Qt::Horizontal);
    connect(m_properties, &PropertiesPanel::hint, bar,
            [bar](const QString &text) { bar->showMessage(text, kHintTimeout); });

    m_undoAction = new QAction(this);
    m_undoAction->setObjectName(QStringLiteral("undoAction"));
    m_undoAction->setIcon(svgIcon(QStringLiteral(":/svg/ui/undo.svg"), 24));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
            [this](bool clean) { setWindowModified(!clean); });
    connect(m_undoAction, &QAction::triggered, m_undoStack, &QUndoStack::undo);
    connect(m_undoStack, &QUndoStack::canUndoChanged, m_undoAction, &QAction::setEnabled);
    m_redoAction = new QAction(this);
    m_redoAction->setObjectName(QStringLiteral("redoAction"));
    m_redoAction->setIcon(svgIcon(QStringLiteral(":/svg/ui/redo.svg"), 24));
    m_redoAction->setShortcuts(
            { QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), QKeySequence(Qt::CTRL | Qt::Key_Y) });
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered, m_undoStack, &QUndoStack::redo);
    connect(m_undoStack, &QUndoStack::canRedoChanged, m_redoAction, &QAction::setEnabled);

    m_wireAction = new QAction(this);
    m_wireAction->setObjectName(QStringLiteral("wireAction"));
    m_wireAction->setIcon(svgIcon(QStringLiteral(":/svg/ui/wire.svg"), 24));
    m_wireAction->setCheckable(true);
    connect(m_wireAction, &QAction::toggled, m_scene, &EditorScene::setWireMode);
    connect(m_scene, &EditorScene::wireModeChanged, m_wireAction, &QAction::setChecked);
    // A window shortcut, so that Esc reaches the tool wherever the focus is, on a section drop-down
    // say. Only while the tool is on, so that otherwise Esc still cancels a device move.
    auto *wireEscapeAction = new QAction(this);
    wireEscapeAction->setObjectName(QStringLiteral("wireEscapeAction"));
    wireEscapeAction->setShortcut(Qt::Key_Escape);
    wireEscapeAction->setEnabled(false);
    addAction(wireEscapeAction);
    connect(m_scene, &EditorScene::wireModeChanged, wireEscapeAction, &QAction::setEnabled);
    connect(wireEscapeAction, &QAction::triggered, m_scene, &EditorScene::escapeWireTool);
    m_wireSectionCombo = new QComboBox(this);
    m_wireSectionCombo->setObjectName(QStringLiteral("wireSectionCombo"));
    m_wireSectionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    for (double section : WireSections)
        m_wireSectionCombo->addItem(QString(), section);
    m_wireSectionCombo->setCurrentIndex(m_wireSectionCombo->findData(m_scene->wireSection()));
    connect(m_wireSectionCombo, &QComboBox::activated, this, [this](int index) {
        m_scene->setWireSection(m_wireSectionCombo->itemData(index).toDouble());
    });

    m_deleteAction = new QAction(this);
    m_deleteAction->setObjectName(QStringLiteral("deleteAction"));
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setShortcutContext(Qt::WidgetShortcut);
    m_deleteAction->setEnabled(false);
    canvas->addAction(m_deleteAction);
    m_scene->setDeviceContextActions({ m_deleteAction });
    const auto updateDelete = [this] {
        m_deleteAction->setEnabled(!m_scene->selectedDeviceId().isEmpty()
                                   || !m_scene->selectedWireId().isEmpty()
                                   || !m_scene->selectedLoadId().isEmpty());
    };
    connect(m_scene, &QGraphicsScene::selectionChanged, this, updateDelete);
    connect(m_project, &Project::changed, this, updateDelete);
    connect(m_deleteAction, &QAction::triggered, this, [this] {
        const QString wireId = m_scene->selectedWireId();
        const QString id = m_scene->selectedDeviceId();
        const QString loadId = m_scene->selectedLoadId();
        if (!wireId.isEmpty())
            m_undoStack->push(new RemoveWireCommand(*m_project, wireId));
        else if (!id.isEmpty())
            m_undoStack->push(new RemoveDeviceCommand(*m_project, id));
        else if (!loadId.isEmpty())
            m_undoStack->push(new RemoveLoadCommand(*m_project, loadId));
    });

    m_fitAction = new QAction(this);
    m_fitAction->setObjectName(QStringLiteral("fitAction"));
    m_fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_fitAction, &QAction::triggered, canvas, &CanvasView::fitToPanel);

    m_showCurrentsAction = new QAction(this);
    m_showCurrentsAction->setObjectName(QStringLiteral("showCurrentsAction"));
    m_showCurrentsAction->setCheckable(true);
    m_showCurrentsAction->setChecked(Settings::savedShowCurrents());
    m_scene->setCurrentsShown(m_showCurrentsAction->isChecked());
    connect(m_showCurrentsAction, &QAction::toggled, this, [this](bool shown) {
        m_scene->setCurrentsShown(shown);
        Settings::saveShowCurrents(shown);
    });

    m_fileMenu = menuBar()->addMenu(QString());
    m_fileMenu->setObjectName(QStringLiteral("fileMenu"));
    // Shortcuts are set explicitly, as Windows has no standard Save As or Quit key.
    const auto addFileAction = [this](const QString &name, const QKeySequence &shortcut) {
        auto *action = m_fileMenu->addAction(QString());
        action->setObjectName(name);
        action->setShortcut(shortcut);
        return action;
    };
    m_newAction = addFileAction(QStringLiteral("newAction"), QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newProject);
    m_openAction = addFileAction(QStringLiteral("openAction"), QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(m_openAction, &QAction::triggered, this, [this] {
        if (!maybeSave())
            return;
        const QString path = chooseFile(false);
        if (!path.isEmpty())
            openFile(path);
    });
    m_recentMenu = m_fileMenu->addMenu(QString());
    m_recentMenu->setObjectName(QStringLiteral("recentMenu"));
    rebuildRecentMenu();
    m_fileMenu->addSeparator();
    m_saveAction = addFileAction(QStringLiteral("saveAction"), QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::save);
    m_saveAsAction = addFileAction(QStringLiteral("saveAsAction"),
                                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);
    m_fileMenu->addSeparator();
    m_quitAction = addFileAction(QStringLiteral("quitAction"), QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_editMenu = menuBar()->addMenu(QString());
    m_editMenu->setObjectName(QStringLiteral("editMenu"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_deleteAction);
    m_viewMenu = menuBar()->addMenu(QString());
    m_viewMenu->setObjectName(QStringLiteral("viewMenu"));
    m_viewMenu->addAction(m_fitAction);
    m_viewMenu->addAction(m_showCurrentsAction);

    m_editToolBar = addToolBar(QString());
    m_editToolBar->setObjectName(QStringLiteral("editToolBar"));
    m_editToolBar->setMovable(false);
    m_editToolBar->addAction(m_undoAction);
    m_editToolBar->addAction(m_redoAction);
    m_editToolBar->addSeparator();
    m_editToolBar->addAction(m_wireAction);
    qobject_cast<QToolButton *>(m_editToolBar->widgetForAction(m_wireAction))
            ->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_editToolBar->addWidget(m_wireSectionCombo);
    m_editToolBar->addSeparator();
    m_timeLabel = new QLabel(this);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_editToolBar->addWidget(m_timeLabel);
    m_simulationTimer = new QTimer(this);
    m_simulationTimer->setObjectName(QStringLiteral("simulationTimer"));
    m_simulationTimer->setInterval(kTickInterval);
    connect(m_simulationTimer, &QTimer::timeout, this,
            [this] { simulation().step(kTickInterval / 1000.0 * m_speed); });
    auto *speedGroup = new QActionGroup(this);
    const auto addSpeedAction = [&](const QString &name, int speed) {
        auto *action = m_editToolBar->addAction(QString());
        action->setObjectName(name);
        action->setCheckable(true);
        speedGroup->addAction(action);
        connect(action, &QAction::toggled, this, [this, speed](bool checked) {
            if (!checked)
                return;
            m_speed = speed;
            if (speed > 0)
                m_simulationTimer->start();
            else
                m_simulationTimer->stop();
        });
        return action;
    };
    m_pauseAction = addSpeedAction(QStringLiteral("pauseAction"), 0);
    m_pauseAction->setIcon(svgIcon(QStringLiteral(":/svg/ui/pause.svg"), 24));
    for (int speed : { 1, 10, 60 }) {
        QAction *action = addSpeedAction(QStringLiteral("speed%1Action").arg(speed), speed);
        action->setText(QStringLiteral("×%1").arg(speed)); // not translated
        action->setChecked(speed == kStartSpeed);
    }

    m_settingsMenu = menuBar()->addMenu(QString());
    m_settingsMenu->setObjectName(QStringLiteral("settingsMenu"));
    m_themeMenu = m_settingsMenu->addMenu(QString());
    m_themeMenu->setObjectName(QStringLiteral("themeMenu"));
    auto *themeGroup = new QActionGroup(this);
    const auto addThemeAction = [&](const QString &name, Theme::Mode mode) {
        auto *action = m_themeMenu->addAction(QString());
        action->setObjectName(QStringLiteral("themeAction_%1").arg(name));
        action->setCheckable(true);
        action->setChecked(Theme::currentMode() == mode);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [mode] {
            Theme::apply(mode);
            Settings::saveTheme(mode);
        });
        return action;
    };
    m_darkAction = addThemeAction(QStringLiteral("dark"), Theme::Mode::Dark);
    m_lightAction = addThemeAction(QStringLiteral("light"), Theme::Mode::Light);

    m_languageMenu = menuBar()->addMenu(QString());
    m_languageMenu->setObjectName(QStringLiteral("languageMenu"));
    auto *languageGroup = new QActionGroup(this);
    for (Language language : kAllLanguages) {
        auto *action = m_languageMenu->addAction(nativeLanguageName(language));
        action->setObjectName(QStringLiteral("languageAction_%1").arg(languageCode(language)));
        action->setCheckable(true);
        action->setChecked(currentLanguage() == language);
        languageGroup->addAction(action);
        connect(action, &QAction::triggered, this, [language] {
            installLanguage(language);
            Settings::saveLanguage(language);
        });
    }

    retranslateUi();

    resize(kDefaultSize);
    if (startsMaximized(screen()->availableGeometry().size()))
        setWindowState(Qt::WindowMaximized);
}

// The scene refers to the project, which as the older child would otherwise be deleted first. The
// properties panel refers to both.
MainWindow::~MainWindow()
{
    delete m_propertiesDock;
    delete m_scene;
}

Project &MainWindow::project()
{
    return *m_project;
}

QUndoStack &MainWindow::undoStack()
{
    return *m_undoStack;
}

Simulation &MainWindow::simulation()
{
    return m_scene->simulation();
}

QString MainWindow::filePath() const
{
    return m_filePath;
}

bool MainWindow::event(QEvent *event)
{
    // Menu items without a status tip would otherwise clear the status message.
    if (event->type() == QEvent::StatusTip
        && static_cast<QStatusTipEvent *>(event)->tip().isEmpty())
        return true;
    return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    m_fileMenu->setTitle(tr("File"));
    m_newAction->setText(tr("New"));
    m_openAction->setText(tr("Open…"));
    m_recentMenu->setTitle(tr("Open Recent"));
    m_saveAction->setText(tr("Save"));
    m_saveAsAction->setText(tr("Save As…"));
    m_quitAction->setText(tr("Quit"));
    updateTitle();
    m_editMenu->setTitle(tr("Edit"));
    m_editToolBar->setWindowTitle(tr("Edit"));
    m_undoAction->setText(tr("Undo"));
    m_redoAction->setText(tr("Redo"));
    m_wireAction->setText(tr("Wire"));
    m_wireAction->setToolTip(tr("Draw wires: click one terminal, then another."));
    m_wireSectionCombo->setToolTip(tr("Section of new wires"));
    for (int i = 0; i < m_wireSectionCombo->count(); ++i) {
        const double section = m_wireSectionCombo->itemData(i).toDouble();
        m_wireSectionCombo->setItemText(i, sectionChoiceText(m_project->catalog(), section));
    }
    m_timeLabel->setText(tr("Time"));
    m_pauseAction->setText(tr("Pause"));
    m_deleteAction->setText(tr("Delete"));
    m_viewMenu->setTitle(tr("View"));
    m_fitAction->setText(tr("Fit to window"));
    m_showCurrentsAction->setText(tr("Show currents"));
    m_settingsMenu->setTitle(tr("Settings"));
    m_themeMenu->setTitle(tr("Theme"));
    m_darkAction->setText(tr("Dark"));
    m_lightAction->setText(tr("Light"));
    m_languageMenu->setTitle(tr("Language"));
    m_dock->setWindowTitle(tr("Components"));
    m_propertiesDock->setWindowTitle(tr("Properties"));
    m_scene->retranslate();
    showIdleText();
}

// A condition per object and kind (trip, heat, danger), as an RCD can be tripped and overloaded at
// once; a new one replaces the object's old one of that kind and is the newest.
void MainWindow::onEvent(const SimulationEvent &event)
{
    m_conditions.removeIf([&event](const SimulationEvent &c) {
        return c.id == event.id && condition(c) == condition(event);
    });
    if (event.kind != SimulationEvent::Kind::Cleared)
        m_conditions.append(event);
    showIdleText();
}

// The newest condition whose object still exists, else "Ready".
void MainWindow::showIdleText()
{
    QString text = tr("Ready");
    for (const auto &condition : std::views::reverse(std::as_const(m_conditions))) {
        const QString id = condition.id;
        if (id == SupplyBreakerId || m_project->device(id) || m_project->wire(id)
            || m_project->load(id)) {
            text = eventText(*m_project, condition);
            break;
        }
    }
    m_idleText = text;
    statusBar()->showMessage(text);
}

bool MainWindow::maybeSave()
{
    // A shortcut leaves the focus in the field where a value may still be typed; it counts.
    m_properties->sendTypedValues();
    if (m_undoStack->isClean())
        return true;
    const QString text = m_filePath.isEmpty()
            ? tr("Save changes to the new project?")
            : tr("Save changes to %1?").arg(QFileInfo(m_filePath).fileName());
    QMessageBox box(QMessageBox::Question, QStringLiteral("Electricity"), text,
                    QMessageBox::NoButton, this);
    box.setObjectName(QStringLiteral("saveChangesBox"));
    QAbstractButton *saveButton = box.addButton(tr("Save"), QMessageBox::AcceptRole);
    saveButton->setObjectName(QStringLiteral("saveChangesSave"));
    QAbstractButton *discardButton = box.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
    discardButton->setObjectName(QStringLiteral("saveChangesDiscard"));
    QAbstractButton *cancelButton = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    cancelButton->setObjectName(QStringLiteral("saveChangesCancel"));
    box.setDefaultButton(qobject_cast<QPushButton *>(saveButton));
    box.setEscapeButton(cancelButton);
    box.exec();
    if (box.clickedButton() == saveButton)
        return save();
    return box.clickedButton() == discardButton;
}

void MainWindow::newProject()
{
    if (!maybeSave())
        return;
    takeOver(Project(m_project->catalog()), {}, QString());
}

// A file that can't be opened leaves the window's project as it was.
void MainWindow::openFile(const QString &path)
{
    Project opened(m_project->catalog());
    const OpenResult result = openProject(path, opened);
    const QString name = QFileInfo(path).fileName();
    switch (result.status) {
    case OpenResult::Status::Opened:
        break;
    case OpenResult::Status::NewerVersion:
        showFileError(tr("%1 was saved by a newer version of Electricity. Update the app to open "
                         "it.")
                              .arg(name));
        return;
    case OpenResult::Status::NotPanelFile:
    case OpenResult::Status::Refused:
        showFileError(tr("%1 is not an Electricity panel file, or it is damaged.").arg(name));
        return;
    }
    Settings::saveFileFolder(QFileInfo(path).absolutePath());
    addRecentFile(path);
    takeOver(opened, result.on, path);
}

// The old conditions go before the new contents arrive, whose solve may trip a device at once. The
// scene keeps items whose ids come back, and a wire start whose terminal does, so the selection and
// a wire in progress go first too.
void MainWindow::takeOver(const Project &source, const QStringList &on, const QString &path)
{
    m_scene->clearSelection();
    if (!m_scene->wireStart().isEmpty())
        m_scene->escapeWireTool(); // the tool stays on
    simulation().reset(on);
    m_conditions.clear();
    m_project->replace(source);
    m_undoStack->clear();
    setFilePath(path);
    showIdleText();
}

bool MainWindow::save()
{
    return m_filePath.isEmpty() ? saveAs() : saveTo(m_filePath);
}

bool MainWindow::saveAs()
{
    QString path = chooseFile(true);
    if (path.isEmpty())
        return false;
    // The dialog adds the suffix only to a name without a dot, so not to "ул. Абая 10"; then the
    // question whether to replace the file is ours, in the words of Qt's dialog.
    if (!path.endsWith(QStringLiteral(".panel"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".panel");
        if (QFileInfo::exists(path)) {
            QMessageBox box(QMessageBox::Warning, QStringLiteral("Electricity"),
                            QCoreApplication::translate(
                                    "QFileDialog", "%1 already exists.\nDo you want to replace it?")
                                    .arg(QFileInfo(path).fileName()),
                            QMessageBox::Yes | QMessageBox::No, this);
            box.setObjectName(QStringLiteral("replaceBox"));
            box.setDefaultButton(QMessageBox::No);
            if (box.exec() != QMessageBox::Yes)
                return false;
        }
    }
    return saveTo(path);
}

bool MainWindow::saveTo(const QString &path)
{
    m_properties->sendTypedValues(); // see maybeSave()
    QString error;
    if (!saveProject(path, *m_project, simulation().switchedOn(), error)) {
        showFileError(tr("Can't save %1: %2").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    m_undoStack->setClean();
    Settings::saveFileFolder(QFileInfo(path).absolutePath());
    addRecentFile(path);
    setFilePath(path);
    return true;
}

void MainWindow::setFilePath(const QString &path)
{
    m_filePath = path;
    updateTitle();
}

void MainWindow::updateTitle()
{
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    setWindowTitle(name + QStringLiteral("[*] — Electricity"));
}

// The system's dialog where there is one; with the default suffix the dialog itself adds .panel
// before it asks whether to replace a file.
QString MainWindow::chooseFile(bool save)
{
    QFileDialog dialog(this, save ? tr("Save As") : tr("Open"), Settings::savedFileFolder(),
                       tr("Panel projects (*.panel)"));
    if (save) {
        dialog.setObjectName(QStringLiteral("saveDialog"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setDefaultSuffix(QStringLiteral("panel"));
    } else {
        dialog.setObjectName(QStringLiteral("openDialog"));
        dialog.setFileMode(QFileDialog::ExistingFile);
    }
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles().value(0);
}

void MainWindow::showFileError(const QString &text)
{
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Electricity"), text, QMessageBox::Ok,
                    this);
    box.setObjectName(QStringLiteral("fileErrorBox"));
    box.exec();
}

void MainWindow::addRecentFile(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QStringList files = Settings::savedRecentFiles();
    files.removeAll(absolute);
    files.prepend(absolute);
    while (files.size() > kMaxRecentFiles)
        files.removeLast();
    Settings::saveRecentFiles(files);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    const QStringList files = Settings::savedRecentFiles();
    for (const QString &path : files) {
        QAction *action = m_recentMenu->addAction(QFileInfo(path).fileName());
        action->setStatusTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] {
            if (!QFileInfo::exists(path)) {
                showFileError(tr("Can't find %1; it was moved or deleted.")
                                      .arg(QFileInfo(path).fileName()));
                QStringList left = Settings::savedRecentFiles();
                left.removeAll(path);
                Settings::saveRecentFiles(left);
                rebuildRecentMenu();
                return;
            }
            if (maybeSave())
                openFile(path);
        });
    }
    m_recentMenu->setEnabled(!files.isEmpty());
}
