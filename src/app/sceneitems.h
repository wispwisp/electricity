// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QFont>
#include <QGraphicsItem>
#include <QLineF>
#include <QList>
#include <QPainterPath>
#include <QPolygonF>
#include <QString>

#include <cstdint>
#include <optional>

enum class Conductor : std::uint8_t;
struct Catalog;
struct PlacedDevice;
struct PlacedLoad;
class QVariantAnimation;

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

// Text in the current palette's Text colour, read at paint time so it follows a theme switch, or
// in a fixed colour.
class TextItem : public QGraphicsItem
{
public:
    explicit TextItem(int pixelSize, QGraphicsItem *parent = nullptr);
    void setText(const QString &text);
    [[nodiscard]] QString text() const;
    void setColor(const QColor &color);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QString m_text;
    QFont m_font;
    std::optional<QColor> m_color;
};

// A placed device: its variant picture and the rating on its label. The origin is the device's
// top-left corner. Its designation is a separate top-level item over the band above the device, so
// that it draws over the wires running up through it.
class DeviceItem : public QGraphicsItem
{
public:
    static constexpr qreal PlacedZ = 1;
    static constexpr qreal LabelZ = 1.75; // designations
    static constexpr qreal LiftedZ = 2;   // the drag preview and a device being moved
    // Room above a device for its designation; "QF10" fits over one 18-unit module.
    static constexpr qreal DesignationHeight = 9;
    static constexpr int DesignationPixelSize = 6;

    DeviceItem(const Catalog &catalog, const QString &id);
    ~DeviceItem() override;
    void setDevice(const PlacedDevice &device, const QString &designation);
    [[nodiscard]] QString designation() const;
    [[nodiscard]] QString rating() const;
    void setRefused(bool refused);
    [[nodiscard]] bool isRefused() const;
    [[nodiscard]] QGraphicsItem *designationItem() const;
    void setOn(bool on); // repaints; a new item is off
    [[nodiscard]] bool isOn() const;
    // A red outline and "!" badge; going off while tripped drops the handle over 150 ms.
    void setTripped(bool tripped);
    [[nodiscard]] bool isTripped() const;
    [[nodiscard]] QRectF tripMarkRect() const;   // item coordinates; empty when not tripped
    [[nodiscard]] QRectF testButtonRect() const; // the picture's "test-button"; empty without one
    [[nodiscard]] QRectF handleRect() const;     // item coordinates, where the handle is drawn now
    // Item coordinates: where a press switches the device, the handle's on and off positions
    // together, so that the second click of a double click still finds the handle it moved.
    [[nodiscard]] QRectF switchRect() const;
    // A chip on the body below the handle slot; "" hides it.
    void setCurrentText(const QString &text);
    [[nodiscard]] QString currentText() const;
    [[nodiscard]] QRectF currentRect() const; // item coordinates; empty when hidden

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void placeDesignation();

    const Catalog &m_catalog;
    QString m_path;
    QSizeF m_size;
    QRectF m_label;  // the picture's "label" element
    QRectF m_handle; // the picture's "handle" element, drawn on (up)
    QRectF m_testButton;
    TextItem *m_designationItem;
    QVariantAnimation *m_drop;
    qreal m_travel = 1; // how far down the handle is, 0 on to 1 off
    QString m_rating;
    QString m_current;
    bool m_refused = false;
    bool m_on = false;
    bool m_tripped = false;
};

// A red outline and "!" badge over the supply picture while the stairwell breaker is tripped, as a
// child of the picture. Clicks go through to the picture.
class TripMarkItem : public QGraphicsItem
{
public:
    explicit TripMarkItem(QGraphicsItem *supply);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};

// A placed load: its picture, the caption under a wired load, and for a socket faint outlines of
// its empty plug places and a cord to each plugged appliance. The origin is the picture's top-left.
class LoadItem : public QGraphicsItem
{
public:
    static constexpr qreal PlacedZ = 1;
    static constexpr qreal LiftedZ = 2;

