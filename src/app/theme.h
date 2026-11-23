// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPalette>

#include <cstdint>

enum class Conductor : std::uint8_t;

namespace Theme {

enum class Mode : std::uint8_t { Dark, Light };

QPalette darkPalette();
QPalette lightPalette();

// Sets the Fusion style if needed, then the palette for the mode (setStyle resets the palette).
void apply(Mode mode);
// The mode last passed to apply().
Mode currentMode();

// Editor colours, the same in both themes.
QColor selectionColor();
QColor refusedColor();
// A running appliance's halo, and a lit lamp's glow.
QColor liveColor();
QColor lampGlowColor();
// A tripped device's outline and badge; also overheating and danger.
QColor trippedColor();
QColor overloadedColor();
QColor faultMarkColor();
// Designations on the enclosure's dark interior.
QColor enclosureTextColor();
// Wires, as on the supply picture (ADR 0013): the conductor colour, yellow stripes on PE, and a
// dark edge.
QColor conductorColor(Conductor conductor);
QColor earthStripeColor();
QColor wireEdgeColor();

} // namespace Theme
