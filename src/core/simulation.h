// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/nodalsolver.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <cstdint>

class Project;
struct ContactSpec;
struct PlacedDevice;

inline constexpr double SupplyVoltage = 230;        // V
inline constexpr double SupplyImpedance = 0.4;      // Ω, on supply L (ADR 0007)
inline constexpr double ContactResistance = 0.001;  // Ω, a closed contact and the supply N/PE ties
inline constexpr double CopperResistivity = 0.0175; // Ω·mm²/m
inline constexpr double FaultResistance = 0.001;  // Ω, short circuit L–N and earth fault L–PE
inline constexpr double LeakageResistance = 1000; // Ω, L to earth: about a person (ADR 0007)
inline constexpr double TouchVoltageLimit = 50;   // V (ПУЭ 1.7.53)
inline constexpr int MaxTripRounds = 32;
inline constexpr QStringView SupplyBreakerId = u"supply"; // the stairwell breaker

enum class Fault : std::uint8_t { None, ShortCircuit, Leakage, EarthFault };
enum class TripCause : std::uint8_t { None, Overload, ShortCircuit, Leakage, Test };

struct SimulationEvent
{
    enum class Kind : std::uint8_t { Tripped, Overloaded, Overheating, Dangerous, Cleared };
    Kind kind = Kind::Tripped;
    QString id;                        // a device, SupplyBreakerId, a wire or a load
    TripCause cause = TripCause::None; // Tripped
    double current = 0; // A: at the trip (residual for Leakage), in the wire or RCD, or through
                        // the person (Dangerous leakage)
    double voltage = 0; // V: touch voltage (Dangerous)
    double limit = 0;   // A: In, IΔn or the wire's ampacity
    double time = 0;    // s, elapsed() when it happened
};

// The live circuit of a project: on/off settings, trips and faults by object id and the latest
// solve (ADR 0008, 0029, 0030). Solves at once, after every Project::changed() and after every
// setting that changes; instant trips happen inside the solve.
class Simulation : public QObject
{
    Q_OBJECT
public:
    explicit Simulation(const Project &project, QObject *parent = nullptr);
    // a device's handle is up, or a load is switched on; false if never switched on or tripped
    [[nodiscard]] bool isOn(QStringView id) const;
    // a device or a non-socket load of the project; false (nothing stored) otherwise. Setting the
    // value it already has stores it and doesn't solve, unless the device is tripped: that clears
    // the trip and solves.
    bool setOn(const QString &id, bool on);
    [[nodiscard]] double wireCurrent(QStringView wireId) const;     // A; 0 if unknown
    [[nodiscard]] double deviceCurrent(QStringView deviceId) const; // A, its largest contact
    [[nodiscard]] double loadCurrent(QStringView loadId) const;     // A
    // V across the terminals it draws from; 0 if it draws from none
    [[nodiscard]] double loadVoltage(QStringView loadId) const;
    [[nodiscard]] bool isEnergized(QStringView loadId) const;
    // the cause while tripped, else None; a device id or SupplyBreakerId
    [[nodiscard]] TripCause tripCause(QStringView id) const;
    // clears the stairwell breaker's trip and solves (it may trip again); false if not tripped
    bool resetSupplyBreaker();
    // any load of the project; false (nothing stored) otherwise; the value it already has doesn't
    // solve
    bool setFault(const QString &loadId, Fault fault);
    [[nodiscard]] Fault fault(QStringView loadId) const; // None if never set
    // trips an RCD or RCBO that is on and has power (cause Test); false otherwise
    bool pressTest(const QString &deviceId);
    [[nodiscard]] double touchVoltage(QStringView loadId) const; // V; 0 unless its fault acts
    [[nodiscard]] bool isDangerous(QStringView loadId) const;    // touchVoltage > TouchVoltageLimit
    [[nodiscard]] double elapsed() const;                        // simulated s

signals:
    void solved();                               // after every solve
    void occurred(const SimulationEvent &event); // after the solved() of the solve that settled it

private:
    struct LoadResult
    {
        double current = 0; // A
        double voltage = 0; // V
        bool energized = false;
    };

    void solve();
    // node voltages of the network as it is now
    void solveNetwork();
    // trips what the latest network solve triggers at once; false if nothing
    bool tripInstantly();
    void trip(const SimulationEvent &event);
    // a Cleared event; false if it wasn't tripped
    bool clearTrip(const QString &id);
    void storeResults();
    [[nodiscard]] QList<ContactSpec> closedContacts(const PlacedDevice &device) const;
    [[nodiscard]] Complex voltage(const QString &terminal) const; // of the latest network solve
    // of the closed contacts, top to bottom
    [[nodiscard]] QList<Complex> poleCurrents(const PlacedDevice &device) const;
    [[nodiscard]] double supplyBreakerCurrent() const; // A, |E − V(supply L)| / Zs

    const Project &m_project;
    // kept after the object is removed, so an undo restores them
    QHash<QString, bool> m_on;
    QHash<QString, TripCause> m_trips; // tripped devices and the stairwell breaker
    QHash<QString, Fault> m_faults;
    double m_elapsed = 0;        // simulated s
    QHash<QString, int> m_nodes; // conductor key -> node of the latest network solve
    QList<Complex> m_voltages;   // V, by node
    QHash<QString, double> m_wireCurrents;
    QHash<QString, double> m_deviceCurrents;
    QHash<QString, LoadResult> m_loads;
    QHash<QString, double> m_touchVoltages;
    QMap<QString, Fault> m_dangerous; // loads and their faults, after the latest solve
    QList<SimulationEvent> m_events;  // waiting for the end of the solve
};
