// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMainWindow>
#include <QSize>

class QAction;
class QDockWidget;
class QMenu;

// True when the screen is too small for the default window size plus its frame.
bool startsMaximized(const QSize &availableScreenSize);

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();

    QDockWidget *m_dock = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QMenu *m_themeMenu = nullptr;
    QMenu *m_languageMenu = nullptr;
    QAction *m_darkAction = nullptr;
    QAction *m_lightAction = nullptr;
};
