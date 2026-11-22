// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

#include <functional>

class EditorScene;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QSpinBox;
class QUndoStack;
class Project;
struct PlacedDevice;
struct PlacedLoad;

// Shows and edits the selected device (rating, poles), load (power), wire (section, and the length
// of a cable to a load) or the enclosure (size); edits go through the undo stack, and one that
// doesn't fit is refused with a hint.
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
    void changePower(int power);
    void changeLength(double length);
    [[nodiscard]] QString terminalText(const QString &terminalId) const;
    // the caption with its number, "Lamp 1", or a plug-in appliance's name
    [[nodiscard]] QString loadName(const PlacedLoad &load) const;

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
    // The load and the wire whose values refresh() last filled in. A typed value arrives when its
    // field loses the focus, which can happen around a click that selects something else, and a
    // selection change sends one still being typed first. So edits go to these rather than to the
    // selection.
    QString m_loadId;
    QString m_wireId;
    QWidget *m_loadBox;
    QLabel *m_loadName;
    QLabel *m_loadType;
    QFormLayout *m_loadForm;
    QLabel *m_powerCaption;
    QSpinBox *m_power;
    QLabel *m_plugStatus;
    QLabel *m_socketContentsCaption;
    QLabel *m_socketContents;
    QWidget *m_wireBox;
    QLabel *m_wireTitle;
    QLabel *m_wireEnds;
    QFormLayout *m_wireForm;
    QLabel *m_sectionCaption;
    QComboBox *m_section;
    QLabel *m_lengthCaption;
    QDoubleSpinBox *m_length;
    QLabel *m_cableNote;
    QWidget *m_enclosureBox;
    QLabel *m_enclosureTitle;
    QLabel *m_sizeCaption;
    QComboBox *m_size;
};
