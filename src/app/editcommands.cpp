// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/editcommands.h"

#include <QtAssert>

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
    [[maybe_unused]] const QString id = m_project.addDevice(m_device);
    Q_ASSERT(id == m_device.id);
    // addDevice appends, so re-add the devices that followed to restore the insertion order.
    for (const PlacedDevice &later : std::as_const(m_later)) {
        [[maybe_unused]] const bool removed = m_project.removeDevice(later.id);
        Q_ASSERT(removed);
        [[maybe_unused]] const QString laterId = m_project.addDevice(later);
        Q_ASSERT(laterId == later.id);
    }
}

void RemoveDeviceCommand::redo()
{
    const QList<PlacedDevice> &devices = m_project.devices();
    m_later.clear();
    for (qsizetype i = 0; i < devices.size(); ++i) {
        if (devices[i].id == m_device.id) {
            m_later = devices.mid(i + 1);
            break;
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
