// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QList>

#include <optional>

class DeviceItem;
class PictureItem;
class Project;
class QUndoStack;
class TextItem;
struct PlacedDevice;

struct RailSlot
{
    int row = 0;
    int module = 0;
    friend bool operator==(const RailSlot &, const RailSlot &) = default;
};

// The panel: the supply block, the enclosure with its N and PE bars, and one item per placed
// device, kept in sync with the project. Scene units are enclosure picture units; the enclosure
// sits at (0, 0). One item is selected at a time: a device or the enclosure. Presets dropped from
// the palette are placed, and devices are moved with the mouse, through the undo stack.
class EditorScene : public QGraphicsScene
{
    Q_OBJECT
public:
    static constexpr int IdKey = 0;

    EditorScene(Project &project, QUndoStack &undoStack, QObject *parent = nullptr);
    // scene rect the device body would occupy
    [[nodiscard]] QRectF slotRect(int row, int module, int modules) const;
    // device centred on scenePos; nullopt outside the enclosure interior
    [[nodiscard]] std::optional<RailSlot> slotAt(QPointF scenePos, int modules) const;
    // supply, its text and the enclosure
    [[nodiscard]] QRectF panelRect() const;
    // "" if none
    [[nodiscard]] QString selectedDeviceId() const;
    [[nodiscard]] bool isEnclosureSelected() const;
    // the "Supply" text
    void retranslate();

signals:
    void hint(const QString &text);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;

private:
    // bounds of an element of the current enclosure picture
    [[nodiscard]] QRectF enclosureElement(const QString &id) const;
    [[nodiscard]] QPointF railCentre(int row) const;
    void syncEnclosure();
    void syncDevices();
    void addBar(const QString &barId, const QString &mountId, int terminals);
    void updateSceneRect();
    // Puts the device on the slot for a body centred on centre; false, with a hint, if it can't
    // go there.
    bool placeAt(PlacedDevice &device, QPointF centre);
    // Shows the item on the slot, tinted if the device doesn't fit there.
    void snapItem(DeviceItem *item, PlacedDevice device, RailSlot slot);
    // A left press on a device may become a move.
    void pressDevice(QGraphicsSceneMouseEvent *event);
    void finishMove(QPointF centre);
    void removePreview();

    Project &m_project;
    QUndoStack &m_undoStack;
    PictureItem *m_enclosure;
    PictureItem *m_supply;
    TextItem *m_supplyText;
    QList<PictureItem *> m_barCells;
    QHash<QString, DeviceItem *> m_devices;
    DeviceItem *m_preview = nullptr; // while a preset is dragged over the scene
    QString m_movingId;              // the device under a left press, "" if none
    QPointF m_grabOffset;            // press point minus the device body centre
    QPoint m_pressScreenPos;
    bool m_moving = false; // the press has moved beyond the start drag distance
};
