// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/catalog.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QRectF>
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

inline constexpr double LoadWireLength = 15; // m
inline constexpr double MaxWireLength = 500; // m

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

inline constexpr int MaxLoadPower = 15000;     // W
inline constexpr double LoadCaptionHeight = 9; // the band under a wired load's picture
// The band above a wired load's picture that its wires come in by, kept free of other loads.
inline constexpr double LoadEntryLaneHeight = 9;

struct PlacedLoad
{
    QString id;       // stable "l<n>"; empty until added
    QString type;     // catalog load type id
    QPointF position; // top-left of the picture, scene units; unused while plugged in
    int power = 0;    // W; 0 for a socket
    QString socket;   // the socket a plug-in appliance is plugged into, "" if none
    int place = 0;    // 1 or 2 while plugged in, else 0
    friend bool operator==(const PlacedLoad &, const PlacedLoad &) = default;
};

enum class LoadPlacement : std::uint8_t {
    Fits,
    OnEnclosure, // the outline overlaps the enclosure
    Overlaps,    // the outline overlaps another load's outline, or one is in the other's entry lane
    PlaceTaken,  // the plug place already holds an appliance
    Invalid      // unknown type, type changed, power out of range, a plug into something that
                 // isn't a socket or by a load that doesn't plug in, place not 1 or 2
};

[[nodiscard]] PlacedLoad loadFromType(const LoadType &type, QPointF position); // default power

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
    // the same, moving exactly these loads with the bottom edge; an undo passes the loads its
    // change moved, as after a shrink a load beside the enclosure can be below the new bottom edge
    bool setEnclosure(const QString &enclosureId, const QStringList &movedLoadIds);

    // same id and index rules as addDevice; returns the id, "" if refused
    QString addWire(Wire wire, qsizetype index = -1);
    bool removeWire(QStringView id);
    // replaces the wire with the same id
    bool updateWire(const Wire &wire);

    // ids of the stored wires in the cable of a wire between these ends, in wires() order; empty if
    // neither end is on a load
    [[nodiscard]] QStringList cableWires(const QString &from, const QString &to) const;
    // the cable's length (its first wire's), else LoadWireLength with a load end, else 0.3
    [[nodiscard]] double newWireLength(const QString &from, const QString &to) const;
    // sets every wire of that wire's cable; one changed(); false if the wire is unknown, has no
    // load end, or length is not in (0, MaxWireLength]
    bool setCableLength(QStringView wireId, double length);

    [[nodiscard]] const QList<PlacedLoad> &loads() const;       // insertion order
    [[nodiscard]] const PlacedLoad *load(QStringView id) const; // nullptr if absent
    // 1-based among the loads of its type in loads() order; 0 if absent
    [[nodiscard]] int loadNumber(QStringView id) const;
    // the picture's top-left: its position, or hanging from its socket's place
    [[nodiscard]] QPointF loadPosition(const PlacedLoad &load) const;
    [[nodiscard]] QRectF loadOutline(const PlacedLoad &load) const;
    // scene rect of a socket's plug place (1 or 2)
    [[nodiscard]] QRectF plugPlaceRect(const PlacedLoad &socket, int place) const;
    // ids of the appliances plugged into the socket, by place
    [[nodiscard]] QStringList pluggedInto(QStringView socketId) const;
    // a load with the same id is ignored, so a move can be checked
    [[nodiscard]] LoadPlacement loadPlacement(const PlacedLoad &load) const;
    // same id and index rules as addDevice; returns the id, "" if refused
    QString addLoad(PlacedLoad load, qsizetype index = -1);
    // removes its wires; a socket's appliances stay where they hung, unplugged
    bool removeLoad(QStringView id);
    bool updateLoad(const PlacedLoad &load);
    // ids of the unplugged loads whose outline top is at or below the enclosure's bottom edge, in
    // loads() order: the loads that follow the edge when the enclosure changes size
    [[nodiscard]] QStringList loadsBelowEnclosure() const;
    // false if moving the loads below the enclosure with its bottom edge would put one onto a load
    // that stays (or the two into each other's entry lane), or the enclosure is unknown
    [[nodiscard]] bool keepsLoadsClear(QStringView enclosureId) const;
    // the same, moving exactly these loads
    [[nodiscard]] bool keepsLoadsClear(QStringView enclosureId,
                                       const QStringList &movedLoadIds) const;

signals:
    void changed();          // after every successful mutation
    void enclosureChanged(); // after changed(), when the enclosure changed

private:
    [[nodiscard]] const EnclosureType *findEnclosure(QAnyStringView id) const;
    // the band above a wired load's picture, kept free of other loads; empty for a plug-in
    [[nodiscard]] QRectF entryLane(const PlacedLoad &load) const;
    // whether the load, moved down by dy, and the other overlap or one is in the other's entry lane
    [[nodiscard]] bool collides(const PlacedLoad &load, qreal dy, const PlacedLoad &other) const;
    // fitsEnclosure(), moving exactly these loads with the bottom edge
    [[nodiscard]] bool fitsEnclosure(QStringView enclosureId,
                                     const QStringList &movedLoadIds) const;

    const Catalog &m_catalog;
    const EnclosureType *m_enclosure;
    QList<PlacedDevice> m_devices;
    QList<Wire> m_wires;
    QList<PlacedLoad> m_loads;
    int m_nextId = 1;
    int m_nextWireId = 1;
    int m_nextLoadId = 1;
};
