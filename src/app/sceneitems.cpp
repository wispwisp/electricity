// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/sceneitems.h"

#include "app/editorscene.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/project.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QPainter>
#include <QSvgRenderer>

namespace {
constexpr qreal kSelectionWidth = 2;
// Room above a device for its designation; "QF10" fits over one 18-unit module.
constexpr qreal kDesignationHeight = 9;
constexpr int kDesignationPixelSize = 6;
constexpr int kRatingPixelSize = 5;
constexpr int kRefusedAlpha = 110;

void drawSelection(QPainter *painter, const QRectF &rect)
{
    painter->setPen(QPen(Theme::selectionColor(), kSelectionWidth));
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
        drawSelection(painter, boundingRect());
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

QRectF TextItem::boundingRect() const
{
    const QFontMetricsF metrics(m_font);
    return { 0, 0, metrics.horizontalAdvance(m_text), metrics.height() };
}

void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setFont(m_font);
    painter->setPen(QApplication::palette().color(QPalette::Text));
    painter->drawText(boundingRect(), Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

DeviceItem::DeviceItem(const Catalog &catalog, const QString &id)
    : m_catalog(catalog)
{
    setData(EditorScene::IdKey, id);
    setFlag(ItemIsSelectable);
    setZValue(1);
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
    m_designation = designation;
    m_rating = ratingText(device);
    update();
}

QString DeviceItem::designation() const
{
    return m_designation;
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

QRectF DeviceItem::boundingRect() const
{
    return { 0, -kDesignationHeight, m_size.width(), m_size.height() + kDesignationHeight };
}

void DeviceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF body(QPointF(0, 0), m_size);
    svgRenderer(m_path).render(painter, body);

    painter->setFont(pixelFont(kRatingPixelSize));
    painter->setPen(Qt::black); // printed on the white label
    painter->drawText(m_label, Qt::AlignCenter, m_rating);
    painter->setFont(pixelFont(kDesignationPixelSize));
    painter->setPen(Theme::enclosureTextColor());
    painter->drawText(QRectF(0, -kDesignationHeight, m_size.width(), kDesignationHeight - 1),
                      Qt::AlignHCenter | Qt::AlignBottom, m_designation);

    if (m_refused) {
        QColor tint = Theme::refusedColor();
        tint.setAlpha(kRefusedAlpha);
        painter->fillRect(body, tint);
    }
    if (isSelected())
        drawSelection(painter, body);
}
