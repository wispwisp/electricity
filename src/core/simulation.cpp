// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/simulation.h"

#include "core/nodalsolver.h"
#include "core/project.h"

#include <algorithm>
#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

// All holes of a bar are one conductor, so they share one key; any other terminal is its own.
QString conductorKey(const QString &terminal)
{
    const QString object = terminalObject(terminal);
    return object == u"nbar" || object == u"pebar" ? object : terminal;
}

double resistance(const Wire &wire)
{
    return CopperResistivity * wire.length / wire.section;
}

// A load is a fixed resistance U²/P.
double admittance(const PlacedLoad &load)
{
    return load.power / (SupplyVoltage * SupplyVoltage);
}

// The terminals a load's power is drawn from: its own L and N, or its socket's while plugged in.
// A socket and an unplugged appliance draw from none.
std::optional<std::pair<QString, QString>> drawsFrom(const Project &project, const PlacedLoad &load)
{
    const LoadType *type = project.catalog().loadType(load.type);
    if (!type || !type->places.isEmpty())
        return std::nullopt;
    const QString source = type->plugsIn ? load.socket : load.id;
    if (!project.load(source))
        return std::nullopt;
    return std::pair(terminalId(source, u"L"_s), terminalId(source, u"N"_s));
}

} // namespace

Simulation::Simulation(const Project &project, QObject *parent)
    : QObject(parent)
    , m_project(project)
{
    connect(&m_project, &Project::changed, this, &Simulation::solve);
    solve();
}

bool Simulation::isOn(QStringView id) const
{
    return m_on.value(id);
}

bool Simulation::setOn(const QString &id, bool on)
{
    const PlacedLoad *load = m_project.load(id);
    const LoadType *type = load ? m_project.catalog().loadType(load->type) : nullptr;
    if (!m_project.device(id) && !(type && type->places.isEmpty()))
        return false;
    const bool changes = isOn(id) != on;
    m_on.insert(id, on);
    if (changes)
        solve();
    return true;
}

double Simulation::wireCurrent(QStringView wireId) const
{
    return m_wireCurrents.value(wireId);
}

double Simulation::deviceCurrent(QStringView deviceId) const
{
    return m_deviceCurrents.value(deviceId);
}

double Simulation::loadCurrent(QStringView loadId) const
{
    return m_loads.value(loadId).current;
}

double Simulation::loadVoltage(QStringView loadId) const
{
    return m_loads.value(loadId).voltage;
}

bool Simulation::isEnergized(QStringView loadId) const
{
    return m_loads.value(loadId).energized;
}

void Simulation::solve()
{
    QHash<QString, int> nodes; // conductor key -> node
    for (const QString &terminal : m_project.terminalIds()) {
        const QString key = conductorKey(terminal);
        if (!nodes.contains(key))
            nodes.insert(key, static_cast<int>(nodes.size()));
    }
    const auto node
            = [&nodes](const QString &terminal) { return nodes.value(conductorKey(terminal)); };
    const QList<ContactSpec> none;
    const auto closedContacts = [&](const PlacedDevice &device) -> const QList<ContactSpec> & {
        const DeviceType *type = m_project.catalog().deviceType(device.type);
        const DeviceVariant *variant = type ? type->variant(device.poles) : nullptr;
        return variant && isOn(device.id) ? variant->contacts : none;
    };

    QList<Admittance> branches;
    QList<Complex> injected(nodes.size());
    for (const TerminalSpec &t : m_project.supply().terminals) {
        const int n = node(terminalId(u"supply"_s, t.name));
        if (t.role == Conductor::L) {
            // a Norton source: E/Zs injected, with 1/Zs to the reference
            branches.append({ n, ReferenceNode, 1 / SupplyImpedance });
            injected[n] = SupplyVoltage / SupplyImpedance;
        } else {
            // N and PE are bonded to the reference at the source (TN-C-S)
            branches.append({ n, ReferenceNode, 1 / ContactResistance });
        }
    }
    for (const Wire &w : m_project.wires())
        branches.append({ node(w.from), node(w.to), 1 / resistance(w) });
    for (const PlacedDevice &d : m_project.devices()) {
        for (const ContactSpec &c : closedContacts(d))
            branches.append({ node(terminalId(d.id, c.top)), node(terminalId(d.id, c.bottom)),
                              1 / ContactResistance });
    }
    for (const PlacedLoad &l : m_project.loads()) {
        const auto terminals = drawsFrom(m_project, l);
        if (terminals && isOn(l.id))
            branches.append({ node(terminals->first), node(terminals->second), admittance(l) });
    }

    const QList<Complex> voltages = solveNodeVoltages(branches, injected);
    const auto across = [&](const QString &a, const QString &b) {
        return std::abs(voltages[node(a)] - voltages[node(b)]);
    };

    m_wireCurrents.clear();
    for (const Wire &w : m_project.wires())
        m_wireCurrents.insert(w.id, across(w.from, w.to) / resistance(w));
    m_deviceCurrents.clear();
    for (const PlacedDevice &d : m_project.devices()) {
        double largest = 0;
        for (const ContactSpec &c : closedContacts(d))
            largest = std::max(largest,
                               across(terminalId(d.id, c.top), terminalId(d.id, c.bottom))
                                       / ContactResistance);
        m_deviceCurrents.insert(d.id, largest);
    }
    m_loads.clear();
    for (const PlacedLoad &l : m_project.loads()) {
        const auto terminals = drawsFrom(m_project, l);
        if (!terminals)
            continue;
        LoadResult result;
        result.voltage = across(terminals->first, terminals->second);
        if (isOn(l.id)) {
            result.current = result.voltage * admittance(l);
            result.energized = result.voltage > 0.5 * SupplyVoltage;
        }
        m_loads.insert(l.id, result);
    }
    emit solved();
}
