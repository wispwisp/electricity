// SPDX-License-Identifier: GPL-3.0-or-later
#include "menuclicks.h"

#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTest>

void clickMenuBarItem(QMainWindow &window, QMenu *menu)
{
    QMenuBar *bar = window.menuBar();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier,
                      bar->actionGeometry(menu->menuAction()).center());
    QTRY_VERIFY(menu->isVisible());
}

void clickMenuItem(QMenu *menu, QAction *action)
{
    const QPoint center = menu->actionGeometry(action).center();
    QTest::mouseMove(menu, center);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, center);
    // LanguageChange events are posted, not delivered synchronously.
    QCoreApplication::processEvents();
}

void openSubmenu(QMenu *menu, QMenu *submenu)
{
    const QPoint center = menu->actionGeometry(submenu->menuAction()).center();
    QTest::mouseMove(menu, center);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY(submenu->isVisible());
}
