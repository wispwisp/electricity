// SPDX-License-Identifier: GPL-3.0-or-later
#include "testsettings.h"

#include <QSettings>
#include <QTemporaryDir>

namespace {
QString iniPath;
}

QString useTemporarySettings()
{
    static const QTemporaryDir dir;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    iniPath = dir.path() + QStringLiteral("/electricity/electricity.ini");
    return iniPath;
}

void clearSettings()
{
    QSettings(iniPath, QSettings::IniFormat).clear();
}
