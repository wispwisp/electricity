// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/sectiontext.h"

#include "app/language.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QCoreApplication>
#include <QLocale>

#include <algorithm>

QString sectionText(double section)
{
    return QLocale(languageCode(currentLanguage())).toString(section) + QStringLiteral(" mm²");
}

// The first breaker preset in catalog order that pairs with the section names its purpose.
QString sectionChoiceText(const Catalog &catalog, double section)
{
    const auto preset = std::ranges::find_if(catalog.presets, [section](const Preset &p) {
        return p.type == QLatin1StringView("breaker") && p.wireSection == section;
    });
    if (preset == catalog.presets.end())
        return sectionText(section);
    return sectionText(section) + QStringLiteral(" — ")
            + QCoreApplication::translate("Catalog", preset->purpose.toUtf8().constData())
            + QStringLiteral(", ") + ratingText(deviceFromPreset(*preset, 0, 0));
}
