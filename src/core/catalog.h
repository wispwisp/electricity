// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

enum class Conductor : std::uint8_t { L, N, PE };

struct TerminalSpec
{
    QString name;
    QPointF position;              // picture units
    std::optional<Conductor> role; // none for poles that real devices don't mark
    friend bool operator==(const TerminalSpec &, const TerminalSpec &) = default;
};

struct ContactSpec
{
    QString top, bottom; // terminal names a closed contact joins
    friend bool operator==(const ContactSpec &, const ContactSpec &) = default;
};

struct DeviceVariant
{
    QString poles;
    int modules = 0;
    QString picture;
    QList<TerminalSpec> terminals;
    QList<ContactSpec> contacts; // in catalog order
};

struct InstantTripBand
{
    double lower = 0; // × In: from here a trip is reported as a short circuit
    double upper = 0; // × In: an instant (magnetic) trip (ADR 0008)
    friend bool operator==(const InstantTripBand &, const InstantTripBand &) = default;
};

struct ProtectionSpec
{
    double timeConstant = 0;               // s, of the heat state
    std::optional<double> tripFactor;      // thermal trip at (factor·In)²; none: never on heat
    QMap<QString, InstantTripBand> curves; // by curve letter; empty: no instant trip on current
    friend bool operator==(const ProtectionSpec &, const ProtectionSpec &) = default;
};

struct DeviceType
{
    QString id, name, description;
    QString designation;       // reference designation prefix, e.g. "QF"
    QJsonObject allowedValues; // parameter name -> array of allowed values
    QList<DeviceVariant> variants;
    std::optional<ProtectionSpec> protection; // none: never trips, never heats
    [[nodiscard]] const DeviceVariant *variant(QStringView poles) const; // nullptr if absent
};

struct BarType
{
    QString id;
    QString picture;
    double pitch = 0;
    QPointF terminal;
    Conductor role = Conductor::N;
};

struct SupplyType
{
    QString id;
    QString picture;
    QList<TerminalSpec> terminals;
    QString upstreamCurve;     // the stairwell breaker on its L (ADR 0008); empty: none
    double upstreamRating = 0; // A
};

struct EnclosureType
{
    QString id;
    int modules = 0;
    int rows = 0;
    int nBarTerminals = 0;
    int peBarTerminals = 0;
    QString picture;
    QSizeF size; // the picture's view box
};

inline constexpr int MaxLoadPower = 15000; // W

struct LoadType
{
    QString id, name, description;
    QString caption; // wired loads: the caption word, e.g. "Lamp"; "" for plug-in
    QString picture;
    QSizeF size;                   // the picture's view box
    int power = 0;                 // default W; 0 for the socket
    bool plugsIn = false;          // a plug-in appliance: no terminals, goes into a socket
    bool light = false;            // glows warm when it runs (the ceiling lamp)
    QList<TerminalSpec> terminals; // wired loads: L, N, PE with matching roles
    QList<QPointF> places;         // the socket's two plug places; empty for other loads
};

struct Preset
{
    QString id, group, name, purpose, type, poles;
    QJsonObject parameters;
    std::optional<double> wireSection;
};

struct Product
{
    QString brand, series, model, article, type, poles;
    QJsonObject parameters;
    QString sourceUrl;
    QDate accessed;
};

struct WireRating
{
    double section = 0;  // mm²
    double ampacity = 0; // A, Iz (ADR 0016)
};

struct Catalog
{
    QList<DeviceType> deviceTypes;
    QList<BarType> bars;
    QList<SupplyType> supplies;
    QList<EnclosureType> enclosures;
    QList<Preset> presets;
    QList<Product> products;
    QList<LoadType> loads;
    double wireTimeConstant = 0; // s
    QList<WireRating> wireRatings;
    [[nodiscard]] const DeviceType *deviceType(QStringView id) const; // nullptr if absent
    [[nodiscard]] const SupplyType *supply(QStringView id) const;     // nullptr if absent
    [[nodiscard]] const LoadType *loadType(QStringView id) const;     // nullptr if absent
    [[nodiscard]] double ampacity(double section) const; // A; 0 if the section has none
};

struct CatalogSources
{
    QByteArray devices, enclosures, presets, products, loads;
};

struct CatalogLoad
{
    Catalog catalog;
    QStringList errors;
};

CatalogLoad loadCatalog(const CatalogSources &sources);

// The catalog shipped inside the executable (resources/catalog/*.json).
CatalogSources embeddedCatalogSources();
// Loaded on first use; a broken embedded catalog is a build defect and aborts with every error.
const Catalog &embeddedCatalog();
