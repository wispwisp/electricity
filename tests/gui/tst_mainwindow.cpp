// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/mainwindow.h"
#include "screenshot.h"

#include <QDockWidget>
#include <QGraphicsView>
#include <QStatusBar>
#include <QTest>

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void startsMaximizedOnlyOnSmallScreens_data();
    void startsMaximizedOnlyOnSmallScreens();
    void launchesMainWindow();
};

void TestMainWindow::startsMaximizedOnlyOnSmallScreens_data()
{
    QTest::addColumn<QSize>("available");
    QTest::addColumn<bool>("maximized");
    QTest::newRow("full HD") << QSize(1920, 1040) << false;
    QTest::newRow("1600x900 minus taskbar") << QSize(1600, 860) << false;
    QTest::newRow("1366x768 laptop") << QSize(1366, 728) << true;
    QTest::newRow("narrow 1280x1024") << QSize(1280, 984) << true;
}

void TestMainWindow::startsMaximizedOnlyOnSmallScreens()
{
    QFETCH(QSize, available);
    QFETCH(bool, maximized);
    QCOMPARE(startsMaximized(available), maximized);
}

void TestMainWindow::launchesMainWindow()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QCOMPARE(window.windowTitle(), QStringLiteral("Electricity"));
    QCOMPARE(window.size(), QSize(1280, 800));
    QVERIFY(!(window.windowState() & Qt::WindowMaximized));

    auto *canvas = window.findChild<QGraphicsView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas);
    QCOMPARE(window.centralWidget(), canvas);
    QVERIFY(canvas->scene());

    auto *dock = window.findChild<QDockWidget *>(QStringLiteral("paletteDock"));
    QVERIFY(dock);
    QVERIFY(dock->isVisible());
    QCOMPARE(window.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
    QCOMPARE(dock->windowTitle(), QStringLiteral("Components"));
    QVERIFY(!dock->features().testFlag(QDockWidget::DockWidgetClosable));
    QVERIFY(!dock->features().testFlag(QDockWidget::DockWidgetFloatable));
    QVERIFY(dock->width() >= 200);

    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Ready"));

    QVERIFY(saveScreenshot(window, QStringLiteral("f01"), QStringLiteral("main-window")));
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
