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

// The object whose L, N and PE terminals a load uses: its own, or its socket's while plugged in.
// None for an unplugged appliance.
std::optional<QString> terminalSource(const Project &project, const PlacedLoad &load)
{
    const LoadType *type = project.catalog().loadType(load.type);
    if (!type)
        return std::nullopt;
    QString source = type->plugsIn ? load.socket : load.id;
    if (!project.load(source))
        return std::nullopt;
    return source;
}

// The terminals a load's power is drawn from: its own L and N, or its socket's while plugged in.
// A socket and an unplugged appliance draw from none.
std::optional<std::pair<QString, QString>> drawsFrom(const Project &project, const PlacedLoad &load)
{
    const std::optional<QString> source = terminalSource(project, load);
    if (!source || !project.catalog().loadType(load.type)->places.isEmpty())
        return std::nullopt;
    return std::pair(terminalId(*source, u"L"_s), terminalId(*source, u"N"_s));
}

// IΔn in A of an RCD or RCBO; none for other devices
std::optional<double> residualLimit(const PlacedDevice &device)
{
    const QJsonValue milliamperes = device.parameters.value(u"residualCurrent");
    if (milliamperes.isUndefined())
        return std::nullopt;
    return milliamperes.toDouble() / 1000;
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
    return m_on.value(id) && !m_trips.contains(id);
}

