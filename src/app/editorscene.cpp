// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/editorscene.h"

#include "app/sceneitems.h"
#include "app/svgpixmap.h"
#include "core/project.h"

#include <QGraphicsSceneMouseEvent>
#include <QSvgRenderer>

#include <algorithm>
#include <limits>
#include <utility>

namespace {
constexpr qreal kModuleWidth = 18;
constexpr qreal kModuleHeight = 90;
constexpr qreal kSupplyGap = 12; // between the supply block and the enclosure
constexpr qreal kSupplyTextGap = 6;
constexpr int kSupplyTextPixelSize = 10;
constexpr qreal kSceneMargin = 1000; // room to pan around the panel

// Ctrl and Shift would add to the selection; one item is selected at a time.
void dropSelectionModifiers(QGraphicsSceneMouseEvent *event)
{
    event->setModifiers(event->modifiers() & ~(Qt::ControlModifier | Qt::ShiftModifier));
}
} // namespace

EditorScene::EditorScene(Project &project, QUndoStack &undoStack, QObject *parent)
    : QGraphicsScene(parent)
    , m_project(project)
    , m_undoStack(undoStack)
    , m_enclosure(new PictureItem(picturePath(project.enclosure().picture),
                                  QStringLiteral("enclosure")))
    , m_supply(new PictureItem(picturePath(QStringLiteral("supply-1p.svg")),
                               QStringLiteral("supply")))
    , m_supplyText(new TextItem(kSupplyTextPixelSize))
{
    m_enclosure->setFlag(QGraphicsItem::ItemIsSelectable);
    addItem(m_enclosure);
    addItem(m_supply);
    addItem(m_supplyText);
    connect(&m_project, &Project::changed, this, &EditorScene::syncDevices);
    connect(&m_project, &Project::enclosureChanged, this, &EditorScene::syncEnclosure);
    syncEnclosure();
    syncDevices();
    retranslate();
}

QRectF EditorScene::enclosureElement(const QString &id) const
{
    QSvgRenderer &renderer = svgRenderer(picturePath(m_project.enclosure().picture));
    return renderer.transformForElement(id).mapRect(renderer.boundsOnElement(id));
}

QPointF EditorScene::railCentre(int row) const
{
    return enclosureElement(QStringLiteral("rail-%1").arg(row + 1)).center();
}

QRectF EditorScene::slotRect(int row, int module, int modules) const
{
    const QPointF rail = railCentre(row);
    const qreal originX = rail.x() - m_project.modulesPerRow() * kModuleWidth / 2;
    return { originX + kModuleWidth * module, rail.y() - kModuleHeight / 2, kModuleWidth * modules,
             kModuleHeight };
}

std::optional<RailSlot> EditorScene::slotAt(QPointF scenePos, int modules) const
{
    if (!enclosureElement(QStringLiteral("interior")).contains(scenePos))
        return std::nullopt;
    RailSlot slot;
    qreal nearest = std::numeric_limits<qreal>::max();
    for (int row = 0; row < m_project.enclosure().rows; ++row) {
        const qreal distance = qAbs(railCentre(row).y() - scenePos.y());
        if (distance < nearest) {
            nearest = distance;
            slot.row = row;
        }
    }
    // The device keeps to the rail: near an end it snaps onto the end modules.
    const qreal originX = slotRect(slot.row, 0, 0).left();
    slot.module = std::clamp(qFloor((scenePos.x() - originX) / kModuleWidth - modules / 2.0 + 0.5),
                             0, m_project.modulesPerRow() - modules);
    return slot;
}

QRectF EditorScene::panelRect() const
{
    return m_supply->sceneBoundingRect() | m_supplyText->sceneBoundingRect()
            | m_enclosure->sceneBoundingRect();
}

QString EditorScene::selectedDeviceId() const
{
    for (const QGraphicsItem *item : selectedItems()) {
        if (dynamic_cast<const DeviceItem *>(item))
            return item->data(IdKey).toString();
    }
    return {};
}

bool EditorScene::isEnclosureSelected() const
{
    return m_enclosure->isSelected();
}

void EditorScene::retranslate()
{
    m_supplyText->setText(tr("Supply"));
    updateSceneRect();
}

void EditorScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    dropSelectionModifiers(event);
    QGraphicsScene::mousePressEvent(event);
}

void EditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    dropSelectionModifiers(event);
    QGraphicsScene::mouseReleaseEvent(event);
}

void EditorScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    dropSelectionModifiers(event);
    QGraphicsScene::mouseDoubleClickEvent(event);
}

// Swaps the picture and bars of the existing enclosure item, so a selected enclosure stays
// selected.
void EditorScene::syncEnclosure()
{
    const EnclosureType &enclosure = m_project.enclosure();
    m_enclosure->setPath(picturePath(enclosure.picture));
    qDeleteAll(m_barCells);
    m_barCells.clear();
    addBar(QStringLiteral("nbar"), QStringLiteral("nbar-mount"), enclosure.nBarTerminals);
    addBar(QStringLiteral("pebar"), QStringLiteral("pebar-mount"), enclosure.peBarTerminals);

    const qreal supplyHeight = m_supply->boundingRect().height();
    m_supply->setPos(slotRect(0, 0, 1).left(), m_enclosure->y() - kSupplyGap - supplyHeight);
    const QRectF supply = m_supply->sceneBoundingRect();
    m_supplyText->setPos(supply.right() + kSupplyTextGap,
                         supply.center().y() - m_supplyText->boundingRect().height() / 2);
    updateSceneRect();
}

void EditorScene::addBar(const QString &barId, const QString &mountId, int terminals)
{
    const QList<BarType> &bars = m_project.catalog().bars;
    const auto bar = std::ranges::find(bars, barId, &BarType::id);
    Q_ASSERT(bar != bars.end());
    const QRectF mount = enclosureElement(mountId);
    for (int i = 0; i < terminals; ++i) {
        auto *cell = new PictureItem(picturePath(bar->picture), barId, m_enclosure);
        cell->setPos(mount.left() + bar->pitch * i, mount.top());
        m_barCells << cell;
    }
}

void EditorScene::syncDevices()
{
    QHash<QString, DeviceItem *> previous = std::exchange(m_devices, {});
    for (const PlacedDevice &device : m_project.devices()) {
        DeviceItem *item = previous.take(device.id);
        if (!item) {
            item = new DeviceItem(m_project.catalog(), device.id);
            addItem(item);
        }
        item->setDevice(device, m_project.designation(device.id));
        item->setPos(slotRect(device.row, device.module, m_project.moduleWidth(device)).topLeft());
        m_devices.insert(device.id, item);
    }
    qDeleteAll(previous);
}

void EditorScene::updateSceneRect()
{
    setSceneRect(panelRect().adjusted(-kSceneMargin, -kSceneMargin, kSceneMargin, kSceneMargin));
}
