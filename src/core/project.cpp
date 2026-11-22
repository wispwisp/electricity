// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/project.h"

#include <QJsonArray>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr QLatin1StringView SupplyObject = "supply"_L1;
constexpr QLatin1StringView NBarObject = "nbar"_L1;
constexpr QLatin1StringView PeBarObject = "pebar"_L1;

bool isBarObject(const QString &object)
{
    return object == NBarObject || object == PeBarObject;
}

int barHoles(const EnclosureType &enclosure, const QString &object)
{
    return object == NBarObject ? enclosure.nBarTerminals : enclosure.peBarTerminals;
}

const TerminalSpec *findTerminal(const QList<TerminalSpec> &terminals, const QString &name)
{
    const auto it = std::ranges::find(terminals, name, &TerminalSpec::name);
    return it == terminals.end() ? nullptr : &*it;
}

} // namespace

QString terminalId(const QString &objectId, const QString &terminal)
{
    return objectId + u':' + terminal;
}

QString terminalObject(const QString &terminalId)
{
    return terminalId.left(terminalId.indexOf(u':'));
}

QString terminalName(const QString &terminalId)
{
    return terminalId.sliced(terminalId.indexOf(u':') + 1);
}

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
    const QString prefix = m_catalog.deviceType(target->type)->designation;
    int number = 1;
    for (const PlacedDevice &other : m_devices) {
        if (m_catalog.deviceType(other.type)->designation == prefix
            && position(other) < position(*target))
            ++number;
    }
    return prefix + QString::number(number);
}

bool Project::fitsEnclosure(QStringView enclosureId) const
{
    const EnclosureType *candidate = findEnclosure(enclosureId);
    if (!candidate)
        return false;
    const int perRow = candidate->modules / candidate->rows;
    return keepsBarWires(enclosureId) && std::ranges::all_of(m_devices, [&](const PlacedDevice &d) {
               return d.row < candidate->rows && d.module + moduleWidth(d) <= perRow;
           });
}

const SupplyType &Project::supply() const
{
    const SupplyType *type = m_catalog.supply(QString::fromLatin1(DefaultSupplyId));
    Q_ASSERT(type);
    return *type;
}

QStringList Project::terminalIds() const
{
    QStringList ids;
    for (const TerminalSpec &t : supply().terminals)
        ids << terminalId(SupplyObject, t.name);
    for (int i = 1; i <= m_enclosure->nBarTerminals; ++i)
        ids << terminalId(NBarObject, QString::number(i));
    for (int i = 1; i <= m_enclosure->peBarTerminals; ++i)
        ids << terminalId(PeBarObject, QString::number(i));
    for (const PlacedDevice &d : m_devices) {
        const DeviceType *type = m_catalog.deviceType(d.type);
        const DeviceVariant *variant = type ? type->variant(d.poles) : nullptr;
        if (!variant)
            continue;
        for (const TerminalSpec &t : variant->terminals)
            ids << terminalId(d.id, t.name);
    }
    return ids;
}

bool Project::hasTerminal(const QString &terminalId) const
{
    return terminalIds().contains(terminalId);
}

std::optional<Conductor> Project::terminalRole(const QString &terminalId) const
{
    const QString object = terminalObject(terminalId);
    const QString name = terminalName(terminalId);
    if (object == SupplyObject) {
        const TerminalSpec *t = findTerminal(supply().terminals, name);
        return t ? t->role : std::nullopt;
    }
    if (isBarObject(object)) {
        const auto it = std::ranges::find(m_catalog.bars, object, &BarType::id);
        return it == m_catalog.bars.end() ? std::nullopt : std::optional(it->role);
    }
    const PlacedDevice *d = device(object);
    const DeviceType *type = d ? m_catalog.deviceType(d->type) : nullptr;
    const DeviceVariant *variant = type ? type->variant(d->poles) : nullptr;
    const TerminalSpec *t = variant ? findTerminal(variant->terminals, name) : nullptr;
    return t ? t->role : std::nullopt;
}

int Project::wireCapacity(const QString &terminalId) const
{
    if (!hasTerminal(terminalId))
        return 0;
    return isBarObject(terminalObject(terminalId)) ? 1 : 2;
}

int Project::wireCount(const QString &terminalId) const
{
    return std::ranges::count_if(
            m_wires, [&](const Wire &w) { return w.from == terminalId || w.to == terminalId; });
}

Conductor Project::conductorFor(const QString &from, const QString &to) const
{
    if (const auto role = terminalRole(from))
        return *role;
    return terminalRole(to).value_or(Conductor::L);
}

const QList<Wire> &Project::wires() const
{
    return m_wires;
}

