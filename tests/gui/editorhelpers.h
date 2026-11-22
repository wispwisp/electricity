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
