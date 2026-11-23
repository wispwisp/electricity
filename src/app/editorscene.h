// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QList>

#include <cstdint>
#include <optional>

class DeviceItem;
class LoadItem;
class PictureItem;
class QAction;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QMenu;
class Project;
class QUndoStack;
class Simulation;
class TextItem;
class WireItem;
enum class LoadPlacement : std::uint8_t;
struct LoadType;
struct PlacedDevice;
struct PlacedLoad;
struct RouteEnd;

struct RailSlot
{
    int row = 0;
    int module = 0;
    friend bool operator==(const RailSlot &, const RailSlot &) = default;
};

// The translated catalog caption and the load's number, "Lamp 1"; "" for a plug-in appliance.
[[nodiscard]] QString loadCaption(const Project &project, const PlacedLoad &load,
                                  const LoadType &type);

// The panel: the supply block, the enclosure with its N and PE bars, one item per placed device,
// one per load and one per wire, kept in sync with the project. Scene units are enclosure picture
// units; the enclosure sits at (0, 0). One item is selected at a time: a device, a load, a wire or
// the enclosure.
// Presets and loads dropped from the palette are placed, devices and loads are moved with the
// mouse, and wires are drawn with the wire tool, through the undo stack.
class EditorScene : public QGraphicsScene
{
    Q_OBJECT
public:
    static constexpr int IdKey = 0;

    EditorScene(Project &project, QUndoStack &undoStack, QObject *parent = nullptr);
    ~EditorScene() override;
    // scene rect the device body would occupy
    [[nodiscard]] QRectF slotRect(int row, int module, int modules) const;
    // device centred on scenePos; nullopt outside the enclosure interior
    [[nodiscard]] std::optional<RailSlot> slotAt(QPointF scenePos, int modules) const;
    // supply, its text, the enclosure and the loads' outlines
    [[nodiscard]] QRectF panelRect() const;
    // scene position of a terminal of the project, for a device at its slot in the model
    [[nodiscard]] QPointF terminalPos(const QString &terminalId) const;
    // "" if none
    [[nodiscard]] QString selectedDeviceId() const;
    // "" if none
    [[nodiscard]] QString selectedWireId() const;
    // "" if none
    [[nodiscard]] QString selectedLoadId() const;
    [[nodiscard]] bool isEnclosureSelected() const;
    // nearest terminal within 4 units, "" if none
    [[nodiscard]] QString terminalAt(QPointF scenePos) const;
    // off cancels a wire in progress and hides the overlays
    void setWireMode(bool on);
    [[nodiscard]] bool isWireMode() const;
    void setWireSection(double section);
    [[nodiscard]] double wireSection() const;
    // terminal id of the wire in progress, "" if none
    [[nodiscard]] QString wireStart() const;
    // Esc with the tool on: cancels the wire in progress, or with none turns the tool off.
    void escapeWireTool();
    // Actions of the menu shown by a right-click on a device, a load or a wire.
    void setDeviceContextActions(const QList<QAction *> &actions);
    // the "Supply" text, the load captions and the current labels
    void retranslate();
    [[nodiscard]] Simulation &simulation(); // created and owned by the scene
    // the current labels on devices and wires; shown by default
    void setCurrentsShown(bool shown);
    [[nodiscard]] bool currentsShown() const;

signals:
    void hint(const QString &text);
    void wireModeChanged(bool on); // also when Esc turns the tool off

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
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
    // Captions are translated here, so retranslate() re-syncs too.
    void syncLoads();
    // Re-routes every wire, with a device being moved still at its slot in the model. Their
    // labels are placed by syncLive(), after the solve that follows every change.
    void syncWires();
    // Shows the simulation's switch settings and results: each device's handle up or down, each
    // load's button and glow, and the current labels where current flows.
    void syncLive();
    // Each shown wire label, in wire order, on or beside its route, never over a label before it,
    // and where the route allows neither centred on nor beside it over another wire, and clear of
    // what wires go around.
    void placeCurrentLabels();
    // The terminal's position and the side its wires leave from.
    [[nodiscard]] RouteEnd routeEnd(const QString &terminalId) const;
    // What wires go around: device bodies with their designations, the supply with its letters,
    // the two bars as one block, and the outlines of loads that aren't plugged in.
    [[nodiscard]] QList<QRectF> routeObstacles() const;
    void addBar(const QString &barId, const QString &mountId, int terminals);
    void updateSceneRect();
    // Puts the device on the slot for a body centred on centre; false, with a hint, if it can't
    // go there.
    bool placeAt(PlacedDevice &device, QPointF centre);
    // Shows the item on the slot, tinted if the device doesn't fit there.
    void snapItem(DeviceItem *item, PlacedDevice device, RailSlot slot);
    // Where the load goes for a drop with the cursor at cursor: an appliance over a socket into its
    // free place nearest the cursor (socket and place), any other load with its picture centred on
    // the cursor and snapped to the grid (position). A plugged appliance over its own socket stays
    // in its place. Returns why it can't go there, or Fits.
    [[nodiscard]] LoadPlacement loadTarget(PlacedLoad &load, QPointF cursor) const;
    // The same; false, with a hint, if the load can't go there.
    bool placeLoadAt(PlacedLoad &load, QPointF cursor);
    // Shows the load where a drop at cursor would put it, tinted if it can't go there.
    void previewLoad(const LoadType &type, QPointF cursor);
    // A left press on a device or a load may become a move; on a device's handle, if it doesn't,
    // the release switches the device.
    void pressItem(QGraphicsSceneMouseEvent *event);
    // Shows the load being moved where a release with its picture centred on centre would put it,
    // tinted if it can't go there; a socket carries its appliances.
    void moveLoadItem(const PlacedLoad &load, QPointF centre);
    // Ends a move with the device body or the load picture centred on centre.
    void finishMove(QPointF centre);
    // Ends a move without changing the model: the item goes back to where the model has it, and
    // the mouse grab of a move whose release never arrived is dropped. A switch press is forgotten.
    void cancelMove();
    void removePreview();
    // The wire tool takes every left press, and a right press that cancels a wire in progress;
    // false for a press it leaves to the editor.
    bool pressWireTool(QGraphicsSceneMouseEvent *event);
    // Starts, cancels or finishes the wire in progress; "" for a click away from terminals.
    void clickTerminal(const QString &terminalId);
    void finishWire(const QString &end);
    void cancelWire();
    // An undo may remove the terminal a wire in progress starts at.
    void syncWireTool();
    // The highlight on the terminal under the cursor, the preview from the start to the cursor.
    void updateWireOverlays();
    [[nodiscard]] bool isTerminalFull(const QString &terminalId) const;
    [[nodiscard]] QString fullTerminalHint(const QString &terminalId) const;

