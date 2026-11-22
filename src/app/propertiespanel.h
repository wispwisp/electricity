// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

#include <functional>

class EditorScene;
class QComboBox;
class QFormLayout;
class QLabel;
class QUndoStack;
class Project;
struct PlacedDevice;

// Shows and edits the selected device (rating, poles), wire (section) or the enclosure (size);
// edits go through the undo stack, and one that doesn't fit is refused with a hint.
class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    PropertiesPanel(Project &project, QUndoStack &undoStack, EditorScene &scene,
                    QWidget *parent = nullptr);

signals:
    void hint(const QString &text);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refresh();
    void retranslate();
    void editDevice(const std::function<void(PlacedDevice &)> &change);
    void changeSize(int index);
    void changeSection(int index);
    [[nodiscard]] QString terminalText(const QString &terminalId) const;

    Project &m_project;
    QUndoStack &m_undoStack;
    EditorScene &m_scene;
    QLabel *m_hint;
    QWidget *m_deviceBox;
    QLabel *m_designation;
    QLabel *m_typeName;
    QFormLayout *m_deviceForm;
    QLabel *m_ratingCaption;
    QComboBox *m_rating;
    QLabel *m_polesCaption;
    QComboBox *m_poles;
    QLabel *m_polesValue;
    QLabel *m_curveCaption;
    QLabel *m_curve;
    QLabel *m_residualCaption;
    QLabel *m_residual;
    QWidget *m_wireBox;
    QLabel *m_wireTitle;
    QLabel *m_wireEnds;
    QLabel *m_sectionCaption;
    QComboBox *m_section;
    QWidget *m_enclosureBox;
    QLabel *m_enclosureTitle;
    QLabel *m_sizeCaption;
    QComboBox *m_size;
};
