// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/editcommands.h"

#include <QtAssert>

#include <algorithm>
#include <utility>

AddDeviceCommand::AddDeviceCommand(Project &project, PlacedDevice device)
    : m_project(project)
    , m_device(std::move(device))
{
}

QString AddDeviceCommand::deviceId() const
{
    return m_device.id;
}

void AddDeviceCommand::undo()
{
    [[maybe_unused]] const bool removed = m_project.removeDevice(m_device.id);
    Q_ASSERT(removed);
}

void AddDeviceCommand::redo()
{
    m_device.id = m_project.addDevice(m_device);
    Q_ASSERT(!m_device.id.isEmpty());
}

RemoveDeviceCommand::RemoveDeviceCommand(Project &project, const QString &id)
    : m_project(project)
{
    const PlacedDevice *device = project.device(id);
    Q_ASSERT(device);
    m_device = *device;
}

void RemoveDeviceCommand::undo()
{
    [[maybe_unused]] const QString id = m_project.addDevice(m_device, m_index);
    Q_ASSERT(id == m_device.id);
    for (qsizetype i = 0; i < m_wires.size(); ++i) {
        [[maybe_unused]] const QString wireId = m_project.addWire(m_wires[i], m_wireIndices[i]);
        Q_ASSERT(wireId == m_wires[i].id);
    }
}

void RemoveDeviceCommand::redo()
{
    m_index = std::ranges::find(m_project.devices(), m_device.id, &PlacedDevice::id)
            - m_project.devices().begin();
    m_wires.clear();
    m_wireIndices.clear();
    for (qsizetype i = 0; i < m_project.wires().size(); ++i) {
        const Wire &wire = m_project.wires()[i];
        if (wire.from.startsWith(m_device.id + u':') || wire.to.startsWith(m_device.id + u':')) {
            m_wires.append(wire);
            m_wireIndices.append(i);
        }
    }
    [[maybe_unused]] const bool removed = m_project.removeDevice(m_device.id);
    Q_ASSERT(removed);
}

UpdateDeviceCommand::UpdateDeviceCommand(Project &project, PlacedDevice after)
    : m_project(project)
    , m_after(std::move(after))
{
    const PlacedDevice *device = project.device(m_after.id);
    Q_ASSERT(device);
    m_before = *device;
}

void UpdateDeviceCommand::undo()
{
    [[maybe_unused]] const bool updated = m_project.updateDevice(m_before);
    Q_ASSERT(updated);
}

void UpdateDeviceCommand::redo()
{
    [[maybe_unused]] const bool updated = m_project.updateDevice(m_after);
    Q_ASSERT(updated);
}

AddWireCommand::AddWireCommand(Project &project, Wire wire)
    : m_project(project)
    , m_wire(std::move(wire))
{
}

QString AddWireCommand::wireId() const
{
    return m_wire.id;
}

void AddWireCommand::undo()
{
    [[maybe_unused]] const bool removed = m_project.removeWire(m_wire.id);
    Q_ASSERT(removed);
}

void AddWireCommand::redo()
{
    m_wire.id = m_project.addWire(m_wire);
    Q_ASSERT(!m_wire.id.isEmpty());
}

RemoveWireCommand::RemoveWireCommand(Project &project, const QString &id)
    : m_project(project)
{
    const Wire *wire = project.wire(id);
    Q_ASSERT(wire);
    m_wire = *wire;
}

void RemoveWireCommand::undo()
{
    [[maybe_unused]] const QString id = m_project.addWire(m_wire, m_index);
    Q_ASSERT(id == m_wire.id);
}

void RemoveWireCommand::redo()
{
    m_index = std::ranges::find(m_project.wires(), m_wire.id, &Wire::id)
            - m_project.wires().begin();
    [[maybe_unused]] const bool removed = m_project.removeWire(m_wire.id);
    Q_ASSERT(removed);
}

UpdateWireCommand::UpdateWireCommand(Project &project, Wire after)
    : m_project(project)
    , m_after(std::move(after))
{
    const Wire *wire = project.wire(m_after.id);
    Q_ASSERT(wire);
    m_before = *wire;
}

void UpdateWireCommand::undo()
{
    [[maybe_unused]] const bool updated = m_project.updateWire(m_before);
    Q_ASSERT(updated);
}

void UpdateWireCommand::redo()
{
    [[maybe_unused]] const bool updated = m_project.updateWire(m_after);
    Q_ASSERT(updated);
}

SetEnclosureCommand::SetEnclosureCommand(Project &project, QString enclosureId)
    : m_project(project)
    , m_before(project.enclosure().id)
    , m_after(std::move(enclosureId))
{
}

void SetEnclosureCommand::undo()
{
    [[maybe_unused]] const bool set = m_project.setEnclosure(m_before);
    Q_ASSERT(set);
}

void SetEnclosureCommand::redo()
{
    [[maybe_unused]] const bool set = m_project.setEnclosure(m_after);
    Q_ASSERT(set);
}
