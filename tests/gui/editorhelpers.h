// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <Qt>

class PaletteWidget;
class Project;
class QComboBox;
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
struct Preset;

// The palette item of a preset, or nullptr.
QTreeWidgetItem *presetItem(QTreeWidget *tree, const QString &id);
// The palette item of a load type, or nullptr.
QTreeWidgetItem *loadItem(QTreeWidget *tree, const QString &typeId);
// A preset of the embedded catalog; aborts if there is none with that id.
const Preset &preset(const QString &id);
// The tutorial panel (ADR 0016) on the model: d1 QF1 2P C40 (0,0), d2 QD1 (0,2),
// d3 QF2 C10 (0,4), d4 QF3 C16 (0,5), d5 QF4 C16 (0,6).
void addTutorialDevices(Project &project);
// Its wires, in this order: supply:L→d1:1 10, supply:N→d1:3 10, d1:2→d2:1 10, d1:4→d2:N1 10,
// d2:2→d3:1 6, d3:1→d4:1 6, d4:1→d5:1 6, d2:N2→nbar:1 10, supply:PE→pebar:1 10.
void addTutorialWires(Project &project);
// The tutorial loads (ADR 0016) and their wires, after addTutorialDevices and addTutorialWires:
// l1 Lamp 1 (0, 270), l2 Lamp 2 (45, 270), l3 Socket 1 (94.5, 270), l4 Socket 2 (175.5, 270),
// l5 Socket 3 (256.5, 270), l6 Socket 4 (337.5, 270); l7 TV in l3 place 1, l8 air conditioner in
// l4 place 1, l9 kettle in l5 place 1, l10 microwave in l5 place 2. Wires, each with
// newWireLength() (15 m): d3:2→l1:L, nbar:2→l1:N, pebar:2→l1:PE, then l1→l2 L, N, PE, all 1.5;
// d4:2→l3:L, nbar:3→l3:N, pebar:3→l3:PE, l3→l4 L, N, PE, and d5:2→l5:L, nbar:4→l5:N,
// pebar:4→l5:PE, l5→l6 L, N, PE, all 2.5.
void addTutorialLoads(Project &project);
// The first scene item whose data(0) is id, or nullptr; and how many there are.
QGraphicsItem *sceneItem(QGraphicsScene *scene, const QString &id);
int countItems(QGraphicsScene *scene, const QString &id);
QPoint viewportPoint(QGraphicsView *view, QPointF scenePos);
// Press at from, five moves ending at to, release at to.
void mouseDrag(QWidget *viewport, QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton,
               Qt::KeyboardModifiers modifiers = {});
void wheel(QWidget *viewport, QPoint pos, int angleDeltaY);
bool fullyVisible(QGraphicsView *view, const QRectF &sceneRect);
// For orthogonal routes, where a segment's bounding rect is the segment itself. The first and the
// last segment are the stubs into the route's own terminals and are skipped.
bool crossesInside(const QPolygonF &route, const QRectF &rect);
// Whether the last segment, the stub into the route's end terminal, runs inside the rect.
bool stubCrossesInside(const QPolygonF &route, const QRectF &rect);
// Offscreen drags end at once (ADR 0025), so these send the canvas viewport the events Qt delivers
// while a preset is dragged from the palette: enter and move at viewportPos, with the palette's own
// drag data, the copy action and the left button.
void dragPresetOver(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                    QPoint viewportPos);
void leaveDrag(QGraphicsView *view);
// Enter, move and drop at viewportPos.
void dropPreset(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                QPoint viewportPos);
// The same for a load type of the palette.
void dragLoadOver(QGraphicsView *view, PaletteWidget *palette, const QString &typeId,
                  QPoint viewportPos);
void dropLoad(QGraphicsView *view, PaletteWidget *palette, const QString &typeId,
              QPoint viewportPos);
// Clicks the combo, waits for its popup view and clicks the item with that text.
void chooseComboItem(QComboBox *combo, const QString &text);
