// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/editorscene.h"

#include "app/editcommands.h"
#include "app/palettewidget.h"
#include "app/sceneitems.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/project.h"
#include "core/wirerouter.h"

#include <QApplication>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPen>
#include <QSvgRenderer>
#include <QUndoStack>

#include <algorithm>
#include <limits>
#include <utility>

namespace {
constexpr qreal kModuleWidth = 18;
constexpr qreal kModuleHeight = 90;
constexpr qreal kSupplyGap = 12; // between the supply block and the enclosure
constexpr qreal kSupplyTextGap = 6;
constexpr int kSupplyTextPixelSize = 10;
// A supply letter starts this far right of its terminal, beside the wire leaving it straight down.
constexpr qreal kSupplyLetterOffset = 2.5;
constexpr qreal kSceneMargin = 1000; // room to pan around the panel
constexpr qreal kPreviewOpacity = 0.6;
constexpr qreal kTerminalReach = 4; // a click this close to a terminal is on it
constexpr qreal kHighlightRadius = 4.5;
constexpr qreal kHighlightWidth = 1.2;
constexpr qreal kWirePreviewWidth = 1.5;
constexpr qreal kWireToolZ = 3; // the wire tool's overlays, above a device lifted for a move

// Ctrl and Shift would add to the selection; one item is selected at a time.
void dropSelectionModifiers(QGraphicsSceneMouseEvent *event)
{
    event->setModifiers(event->modifiers() & ~(Qt::ControlModifier | Qt::ShiftModifier));
}

// The preset a palette drag carries, or nullptr.
const Preset *draggedPreset(const Catalog &catalog, const QMimeData *data)
{
    const QString format = QString::fromLatin1(PaletteWidget::PresetMimeType);
    if (!data || !data->hasFormat(format))
        return nullptr;
    const auto it = std::ranges::find(catalog.presets, QString::fromUtf8(data->data(format)),
                                      &Preset::id);
    return it == catalog.presets.end() ? nullptr : &*it;
}

// nullptr if absent
const TerminalSpec *findTerminal(const QList<TerminalSpec> &terminals, const QString &name)
{
    const auto it = std::ranges::find(terminals, name, &TerminalSpec::name);
    return it == terminals.end() ? nullptr : &*it;
}
} // namespace

