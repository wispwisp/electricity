// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"

#include "app/theme.h"
#include "core/version.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>

void setupApplication(QApplication &app)
{
    for (const QString &path : { QStringLiteral(":/fonts/DejaVuSans.ttf"),
                                 QStringLiteral(":/fonts/DejaVuSans-Bold.ttf") }) {
        if (QFontDatabase::addApplicationFont(path) == -1)
            qWarning() << "Cannot load embedded font" << path;
    }
    QFont font = QApplication::font();
    font.setFamily(QStringLiteral("DejaVu Sans"));
    QApplication::setFont(font);

    Theme::apply();
    app.setApplicationVersion(appVersion());
}