    LoadItem(const Catalog &catalog, const QString &id);
    // places and cords in item coordinates
    void setLoad(const PlacedLoad &load, const QString &caption, const QList<QRectF> &emptyPlaces,
                 const QList<QLineF> &cords);
    [[nodiscard]] QString caption() const;
    [[nodiscard]] QList<QRectF> emptyPlaces() const;
    [[nodiscard]] QList<QLineF> cords() const;
    void setRefused(bool refused);
    [[nodiscard]] bool isRefused() const;
    // the caption as painted, centred in the caption band and never wider than it
    [[nodiscard]] QRectF captionRect() const;
    void setOn(bool on);
    [[nodiscard]] bool isOn() const;
    // item coordinates: the on/off button near the picture's bottom-right corner; empty for a
    // socket
    [[nodiscard]] QRectF buttonRect() const;
    // a warm glow for a light, a green halo otherwise
    void setRunning(bool running);
    [[nodiscard]] bool isRunning() const;
    // a lightning mark bottom-left in the picture, red when the fault is dangerous
    void setFault(bool faulted, bool dangerous);
    // item coordinates; empty without a fault
    [[nodiscard]] QRectF faultMarkRect() const;
    [[nodiscard]] bool isDangerous() const;

    // what it paints: the picture, the caption band, the empty places, the cords and, for a
    // switchable load, the glow
    [[nodiscard]] QRectF boundingRect() const override;
    // the picture and the caption band, so that an empty plug place doesn't take clicks
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void fitCaption();

    const Catalog &m_catalog;
    QString m_path;
    QSizeF m_size;
    QRectF m_band; // under the picture of a wired load, else empty
    QFont m_font;
    QString m_caption;
    QString m_shownCaption; // m_caption, elided if it can't fit even at the smallest scale
    qreal m_captionScale = 1;
    QList<QRectF> m_emptyPlaces;
    QList<QLineF> m_cords;
    bool m_refused = false;
    bool m_switchable = false;
    bool m_light = false;
    bool m_on = false;
    bool m_running = false;
    bool m_faulted = false;
    bool m_dangerous = false;
};

// A wire's current: light text on a dark chip, the same in both themes, centred on its position.
// Clicks go through to the wire under it.
class CurrentLabelItem : public QGraphicsItem
{
public:
    static constexpr qreal LabelZ = 1.8; // above wires and designations

    CurrentLabelItem();
    void setText(const QString &text); // "" hides it
    [[nodiscard]] QString text() const;

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QString m_text;
    QFont m_font;
};

// A wire along its route, which is in scene coordinates: a dark edge, the conductor colour and, on
// PE, yellow stripes. Its current label is a separate top-level item, so that it draws over the
// other wires.
class WireItem : public QGraphicsItem
{
public:
    static constexpr qreal WireZ = 1.5;

    explicit WireItem(const QString &id);
    ~WireItem() override;
    void setWire(const QPolygonF &route, Conductor conductor, double section);
    [[nodiscard]] QPolygonF route() const;
    [[nodiscard]] Conductor conductor() const;
    void setCurrentText(const QString &text); // "" hides the label
    [[nodiscard]] QString currentText() const;
    [[nodiscard]] CurrentLabelItem *currentLabel() const;
    // The route's segments, each with the width of a wire's core around it.
    [[nodiscard]] QList<QRectF> cores() const;
    // Puts the label on its route or beside it. Spots rank by how much of labels they cover, then
    // by whether the label would read as another wire's (centred on one of otherWires' cores, or
    // beside the route over one), then by how much of obstacles they cover; of equal spots, on the
    // route before beside it, on the longest segment first and nearest its middle, so after
    // setWire() on the middle of the longest segment. When every spot covers a label, it steps out
    // from beside the longest segment's middle until it covers none. Returns the label's scene
    // rect.
    QRectF placeCurrentLabel(const QList<QRectF> &labels, const QList<QRectF> &obstacles,
                             const QList<QRectF> &otherWires);

    [[nodiscard]] QRectF boundingRect() const override;
    // the route stroked at least 6 wide, for clicking
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QPolygonF m_route;
    QPainterPath m_shape;
    Conductor m_conductor{};
    double m_section = 0;
    CurrentLabelItem *m_currentLabel;
};
