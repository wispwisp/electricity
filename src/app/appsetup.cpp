// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"

#include "app/language.h"
#include "app/languagedialog.h"
#include "app/settings.h"
#include "app/theme.h"
#include "core/version.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QLocale>

void applySavedPreferences()
{
    Theme::apply(Settings::savedTheme());
    installLanguage(Settings::savedLanguage().value_or(defaultLanguage(QLocale::system())));
}

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

    applySavedPreferences();
    app.setApplicationVersion(appVersion());
}

void askLanguageOnFirstLaunch()
{
    if (Settings::savedLanguage())
        return;
    LanguageDialog dialog(defaultLanguage(QLocale::system()));
    dialog.exec();
    installLanguage(dialog.chosenLanguage());
    Settings::saveLanguage(dialog.chosenLanguage());
}
