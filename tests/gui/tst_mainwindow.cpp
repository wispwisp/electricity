// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/mainwindow.h"
#include "app/settings.h"
#include "app/theme.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QApplication>
#include <QDockWidget>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGraphicsView>
#include <QStatusBar>
#include <QStyle>
#include <QTest>

#include <string_view>

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void startsMaximizedOnlyOnSmallScreens_data();
    void startsMaximizedOnlyOnSmallScreens();
    void launchesMainWindow();
    void appliesDarkFusionThemeAndEmbeddedFont();
};

void TestMainWindow::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

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

void TestMainWindow::appliesDarkFusionThemeAndEmbeddedFont()
{
    QCOMPARE(QApplication::style()->name(), QStringLiteral("fusion"));

    MainWindow window;
    const QColor windowColor = window.palette().color(QPalette::Window);
    QCOMPARE(windowColor, Theme::darkPalette().color(QPalette::Window));
    QVERIFY(windowColor.lightness() < 80);
    QVERIFY(window.palette().color(QPalette::WindowText).lightness() > 180);
    auto *canvas = window.findChild<QGraphicsView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas->palette().color(QPalette::Base).lightness() < 80);

    QCOMPARE(QApplication::font().family(), QStringLiteral("DejaVu Sans"));
    QCOMPARE(QFontInfo(window.font()).family(), QStringLiteral("DejaVu Sans"));

    const QFontMetrics metrics(window.font());
    for (char32_t letter : std::u32string_view(U"әғқңөұүһіЁё"))
        QVERIFY2(metrics.inFontUcs4(letter), qPrintable(QString::number(letter, 16)));

    // Loading succeeds only from the embedded resource, never from a system copy.
    QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DejaVuSans.ttf")) != -1);
    QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DejaVuSans-Bold.ttf")) != -1);
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
