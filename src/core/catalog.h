// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <optional>

struct TerminalSpec
{
    QString name;
    QPointF position; // picture units
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
};

struct EnclosureType
{
    QString id;
    int modules = 0;
    int rows = 0;
    int nBarTerminals = 0;
    int peBarTerminals = 0;
    QString picture;
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
    QList<EnclosureType> enclosures;
    QList<Preset> presets;
    QList<Product> products;
    [[nodiscard]] const DeviceType *deviceType(QStringView id) const; // nullptr if absent
};

struct CatalogSources
{
    QByteArray devices, enclosures, presets, products;
};

struct CatalogLoad
{
    Catalog catalog;
    QStringList errors;
};

CatalogLoad loadCatalog(const CatalogSources &sources);
