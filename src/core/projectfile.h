// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstdint>

class Project;

inline constexpr int ProjectFileVersion = 1;

struct OpenResult
{
    enum class Status : std::uint8_t { Opened, NotPanelFile, NewerVersion, Refused };
    Status status = Status::Opened;
    QString reason; // English, for tests and debugging, never shown; "" when Opened
    QStringList on; // ids saved with "on": true (devices and non-socket loads), in file order
};

// Reads a file's contents into project, which must be new. Unless Opened, project holds part of
// the file and is to be thrown away.
[[nodiscard]] OpenResult readProject(const QByteArray &data, Project &project);
// The same from a file; a file that can't be read is NotPanelFile.
[[nodiscard]] OpenResult openProject(const QString &path, Project &project);
[[nodiscard]] QByteArray writeProject(const Project &project, const QStringList &on);
// Atomic (QSaveFile). On failure: false, error = the system's reason, an existing file unchanged.
bool saveProject(const QString &path, const Project &project, const QStringList &on,
                 QString &error);
