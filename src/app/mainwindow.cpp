// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/mainwindow.h"

#include "app/canvasview.h"
#include "app/editcommands.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/palettewidget.h"
#include "app/settings.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QToolBar>
#include <QUndoStack>

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kFrameAllowance(20, 60);
constexpr int kPaletteWidth = 300;
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

    m_deleteAction = new QAction(this);
    m_deleteAction->setObjectName(QStringLiteral("deleteAction"));
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setShortcutContext(Qt::WidgetShortcut);
    m_deleteAction->setEnabled(false);
    canvas->addAction(m_deleteAction);
    m_scene->setDeviceContextActions({ m_deleteAction });
    const auto updateDelete
            = [this] { m_deleteAction->setEnabled(!m_scene->selectedDeviceId().isEmpty()); };
    connect(m_scene, &QGraphicsScene::selectionChanged, this, updateDelete);
    connect(m_project, &Project::changed, this, updateDelete);
    connect(m_deleteAction, &QAction::triggered, this, [this] {
        const QString id = m_scene->selectedDeviceId();
        if (!id.isEmpty())
            m_undoStack->push(new RemoveDeviceCommand(*m_project, id));
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

// The scene refers to the project, which as the older child would otherwise be deleted first.
MainWindow::~MainWindow()
{
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
    m_deleteAction->setText(tr("Delete"));
    m_viewMenu->setTitle(tr("View"));
    m_fitAction->setText(tr("Fit to window"));
    m_settingsMenu->setTitle(tr("Settings"));
    m_themeMenu->setTitle(tr("Theme"));
    m_darkAction->setText(tr("Dark"));
    m_lightAction->setText(tr("Light"));
    m_languageMenu->setTitle(tr("Language"));
    m_dock->setWindowTitle(tr("Components"));
    m_scene->retranslate();
    statusBar()->showMessage(tr("Ready"));
}