    Project &m_project;
    QUndoStack &m_undoStack;
    PictureItem *m_enclosure;
    PictureItem *m_supply;
    TextItem *m_supplyText;
    QList<TextItem *> m_supplyLetters; // in the order of the supply's terminals
    QList<PictureItem *> m_barCells;
    QHash<QString, DeviceItem *> m_devices;
    QHash<QString, LoadItem *> m_loads;
    QHash<QString, WireItem *> m_wires;
    Simulation *m_simulation = nullptr;
    bool m_currentsShown = true;
    DeviceItem *m_preview = nullptr;   // while a preset is dragged over the scene
    LoadItem *m_loadPreview = nullptr; // while a load is dragged over the scene
    QString m_movingId;                // the device or load under a left press, "" if none
    QString m_switchId;                // m_movingId when the press is on its handle, else ""
    QPointF m_grabOffset; // press point minus the device body centre or the load picture centre
    QList<QAction *> m_contextActions;
    QMenu *m_contextMenu = nullptr;
    QPoint m_pressScreenPos;
    bool m_moving = false; // the press has moved beyond the start drag distance
    QGraphicsEllipseItem *m_terminalHighlight;
    QGraphicsLineItem *m_wirePreview;
    bool m_wireMode = false;
    double m_wireSection = 2.5; // mm², the toolbar default
    QString m_wireStart;
    QPointF m_wireCursor; // the last cursor position the wire tool saw
    // The cursor has been on the canvas since the tool turned on or the cursor last left.
    bool m_wireCursorOnCanvas = false;
};
