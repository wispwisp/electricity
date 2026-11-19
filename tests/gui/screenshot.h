// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QWidget;

// Saves widget.grab() to SCREENSHOT_DIR/<test>-<step>.png, creating the folder if needed.
bool saveScreenshot(QWidget &widget, const QString &test, const QString &step);
