// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class Project;
struct SimulationEvent;

// The status bar text of an event that starts a condition, in the current language:
// "QF4 tripped: overload, 22.0 A on C16"; "" for Cleared.
[[nodiscard]] QString eventText(const Project &project, const SimulationEvent &event);
