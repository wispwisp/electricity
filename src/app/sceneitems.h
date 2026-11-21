// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFont>
#include <QGraphicsItem>
#include <QString>

struct Catalog;
struct PlacedDevice;

// An SVG picture at its view-box size, with its scene id in data(0).
class PictureItem : public QGraphicsItem
{
public:
    PictureItem(QString path, const QString &id, QGraphicsItem *parent = nullptr);
    void setPath(const QString &path);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QString m_path;
    QSizeF m_size;
};

// Text in the current palette's Text colour, read at paint time so it follows a theme switch.
class TextItem : public QGraphicsItem
{
public:
    explicit TextItem(int pixelSize, QGraphicsItem *parent = nullptr);
    void setText(const QString &text);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QString m_text;
    QFont m_font;
};

// A placed device: its variant picture, the rating on its label and the designation above it.
// The origin is the device's top-left corner.
class DeviceItem : public QGraphicsItem
{
public:
    static constexpr qreal PlacedZ = 1;
    static constexpr qreal LiftedZ = 2; // the drag preview and a device being moved

    DeviceItem(const Catalog &catalog, const QString &id);
    void setDevice(const PlacedDevice &device, const QString &designation);
    [[nodiscard]] QString designation() const;
    [[nodiscard]] QString rating() const;
    void setRefused(bool refused);
    [[nodiscard]] bool isRefused() const;

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    const Catalog &m_catalog;
    QString m_path;
    QSizeF m_size;
    QRectF m_label; // the picture's "label" element
    QString m_designation;
    QString m_rating;
    bool m_refused = false;
};
