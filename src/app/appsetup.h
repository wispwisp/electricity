// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QApplication;

// Startup steps shared by the executable and the GUI tests.
void setupApplication(QApplication &app);
// Applies the saved theme and installs the saved language (or the system default).
void applySavedPreferences();
// Asks for the language when none is saved yet, then installs and saves the choice.
void askLanguageOnFirstLaunch();
