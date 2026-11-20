// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/language.h"
#include "app/mainwindow.h"
#include "app/palettewidget.h"
#include "app/settings.h"
#include "app/theme.h"
#include "menuclicks.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QApplication>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMenu>
#include <QTest>
#include <QToolTip>
#include <QTreeWidget>

namespace {
QTreeWidgetItem *presetItem(QTreeWidget *tree, const QString &id)
{
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = tree->topLevelItem(g);
        for (int i = 0; i < group->childCount(); ++i) {
            if (group->child(i)->data(0, PaletteWidget::PresetIdRole).toString() == id)
                return group->child(i);
        }
    }
    return nullptr;
}

void hover(QTreeWidget *tree, QTreeWidgetItem *item)
{
    const QPoint center = tree->visualItemRect(item).center();
    QTest::mouseMove(tree->viewport(), center);
    QHelpEvent event(QEvent::ToolTip, center, tree->viewport()->mapToGlobal(center));
    QApplication::sendEvent(tree->viewport(), &event);
}

QWidget *tooltipLabel()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == QLatin1String("qtooltip_label") && widget->isVisible())
            return widget;
    }
    return nullptr;
}

void chooseLanguage(MainWindow &window, const QString &code)
{
    auto *menu = window.findChild<QMenu *>("languageMenu");
    clickMenuBarItem(window, menu);
    clickMenuItem(menu, window.findChild<QAction *>("languageAction_" + code));
}
} // namespace

class TestPalette : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void showsProtectionPresetsWithTooltips();
    void switchesLanguageAndTheme();
    void textFitsInEveryLanguage();
};

void TestPalette::initTestCase()
{
    useTemporarySettings();
    Settings::saveLanguage(Language::English);
    setupApplication(*qApp);
}

void TestPalette::showsProtectionPresetsWithTooltips()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *tree = window.findChild<QTreeWidget *>("paletteTree");
    QVERIFY(tree);
    QCOMPARE(tree->topLevelItemCount(), 1);
    QTreeWidgetItem *group = tree->topLevelItem(0);
    QCOMPARE(group->text(0), QStringLiteral("Protection"));
    QVERIFY(group->isExpanded());
    const QList<std::pair<QString, QString>> expected{
        { "breaker-2p-c40", "Breaker 2P C40\napartment main" },
        { "rcd-2p-40-30", "RCD 40 A 30 mA\nleakage protection" },
        { "breaker-1p-c10", "Breaker 1P C10\nlighting" },
        { "breaker-1p-c16", "Breaker 1P C16\nsockets" },
        { "breaker-1p-c32", "Breaker 1P C32\ncooktop" },
        { "rcbo-c16-30", "RCBO C16 30 mA\nsockets, breaker and RCD in one" }
    };
    QCOMPARE(group->childCount(), int(expected.size()));
    for (int i = 0; i < expected.size(); ++i) {
        QTreeWidgetItem *item = group->child(i);
        QCOMPARE(item->data(0, PaletteWidget::PresetIdRole).toString(), expected[i].first);
        QCOMPARE(item->text(0), expected[i].second);
        QVERIFY(!item->icon(0).isNull());
        QCOMPARE(item->icon(0).actualSize(tree->iconSize()), tree->iconSize());
        QVERIFY(tree->viewport()->rect().contains(tree->visualItemRect(item))); // no scrolling
    }
    QTreeWidgetItem *c10 = presetItem(tree, "breaker-1p-c10");
    QVERIFY(c10->toolTip(0).contains("Circuit breaker"));
    QVERIFY(c10->toolTip(0).contains("Turns it off by itself on overload or short circuit."));
    QVERIFY(c10->toolTip(0).contains("Usual wire: 1.5 mm²"));
    QVERIFY(presetItem(tree, "rcd-2p-40-30")->toolTip(0).contains("Residual current device (RCD)"));
    QVERIFY(!presetItem(tree, "rcd-2p-40-30")->toolTip(0).contains("Usual wire"));
    QVERIFY(presetItem(tree, "rcbo-c16-30")->toolTip(0).contains("Usual wire: 2.5 mm²"));
    hover(tree, c10);
    QTRY_VERIFY(QToolTip::isVisible());
    QCOMPARE(QToolTip::text(), c10->toolTip(0));
    QWidget *tipLabel = tooltipLabel();
    QVERIFY(tipLabel);
    QVERIFY(saveScreenshot(*tipLabel, "f04", "tooltip"));
    QToolTip::hideText();
    QVERIFY(saveScreenshot(window, "f04", "palette-dark"));
}

void TestPalette::switchesLanguageAndTheme()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *tree = window.findChild<QTreeWidget *>("paletteTree");
    QVERIFY(tree);

    chooseLanguage(window, "ru");
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Защита"));
    QTreeWidgetItem *c10 = presetItem(tree, "breaker-1p-c10");
    QCOMPARE(c10->text(0), QStringLiteral("Автомат 1P C10\nосвещение"));
    QVERIFY(c10->toolTip(0).contains("Автоматический выключатель"));
    QVERIFY(c10->toolTip(0).contains("Обычный провод: 1,5 mm²"));
    QVERIFY(saveScreenshot(window, "f04", "palette-ru"));

    chooseLanguage(window, "kk");
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Қорғаныс"));
    QCOMPARE(c10->text(0), QStringLiteral("Автомат 1P C10\nжарықтандыру"));
    QCOMPARE(presetItem(tree, "rcbo-c16-30")->text(0),
             QStringLiteral("Дифавтомат C16 30 mA\nрозеткалар, автомат пен УЗО бірге"));
    QVERIFY(c10->toolTip(0).contains("Әдеттегі сым: 1,5 mm²"));
    QVERIFY(saveScreenshot(window, "f04", "palette-kk"));

    chooseLanguage(window, "en");
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Protection"));

    auto *settingsMenu = window.findChild<QMenu *>("settingsMenu");
    auto *themeMenu = window.findChild<QMenu *>("themeMenu");
    clickMenuBarItem(window, settingsMenu);
    openSubmenu(settingsMenu, themeMenu);
    clickMenuItem(themeMenu, window.findChild<QAction *>("themeAction_light"));
    QCOMPARE(Theme::currentMode(), Theme::Mode::Light);
    QVERIFY(saveScreenshot(window, "f04", "palette-light"));

    clickMenuBarItem(window, settingsMenu);
    openSubmenu(settingsMenu, themeMenu);
    clickMenuItem(themeMenu, window.findChild<QAction *>("themeAction_dark"));
    QCOMPARE(Theme::currentMode(), Theme::Mode::Dark);
}

void TestPalette::textFitsInEveryLanguage()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *tree = window.findChild<QTreeWidget *>("paletteTree");
    QVERIFY(tree);
    for (const QString &code : QStringList{ "ru", "kk", "en" }) {
        chooseLanguage(window, code);
        const QFontMetrics metrics(tree->font());
        QTreeWidgetItem *group = tree->topLevelItem(0);
        for (int i = 0; i < group->childCount(); ++i) {
            QTreeWidgetItem *item = group->child(i);
            const int available = tree->visualItemRect(item).width() - tree->iconSize().width() - 8;
            for (const QString &line : item->text(0).split('\n')) {
                QVERIFY2(metrics.horizontalAdvance(line) <= available,
                         qPrintable(code + ": " + line));
            }
        }
    }
}

QTEST_MAIN(TestPalette)
#include "tst_palette.moc"
