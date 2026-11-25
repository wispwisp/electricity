// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/simulation.h"

#include "core/nodalsolver.h"
#include "core/project.h"

#include <algorithm>
#include <cmath>
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

// A section without an ampacity stays Normal.
WireHeat wireHeatFor(double heat, double ampacity)
{
    if (ampacity <= 0)
        return WireHeat::Normal;
    if (heat >= std::pow(OverheatingFactor * ampacity, 2))
        return WireHeat::Overheating;
    return heat > ampacity * ampacity ? WireHeat::Overloaded : WireHeat::Normal;
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

QStringList Simulation::switchedOn() const
{
    QStringList ids;
    for (const PlacedDevice &d : m_project.devices()) {
        if (isOn(d.id))
            ids.append(d.id);
    }
    for (const PlacedLoad &l : m_project.loads()) {
        if (isOn(l.id))
            ids.append(l.id);
    }
    return ids;
}

void Simulation::reset(const QStringList &on)
{
    m_on.clear();
    m_trips.clear();
    m_faults.clear();
    m_heat.clear();
    m_wireHeat.clear();
    m_overloaded.clear();
    m_dangerous.clear();
    m_events.clear();
    m_elapsed = 0;
    for (const QString &id : on)
        m_on.insert(id, true);
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

void Simulation::step(double seconds)
{
    // Currents only change in a solve, and a solve only follows a trip, so heat runs exactly from
    // one thermal trip to the next (ADR 0030).
    bool tripped = false;
    while (seconds > 0) {
        double moment = seconds;
        QList<SimulationEvent> due = earliestThermalTrips(moment);
        advance(moment);
        seconds -= moment;
        if (due.isEmpty())
            continue;
        for (SimulationEvent &event : due) {
            event.time = m_elapsed;
            trip(event);
        }
        solve();
        tripped = true;
    }
    if (!tripped)
        emit heated();
    emitEvents();
}

WireHeat Simulation::wireHeat(QStringView wireId) const
{
    return m_wireHeat.value(wireId);
}

double Simulation::heatLevel(QStringView deviceId) const
{
    const PlacedDevice *device = m_project.device(deviceId);
    const DeviceType *type = device ? m_project.catalog().deviceType(device->type) : nullptr;
    if (!type || !type->protection)
        return 0;
    if (!type->protection->tripFactor)
        return isOverloaded(deviceId) ? 1 : 0;
    const double rated = device->parameters.value(u"ratedCurrent").toDouble();
    const double trip = std::pow(*type->protection->tripFactor * rated, 2);
    return std::clamp((m_heat.value(deviceId) - rated * rated) / (trip - rated * rated), 0.0, 1.0);
}

bool Simulation::isOverloaded(QStringView deviceId) const
{
    return m_overloaded.contains(deviceId.toString());
}

void Simulation::solve()
{
    solveNetwork();
    for (int round = 0; round < MaxTripRounds && tripInstantly(); ++round)
        solveNetwork();
    storeResults();
    emit solved();
    emitEvents();
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
    m_events.append({ .kind = SimulationEvent::Kind::Cleared,
                      .ends = SimulationEvent::Kind::Tripped,
                      .id = id,
                      .time = m_elapsed });
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
            m_events.append({ .kind = SimulationEvent::Kind::Cleared,
                              .ends = SimulationEvent::Kind::Dangerous,
                              .id = id,
                              .time = m_elapsed });
    }
    m_dangerous = dangerous;
}

void Simulation::emitEvents()
{
    const QList<SimulationEvent> events = std::exchange(m_events, {});
    for (const SimulationEvent &event : events)
        emit occurred(event);
}

QList<SimulationEvent> Simulation::earliestThermalTrips(double &seconds) const
{
    QList<SimulationEvent> due;
    const auto consider = [&](const QString &id, double current, double rated,
                              const ProtectionSpec &protection, const QString &curve) {
        if (!protection.tripFactor)
            return;
        const double limit = std::pow(*protection.tripFactor * rated, 2);
        const double squared = current * current;
        // heat that settles at I² at or under the limit never trips, however hot it is now
        if (squared <= limit)
            return;
        const double heat = m_heat.value(id);
        const double t = heat >= limit
                ? 0
                : protection.timeConstant * std::log((squared - heat) / (squared - limit));
        if (t > seconds)
            return;
        if (t < seconds)
            due.clear();
        seconds = t;
        const bool shortCircuit = protection.curves.contains(curve)
                && current >= protection.curves.value(curve).lower * rated;
        due.append({ .id = id,
                     .cause = shortCircuit ? TripCause::ShortCircuit : TripCause::Overload,
                     .current = current,
                     .limit = rated });
    };

    const SupplyType &supply = m_project.supply();
    if (const ProtectionSpec *protection = supplyBreakerProtection();
        protection && !m_trips.contains(SupplyBreakerId))
        consider(SupplyBreakerId.toString(), supplyBreakerCurrent(), supply.upstreamRating,
                 *protection, supply.upstreamCurve);
    for (const PlacedDevice &d : m_project.devices()) {
        const DeviceType *type = m_project.catalog().deviceType(d.type);
        if (type && type->protection && isOn(d.id))
            consider(d.id, m_deviceCurrents.value(d.id),
                     d.parameters.value(u"ratedCurrent").toDouble(), *type->protection,
                     d.parameters.value(u"curve").toString());
    }
    return due;
}

void Simulation::advance(double seconds)
{
    if (seconds <= 0)
        return;
    m_elapsed += seconds;
    // exact for a steady current: H ← I² + (H − I²)·e^(−t/τ)
    const auto heat = [&](const QString &id, double current, double tau) {
        const double squared = current * current;
        const double h = squared + (m_heat.value(id) - squared) * std::exp(-seconds / tau);
        m_heat.insert(id, h);
        return h;
    };

    if (const ProtectionSpec *protection = supplyBreakerProtection())
        heat(SupplyBreakerId.toString(), supplyBreakerCurrent(), protection->timeConstant);
    for (const PlacedDevice &d : m_project.devices()) {
        const DeviceType *type = m_project.catalog().deviceType(d.type);
        if (!type || !type->protection)
            continue;
        const double current = m_deviceCurrents.value(d.id);
        const double h = heat(d.id, current, type->protection->timeConstant);
        if (type->protection->tripFactor)
            continue;
        // an RCD never trips on heat; it only warns
        const double rated = d.parameters.value(u"ratedCurrent").toDouble();
        const bool overloaded = h > rated * rated;
        if (overloaded == m_overloaded.contains(d.id))
            continue;
        if (overloaded)
            m_overloaded.insert(d.id);
        else
            m_overloaded.remove(d.id);
        m_events.append({ .kind = overloaded ? SimulationEvent::Kind::Overloaded
                                             : SimulationEvent::Kind::Cleared,
                          .ends = SimulationEvent::Kind::Overloaded,
                          .id = d.id,
                          .current = current,
                          .limit = rated,
                          .time = m_elapsed });
    }
    for (const Wire &w : m_project.wires()) {
        const double current = m_wireCurrents.value(w.id);
        const double ampacity = m_project.catalog().ampacity(w.section);
        const WireHeat state
                = wireHeatFor(heat(w.id, current, m_project.catalog().wireTimeConstant), ampacity);
        if (state == m_wireHeat.value(w.id))
            continue;
        m_wireHeat.insert(w.id, state);
        const SimulationEvent::Kind kind = state == WireHeat::Normal
                ? SimulationEvent::Kind::Cleared
                : (state == WireHeat::Overloaded ? SimulationEvent::Kind::Overloaded
                                                 : SimulationEvent::Kind::Overheating);
        m_events.append({ .kind = kind,
                          .ends = SimulationEvent::Kind::Overloaded,
                          .id = w.id,
                          .current = current,
                          .limit = ampacity,
                          .time = m_elapsed });
    }
}

const ProtectionSpec *Simulation::supplyBreakerProtection() const
{
    const DeviceType *breaker = m_project.catalog().deviceType(u"breaker");
    if (!breaker || !breaker->protection
        || !breaker->protection->curves.contains(m_project.supply().upstreamCurve))
        return nullptr;
    return &*breaker->protection;
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
    // the source is gone, so V(supply L) = 0 would read as E / Zs
    if (m_trips.contains(SupplyBreakerId))
        return 0;
    double largest = 0;
    for (const TerminalSpec &t : m_project.supply().terminals) {
        if (t.role == Conductor::L)
            largest = std::max(largest,
                               std::abs(SupplyVoltage - voltage(terminalId(u"supply"_s, t.name)))
                                       / SupplyImpedance);
    }
    return largest;
}
