// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Whether a current label shows: from 0.005 A, which reads as at least "0.01 A".
[[nodiscard]] bool showsCurrent(double amperes);
// "0.04 A", "13.4 A", "575 A", with the number format and unit of the current language.
[[nodiscard]] QString currentText(double amperes);