bool Simulation::setOn(const QString &id, bool on)
{
    const PlacedLoad *load = m_project.load(id);
    const LoadType *type = load ? m_project.catalog().loadType(load->type) : nullptr;
    if (!m_project.device(id) && !(type && type->places.isEmpty()))
        return false;
    const bool wasTripped = clearTrip(id);
    const bool changes = wasTripped || isOn(id) != on;
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

TripCause Simulation::tripCause(QStringView id) const
{
    return m_trips.value(id);
}

bool Simulation::resetSupplyBreaker()
{
    if (!clearTrip(SupplyBreakerId.toString()))
        return false;
    solve();
    return true;
}

bool Simulation::setFault(const QString &loadId, Fault fault)
{
    if (!m_project.load(loadId))
        return false;
    const bool changes = this->fault(loadId) != fault;
    m_faults.insert(loadId, fault);
    if (changes)
        solve();
    return true;
}

Fault Simulation::fault(QStringView loadId) const
{
    return m_faults.value(loadId);
}

bool Simulation::pressTest(const QString &deviceId)
{
    const PlacedDevice *device = m_project.device(deviceId);
    const std::optional<double> limit = device ? residualLimit(*device) : std::nullopt;
    if (!limit || !isOn(deviceId))
        return false;
    const QList<ContactSpec> contacts = closedContacts(*device);
    if (contacts.size() < 2
        || std::abs(voltage(terminalId(deviceId, contacts[0].top))
                    - voltage(terminalId(deviceId, contacts[1].top)))
                <= 0.5 * SupplyVoltage)
        return false;
    trip({ .id = deviceId, .cause = TripCause::Test, .limit = *limit, .time = m_elapsed });
    solve();
    return true;
}

double Simulation::touchVoltage(QStringView loadId) const
{
    return m_touchVoltages.value(loadId);
}

bool Simulation::isDangerous(QStringView loadId) const
{
    return touchVoltage(loadId) > TouchVoltageLimit;
}

double Simulation::elapsed() const
{
    return m_elapsed;
}

void Simulation::solve()
{
    solveNetwork();
    for (int round = 0; round < MaxTripRounds && tripInstantly(); ++round)
        solveNetwork();
    storeResults();
    emit solved();
    const QList<SimulationEvent> events = std::exchange(m_events, {});
    for (const SimulationEvent &event : events)
        emit occurred(event);
}

void Simulation::solveNetwork()
{
    m_nodes.clear();
    for (const QString &terminal : m_project.terminalIds()) {
        const QString key = conductorKey(terminal);
        if (!m_nodes.contains(key))
            m_nodes.insert(key, static_cast<int>(m_nodes.size()));
    }
    const auto node
            = [this](const QString &terminal) { return m_nodes.value(conductorKey(terminal)); };

    QList<Admittance> branches;
    QList<Complex> injected(m_nodes.size());
    for (const TerminalSpec &t : m_project.supply().terminals) {
        const int n = node(terminalId(u"supply"_s, t.name));
        if (t.role == Conductor::L) {
            // a Norton source: E/Zs injected, with 1/Zs to the reference; the stairwell breaker
            // disconnects it
            if (!m_trips.contains(SupplyBreakerId)) {
                branches.append({ n, ReferenceNode, 1 / SupplyImpedance });
                injected[n] = SupplyVoltage / SupplyImpedance;
            }
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
    for (const PlacedLoad &l : m_project.loads()) {
        const std::optional<QString> source = terminalSource(m_project, l);
        if (!source)
            continue;
        const int line = node(terminalId(*source, u"L"_s));
        switch (fault(l.id)) {
        case Fault::None:
            break;
        case Fault::ShortCircuit:
            branches.append({ line, node(terminalId(*source, u"N"_s)), 1 / FaultResistance });
            break;
        case Fault::Leakage:
            branches.append({ line, ReferenceNode, 1 / LeakageResistance });
            break;
        case Fault::EarthFault:
            branches.append({ line, node(terminalId(*source, u"PE"_s)), 1 / FaultResistance });
            break;
        }
    }

    m_voltages = solveNodeVoltages(branches, injected);
}

bool Simulation::tripInstantly()
{
    // Every residual-triggered RCD or RCBO trips; of the current-triggered breakers only the one
    // with the smallest In (ties: the largest current), so the closest one clears a short
    // (ADR 0008).
    QList<SimulationEvent> trips;
    std::optional<SimulationEvent> smallest;
    const auto triggered = [&](const QString &id, double current, double rated) {
        if (!smallest || rated < smallest->limit
            || (rated == smallest->limit && current > smallest->current))
            smallest = SimulationEvent{ .id = id,
                                        .cause = TripCause::ShortCircuit,
                                        .current = current,
                                        .limit = rated,
                                        .time = m_elapsed };
    };

    const SupplyType &supply = m_project.supply();
    const DeviceType *breaker = m_project.catalog().deviceType(u"breaker");
    if (!m_trips.contains(SupplyBreakerId) && breaker && breaker->protection
        && breaker->protection->curves.contains(supply.upstreamCurve)) {
        const double current = supplyBreakerCurrent();
        if (current >= breaker->protection->curves.value(supply.upstreamCurve).upper
                    * supply.upstreamRating)
            triggered(SupplyBreakerId.toString(), current, supply.upstreamRating);
    }

    for (const PlacedDevice &d : m_project.devices()) {
        const DeviceType *type = m_project.catalog().deviceType(d.type);
        if (!type || !type->protection || !isOn(d.id))
            continue;
        const QMap<QString, InstantTripBand> &curves = type->protection->curves;
        double largest = 0;
        Complex sum;
        for (const Complex &current : poleCurrents(d)) {
            largest = std::max(largest, std::abs(current));
            sum += current;
        }
        const double rated = d.parameters.value(u"ratedCurrent").toDouble();
        const QString curve = d.parameters.value(u"curve").toString();
        const bool byCurrent
                = curves.contains(curve) && largest >= curves.value(curve).upper * rated;
        if (byCurrent)
            triggered(d.id, largest, rated);
        const std::optional<double> limit = residualLimit(d);
        if (limit && std::abs(sum) >= *limit) {
            // an RCBO triggered both ways reports the short circuit
            trips.append(byCurrent ? SimulationEvent{ .id = d.id,
                                                      .cause = TripCause::ShortCircuit,
                                                      .current = largest,
                                                      .limit = rated,
                                                      .time = m_elapsed }
                                   : SimulationEvent{ .id = d.id,
                                                      .cause = TripCause::Leakage,
                                                      .current = std::abs(sum),
                                                      .limit = *limit,
                                                      .time = m_elapsed });
        }
    }
    if (smallest
        && std::ranges::none_of(trips, [&](const auto &t) { return t.id == smallest->id; }))
        trips.append(*smallest);

    for (const SimulationEvent &t : std::as_const(trips))
        trip(t);
    return !trips.isEmpty();
}

void Simulation::trip(const SimulationEvent &event)
{
    m_trips.insert(event.id, event.cause);
    m_events.append(event);
}

bool Simulation::clearTrip(const QString &id)
{
    if (!m_trips.remove(id))
        return false;
    m_events.append({ .kind = SimulationEvent::Kind::Cleared, .id = id, .time = m_elapsed });
    return true;
}

void Simulation::storeResults()
{
    const auto across = [this](const QString &a, const QString &b) {
        return std::abs(voltage(a) - voltage(b));
    };

    m_wireCurrents.clear();
    for (const Wire &w : m_project.wires())
        m_wireCurrents.insert(w.id, across(w.from, w.to) / resistance(w));
    m_deviceCurrents.clear();
    for (const PlacedDevice &d : m_project.devices()) {
        double largest = 0;
        for (const Complex &current : poleCurrents(d))
            largest = std::max(largest, std::abs(current));
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

    // Danger is judged once the trips have settled, against the previous solve's.
    m_touchVoltages.clear();
    QMap<QString, Fault> dangerous;
    for (const PlacedLoad &l : m_project.loads()) {
        const std::optional<QString> source = terminalSource(m_project, l);
        const Fault f = fault(l.id);
        if (!source || (f != Fault::Leakage && f != Fault::EarthFault))
            continue;
        // a person touches L for leakage, the case (PE) for an earth fault
        const QString touched = terminalId(*source, f == Fault::Leakage ? u"L"_s : u"PE"_s);
        const double touch = std::abs(voltage(touched));
        m_touchVoltages.insert(l.id, touch);
        if (touch <= TouchVoltageLimit)
            continue;
        dangerous.insert(l.id, f);
        // None if it wasn't dangerous; a changed fault kind is a new danger to describe
        if (m_dangerous.value(l.id) != f)
            m_events.append({ .kind = SimulationEvent::Kind::Dangerous,
                              .id = l.id,
                              .current = f == Fault::Leakage ? touch / LeakageResistance : 0,
                              .voltage = touch,
                              .time = m_elapsed });
    }
    for (const QString &id : m_dangerous.keys()) {
        if (!dangerous.contains(id))
            m_events.append(
                    { .kind = SimulationEvent::Kind::Cleared, .id = id, .time = m_elapsed });
    }
    m_dangerous = dangerous;
}

QList<ContactSpec> Simulation::closedContacts(const PlacedDevice &device) const
{
    const DeviceType *type = m_project.catalog().deviceType(device.type);
    const DeviceVariant *variant = type ? type->variant(device.poles) : nullptr;
    return variant && isOn(device.id) ? variant->contacts : QList<ContactSpec>();
}

Complex Simulation::voltage(const QString &terminal) const
{
    return m_voltages.value(m_nodes.value(conductorKey(terminal), -1));
}

QList<Complex> Simulation::poleCurrents(const PlacedDevice &device) const
{
    QList<Complex> currents;
    for (const ContactSpec &c : closedContacts(device))
        currents << (voltage(terminalId(device.id, c.top))
                     - voltage(terminalId(device.id, c.bottom)))
                        / ContactResistance;
    return currents;
}

double Simulation::supplyBreakerCurrent() const
{
    double largest = 0;
    for (const TerminalSpec &t : m_project.supply().terminals) {
        if (t.role == Conductor::L)
            largest = std::max(largest,
                               std::abs(SupplyVoltage - voltage(terminalId(u"supply"_s, t.name)))
                                       / SupplyImpedance);
    }
    return largest;
}
