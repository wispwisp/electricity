// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class Project;

inline constexpr double SupplyVoltage = 230;        // V
inline constexpr double SupplyImpedance = 0.4;      // Ω, on supply L (ADR 0007)
inline constexpr double ContactResistance = 0.001;  // Ω, a closed contact and the supply N/PE ties
inline constexpr double CopperResistivity = 0.0175; // Ω·mm²/m

// The live circuit of a project: on/off settings by object id and the latest solve (ADR 0008,
// 0029). Solves at once, after every Project::changed() and after every setting that changes.
class Simulation : public QObject
{
    Q_OBJECT
public:
    explicit Simulation(const Project &project, QObject *parent = nullptr);
    // a device's handle is up, or a load is switched on; false if never switched on
    [[nodiscard]] bool isOn(QStringView id) const;
    // a device or a non-socket load of the project; false (nothing stored) otherwise. Setting the
    // value it already has stores it and doesn't solve.
    bool setOn(const QString &id, bool on);
    [[nodiscard]] double wireCurrent(QStringView wireId) const;     // A; 0 if unknown
    [[nodiscard]] double deviceCurrent(QStringView deviceId) const; // A, its largest contact
    [[nodiscard]] double loadCurrent(QStringView loadId) const;     // A
    // V across the terminals it draws from; 0 if it draws from none
    [[nodiscard]] double loadVoltage(QStringView loadId) const;
    [[nodiscard]] bool isEnergized(QStringView loadId) const;

signals:
    void solved(); // after every solve

private:
    struct LoadResult
    {
        double current = 0; // A
        double voltage = 0; // V
        bool energized = false;
    };

    void solve();

    const Project &m_project;
    QHash<QString, bool> m_on; // kept after the object is removed, so an undo restores it
    QHash<QString, double> m_wireCurrents;
    QHash<QString, double> m_deviceCurrents;
    QHash<QString, LoadResult> m_loads;
};