const Wire *Project::wire(QStringView id) const
{
    const auto it = std::ranges::find(m_wires, id, &Wire::id);
    return it == m_wires.end() ? nullptr : &*it;
}

WireCheck Project::wireCheck(const Wire &wire) const
{
    if (!hasTerminal(wire.from) || !hasTerminal(wire.to))
        return WireCheck::UnknownTerminal;
    if (wire.from == wire.to)
        return WireCheck::SameTerminal;
    if (std::ranges::find(WireSections, wire.section) == WireSections.end())
        return WireCheck::InvalidSection;
    const auto others = [&](const QString &terminal) {
        return std::ranges::count_if(m_wires, [&](const Wire &w) {
            return w.id != wire.id && (w.from == terminal || w.to == terminal);
        });
    };
    if (others(wire.from) >= wireCapacity(wire.from) || others(wire.to) >= wireCapacity(wire.to))
        return WireCheck::TerminalFull;
    const bool duplicate = std::ranges::any_of(m_wires, [&](const Wire &w) {
        return w.id != wire.id
                && ((w.from == wire.from && w.to == wire.to)
                    || (w.from == wire.to && w.to == wire.from));
    });
    return duplicate ? WireCheck::Duplicate : WireCheck::Ok;
}

QStringList Project::orphanedTerminals(const PlacedDevice &device) const
{
    const PlacedDevice *stored = this->device(device.id);
    if (!stored)
        return {};
    const DeviceType *oldType = m_catalog.deviceType(stored->type);
    const DeviceVariant *oldVariant = oldType ? oldType->variant(stored->poles) : nullptr;
    const DeviceType *newType = m_catalog.deviceType(device.type);
    const DeviceVariant *newVariant = newType ? newType->variant(device.poles) : nullptr;
    QStringList orphans;
    if (!oldVariant)
        return orphans;
    for (const TerminalSpec &t : oldVariant->terminals) {
        const bool kept = newVariant && findTerminal(newVariant->terminals, t.name);
        if (!kept && wireCount(terminalId(stored->id, t.name)) > 0)
            orphans << t.name;
    }
    return orphans;
}

bool Project::keepsBarWires(QStringView enclosureId) const
{
    const EnclosureType *candidate = findEnclosure(enclosureId);
    if (!candidate)
        return false;
    const auto fits = [&](const QString &terminal) {
        const QString object = terminalObject(terminal);
        return !isBarObject(object)
                || terminalName(terminal).toInt() <= barHoles(*candidate, object);
    };
    return std::ranges::all_of(m_wires, [&](const Wire &w) { return fits(w.from) && fits(w.to); });
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
    } else if (device.id.startsWith(u'd')) {
        // A kept "d<n>" (a restored or loaded device) must never be handed out again.
        bool ok = false;
        const int n = device.id.sliced(1).toInt(&ok);
        if (ok)
            m_nextId = std::max(m_nextId, n + 1);
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
    erase_if(m_wires, [&](const Wire &w) {
        return terminalObject(w.from) == id || terminalObject(w.to) == id;
    });
    emit changed();
    return true;
}

bool Project::updateDevice(const PlacedDevice &device)
{
    const auto it = std::ranges::find(m_devices, device.id, &PlacedDevice::id);
    if (it == m_devices.end() || placement(device) != Placement::Fits
        || !orphanedTerminals(device).isEmpty())
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

QString Project::addWire(Wire wire, qsizetype index)
{
    if (index < -1 || index > m_wires.size())
        return {};
    if (wireCheck(wire) != WireCheck::Ok)
        return {};
    if (wire.id.isEmpty()) {
        do
            wire.id = u"w%1"_s.arg(m_nextWireId++);
        while (this->wire(wire.id));
    } else if (this->wire(wire.id)) {
        return {};
    } else if (wire.id.startsWith(u'w')) {
        // A kept "w<n>" (a restored or loaded wire) must never be handed out again.
        bool ok = false;
        const int n = wire.id.sliced(1).toInt(&ok);
        if (ok)
            m_nextWireId = std::max(m_nextWireId, n + 1);
    }
    if (index == -1)
        index = m_wires.size();
    m_wires.insert(index, wire);
    emit changed();
    return wire.id;
}

bool Project::removeWire(QStringView id)
{
    const auto it = std::ranges::find(m_wires, id, &Wire::id);
    if (it == m_wires.end())
        return false;
    m_wires.erase(it);
    emit changed();
    return true;
}

bool Project::updateWire(const Wire &wire)
{
    const auto it = std::ranges::find(m_wires, wire.id, &Wire::id);
    if (it == m_wires.end() || wireCheck(wire) != WireCheck::Ok)
        return false;
    *it = wire;
    emit changed();
    return true;
}
