// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/mainwindow.h"

#include "app/canvasview.h"
#include "app/editcommands.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/palettewidget.h"
#include "app/propertiespanel.h"
#include "app/sectiontext.h"
#include "app/settings.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kFrameAllowance(20, 60);
constexpr int kPaletteWidth = 300;
constexpr int kPropertiesWidth = 230;
constexpr int kHintTimeout = 5000; // ms
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
    setWindowTitle(QStringLiteral("Electricity"));

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
    connect(bar, &QStatusBar::messageChanged, bar, [bar](const QString &message) {
        if (message.isEmpty())
            bar->showMessage(tr("Ready"));
    });

    m_dock = new QDockWidget(this);
    m_dock->setObjectName(QStringLiteral("paletteDock"));
    m_dock->setWidget(new PaletteWidget(embeddedCatalog(), m_dock));
    m_dock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, m_dock);
    resizeDocks({ m_dock }, { kPaletteWidth }, Qt::Horizontal);

    m_propertiesDock = new QDockWidget(this);
    m_propertiesDock->setObjectName(QStringLiteral("propertiesDock"));
    auto *properties = new PropertiesPanel(*m_project, *m_undoStack, *m_scene, m_propertiesDock);
    m_propertiesDock->setWidget(properties);
    m_propertiesDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    resizeDocks({ m_propertiesDock }, { kPropertiesWidth }, Qt::Horizontal);
    connect(properties, &PropertiesPanel::hint, bar,
            [bar](const QString &text) { bar->showMessage(text, kHintTimeout); });

    m_undoAction = new QAction(this);
    m_undoAction->setObjectName(QStringLiteral("undoAction"));
    m_undoAction->setIcon(svgIcon(QStringLiteral(":/svg/ui/undo.svg"), 24));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
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

    m_editMenu = menuBar()->addMenu(QString());
    m_editMenu->setObjectName(QStringLiteral("editMenu"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_deleteAction);
    m_viewMenu = menuBar()->addMenu(QString());
    m_viewMenu->setObjectName(QStringLiteral("viewMenu"));
    m_viewMenu->addAction(m_fitAction);

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

bool MainWindow::event(QEvent *event)
{
    // Menu items without a status tip would otherwise clear the status message.
    if (event->type() == QEvent::StatusTip
        && static_cast<QStatusTipEvent *>(event)->tip().isEmpty())
        return true;
    return QMainWindow::event(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
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
    m_deleteAction->setText(tr("Delete"));
    m_viewMenu->setTitle(tr("View"));
    m_fitAction->setText(tr("Fit to window"));
    m_settingsMenu->setTitle(tr("Settings"));
    m_themeMenu->setTitle(tr("Theme"));
    m_darkAction->setText(tr("Dark"));
    m_lightAction->setText(tr("Light"));
    m_languageMenu->setTitle(tr("Language"));
    m_dock->setWindowTitle(tr("Components"));
    m_propertiesDock->setWindowTitle(tr("Properties"));
    m_scene->retranslate();
    statusBar()->showMessage(tr("Ready"));
}
