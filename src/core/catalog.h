// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDate>
#include <QJsonObject>
#include <QList>
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

struct DeviceVariant
{
    QString poles;
    int modules = 0;
    QString picture;
    QList<TerminalSpec> terminals;
};

struct DeviceType
{
    QString id, name, description;
    QString designation;       // reference designation prefix, e.g. "QF"
    QJsonObject allowedValues; // parameter name -> array of allowed values
    QList<DeviceVariant> variants;
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

struct LoadType
{
    QString id, name, description;
    QString caption; // wired loads: the caption word, e.g. "Lamp"; "" for plug-in
    QString picture;
    QSizeF size;                   // the picture's view box
    int power = 0;                 // default W; 0 for the socket
    bool plugsIn = false;          // a plug-in appliance: no terminals, goes into a socket
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

struct Catalog
{
    QList<DeviceType> deviceTypes;
    QList<BarType> bars;
    QList<SupplyType> supplies;
    QList<EnclosureType> enclosures;
    QList<Preset> presets;
    QList<Product> products;
    QList<LoadType> loads;
    [[nodiscard]] const DeviceType *deviceType(QStringView id) const; // nullptr if absent
    [[nodiscard]] const SupplyType *supply(QStringView id) const;     // nullptr if absent
    [[nodiscard]] const LoadType *loadType(QStringView id) const;     // nullptr if absent
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
