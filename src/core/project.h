// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/catalog.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <cstdint>

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

class Project : public QObject
{
    Q_OBJECT
public:
    static constexpr char DefaultEnclosureId[] = "enclosure-12"; // NOLINT(modernize-avoid-c-arrays)

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
    // false if unknown or a device would fall off
    [[nodiscard]] bool fitsEnclosure(QStringView enclosureId) const;

    // keeps a non-empty id; returns the id, "" if refused
    QString addDevice(PlacedDevice device);
    bool removeDevice(QStringView id);
    // replaces the device with the same id
    bool updateDevice(const PlacedDevice &device);
    bool setEnclosure(const QString &enclosureId);

signals:
    void changed();          // after every successful mutation
    void enclosureChanged(); // after changed(), when the enclosure changed

private:
    [[nodiscard]] const EnclosureType *findEnclosure(QAnyStringView id) const;

    const Catalog &m_catalog;
    const EnclosureType *m_enclosure;
    QList<PlacedDevice> m_devices;
    int m_nextId = 1;
};
