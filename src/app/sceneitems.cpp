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
#include <limits>
#include <tuple>
#include <utility>

namespace {
constexpr qreal kSelectionWidth = 2;
constexpr int kRatingPixelSize = 5;
constexpr int kRefusedAlpha = 150;
constexpr qreal kWireEdgeWidth = 0.8; // the dark edge, 0.4 on each side of the core
// A selected wire has, instead of the edge, a white line and then the selection colour on each side
// of its core, together at most kWireSelectionWidth wide, and stays within its 4.5-unit lane.
constexpr qreal kWireSelectionWidth = 0.8;
constexpr qreal kWireSelectionGap = 0.3; // the white line
constexpr qreal kWireLaneWidth = 4.5;
constexpr qreal kWireHitMargin = 2.4; // the hit shape reaches 1.2 beyond the edge
constexpr qreal kWireHitWidth = 6;
constexpr qreal kEarthStripeLength = 2.2; // as on the supply picture
constexpr qreal kButtonRadius = 4;
constexpr qreal kButtonInset = 6; // from the picture's bottom-right corner to the centre
constexpr qreal kButtonRingWidth = 0.6;
constexpr qreal kGlowMargin = 4.5;
constexpr int kGlowAlpha = 70; // per ring, so the innermost ones add up to nearly solid
constexpr qreal kPlaceOutlineWidth = 0.75;
constexpr qreal kCordWidth = 1.2;
const QColor kCordColor(0x1c, 0x1e, 0x21);
// A light edge, as the pictures' outlines, so that the dark cord shows on the dark canvas.
constexpr qreal kCordEdgeWidth = 0.8; // 0.4 on each side of the core
const QColor kCordEdgeColor(0xe6, 0xe8, 0xeb);
constexpr qreal kMinCaptionScale = 4.0 / DeviceItem::DesignationPixelSize; // as 4-pixel text
constexpr int kCurrentPixelSize = 5;
constexpr qreal kCurrentChipHeight = 7;
constexpr qreal kCurrentChipPadding = 1; // beside the text
constexpr qreal kCurrentChipRadius = 1;
constexpr qreal kCurrentLabelGap = 1; // kept around a wire's label
// From a wire's centre line to a label beside it, clear of the widest wire with its edge.
constexpr qreal kCurrentLabelAside = 2;
constexpr qreal kWireCoreReach = 1; // from a wire's centre line, about half its core
// Below the slot of an off handle (36–62) and above the bottom terminals (78.4 and lower).
constexpr qreal kDeviceCurrentTop = 65;
constexpr qreal kDeviceCurrentInset = 1; // keeps the body's outline beside the chip
constexpr qreal kDeviceCurrentEdgeWidth = 0.5;
const QColor kDeviceCurrentEdgeColor(0x8c, 0x91, 0x97); // as the picture's rating label
const QColor kWireCurrentTextColor(0xe6, 0xe8, 0xeb);
const QColor kWireCurrentChipColor(0x1c, 0x1e, 0x21, 220);

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

// A power symbol: an arc open at the top with a line through the gap. On, a disc of the text
// colour; off, a ring of it around the canvas colour, so that on reads as filled on both themes.
void drawButton(QPainter *painter, const QRectF &rect, bool on)
{
    const QColor ink = QApplication::palette().color(QPalette::Text);
    const QColor paper = QApplication::palette().color(QPalette::Base);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(ink, kButtonRingWidth));
    painter->setBrush(on ? ink : paper);
    painter->drawEllipse(rect);
    painter->setPen(QPen(on ? paper : ink, kButtonRingWidth, Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(Qt::NoBrush);
    const QRectF arc = QRectF(rect.center() - QPointF(2, 2), QSizeF(4, 4));
    painter->drawArc(arc, 120 * 16, 300 * 16);
    painter->drawLine(QPointF(rect.center().x(), rect.center().y() - 2.4),
                      QPointF(rect.center().x(), rect.center().y() - 0.2));
}

// At full strength even on the translucent drag preview, so a refusal reads as red.
void drawRefused(QPainter *painter, const QRectF &rect)
{
    painter->setOpacity(1);
    QColor tint = Theme::refusedColor();
    tint.setAlpha(kRefusedAlpha);
    painter->fillRect(rect, tint);
    drawOutline(painter, rect, Theme::refusedColor());
}

QFont pixelFont(int pixelSize)
{
    QFont font = QApplication::font();
    font.setPixelSize(pixelSize);
    return font;
}

// Centred in rect, and scaled down to its width if wider.
void drawFittedText(QPainter *painter, const QRectF &rect, const QFont &font, const QString &text)
{
    const qreal width = QFontMetricsF(font).horizontalAdvance(text);
    const qreal scale = std::min(1.0, rect.width() / width);
    painter->save();
    painter->translate(rect.center());
    painter->scale(scale, scale);
    painter->setFont(font);
    const QSizeF size = rect.size() / scale;
    painter->drawText(QRectF(QPointF(-size.width() / 2, -size.height() / 2), size), Qt::AlignCenter,
                      text);
    painter->restore();
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
        const auto bounds = [&renderer](const QString &id) {
            return renderer.transformForElement(id).mapRect(renderer.boundsOnElement(id));
        };
        m_label = bounds(QStringLiteral("label"));
        m_handle = bounds(QStringLiteral("handle"));
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

void DeviceItem::setOn(bool on)
{
    if (std::exchange(m_on, on) != on)
        update();
}

bool DeviceItem::isOn() const
{
    return m_on;
}

QRectF DeviceItem::handleRect() const
{
    return m_on ? m_handle : m_handle.translated(0, HandleTravel);
}

QRectF DeviceItem::switchRect() const
{
    return m_handle.united(m_handle.translated(0, HandleTravel));
}

void DeviceItem::setCurrentText(const QString &text)
{
    if (std::exchange(m_current, text) != text)
        update();
}

QString DeviceItem::currentText() const
{
    return m_current;
}

// As wide as the text, but no wider than the body, so that neighbours' chips keep apart.
QRectF DeviceItem::currentRect() const
{
    if (m_current.isEmpty())
        return {};
    const qreal text = QFontMetricsF(pixelFont(kCurrentPixelSize)).horizontalAdvance(m_current);
    const qreal width
            = std::min(text + 2 * kCurrentChipPadding, m_size.width() - 2 * kDeviceCurrentInset);
    return { (m_size.width() - width) / 2, kDeviceCurrentTop, width, kCurrentChipHeight };
}

QRectF DeviceItem::boundingRect() const
{
    return { 0, -DesignationHeight, m_size.width(), m_size.height() + DesignationHeight };
}

void DeviceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF body(QPointF(0, 0), m_size);
    (m_on ? svgRenderer(m_path) : switchedOffRenderer(m_path)).render(painter, body);

    painter->setFont(pixelFont(kRatingPixelSize));
    painter->setPen(Qt::black); // printed on the white label
    painter->drawText(m_label, Qt::AlignCenter, m_rating);

    if (!m_current.isEmpty()) {
        const QRectF chip = currentRect();
        painter->setPen(QPen(kDeviceCurrentEdgeColor, kDeviceCurrentEdgeWidth));
        painter->setBrush(Qt::white);
        painter->drawRoundedRect(chip, kCurrentChipRadius, kCurrentChipRadius);
        painter->setPen(Qt::black);
        drawFittedText(painter, chip.adjusted(kCurrentChipPadding, 0, -kCurrentChipPadding, 0),
                       pixelFont(kCurrentPixelSize), m_current);
    }

    if (isSelected())
        drawOutline(painter, body, Theme::selectionColor());
    if (m_refused)
        drawRefused(painter, body);
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

LoadItem::LoadItem(const Catalog &catalog, const QString &id)
    : m_catalog(catalog)
    , m_font(pixelFont(DeviceItem::DesignationPixelSize))
{
    setData(EditorScene::IdKey, id);
    setFlag(ItemIsSelectable);
    setZValue(PlacedZ);
}

void LoadItem::setLoad(const PlacedLoad &load, const QString &caption,
                       const QList<QRectF> &emptyPlaces, const QList<QLineF> &cords)
{
    const LoadType *type = m_catalog.loadType(load.type);
    Q_ASSERT(type); // the project only holds valid loads
    prepareGeometryChange();
    m_path = loadPicturePath(type->picture);
    m_size = type->size;
    m_switchable = type->places.isEmpty();
    m_light = type->light;
    m_band = type->caption.isEmpty()
            ? QRectF()
            : QRectF(0, m_size.height(), m_size.width(), LoadCaptionHeight);
    m_caption = caption;
    fitCaption();
    m_emptyPlaces = emptyPlaces;
    m_cords = cords;
}

QString LoadItem::caption() const
{
    return m_caption;
}

QList<QRectF> LoadItem::emptyPlaces() const
{
    return m_emptyPlaces;
}

QList<QLineF> LoadItem::cords() const
{
    return m_cords;
}

void LoadItem::setRefused(bool refused)
{
    m_refused = refused;
    update();
}

bool LoadItem::isRefused() const
{
    return m_refused;
}

void LoadItem::setOn(bool on)
{
    if (std::exchange(m_on, on) != on)
        update();
}

bool LoadItem::isOn() const
{
    return m_on;
}

QRectF LoadItem::buttonRect() const
{
    if (!m_switchable)
        return {};
    const QPointF centre(m_size.width() - kButtonInset, m_size.height() - kButtonInset);
    return { centre - QPointF(kButtonRadius, kButtonRadius),
             QSizeF(2 * kButtonRadius, 2 * kButtonRadius) };
}

void LoadItem::setRunning(bool running)
{
    if (std::exchange(m_running, running) != running)
        update();
}

bool LoadItem::isRunning() const
{
    return m_running;
}

QRectF LoadItem::boundingRect() const
{
    QRectF rect = QRectF(QPointF(0, 0), m_size) | m_band;
    if (m_switchable)
        rect |= QRectF(QPointF(0, 0), m_size)
                        .adjusted(-kGlowMargin, -kGlowMargin, kGlowMargin, kGlowMargin);
    for (const QRectF &place : m_emptyPlaces)
        rect |= place;
    const qreal margin = (kCordWidth + kCordEdgeWidth) / 2;
    for (const QLineF &cord : m_cords) {
        const QRectF line = QRectF(cord.p1(), cord.p2()).normalized();
        rect |= line.adjusted(-margin, -margin, margin, margin);
    }
    return rect;
}

QPainterPath LoadItem::shape() const
{
    QPainterPath path;
    path.addRect(QRectF(QPointF(0, 0), m_size) | m_band);
    return path;
}

void LoadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF picture(QPointF(0, 0), m_size);
    if (m_running) {
        const QColor base = m_light ? Theme::lampGlowColor() : Theme::liveColor();
        painter->setPen(Qt::NoPen);
        // Nested rings around the picture, each fainter and wider, so that the strongest colour is
        // at the picture's edge and nothing tints the picture itself.
        constexpr int steps = 6;
        QPainterPath inside;
        inside.addRect(picture);
        for (int i = 0; i < steps; ++i) {
            const qreal grow = kGlowMargin * (steps - i) / steps;
            QPainterPath ring;
            ring.addRoundedRect(picture.adjusted(-grow, -grow, grow, grow), grow, grow);
            QColor color = base;
            color.setAlpha(kGlowAlpha);
            painter->fillPath(ring.subtracted(inside), color);
        }
    }
    svgRenderer(m_path).render(painter, picture);

    if (!m_shownCaption.isEmpty()) {
        painter->save();
        painter->translate(m_band.center());
        painter->scale(m_captionScale, m_captionScale);
        painter->setFont(m_font);
        painter->setPen(QApplication::palette().color(QPalette::Text));
        const QSizeF band = m_band.size() / m_captionScale;
        painter->drawText(QRectF(QPointF(-band.width() / 2, -band.height() / 2), band),
                          Qt::AlignCenter, m_shownCaption);
        painter->restore();
    }
    painter->setBrush(Qt::NoBrush);
    painter->setPen(
            QPen(QApplication::palette().color(QPalette::Mid), kPlaceOutlineWidth, Qt::DashLine));
    const qreal inset = kPlaceOutlineWidth / 2;
    for (const QRectF &place : m_emptyPlaces)
        painter->drawRect(place.adjusted(inset, inset, -inset, -inset));
    painter->setPen(QPen(kCordEdgeColor, kCordWidth + kCordEdgeWidth, Qt::SolidLine, Qt::FlatCap));
    painter->drawLines(m_cords);
    painter->setPen(QPen(kCordColor, kCordWidth, Qt::SolidLine, Qt::FlatCap));
    painter->drawLines(m_cords);

    if (m_switchable)
        drawButton(painter, buttonRect(), m_on);
    if (isSelected())
        drawOutline(painter, picture, Theme::selectionColor());
    if (m_refused)
        drawRefused(painter, picture);
}

QRectF LoadItem::captionRect() const
{
    if (m_shownCaption.isEmpty())
        return {};
    const qreal width = QFontMetricsF(m_font).horizontalAdvance(m_shownCaption) * m_captionScale;
    return { m_band.center().x() - width / 2, m_band.top(), width, m_band.height() };
}

// Wires keep clear of the load's outline only, so a caption wider than the band shrinks to fit it,
// down to kMinCaptionScale, and beyond that is elided.
void LoadItem::fitCaption()
{
    m_shownCaption.clear();
    m_captionScale = 1;
    if (m_caption.isEmpty() || m_band.isEmpty())
        return;
    const QFontMetricsF metrics(m_font);
    const qreal room = m_band.width();
    m_shownCaption = metrics.elidedText(m_caption, Qt::ElideRight, room / kMinCaptionScale);
    const qreal width = metrics.horizontalAdvance(m_shownCaption);
    if (width > room)
        m_captionScale = room / width;
}

CurrentLabelItem::CurrentLabelItem()
    : m_font(pixelFont(kCurrentPixelSize))
{
    setData(EditorScene::IdKey, QStringLiteral("currentLabel"));
    setZValue(LabelZ);
    setAcceptedMouseButtons(Qt::NoButton);
    hide();
}

void CurrentLabelItem::setText(const QString &text)
{
    if (text == m_text)
        return;
    prepareGeometryChange();
    m_text = text;
    setVisible(!m_text.isEmpty());
}

QString CurrentLabelItem::text() const
{
    return m_text;
}

QRectF CurrentLabelItem::boundingRect() const
{
    const qreal width = QFontMetricsF(m_font).horizontalAdvance(m_text) + 2 * kCurrentChipPadding;
    return { -width / 2, -kCurrentChipHeight / 2, width, kCurrentChipHeight };
}

void CurrentLabelItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF chip = boundingRect();
    painter->setPen(Qt::NoPen);
    painter->setBrush(kWireCurrentChipColor);
    painter->drawRoundedRect(chip, kCurrentChipRadius, kCurrentChipRadius);
    painter->setFont(m_font);
    painter->setPen(kWireCurrentTextColor);
    painter->drawText(chip, Qt::AlignCenter, m_text);
}

WireItem::WireItem(const QString &id)
    : m_currentLabel(new CurrentLabelItem)
{
    setData(EditorScene::IdKey, id);
    setFlag(ItemIsSelectable);
    setZValue(WireZ);
}

WireItem::~WireItem()
{
    delete m_currentLabel;
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
    stroker.setWidth(
            std::max(kWireHitWidth, wireCoreWidth(m_section) + kWireEdgeWidth + kWireHitMargin));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    m_shape = stroker.createStroke(path);
    placeCurrentLabel({}, {}, {});
}

QPolygonF WireItem::route() const
{
    return m_route;
}

Conductor WireItem::conductor() const
{
    return m_conductor;
}

void WireItem::setCurrentText(const QString &text)
{
    m_currentLabel->setText(text);
}

QString WireItem::currentText() const
{
    return m_currentLabel->text();
}

CurrentLabelItem *WireItem::currentLabel() const
{
    return m_currentLabel;
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
    if (isSelected()) {
        // The white keeps the selection colour apart from an N wire's similar blue.
        const qreal outer = std::min(kWireLaneWidth, core + 2 * kWireSelectionWidth);
        stroke(Theme::selectionColor(), outer);
        stroke(Qt::white, core + 2 * kWireSelectionGap);
    } else {
        stroke(Theme::wireEdgeColor(), core + kWireEdgeWidth);
    }
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

QVariant WireItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSceneHasChanged) {
        if (auto *newScene = value.value<QGraphicsScene *>())
            newScene->addItem(m_currentLabel);
    }
    return QGraphicsItem::itemChange(change, value);
}

