// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/settings.h"

#include <QSettings>

namespace Settings {

namespace {
QSettings open()
{
    return { QSettings::IniFormat, QSettings::UserScope, QStringLiteral("electricity"),
             QStringLiteral("electricity") };
}
} // namespace

std::optional<Language> savedLanguage()
{
    return languageFromCode(open().value(QStringLiteral("language")).toString());
}

void saveLanguage(Language language)
{
    open().setValue(QStringLiteral("language"), languageCode(language));
}

Theme::Mode savedTheme()
{
    return open().value(QStringLiteral("theme")).toString() == QStringLiteral("light")
            ? Theme::Mode::Light
            : Theme::Mode::Dark;
}

void saveTheme(Theme::Mode mode)
{
    open().setValue(QStringLiteral("theme"),
                    mode == Theme::Mode::Light ? QStringLiteral("light") : QStringLiteral("dark"));
}

} // namespace Settings
