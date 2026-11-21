// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/project.h"

#include <QJsonArray>

#include <algorithm>

using namespace Qt::StringLiterals;

PlacedDevice deviceFromPreset(const Preset &preset, int row, int module)
{
    return { QString(), preset.type, preset.poles, preset.parameters, row, module };
}

QString ratingText(const PlacedDevice &device)
{
    const int current = device.parameters.value("ratedCurrent").toInt();
    if (device.parameters.contains("curve"))
        return device.parameters.value("curve").toString() + QString::number(current);
    return u"%1 A"_s.arg(current);
}

Project::Project(const Catalog &catalog, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_enclosure(findEnclosure(DefaultEnclosureId))
{
    Q_ASSERT(m_enclosure);
}

const EnclosureType *Project::findEnclosure(QAnyStringView id) const
{
    const auto it = std::ranges::find_if(m_catalog.enclosures, [&](const EnclosureType &e) {
        return QAnyStringView(e.id) == id;
    });
    return it == m_catalog.enclosures.end() ? nullptr : &*it;
}

const Catalog &Project::catalog() const
{
    return m_catalog;
}

const EnclosureType &Project::enclosure() const
{
    return *m_enclosure;
}

int Project::modulesPerRow() const
{
    return m_enclosure->modules / m_enclosure->rows;
}

const QList<PlacedDevice> &Project::devices() const
{
    return m_devices;
}

const PlacedDevice *Project::device(QStringView id) const
{
    const auto it = std::ranges::find(m_devices, id, &PlacedDevice::id);
    return it == m_devices.end() ? nullptr : &*it;
}

int Project::moduleWidth(const PlacedDevice &device) const
{
    const DeviceType *type = m_catalog.deviceType(device.type);
    const DeviceVariant *variant = type ? type->variant(device.poles) : nullptr;
    return variant ? variant->modules : 0;
}

Placement Project::placement(const PlacedDevice &device) const
{
    const DeviceType *type = m_catalog.deviceType(device.type);
    if (!type || !type->variant(device.poles))
        return Placement::Invalid;
    if (device.parameters.keys().size() != type->allowedValues.keys().size())
        return Placement::Invalid;
    for (auto it = device.parameters.begin(); it != device.parameters.end(); ++it) {
        if (!type->allowedValues.value(it.key()).toArray().contains(it.value()))
            return Placement::Invalid;
    }

    if (device.row < 0 || device.row >= m_enclosure->rows)
        return Placement::NoRail;
    const int width = moduleWidth(device);
    if (device.module < 0 || device.module + width > modulesPerRow())
        return Placement::PastRailEnd;
    for (const PlacedDevice &other : m_devices) {
        if (other.id == device.id || other.row != device.row)
            continue;
        if (device.module < other.module + moduleWidth(other)
            && other.module < device.module + width)
            return Placement::Overlaps;
    }
    return Placement::Fits;
}

QString Project::designation(QStringView id) const
{
    const PlacedDevice *target = device(id);
    if (!target)
        return {};
    const auto position = [](const PlacedDevice &d) { return std::pair(d.row, d.module); };
    int number = 1;
    for (const PlacedDevice &other : m_devices) {
        if (other.type == target->type && position(other) < position(*target))
            ++number;
    }
    return m_catalog.deviceType(target->type)->designation + QString::number(number);
}

bool Project::fitsEnclosure(QStringView enclosureId) const
{
    const EnclosureType *candidate = findEnclosure(enclosureId);
    if (!candidate)
        return false;
    const int perRow = candidate->modules / candidate->rows;
    return std::ranges::all_of(m_devices, [&](const PlacedDevice &d) {
        return d.row < candidate->rows && d.module + moduleWidth(d) <= perRow;
    });
}

QString Project::addDevice(PlacedDevice device, qsizetype index)
{
    if (index < -1 || index > m_devices.size())
        return {};
    if (placement(device) != Placement::Fits)
        return {};
    if (device.id.isEmpty()) {
        do
            device.id = u"d%1"_s.arg(m_nextId++);
        while (this->device(device.id));
    } else if (this->device(device.id)) {
        return {};
    }
    if (index == -1)
        index = m_devices.size();
    m_devices.insert(index, device);
    emit changed();
    return device.id;
}

bool Project::removeDevice(QStringView id)
{
    const auto it = std::ranges::find(m_devices, id, &PlacedDevice::id);
    if (it == m_devices.end())
        return false;
    m_devices.erase(it);
    emit changed();
    return true;
}

bool Project::updateDevice(const PlacedDevice &device)
{
    const auto it = std::ranges::find(m_devices, device.id, &PlacedDevice::id);
    if (it == m_devices.end() || placement(device) != Placement::Fits)
        return false;
    *it = device;
    emit changed();
    return true;
}

bool Project::setEnclosure(const QString &enclosureId)
{
    if (!fitsEnclosure(enclosureId))
        return false;
    const EnclosureType *next = findEnclosure(enclosureId);
    if (next == m_enclosure)
        return true;
    m_enclosure = next;
    emit changed();
    emit enclosureChanged();
    return true;
}
