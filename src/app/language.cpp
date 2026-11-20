// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/language.h"

QString languageCode(Language language)
{
    switch (language) {
    case Language::Russian:
        return QStringLiteral("ru");
    case Language::Kazakh:
        return QStringLiteral("kk");
    case Language::English:
        return QStringLiteral("en");
    }
    Q_UNREACHABLE();
}

std::optional<Language> languageFromCode(QStringView code)
{
    for (Language language : kAllLanguages) {
        if (code == languageCode(language))
            return language;
    }
    return std::nullopt;
}

QString nativeLanguageName(Language language)
{
    switch (language) {
    case Language::Russian:
        return QStringLiteral("Русский");
    case Language::Kazakh:
        return QStringLiteral("Қазақша");
    case Language::English:
        return QStringLiteral("English");
    }
    Q_UNREACHABLE();
}

Language defaultLanguage(const QLocale &systemLocale)
{
    return systemLocale.language() == QLocale::Kazakh ? Language::Kazakh : Language::Russian;
}
