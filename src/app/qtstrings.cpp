// SPDX-License-Identifier: GPL-3.0-or-later
#include <QStringList>
#include <QtGlobal>

// Qt's own texts in its file dialog, which the static Qt has no translations for (ADR 0031). Listed
// only so that lupdate puts them into our .ts files; never called. Spelled exactly as in Qt
// 6.12.0's sources, in the contexts where Qt looks them up.
QStringList qtTranslationSources();

namespace {
// What QT_TRANSLATE_NOOP3 expands to.
struct Message
{
    const char *source;
    const char *comment;
};
} // namespace

QStringList qtTranslationSources()
{
    QStringList sources{
        QT_TRANSLATE_NOOP("QFileDialog", "&Look in:"),
        QT_TRANSLATE_NOOP("QFileDialog", "Back"),
        QT_TRANSLATE_NOOP("QFileDialog", "Go back"),
        QT_TRANSLATE_NOOP("QFileDialog", "Alt+Left"),
        QT_TRANSLATE_NOOP("QFileDialog", "Forward"),
        QT_TRANSLATE_NOOP("QFileDialog", "Go forward"),
        QT_TRANSLATE_NOOP("QFileDialog", "Alt+Right"),
        QT_TRANSLATE_NOOP("QFileDialog", "Parent Directory"),
        QT_TRANSLATE_NOOP("QFileDialog", "Go to the parent directory"),
        QT_TRANSLATE_NOOP("QFileDialog", "Alt+Up"),
        QT_TRANSLATE_NOOP("QFileDialog", "Create New Folder"),
        QT_TRANSLATE_NOOP("QFileDialog", "Create a New Folder"),
        QT_TRANSLATE_NOOP("QFileDialog", "List View"),
        QT_TRANSLATE_NOOP("QFileDialog", "Change to list view mode"),
        QT_TRANSLATE_NOOP("QFileDialog", "Detail View"),
        QT_TRANSLATE_NOOP("QFileDialog", "Change to detail view mode"),
        QT_TRANSLATE_NOOP("QFileDialog", "Sidebar"),
        QT_TRANSLATE_NOOP("QFileDialog", "List of places and bookmarks"),
        QT_TRANSLATE_NOOP("QFileDialog", "Files"),
        QT_TRANSLATE_NOOP("QFileDialog", "Files of &type:"),
        QT_TRANSLATE_NOOP("QFileDialog", "Find Directory"),
        QT_TRANSLATE_NOOP("QFileDialog", "Open"),
        QT_TRANSLATE_NOOP("QFileDialog", "Save As"),
        QT_TRANSLATE_NOOP("QFileDialog", "Directory:"),
        QT_TRANSLATE_NOOP("QFileDialog", "File &name:"),
        QT_TRANSLATE_NOOP("QFileDialog", "&Open"),
        QT_TRANSLATE_NOOP("QFileDialog", "&Choose"),
        QT_TRANSLATE_NOOP("QFileDialog", "&Save"),
        QT_TRANSLATE_NOOP("QFileDialog", "Show "),
        QT_TRANSLATE_NOOP("QFileDialog", "&Rename"),
        QT_TRANSLATE_NOOP("QFileDialog", "&Delete"),
        QT_TRANSLATE_NOOP("QFileDialog", "Show &hidden files"),
        QT_TRANSLATE_NOOP("QFileDialog", "&New Folder"),
        QT_TRANSLATE_NOOP("QFileDialog", "All files (*)"),
        QT_TRANSLATE_NOOP("QFileDialog", "Directories"),
        // NOLINTBEGIN(bugprone-suspicious-missing-comma)
        QT_TRANSLATE_NOOP("QFileDialog",
                          "%1 already exists.\n"
                          "Do you want to replace it?"),
        QT_TRANSLATE_NOOP("QFileDialog",
                          "%1\n"
                          "Directory not found.\n"
                          "Please verify the correct directory name was given."),
        QT_TRANSLATE_NOOP("QFileDialog",
                          "%1\n"
                          "File not found.\n"
                          "Please verify the correct file name was given."),
        QT_TRANSLATE_NOOP("QFileDialog", "New Folder"),
        QT_TRANSLATE_NOOP("QFileDialog", "Delete"),
        QT_TRANSLATE_NOOP("QFileDialog",
                          "'%1' is write protected.\n"
                          "Do you want to delete it anyway?"),
        // NOLINTEND(bugprone-suspicious-missing-comma)
        QT_TRANSLATE_NOOP("QFileDialog", "Are you sure you want to delete '%1'?"),
        QT_TRANSLATE_NOOP("QFileDialog", "Could not delete directory."),
        QT_TRANSLATE_NOOP("QFileDialog", "Recent Places"),
        QT_TRANSLATE_NOOP("QFileDialog", "Remove"),
        QT_TRANSLATE_NOOP("QFileSystemModel", "Name"),
        QT_TRANSLATE_NOOP("QFileSystemModel", "Size"),
        QT_TRANSLATE_NOOP("QFileSystemModel", "Date Modified"),
        QT_TRANSLATE_NOOP("QFileSystemModel", "Computer"),
        QT_TRANSLATE_NOOP("QAbstractFileIconProvider", "Drive"),
        QT_TRANSLATE_NOOP("QAbstractFileIconProvider", "Unknown"),
        QT_TRANSLATE_NOOP("QLineEdit", "&Undo"),
        QT_TRANSLATE_NOOP("QLineEdit", "&Redo"),
        QT_TRANSLATE_NOOP("QLineEdit", "Cu&t"),
        QT_TRANSLATE_NOOP("QLineEdit", "&Copy"),
        QT_TRANSLATE_NOOP("QLineEdit", "&Paste"),
        QT_TRANSLATE_NOOP("QLineEdit", "Delete"),
        QT_TRANSLATE_NOOP("QLineEdit", "Select All"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "OK"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Save"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Save All"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Open"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "&Yes"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Yes to &All"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "&No"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "N&o to All"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Abort"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Retry"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Ignore"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Close"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Discard"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Help"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Apply"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Reset"),
        QT_TRANSLATE_NOOP("QPlatformTheme", "Restore Defaults"),
    };
    for (const Message &message :
         { Message QT_TRANSLATE_NOOP3("QFileSystemModel", "Kind", "Match OS X Finder"),
           Message QT_TRANSLATE_NOOP3("QFileSystemModel", "Type", "All other platforms"),
           Message QT_TRANSLATE_NOOP3("QAbstractFileIconProvider", "Folder", "All other platforms"),
           Message QT_TRANSLATE_NOOP3("QAbstractFileIconProvider", "Shortcut",
                                      "All other platforms") })
        sources << message.source;
    return sources;
}
