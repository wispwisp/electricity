// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/mainwindow.h"

#include <QDockWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScreen>
#include <QStatusBar>

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kFrameAllowance(20, 60);
constexpr int kPaletteWidth = 240;
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

    auto *canvas = new QGraphicsView(new QGraphicsScene(this), this);
    canvas->setObjectName(QStringLiteral("canvasView"));
    setCentralWidget(canvas);

    auto *dock = new QDockWidget(tr("Components"), this);
    dock->setObjectName(QStringLiteral("paletteDock"));
    dock->setWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    resizeDocks({ dock }, { kPaletteWidth }, Qt::Horizontal);

    statusBar()->showMessage(tr("Ready"));

    resize(kDefaultSize);
    if (startsMaximized(screen()->availableGeometry().size()))
        setWindowState(Qt::WindowMaximized);
}
