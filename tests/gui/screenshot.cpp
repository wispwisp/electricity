// SPDX-License-Identifier: GPL-3.0-or-later
#include "screenshot.h"

#include <QDir>
#include <QPixmap>
#include <QWidget>

bool saveScreenshot(QWidget &widget, const QString &test, const QString &step)
{
    const QString dir = QStringLiteral(SCREENSHOT_DIR);
    if (!QDir().mkpath(dir))
        return false;
    return widget.grab().save(dir + QLatin1Char('/') + test + QLatin1Char('-') + step
                              + QStringLiteral(".png"));
}