QList<QRectF> WireItem::cores() const
{
    QList<QRectF> cores;
    for (qsizetype i = 1; i < m_route.size(); ++i) {
        cores << QRectF(m_route[i - 1], m_route[i])
                         .normalized()
                         .adjusted(-kWireCoreReach, -kWireCoreReach, kWireCoreReach,
                                   kWireCoreReach);
    }
    return cores;
}

QRectF WireItem::placeCurrentLabel(const QList<QRectF> &labels, const QList<QRectF> &obstacles,
                                   const QList<QRectF> &otherWires)
{
    QList<QLineF> segments;
    for (qsizetype i = 1; i < m_route.size(); ++i)
        segments << QLineF(m_route[i - 1], m_route[i]);
    std::ranges::stable_sort(segments, std::ranges::greater(), &QLineF::length);
    const QRectF label = m_currentLabel->boundingRect();
    // Routes are orthogonal. From a segment's centre line to a label beside it.
    const auto aside = [&label](const QLineF &segment) {
        return segment.dy() == 0 ? QPointF(0, label.height() / 2 + kCurrentLabelAside)
                                 : QPointF(label.width() / 2 + kCurrentLabelAside, 0);
    };
    struct Spot
    {
        QPointF centre;
        bool beside; // moved off the wire to one side
    };
    QList<Spot> spots;
    QList<Spot> beside;
    for (const QLineF &segment : std::as_const(segments)) {
        const auto add = [&](QPointF spot) {
            spots << Spot{ spot, false };
            beside << Spot{ spot - aside(segment), true } << Spot{ spot + aside(segment), true };
        };
        // From the middle outwards while the label stays within the segment; on a segment shorter
        // than the label, only its middle.
        const bool horizontal = segment.dy() == 0;
        const qreal step = (horizontal ? label.width() : label.height()) + kCurrentLabelGap;
        const QPointF along = horizontal ? QPointF(step, 0) : QPointF(0, step);
        const int steps = std::max(0, int((segment.length() - step) / 2 / step));
        add(segment.center());
        for (int i = 1; i <= steps; ++i) {
            add(segment.center() + along * i);
            add(segment.center() - along * i);
        }
    }
    spots << beside;
    const auto covered = [&label](QPointF centre, const QList<QRectF> &rects) {
        const QRectF rect = label.translated(centre).adjusted(-kCurrentLabelGap, -kCurrentLabelGap,
                                                              kCurrentLabelGap, kCurrentLabelGap);
        qreal area = 0;
        for (const QRectF &r : rects) {
            const QRectF common = r & rect;
            area += common.width() * common.height();
        }
        return area;
    };
    // The label would read as another wire's: on the route, centred within half a lane of one;
    // beside it, over one.
    const auto onOtherWire = [&](const Spot &spot) {
        constexpr qreal reach = kWireLaneWidth / 2 - kWireCoreReach; // with the core, half a lane
        const QRectF area = spot.beside
                ? label.translated(spot.centre)
                : QRectF(spot.centre, QSizeF()).adjusted(-reach, -reach, reach, reach);
        return std::ranges::any_of(otherWires,
                                   [&area](const QRectF &wire) { return wire.intersects(area); });
    };
    // Covering a label is worst, then reading as another wire's; of equal spots the first. A spot
    // with none of these is the best there can be, so the first such spot wins.
    using Rank = std::tuple<qreal, bool, qreal>;
    Rank best(std::numeric_limits<qreal>::infinity(), true, 0);
    QPointF centre;
    for (const Spot &spot : std::as_const(spots)) {
        const Rank rank(covered(spot.centre, labels), onOtherWire(spot),
                        covered(spot.centre, obstacles));
        if (rank < best) {
            best = rank;
            centre = spot.centre;
        }
        if (best == Rank())
            break;
    }
    // Last resort, when every spot covers a label: out from beside the longest segment's middle, a
    // unit at a time, so that the label ends as near its wire as the labels allow.
    if (!segments.isEmpty() && covered(centre, labels) > 0) {
        const QLineF &longest = segments.first();
        const QPointF out = longest.dy() == 0 ? QPointF(0, -1) : QPointF(-1, 0);
        centre = longest.center() - aside(longest);
        while (covered(centre, labels) > 0)
            centre += out;
    }
    m_currentLabel->setPos(centre);
    return label.translated(centre);
}
