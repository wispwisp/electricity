// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/language.h"
#include "app/theme.h"

#include <optional>

namespace Settings {

// Unknown or missing values count as not saved.
std::optional<Language> savedLanguage();
void saveLanguage(Language language);
// Dark when unset or unknown.
Theme::Mode savedTheme();
void saveTheme(Theme::Mode mode);

} // namespace Settings
