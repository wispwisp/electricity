// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/catalogstrings.h"

#include <QtGlobal>

QStringList catalogTranslationSources()
{
    return {
        QT_TRANSLATE_NOOP("Catalog", "Protection"), QT_TRANSLATE_NOOP("Catalog", "Breaker 2P C40"),
        QT_TRANSLATE_NOOP("Catalog", "RCD 40 A 30 mA"),
        QT_TRANSLATE_NOOP("Catalog", "Breaker 1P C10"),
        QT_TRANSLATE_NOOP("Catalog", "Breaker 1P C16"),
        QT_TRANSLATE_NOOP("Catalog", "Breaker 1P C32"),
        QT_TRANSLATE_NOOP("Catalog", "RCBO C16 30 mA"),
        QT_TRANSLATE_NOOP("Catalog", "apartment main"),
        QT_TRANSLATE_NOOP("Catalog", "leakage protection"),
        QT_TRANSLATE_NOOP("Catalog", "lighting"), QT_TRANSLATE_NOOP("Catalog", "sockets"),
        QT_TRANSLATE_NOOP("Catalog", "cooktop"),
        QT_TRANSLATE_NOOP("Catalog", "sockets, breaker and RCD in one"),
        QT_TRANSLATE_NOOP("Catalog", "Circuit breaker"),
        QT_TRANSLATE_NOOP("Catalog", "Residual current device (RCD)"),
        QT_TRANSLATE_NOOP("Catalog", "Residual current circuit breaker (RCBO)"),
        QT_TRANSLATE_NOOP(
                "Catalog",
                "Switches a line on and off. Turns it off by itself on overload or short circuit."),
        // NOLINTBEGIN(bugprone-suspicious-missing-comma)
        QT_TRANSLATE_NOOP("Catalog",
                          "Turns the power off when current leaks to earth, for example through a "
                          "person. Does not protect against overload."),
        QT_TRANSLATE_NOOP("Catalog",
                          "Breaker and RCD in one device: turns off on overload, short circuit and "
                          "current leakage."),
        // NOLINTEND(bugprone-suspicious-missing-comma)
    };
}
