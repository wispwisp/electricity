// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QLocale>
#include <QString>
#include <QStringView>

#include <array>
#include <cstdint>
#include <optional>

enum class Language : std::uint8_t { Russian, Kazakh, English };

inline constexpr std::array<Language, 3> kAllLanguages{ Language::Russian, Language::Kazakh,
                                                        Language::English };

QString languageCode(Language language);
// Exact match only: "RU" and "ru_RU" are not codes.
std::optional<Language> languageFromCode(QStringView code);
// Shown in the language's own script and never translated.
QString nativeLanguageName(Language language);
// Kazakh for a Kazakh system locale, Russian for everything else.
Language defaultLanguage(const QLocale &systemLocale);
// Replaces the installed translation; English before the first call.
void installLanguage(Language language);
Language currentLanguage();
