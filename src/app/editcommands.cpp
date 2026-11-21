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
}

void RemoveDeviceCommand::redo()
{
    m_index = std::ranges::find(m_project.devices(), m_device.id, &PlacedDevice::id)
            - m_project.devices().begin();
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
