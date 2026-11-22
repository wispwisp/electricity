// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/catalog.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <optional>

struct PlacedDevice
{
    QString id;             // stable "d<n>"; empty until added
    QString type;           // catalog device type id
    QString poles;          // variant label: "1P", "2P", "1P+N"
    QJsonObject parameters; // full rating parameters, e.g. {"curve":"C","ratedCurrent":16}
    int row = 0;
    int module = 0;
    friend bool operator==(const PlacedDevice &, const PlacedDevice &) = default;
};

enum class Placement : std::uint8_t {
    Fits,
    Overlaps,
    PastRailEnd,
    NoRail,
    Invalid // unknown type or variant, or parameters that aren't exactly the type's allowed keys
            // and values
};

[[nodiscard]] PlacedDevice deviceFromPreset(const Preset &preset, int row, int module);
[[nodiscard]] QString ratingText(const PlacedDevice &device); // "C16", "40 A"

inline constexpr std::array<double, 6> WireSections{ 1.5, 2.5, 4, 6, 10, 16 };

struct Wire
{
    QString id;   // stable "w<n>"; empty until added
    QString from; // terminal id, the end clicked first
    QString to;
    double section = 2.5; // mm², one of WireSections
    Conductor conductor = Conductor::L;
    double length = 0.3; // m
    friend bool operator==(const Wire &, const Wire &) = default;
};

enum class WireCheck : std::uint8_t {
    Ok,
    UnknownTerminal,
    SameTerminal,
    TerminalFull,
    Duplicate,
    InvalidSection
};

[[nodiscard]] QString terminalId(const QString &objectId, const QString &terminal); // "d1:N1"
[[nodiscard]] QString terminalObject(const QString &terminalId);                    // "d1"
[[nodiscard]] QString terminalName(const QString &terminalId);                      // "N1"

class Project : public QObject
{
    Q_OBJECT
public:
    static constexpr char DefaultEnclosureId[] = "enclosure-12"; // NOLINT(modernize-avoid-c-arrays)
    static constexpr char DefaultSupplyId[] = "supply-1p";       // NOLINT(modernize-avoid-c-arrays)

    explicit Project(const Catalog &catalog, QObject *parent = nullptr);

    [[nodiscard]] const Catalog &catalog() const;
    [[nodiscard]] const EnclosureType &enclosure() const;
    // enclosure modules / rows
    [[nodiscard]] int modulesPerRow() const;
    // insertion order
    [[nodiscard]] const QList<PlacedDevice> &devices() const;
    // nullptr if absent
    [[nodiscard]] const PlacedDevice *device(QStringView id) const;
    // variant modules, 0 if unknown
    [[nodiscard]] int moduleWidth(const PlacedDevice &device) const;
    // a device with the same id is ignored
    [[nodiscard]] Placement placement(const PlacedDevice &device) const;
    // "" if absent
    [[nodiscard]] QString designation(QStringView id) const;
    // false if unknown, a device would fall off or a bar hole with a wire would not exist
    [[nodiscard]] bool fitsEnclosure(QStringView enclosureId) const;

    [[nodiscard]] const SupplyType &supply() const;
    // supply, N bar holes, PE bar holes, then each device's variant terminals in devices() order
    [[nodiscard]] QStringList terminalIds() const;
    [[nodiscard]] bool hasTerminal(const QString &terminalId) const;
    [[nodiscard]] std::optional<Conductor> terminalRole(const QString &terminalId) const;
    // 1 for a bar hole, 2 for other terminals, 0 if absent
    [[nodiscard]] int wireCapacity(const QString &terminalId) const;
    [[nodiscard]] int wireCount(const QString &terminalId) const;
    [[nodiscard]] Conductor conductorFor(const QString &from, const QString &to) const;
    // insertion order
    [[nodiscard]] const QList<Wire> &wires() const;
    // nullptr if absent
    [[nodiscard]] const Wire *wire(QStringView id) const;
    // a wire with the same id is ignored, so an update can be checked
    [[nodiscard]] WireCheck wireCheck(const Wire &wire) const;
    // terminal names of the stored device with this id that carry wires and that this
    // version of it lacks; in variant order
    [[nodiscard]] QStringList orphanedTerminals(const PlacedDevice &device) const;
    // false if a bar hole with a wire would not exist in that enclosure (or it is unknown)
    [[nodiscard]] bool keepsBarWires(QStringView enclosureId) const;

    // keeps a non-empty id; returns the id, "" if refused. index is the position in devices()
    // to insert at, -1 appends; any other index outside [0, devices().size()] is refused.
    QString addDevice(PlacedDevice device, qsizetype index = -1);
    bool removeDevice(QStringView id);
    // replaces the device with the same id
    bool updateDevice(const PlacedDevice &device);
    bool setEnclosure(const QString &enclosureId);

    // same id and index rules as addDevice; returns the id, "" if refused
    QString addWire(Wire wire, qsizetype index = -1);
    bool removeWire(QStringView id);
    // replaces the wire with the same id
    bool updateWire(const Wire &wire);

signals:
    void changed();          // after every successful mutation
    void enclosureChanged(); // after changed(), when the enclosure changed

private:
    [[nodiscard]] const EnclosureType *findEnclosure(QAnyStringView id) const;

    const Catalog &m_catalog;
    const EnclosureType *m_enclosure;
    QList<PlacedDevice> m_devices;
    QList<Wire> m_wires;
    int m_nextId = 1;
    int m_nextWireId = 1;
};
