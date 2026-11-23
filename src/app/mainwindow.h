// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/simulation.h"

#include <QList>
#include <QMainWindow>
#include <QSize>

class EditorScene;
class Project;
class QAction;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QTimer;
class QToolBar;
class QUndoStack;

// True when the screen is too small for the default window size plus its frame.
bool startsMaximized(const QSize &availableScreenSize);

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    Project &project();
    QUndoStack &undoStack();
    Simulation &simulation(); // the scene's

protected:
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();
    void onEvent(const SimulationEvent &event);
    void showIdleText();

    Project *m_project = nullptr;
    QUndoStack *m_undoStack = nullptr;
    EditorScene *m_scene = nullptr;
    QDockWidget *m_dock = nullptr;
    QDockWidget *m_propertiesDock = nullptr;
    QMenu *m_editMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QToolBar *m_editToolBar = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_wireAction = nullptr;
    QComboBox *m_wireSectionCombo = nullptr;
    QLabel *m_timeLabel = nullptr;
    QAction *m_pauseAction = nullptr;
    QTimer *m_simulationTimer = nullptr;
    int m_speed = 0; // simulated seconds per second
    QAction *m_deleteAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_showCurrentsAction = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QMenu *m_themeMenu = nullptr;
    QMenu *m_languageMenu = nullptr;
    QAction *m_darkAction = nullptr;
    QAction *m_lightAction = nullptr;
    QString m_idleText;                  // the one shown last
    QList<SimulationEvent> m_conditions; // active, the newest last
};
