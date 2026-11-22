// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/sceneitems.h"

#include "app/editorscene.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/project.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QPainter>
#include <QPainterPathStroker>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kSelectionWidth = 2;
constexpr int kRatingPixelSize = 5;
constexpr int kRefusedAlpha = 150;
constexpr qreal kWireEdgeWidth = 0.8;      // the dark edge, 0.4 on each side of the core
constexpr qreal kWireSelectionWidth = 2.4; // the selection colour, 1.2 outside the edge
constexpr qreal kWireHitWidth = 6;
constexpr qreal kEarthStripeLength = 2.2; // as on the supply picture

// Grows with the section like a conductor's diameter, from 1.8 at 1.5 mm² to 3.2 at 16 mm², so
// that with its edge the thickest wire is 4 wide and wires on neighbouring 4.5-unit lanes keep
// apart.
qreal wireCoreWidth(double section)
{
    return 1.2 + 0.5 * std::sqrt(section);
}

void drawOutline(QPainter *painter, const QRectF &rect, const QColor &color)
{
    painter->setPen(QPen(color, kSelectionWidth));
    painter->setBrush(Qt::NoBrush);
    const qreal inset = kSelectionWidth / 2;
    painter->drawRect(rect.adjusted(inset, inset, -inset, -inset));
}

QFont pixelFont(int pixelSize)
{
    QFont font = QApplication::font();
    font.setPixelSize(pixelSize);
    return font;
}
} // namespace

PictureItem::PictureItem(QString path, const QString &id, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_path(std::move(path))
    , m_size(svgRenderer(m_path).viewBoxF().size())
{
    setData(EditorScene::IdKey, id);
}

void PictureItem::setPath(const QString &path)
{
    prepareGeometryChange();
    m_path = path;
    m_size = svgRenderer(m_path).viewBoxF().size();
}

QRectF PictureItem::boundingRect() const
{
    return { QPointF(0, 0), m_size };
}

void PictureItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    svgRenderer(m_path).render(painter, boundingRect());
    if (isSelected())
        drawOutline(painter, boundingRect(), Theme::selectionColor());
}

TextItem::TextItem(int pixelSize, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_font(pixelFont(pixelSize))
{
}

void TextItem::setText(const QString &text)
{
    prepareGeometryChange();
    m_text = text;
}

QString TextItem::text() const
{
    return m_text;
}

void TextItem::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QRectF TextItem::boundingRect() const
{
    const QFontMetricsF metrics(m_font);
    return { 0, 0, metrics.horizontalAdvance(m_text), metrics.height() };
}

void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setFont(m_font);
    painter->setPen(m_color.value_or(QApplication::palette().color(QPalette::Text)));
    painter->drawText(boundingRect(), Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

DeviceItem::DeviceItem(const Catalog &catalog, const QString &id)
    : m_catalog(catalog)
    , m_designationItem(new TextItem(DesignationPixelSize))
{
    setData(EditorScene::IdKey, id);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(PlacedZ);
    m_designationItem->setData(EditorScene::IdKey, QStringLiteral("designation"));
    m_designationItem->setColor(Theme::enclosureTextColor());
    m_designationItem->setZValue(LabelZ);
}

DeviceItem::~DeviceItem()
{
    delete m_designationItem;
}

void DeviceItem::setDevice(const PlacedDevice &device, const QString &designation)
{
    const DeviceType *type = m_catalog.deviceType(device.type);
    const DeviceVariant *variant = type ? type->variant(device.poles) : nullptr;
    Q_ASSERT(variant); // the project only holds valid devices
    const QString path = picturePath(variant->picture);
    if (path != m_path) {
        prepareGeometryChange();
        m_path = path;
        QSvgRenderer &renderer = svgRenderer(m_path);
        m_size = renderer.viewBoxF().size();
        const QString label = QStringLiteral("label");
        m_label = renderer.transformForElement(label).mapRect(renderer.boundsOnElement(label));
    }
    m_designationItem->setText(designation);
    placeDesignation();
    m_rating = ratingText(device);
    update();
}

QString DeviceItem::designation() const
{
    return m_designationItem->text();
}

QString DeviceItem::rating() const
{
    return m_rating;
}

void DeviceItem::setRefused(bool refused)
{
    m_refused = refused;
    update();
}

bool DeviceItem::isRefused() const
{
    return m_refused;
}

QGraphicsItem *DeviceItem::designationItem() const
{
    return m_designationItem;
}

QRectF DeviceItem::boundingRect() const
{
    return { 0, -DesignationHeight, m_size.width(), m_size.height() + DesignationHeight };
}

void DeviceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF body(QPointF(0, 0), m_size);
    svgRenderer(m_path).render(painter, body);

    painter->setFont(pixelFont(kRatingPixelSize));
    painter->setPen(Qt::black); // printed on the white label
    painter->drawText(m_label, Qt::AlignCenter, m_rating);

    if (isSelected())
        drawOutline(painter, body, Theme::selectionColor());
    if (m_refused) {
        // At full strength even on the translucent drag preview, so a refusal reads as red.
        painter->setOpacity(1);
        QColor tint = Theme::refusedColor();
        tint.setAlpha(kRefusedAlpha);
        painter->fillRect(body, tint);
        drawOutline(painter, body, Theme::refusedColor());
    }
}

QVariant DeviceItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        placeDesignation();
    } else if (change == ItemSceneHasChanged) {
        if (auto *newScene = value.value<QGraphicsScene *>())
            newScene->addItem(m_designationItem);
    }
    return QGraphicsItem::itemChange(change, value);
}

void DeviceItem::placeDesignation()
{
    const QRectF text = m_designationItem->boundingRect();
    m_designationItem->setPos(pos()
                              + QPointF((m_size.width() - text.width()) / 2,
                                        -(DesignationHeight + text.height()) / 2));
}

WireItem::WireItem(const QString &id)
{
    setData(EditorScene::IdKey, id);
    setFlag(ItemIsSelectable);
    setZValue(WireZ);
}

void WireItem::setWire(const QPolygonF &route, Conductor conductor, double section)
{
    prepareGeometryChange();
    m_route = route;
    m_conductor = conductor;
    m_section = section;
    QPainterPath path;
    path.addPolygon(m_route);
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(kWireHitWidth,
                              wireCoreWidth(m_section) + kWireEdgeWidth + kWireSelectionWidth));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    m_shape = stroker.createStroke(path);
}

QPolygonF WireItem::route() const
{
    return m_route;
}

Conductor WireItem::conductor() const
{
    return m_conductor;
}

// The shape is at least as wide as anything painted.
QRectF WireItem::boundingRect() const
{
    return m_shape.boundingRect();
}

QPainterPath WireItem::shape() const
{
    return m_shape;
}

void WireItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const qreal core = wireCoreWidth(m_section);
    const auto stroke = [&](const QColor &color, qreal width) {
        painter->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPolyline(m_route);
    };
    painter->setBrush(Qt::NoBrush);
    if (isSelected())
        stroke(Theme::selectionColor(), core + kWireEdgeWidth + kWireSelectionWidth);
    stroke(Theme::wireEdgeColor(), core + kWireEdgeWidth);
    stroke(Theme::conductorColor(m_conductor), core);
    if (m_conductor == Conductor::PE) {
        QPen stripes(Theme::earthStripeColor(), core, Qt::CustomDashLine, Qt::FlatCap,
                     Qt::RoundJoin);
        // Dash lengths are in pen widths.
        stripes.setDashPattern({ kEarthStripeLength / core, kEarthStripeLength / core });
        painter->setPen(stripes);
        painter->drawPolyline(m_route);
    }
}
