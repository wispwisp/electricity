// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/project.h"

#include <QJsonArray>

#include <algorithm>
#include <array>

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

// The largest plug-in appliance: a plug place's rect hangs from the place point.
constexpr QSizeF PlugPlaceSize{ 36, 36 };

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
    return fitsEnclosure(enclosureId, loadsBelowEnclosure());
}

bool Project::fitsEnclosure(QStringView enclosureId, const QStringList &movedLoadIds) const
{
    const EnclosureType *candidate = findEnclosure(enclosureId);
    if (!candidate)
        return false;
    const int perRow = candidate->modules / candidate->rows;
    return keepsBarWires(enclosureId) && keepsLoadsClear(enclosureId, movedLoadIds)
            && std::ranges::all_of(m_devices, [&](const PlacedDevice &d) {
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
    for (const PlacedLoad &l : m_loads) {
        const LoadType *type = m_catalog.loadType(l.type);
        if (!type)
            continue;
        for (const TerminalSpec &t : type->terminals)
            ids << terminalId(l.id, t.name);
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
    if (const PlacedLoad *l = load(object)) {
        const LoadType *loadType = m_catalog.loadType(l->type);
        const TerminalSpec *t = loadType ? findTerminal(loadType->terminals, name) : nullptr;
        return t ? t->role : std::nullopt;
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
    return static_cast<int>(std::ranges::count_if(
            m_wires, [&](const Wire &w) { return w.from == terminalId || w.to == terminalId; }));
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
    return setEnclosure(enclosureId, loadsBelowEnclosure());
}

bool Project::setEnclosure(const QString &enclosureId, const QStringList &movedLoadIds)
{
    if (!fitsEnclosure(enclosureId, movedLoadIds))
        return false;
    const EnclosureType *next = findEnclosure(enclosureId);
    if (next == m_enclosure)
        return true;
    const qreal shift = next->size.height() - m_enclosure->size.height();
    for (PlacedLoad &load : m_loads) {
        if (movedLoadIds.contains(load.id))
            load.position.ry() += shift;
    }
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

QStringList Project::cableWires(const QString &from, const QString &to) const
{
    const auto side = [this](const QString &terminalId) {
        const QString object = terminalObject(terminalId);
        return load(object) ? object : u"panel"_s;
    };
    const auto sides = [&side](const QString &a, const QString &b) {
        return std::minmax({ side(a), side(b) });
    };
    const auto target = sides(from, to);
    if (target.first == u"panel" && target.second == u"panel")
        return {};
    QStringList ids;
    for (const Wire &w : m_wires) {
        if (sides(w.from, w.to) == target)
            ids.append(w.id);
    }
    return ids;
}

double Project::newWireLength(const QString &from, const QString &to) const
{
    const QStringList cable = cableWires(from, to);
    if (!cable.isEmpty())
        return wire(cable.first())->length;
    return load(terminalObject(from)) || load(terminalObject(to)) ? LoadWireLength : 0.3;
}

bool Project::setCableLength(QStringView wireId, double length)
{
    const Wire *w = wire(wireId);
    if (!w || !(length > 0) || length > MaxWireLength)
        return false;
    const QStringList cable = cableWires(w->from, w->to);
    if (cable.isEmpty())
        return false;
    for (Wire &other : m_wires) {
        if (cable.contains(other.id))
            other.length = length;
    }
    emit changed();
    return true;
}

PlacedLoad loadFromType(const LoadType &type, QPointF position)
{
    return { {}, type.id, position, type.power, {}, 0 };
}

const QList<PlacedLoad> &Project::loads() const
{
    return m_loads;
}

const PlacedLoad *Project::load(QStringView id) const
{
    const auto it = std::ranges::find(m_loads, id, &PlacedLoad::id);
    return it == m_loads.end() ? nullptr : &*it;
}

int Project::loadNumber(QStringView id) const
{
    const PlacedLoad *target = load(id);
    if (!target)
        return 0;
    int number = 0;
    for (const PlacedLoad &l : m_loads) {
        if (l.type == target->type)
            ++number;
        if (l.id == id)
            break;
    }
    return number;
}

QPointF Project::loadPosition(const PlacedLoad &load) const
{
    const PlacedLoad *socket = this->load(load.socket);
    const LoadType *socketType = socket ? m_catalog.loadType(socket->type) : nullptr;
    const LoadType *type = m_catalog.loadType(load.type);
    if (load.socket.isEmpty() || !type || !socketType || load.place < 1
        || load.place > socketType->places.size())
        return load.position;
    return socket->position + socketType->places[load.place - 1]
            - QPointF(type->size.width() / 2, 0);
}

QRectF Project::plugPlaceRect(const PlacedLoad &socket, int place) const
{
    const LoadType *type = m_catalog.loadType(socket.type);
    if (!type || place < 1 || place > type->places.size())
        return {};
    const QPointF point = socket.position + type->places[place - 1];
    return { QPointF(point.x() - PlugPlaceSize.width() / 2, point.y()), PlugPlaceSize };
}

QRectF Project::loadOutline(const PlacedLoad &load) const
{
    const LoadType *type = m_catalog.loadType(load.type);
    if (!type)
        return {};
    QRectF outline(loadPosition(load), type->size);
    if (!type->caption.isEmpty())
        outline = outline.united(
                QRectF(outline.bottomLeft(), QSizeF(type->size.width(), LoadCaptionHeight)));
    for (int place = 1; place <= type->places.size(); ++place)
        outline = outline.united(plugPlaceRect(load, place));
    return outline;
}

// The set is not the same for a change and its reverse: after a shrink, a load beside the enclosure
// that stayed can reach below the new bottom edge. So an undo moves the loads its change moved.
QStringList Project::loadsBelowEnclosure() const
{
    QStringList below;
    for (const PlacedLoad &load : m_loads) {
        if (load.socket.isEmpty() && loadOutline(load).top() >= m_enclosure->size.height())
            below << load.id;
    }
    return below;
}

bool Project::keepsLoadsClear(QStringView enclosureId) const
{
    return keepsLoadsClear(enclosureId, loadsBelowEnclosure());
}

bool Project::keepsLoadsClear(QStringView enclosureId, const QStringList &movedLoadIds) const
{
    const EnclosureType *candidate = findEnclosure(enclosureId);
    if (!candidate)
        return false;
    const qreal shift = candidate->size.height() - m_enclosure->size.height();
    for (const PlacedLoad &stays : m_loads) {
        if (movedLoadIds.contains(stays.id) || !stays.socket.isEmpty())
            continue;
        const QRectF outline = loadOutline(stays);
        if (std::ranges::any_of(m_loads, [&](const PlacedLoad &moved) {
                return movedLoadIds.contains(moved.id)
                        && loadOutline(moved).translated(0, shift).intersects(outline);
            }))
            return false;
    }
    return true;
}

QStringList Project::pluggedInto(QStringView socketId) const
{
    std::array<QStringList, 2> ids;
    for (const PlacedLoad &l : m_loads) {
        if (l.socket == socketId && (l.place == 1 || l.place == 2))
            ids[l.place - 1] << l.id;
    }
    return ids[0] + ids[1];
}

LoadPlacement Project::loadPlacement(const PlacedLoad &load) const
{
    const LoadType *type = m_catalog.loadType(load.type);
    if (!type || load.power < (type->places.isEmpty() ? 1 : 0)
        || load.power > (type->places.isEmpty() ? MaxLoadPower : 0))
        return LoadPlacement::Invalid;
    if (!load.socket.isEmpty()) {
        const PlacedLoad *socket = this->load(load.socket);
        const LoadType *socketType = socket ? m_catalog.loadType(socket->type) : nullptr;
        if (!type->plugsIn || !socketType || socketType->places.isEmpty() || load.place < 1
            || load.place > 2)
            return LoadPlacement::Invalid;
        const bool taken = std::ranges::any_of(m_loads, [&](const PlacedLoad &l) {
            return l.id != load.id && l.socket == load.socket && l.place == load.place;
        });
        return taken ? LoadPlacement::PlaceTaken : LoadPlacement::Fits;
    }
    const QRectF outline = loadOutline(load);
    if (outline.intersects(QRectF(QPointF(0, 0), m_enclosure->size)))
        return LoadPlacement::OnEnclosure;
    const bool overlaps = std::ranges::any_of(m_loads, [&](const PlacedLoad &l) {
        return l.id != load.id && l.socket.isEmpty() && outline.intersects(loadOutline(l));
    });
    return overlaps ? LoadPlacement::Overlaps : LoadPlacement::Fits;
}

QString Project::addLoad(PlacedLoad load, qsizetype index)
{
    if (index < -1 || index > m_loads.size())
        return {};
    if (loadPlacement(load) != LoadPlacement::Fits)
        return {};
    if (load.id.isEmpty()) {
        do
            load.id = u"l%1"_s.arg(m_nextLoadId++);
        while (this->load(load.id));
    } else if (this->load(load.id)) {
        return {};
    } else if (load.id.startsWith(u'l')) {
        // A kept "l<n>" (a restored or loaded load) must never be handed out again.
        bool ok = false;
        const int n = load.id.sliced(1).toInt(&ok);
        if (ok)
            m_nextLoadId = std::max(m_nextLoadId, n + 1);
    }
    if (index == -1)
        index = m_loads.size();
    m_loads.insert(index, load);
    emit changed();
    return load.id;
}

bool Project::removeLoad(QStringView id)
{
    const auto it = std::ranges::find(m_loads, id, &PlacedLoad::id);
    if (it == m_loads.end())
        return false;
    for (PlacedLoad &l : m_loads) {
        if (l.socket == id) {
            l.position = loadPosition(l);
            l.socket.clear();
            l.place = 0;
        }
    }
    m_loads.erase(it);
    erase_if(m_wires, [&](const Wire &w) {
        return terminalObject(w.from) == id || terminalObject(w.to) == id;
    });
    emit changed();
    return true;
}

bool Project::updateLoad(const PlacedLoad &load)
{
    const auto it = std::ranges::find(m_loads, load.id, &PlacedLoad::id);
    if (it == m_loads.end() || it->type != load.type || loadPlacement(load) != LoadPlacement::Fits)
        return false;
    *it = load;
    emit changed();
    return true;
}
