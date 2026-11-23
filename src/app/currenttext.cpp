// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/currenttext.h"

#include "app/language.h"

#include <QCoreApplication>
#include <QLocale>

bool showsCurrent(double amperes)
{
    return amperes >= 0.005;
}

// Two decimals below 10 A, one below 100 A, else none; the limits are where rounding reaches the
// next range, so 9.996 reads "10.0 A" and 99.96 "100 A".
QString currentText(double amperes)
{
    const int decimals = amperes < 9.995 ? 2 : amperes < 99.95 ? 1 : 0;
    return QCoreApplication::translate("CurrentText", "%1 A")
            .arg(QLocale(languageCode(currentLanguage())).toString(amperes, 'f', decimals));
}
