// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

struct Catalog;

// "2.5 mm²", with the decimal separator of the current language.
QString sectionText(double section);
// sectionText() plus " — <purpose>, <rating>" when a breaker preset pairs with the section.
QString sectionChoiceText(const Catalog &catalog, double section);
