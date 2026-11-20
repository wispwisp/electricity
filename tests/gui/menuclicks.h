// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QAction;
class QMainWindow;
class QMenu;

// Opens a top-level menu by clicking its menu bar entry.
void clickMenuBarItem(QMainWindow &window, QMenu *menu);
// Clicks an item of an open menu and delivers the posted LanguageChange events.
void clickMenuItem(QMenu *menu, QAction *action);
// Opens a submenu of an open menu by clicking its entry.
void openSubmenu(QMenu *menu, QMenu *submenu);
