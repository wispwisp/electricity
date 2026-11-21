// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/mainwindow.h"

#include "app/canvasview.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/palettewidget.h"
#include "app/settings.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QActionGroup>
#include <QDockWidget>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QUndoStack>

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kFrameAllowance(20, 60);
constexpr int kPaletteWidth = 300;
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

    m_dock = new QDockWidget(this);
    m_dock->setObjectName(QStringLiteral("paletteDock"));
    m_dock->setWidget(new PaletteWidget(embeddedCatalog(), m_dock));
    m_dock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, m_dock);
    resizeDocks({ m_dock }, { kPaletteWidth }, Qt::Horizontal);

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
    m_settingsMenu->setTitle(tr("Settings"));
    m_themeMenu->setTitle(tr("Theme"));
    m_darkAction->setText(tr("Dark"));
    m_lightAction->setText(tr("Light"));
    m_languageMenu->setTitle(tr("Language"));
    m_dock->setWindowTitle(tr("Components"));
    m_scene->retranslate();
    statusBar()->showMessage(tr("Ready"));
}
