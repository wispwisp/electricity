// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Redirects QSettings' INI user scope to a temporary folder that lives until the process ends.
// Returns the INI file path the application will use.
QString useTemporarySettings();
// Empties that INI file.
void clearSettings();
