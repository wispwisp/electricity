// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <Qt>

class PaletteWidget;
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
struct Preset;

// The palette item of a preset, or nullptr.
QTreeWidgetItem *presetItem(QTreeWidget *tree, const QString &id);
// A preset of the embedded catalog; aborts if there is none with that id.
const Preset &preset(const QString &id);
// The first scene item whose data(0) is id, or nullptr; and how many there are.
QGraphicsItem *sceneItem(QGraphicsScene *scene, const QString &id);
int countItems(QGraphicsScene *scene, const QString &id);
QPoint viewportPoint(QGraphicsView *view, QPointF scenePos);
// Press at from, five moves ending at to, release at to.
void mouseDrag(QWidget *viewport, QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton,
               Qt::KeyboardModifiers modifiers = {});
void wheel(QWidget *viewport, QPoint pos, int angleDeltaY);
bool fullyVisible(QGraphicsView *view, const QRectF &sceneRect);
// Offscreen drags end at once (ADR 0025), so these send the canvas viewport the events Qt delivers
// while a preset is dragged from the palette: enter and move at viewportPos, with the palette's own
// drag data, the copy action and the left button.
void dragPresetOver(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                    QPoint viewportPos);
void leaveDrag(QGraphicsView *view);
// Enter, move and drop at viewportPos.
void dropPreset(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                QPoint viewportPos);
