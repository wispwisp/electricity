// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"

#include "app/theme.h"
#include "core/version.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

void setupApplication(QApplication &app)
{
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DejaVuSans.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DejaVuSans-Bold.ttf"));
    QFont font = QApplication::font();
    font.setFamily(QStringLiteral("DejaVu Sans"));
    QApplication::setFont(font);

    Theme::apply();
    app.setApplicationVersion(appVersion());
}
