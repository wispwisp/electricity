// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPalette>

namespace Theme {

QPalette darkPalette();

// Sets the Fusion style, then the dark palette (setStyle can reset the palette).
void apply();

} // namespace Theme
