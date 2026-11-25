// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/settings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace Settings {

namespace {
QSettings open()
{
    return { QSettings::IniFormat, QSettings::UserScope, QStringLiteral("electricity"),
             QStringLiteral("electricity") };
}
} // namespace

std::optional<Language> savedLanguage()
{
    return languageFromCode(open().value(QStringLiteral("language")).toString());
}

void saveLanguage(Language language)
{
    open().setValue(QStringLiteral("language"), languageCode(language));
}

Theme::Mode savedTheme()
{
    return open().value(QStringLiteral("theme")).toString() == QStringLiteral("light")
            ? Theme::Mode::Light
            : Theme::Mode::Dark;
}

void saveTheme(Theme::Mode mode)
{
    open().setValue(QStringLiteral("theme"),
                    mode == Theme::Mode::Light ? QStringLiteral("light") : QStringLiteral("dark"));
}

bool savedShowCurrents()
{
    return open().value(QStringLiteral("showCurrents"), true).toBool();
}

void saveShowCurrents(bool shown)
{
    open().setValue(QStringLiteral("showCurrents"), shown);
}

// Given a folder that doesn't exist, the file dialog would open its parent with the folder's name
// typed in as the file name.
QString savedFileFolder()
{
    QString folder = open().value(QStringLiteral("fileFolder")).toString();
    if (QFileInfo(folder).isDir())
        return folder;
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QFileInfo(documents).isDir() ? documents : QDir::homePath();
}

void saveFileFolder(const QString &folder)
{
    open().setValue(QStringLiteral("fileFolder"), folder);
}

QStringList savedRecentFiles()
{
    return open().value(QStringLiteral("recentFiles")).toStringList();
}

void saveRecentFiles(const QStringList &files)
{
    open().setValue(QStringLiteral("recentFiles"), files);
}

} // namespace Settings
