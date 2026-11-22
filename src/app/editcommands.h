// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/project.h"

#include <QString>
#include <QUndoCommand>

// Callers check placement() / fitsEnclosure() before pushing; each command asserts its model call
// succeeded.

class AddDeviceCommand : public QUndoCommand
{
public:
    AddDeviceCommand(Project &project, PlacedDevice device);
    [[nodiscard]] QString deviceId() const;
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    PlacedDevice m_device; // gets its id on the first redo
};

class RemoveDeviceCommand : public QUndoCommand
{
public:
    RemoveDeviceCommand(Project &project, const QString &id);
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    PlacedDevice m_device;
    qsizetype m_index = 0; // position in devices() when removed
    QList<Wire> m_wires;   // the device's wires, ascending by index in wires()
    QList<qsizetype> m_wireIndices;
};

class UpdateDeviceCommand : public QUndoCommand
{
public:
    UpdateDeviceCommand(Project &project, PlacedDevice after);
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    PlacedDevice m_before;
    PlacedDevice m_after;
};

class AddWireCommand : public QUndoCommand
{
public:
    AddWireCommand(Project &project, Wire wire);
    [[nodiscard]] QString wireId() const; // set by the first redo
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    Wire m_wire;
};

class RemoveWireCommand : public QUndoCommand
{
public:
    RemoveWireCommand(Project &project, const QString &id); // undo restores the id and index
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    Wire m_wire;
    qsizetype m_index = 0; // position in wires() when removed
};

class UpdateWireCommand : public QUndoCommand
{
public:
    UpdateWireCommand(Project &project, Wire after);
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    Wire m_before;
    Wire m_after;
};

class SetEnclosureCommand : public QUndoCommand
{
public:
    SetEnclosureCommand(Project &project, QString enclosureId);
    void undo() override;
    void redo() override;

private:
    Project &m_project;
    QString m_before;
    QString m_after;
};