EditorScene::EditorScene(Project &project, QUndoStack &undoStack, QObject *parent)
    : QGraphicsScene(parent)
    , m_project(project)
    , m_undoStack(undoStack)
    , m_enclosure(new PictureItem(picturePath(project.enclosure().picture),
                                  QStringLiteral("enclosure")))
    , m_supply(new PictureItem(picturePath(project.supply().picture), QStringLiteral("supply")))
    , m_supplyText(new TextItem(kSupplyTextPixelSize))
    , m_terminalHighlight(new QGraphicsEllipseItem(-kHighlightRadius, -kHighlightRadius,
                                                   2 * kHighlightRadius, 2 * kHighlightRadius))
    , m_wirePreview(new QGraphicsLineItem)
{
    m_enclosure->setFlag(QGraphicsItem::ItemIsSelectable);
    addItem(m_enclosure);
    addItem(m_supply);
    addItem(m_supplyText);
    for (const TerminalSpec &terminal : project.supply().terminals) {
        auto *letter = new TextItem(DeviceItem::DesignationPixelSize);
        letter->setData(IdKey, QStringLiteral("supplyLetter"));
        letter->setText(terminal.name); // a conductor code, not translated
        addItem(letter);
        m_supplyLetters << letter;
    }
    const auto addOverlay = [this](QGraphicsItem *overlay, const QString &id) {
        overlay->setData(IdKey, id);
        overlay->setZValue(kWireToolZ);
        overlay->setAcceptedMouseButtons(Qt::NoButton);
        overlay->hide();
        addItem(overlay);
    };
    addOverlay(m_terminalHighlight, QStringLiteral("terminalHighlight"));
    addOverlay(m_wirePreview, QStringLiteral("wirePreview"));
    connect(&m_project, &Project::changed, this, &EditorScene::syncDevices);
    connect(&m_project, &Project::changed, this, &EditorScene::syncWires);
    connect(&m_project, &Project::changed, this, &EditorScene::syncWireTool);
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

QPointF EditorScene::terminalPos(const QString &terminalId) const
{
    return routeEnd(terminalId).point;
}

QString EditorScene::selectedDeviceId() const
{
    for (const QGraphicsItem *item : selectedItems()) {
        if (dynamic_cast<const DeviceItem *>(item))
            return item->data(IdKey).toString();
    }
    return {};
}

QString EditorScene::selectedWireId() const
{
    for (const QGraphicsItem *item : selectedItems()) {
        if (dynamic_cast<const WireItem *>(item))
            return item->data(IdKey).toString();
    }
    return {};
}

bool EditorScene::isEnclosureSelected() const
{
    return m_enclosure->isSelected();
}

QString EditorScene::terminalAt(QPointF scenePos) const
{
    QString nearest;
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    for (const QString &id : m_project.terminalIds()) {
        const qreal distance = QLineF(terminalPos(id), scenePos).length();
        if (distance < nearestDistance) {
            nearest = id;
            nearestDistance = distance;
        }
    }
    return nearestDistance <= kTerminalReach ? nearest : QString();
}

// Moves never start under the tool, and one whose release never arrived must not go on under it.
void EditorScene::setWireMode(bool on)
{
    if (on == m_wireMode)
        return;
    m_wireMode = on;
    m_wireStart.clear();
    cancelMove();
    updateWireOverlays();
    if (on)
        emit hint(tr("Click a terminal to start a wire."));
    emit wireModeChanged(on);
}

bool EditorScene::isWireMode() const
{
    return m_wireMode;
}

void EditorScene::setWireSection(double section)
{
    m_wireSection = section;
}

double EditorScene::wireSection() const
{
    return m_wireSection;
}

QString EditorScene::wireStart() const
{
    return m_wireStart;
}

void EditorScene::retranslate()
{
    m_supplyText->setText(tr("Supply"));
    updateSceneRect();
}

// ~QGraphicsScene deletes the selected items and would emit selectionChanged to listeners that
// call back into this already destroyed EditorScene. Device items go first: each deletes its
// designation item, which ~QGraphicsScene would otherwise delete in any order.
EditorScene::~EditorScene()
{
    blockSignals(true);
    qDeleteAll(m_devices);
    removePreview();
}

void EditorScene::setDeviceContextActions(const QList<QAction *> &actions)
{
    m_contextActions = actions;
}

void EditorScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_wireMode && pressWireTool(event))
        return;
    // The menu takes the mouse, so with the left button held a move would never see its release.
    if (event->button() == Qt::RightButton && !(event->buttons() & Qt::LeftButton)) {
        const QList<QGraphicsItem *> hits = items(event->scenePos());
        for (QGraphicsItem *hit : hits) {
            const QString id = hit->data(IdKey).toString();
            if (!m_devices.contains(id) && !m_wires.contains(id))
                continue;
            clearSelection();
            hit->setSelected(true);
            if (!m_contextMenu)
                m_contextMenu = new QMenu(event->widget());
            m_contextMenu->setObjectName(QStringLiteral("deviceContextMenu"));
            m_contextMenu->clear();
            m_contextMenu->addActions(m_contextActions);
            m_contextMenu->popup(event->screenPos());
            event->accept();
            return;
        }
    }
    dropSelectionModifiers(event);
    if (event->button() == Qt::LeftButton)
        cancelMove(); // one still marked lost its release (to a popup, say)
    QGraphicsScene::mousePressEvent(event);
    pressDevice(event);
}

void EditorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (m_wireMode) {
        m_wireCursor = event->scenePos();
        updateWireOverlays();
    }
    if (m_movingId.isEmpty() || !(event->buttons() & Qt::LeftButton))
        return;
    if (!m_moving
        && (event->screenPos() - m_pressScreenPos).manhattanLength()
                < QApplication::startDragDistance())
        return;
    const PlacedDevice *device = m_project.device(m_movingId);
    DeviceItem *item = m_devices.value(m_movingId);
    if (!device || !item)
        return;
    m_moving = true;
    item->setZValue(DeviceItem::LiftedZ);
    const QPointF centre = event->scenePos() - m_grabOffset;
    const int modules = m_project.moduleWidth(*device);
    if (const std::optional<RailSlot> slot = slotAt(centre, modules)) {
        snapItem(item, *device, *slot);
    } else {
        item->setPos(centre - QPointF(kModuleWidth * modules, kModuleHeight) / 2);
        item->setRefused(true);
    }
}

void EditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    dropSelectionModifiers(event);
    QGraphicsScene::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton || m_movingId.isEmpty())
        return;
    if (std::exchange(m_moving, false))
        finishMove(event->scenePos() - m_grabOffset);
    m_movingId.clear();
}

void EditorScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_wireMode && event->button() == Qt::LeftButton) {
        // The press of the first click has already acted.
        event->accept();
        return;
    }
    if (m_wireMode && pressWireTool(event))
        return;
    dropSelectionModifiers(event);
    if (event->button() == Qt::LeftButton)
        cancelMove();
    QGraphicsScene::mouseDoubleClickEvent(event);
    // The press of a quick second drag arrives as a double click.
    pressDevice(event);
}

// With the wire tool on, Esc cancels a wire in progress, otherwise it turns the tool off. With the
// tool off, Esc cancels a move.
void EditorScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_wireMode) {
        if (m_wireStart.isEmpty())
            setWireMode(false);
        else
            cancelWire();
        event->accept();
        return;
    }
    if (event->key() != Qt::Key_Escape || m_movingId.isEmpty()) {
        QGraphicsScene::keyPressEvent(event);
        return;
    }
    cancelMove();
    event->accept();
}

// Other buttons pressed during a move leave it running.
void EditorScene::pressDevice(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const auto *item = dynamic_cast<DeviceItem *>(mouseGrabberItem());
    const PlacedDevice *device = item ? m_project.device(item->data(IdKey).toString()) : nullptr;
    if (!device)
        return;
    m_movingId = device->id;
    m_grabOffset = event->scenePos()
            - slotRect(device->row, device->module, m_project.moduleWidth(*device)).center();
    m_pressScreenPos = event->screenPos();
}

void EditorScene::finishMove(QPointF centre)
{
    const PlacedDevice *device = m_project.device(m_movingId);
    if (!device)
        return;
    // Back where the model has it; a move that fits then syncs it to the new slot.
    cancelMove();
    PlacedDevice moved = *device;
    if (placeAt(moved, centre) && moved != *device)
        m_undoStack.push(new UpdateDeviceCommand(m_project, moved));
}

void EditorScene::cancelMove()
{
    const PlacedDevice *device = m_project.device(m_movingId);
    DeviceItem *item = m_devices.value(m_movingId);
    m_moving = false;
    m_movingId.clear();
    if (!device || !item)
        return;
    item->setZValue(DeviceItem::PlacedZ);
    item->setRefused(false);
    item->setPos(slotRect(device->row, device->module, m_project.moduleWidth(*device)).topLeft());
    if (mouseGrabberItem() == item)
        item->ungrabMouse();
}

bool EditorScene::pressWireTool(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_wireCursor = event->scenePos();
        clickTerminal(terminalAt(m_wireCursor));
        updateWireOverlays();
    } else if (event->button() == Qt::RightButton && !m_wireStart.isEmpty()) {
        cancelWire();
    } else {
        return false;
    }
    event->accept();
    return true;
}

void EditorScene::clickTerminal(const QString &terminalId)
{
    if (terminalId.isEmpty())
        return;
    if (m_wireStart.isEmpty()) {
        if (isTerminalFull(terminalId)) {
            emit hint(fullTerminalHint(terminalId));
        } else {
            m_wireStart = terminalId;
            emit hint(tr("Click the second terminal. Esc cancels."));
        }
    } else if (terminalId == m_wireStart) {
        m_wireStart.clear();
    } else {
        finishWire(terminalId);
    }
}

