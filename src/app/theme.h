// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPalette>

#include <cstdint>

namespace Theme {

enum class Mode : std::uint8_t { Dark, Light };

QPalette darkPalette();
QPalette lightPalette();

// Sets the Fusion style if needed, then the palette for the mode (setStyle resets the palette).
void apply(Mode mode);
// The mode last passed to apply().
Mode currentMode();

} // namespace Theme
