// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/language.h"

#include <QCoreApplication>
#include <QTranslator>

namespace {
Language g_currentLanguage = Language::English;
}

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

void installLanguage(Language language)
{
    static QTranslator translator;
    // removeTranslator sends LanguageChange, but installing the message-less English
    // translator does not, so removal is what retranslates the UI when switching to English.
    QCoreApplication::removeTranslator(&translator);
    if (!translator.load(QStringLiteral(":/i18n/electricity_%1.qm").arg(languageCode(language))))
        qWarning("Cannot load translation for %s", qPrintable(languageCode(language)));
    QCoreApplication::installTranslator(&translator);
    g_currentLanguage = language;
}

Language currentLanguage()
{
    return g_currentLanguage;
}