// A refused wire keeps the wire in progress, so another end can be picked.
void EditorScene::finishWire(const QString &end)
{
    const Conductor conductor = m_project.conductorFor(m_wireStart, end);
    const Wire wire{ {}, m_wireStart, end, m_wireSection, conductor };
    const WireCheck check = m_project.wireCheck(wire);
    if (check == WireCheck::Ok) {
        m_wireStart.clear();
        auto *command = new AddWireCommand(m_project, wire);
        m_undoStack.push(command);
        clearSelection();
        m_wires.value(command->wireId())->setSelected(true);
    } else if (check == WireCheck::TerminalFull) {
        // An undo or a redo may have filled the start since it was clicked.
        emit hint(fullTerminalHint(isTerminalFull(end) ? end : m_wireStart));
    } else if (check == WireCheck::Duplicate) {
        emit hint(tr("These terminals are already joined by a wire."));
    }
}

void EditorScene::cancelWire()
{
    m_wireStart.clear();
    updateWireOverlays();
}

void EditorScene::syncWireTool()
{
    if (!m_wireStart.isEmpty() && !m_project.hasTerminal(m_wireStart))
        m_wireStart.clear();
    updateWireOverlays();
}

void EditorScene::updateWireOverlays()
{
    const QString terminal = m_wireMode ? terminalAt(m_wireCursor) : QString();
    m_terminalHighlight->setVisible(!terminal.isEmpty());
    if (!terminal.isEmpty()) {
        m_terminalHighlight->setPos(terminalPos(terminal));
        const QColor color
                = isTerminalFull(terminal) ? Theme::refusedColor() : Theme::selectionColor();
        m_terminalHighlight->setPen(QPen(color, kHighlightWidth));
    }
    m_wirePreview->setVisible(!m_wireStart.isEmpty());
    if (!m_wireStart.isEmpty()) {
        const Conductor conductor = m_project.terminalRole(m_wireStart).value_or(Conductor::L);
        m_wirePreview->setPen(
                QPen(Theme::conductorColor(conductor), kWirePreviewWidth, Qt::DashLine));
        m_wirePreview->setLine(QLineF(terminalPos(m_wireStart), m_wireCursor));
    }
}

bool EditorScene::isTerminalFull(const QString &terminalId) const
{
    return m_project.wireCount(terminalId) >= m_project.wireCapacity(terminalId);
}

QString EditorScene::fullTerminalHint(const QString &terminalId) const
{
    return m_project.wireCapacity(terminalId) == 1 ? tr("This bar hole already has a wire.")
                                                   : tr("This terminal already has two wires.");
}

bool EditorScene::placeAt(PlacedDevice &device, QPointF centre)
{
    const std::optional<RailSlot> slot = slotAt(centre, m_project.moduleWidth(device));
    if (!slot) {
        emit hint(tr("Devices go on a DIN rail inside the enclosure."));
        return false;
    }
    device.row = slot->row;
    device.module = slot->module;
    if (m_project.placement(device) != Placement::Fits) {
        emit hint(tr("No room here: another device is in the way."));
        return false;
    }
    return true;
}

void EditorScene::snapItem(DeviceItem *item, PlacedDevice device, RailSlot slot)
{
    device.row = slot.row;
    device.module = slot.module;
    item->setPos(slotRect(slot.row, slot.module, m_project.moduleWidth(device)).topLeft());
    item->setRefused(m_project.placement(device) != Placement::Fits);
}

void EditorScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    removePreview();
    dragMoveEvent(event);
}

// Every preset drag is accepted, even where the device doesn't fit, so that the drop arrives and
// can say why.
void EditorScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    const Preset *preset = draggedPreset(m_project.catalog(), event->mimeData());
    if (!preset) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    const PlacedDevice device = deviceFromPreset(*preset, 0, 0);
    if (!m_preview) {
        m_preview = new DeviceItem(m_project.catalog(), QStringLiteral("preview"));
        m_preview->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_preview->setOpacity(kPreviewOpacity);
        m_preview->setZValue(DeviceItem::LiftedZ);
        m_preview->setDevice(device, QString());
        addItem(m_preview);
    }
    const std::optional<RailSlot> slot = slotAt(event->scenePos(), m_project.moduleWidth(device));
    m_preview->setVisible(slot.has_value());
    if (slot)
        snapItem(m_preview, device, *slot);
}

void EditorScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *)
{
    removePreview();
}

void EditorScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    removePreview();
    const Preset *preset = draggedPreset(m_project.catalog(), event->mimeData());
    if (!preset) {
        event->ignore();
        return;
    }
    PlacedDevice device = deviceFromPreset(*preset, 0, 0);
    if (!placeAt(device, event->scenePos())) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    auto *command = new AddDeviceCommand(m_project, device);
    m_undoStack.push(command);
    clearSelection();
    m_devices.value(command->deviceId())->setSelected(true);
}

void EditorScene::removePreview()
{
    delete std::exchange(m_preview, nullptr);
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
    const QList<TerminalSpec> &terminals = m_project.supply().terminals;
    for (qsizetype i = 0; i < m_supplyLetters.size(); ++i) {
        m_supplyLetters[i]->setPos(supply.left() + terminals[i].position.x() + kSupplyLetterOffset,
                                   supply.bottom());
    }
    updateSceneRect();
    syncWires();
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

void EditorScene::syncWires()
{
    const QList<Wire> &wires = m_project.wires();
    QList<RouteRequest> requests;
    for (const Wire &wire : wires)
        requests << RouteRequest{ routeEnd(wire.from), routeEnd(wire.to) };
    const QList<QList<QPointF>> routes
            = routeWires(routeObstacles(), enclosureElement(QStringLiteral("interior")), requests);
    QHash<QString, WireItem *> previous = std::exchange(m_wires, {});
    for (qsizetype i = 0; i < wires.size(); ++i) {
        WireItem *item = previous.take(wires[i].id);
        if (!item) {
            item = new WireItem(wires[i].id);
            addItem(item);
        }
        item->setWire(QPolygonF(routes[i]), wires[i].conductor, wires[i].section);
        m_wires.insert(wires[i].id, item);
    }
    qDeleteAll(previous);
}

RouteEnd EditorScene::routeEnd(const QString &terminalId) const
{
    const QString object = terminalObject(terminalId);
    const QString name = terminalName(terminalId);
    const QList<BarType> &bars = m_project.catalog().bars;
    if (object == QStringLiteral("supply")) {
        if (const TerminalSpec *terminal = findTerminal(m_project.supply().terminals, name))
            return { m_supply->pos() + terminal->position, Exit::Down };
    } else if (const auto bar = std::ranges::find(bars, object, &BarType::id); bar != bars.end()) {
        const QRectF mount = enclosureElement(object + QStringLiteral("-mount"));
        const QPointF hole(bar->pitch * (name.toInt() - 1) + bar->terminal.x(), bar->terminal.y());
        // The N bar sits above the PE bar, and each is wired from the side away from the other.
        return { mount.topLeft() + hole, object == QStringLiteral("nbar") ? Exit::Up : Exit::Down };
    } else if (const PlacedDevice *device = m_project.device(object)) {
        const DeviceType *type = m_project.catalog().deviceType(device->type);
        const DeviceVariant *variant = type ? type->variant(device->poles) : nullptr;
        Q_ASSERT(variant); // the project only holds valid devices
        if (const TerminalSpec *terminal = findTerminal(variant->terminals, name)) {
            return { slotRect(device->row, device->module, variant->modules).topLeft()
                             + terminal->position,
                     terminal->position.y() < kModuleHeight / 2 ? Exit::Up : Exit::Down };
        }
    }
    return { {}, Exit::Down };
}

QList<QRectF> EditorScene::routeObstacles() const
{
    QList<QRectF> obstacles;
    for (const PlacedDevice &device : m_project.devices()) {
        obstacles << slotRect(device.row, device.module, m_project.moduleWidth(device))
                             .adjusted(0, -DeviceItem::DesignationHeight, 0, 0);
    }
    QRectF supply = m_supply->sceneBoundingRect();
    for (const TextItem *letter : std::as_const(m_supplyLetters))
        supply |= letter->sceneBoundingRect();
    obstacles << supply;
    // One block, so that no wire runs between the bars.
    QRectF bars;
    for (const PictureItem *cell : std::as_const(m_barCells))
        bars |= cell->sceneBoundingRect();
    obstacles << bars;
    return obstacles;
}

void EditorScene::updateSceneRect()
{
    setSceneRect(panelRect().adjusted(-kSceneMargin, -kSceneMargin, kSceneMargin, kSceneMargin));
}
