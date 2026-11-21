// SPDX-License-Identifier: GPL-3.0-or-later
#include "editorhelpers.h"

#include "app/palettewidget.h"
#include "core/catalog.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QTest>
#include <QTreeWidget>
#include <QWheelEvent>

#include <algorithm>

namespace {
// The data of the drag in progress. Qt keeps it until the drag ends, and the view hands it to the
// scene again on leave; owned by the view, so it goes away with a window closed mid-drag.
QPointer<QMimeData> dragData;
} // namespace

QTreeWidgetItem *presetItem(QTreeWidget *tree, const QString &id)
{
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = tree->topLevelItem(g);
        for (int i = 0; i < group->childCount(); ++i) {
            if (group->child(i)->data(0, PaletteWidget::PresetIdRole).toString() == id)
                return group->child(i);
        }
    }
    return nullptr;
}

const Preset &preset(const QString &id)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == id)
            return p;
    }
    qFatal("unknown preset %s", qPrintable(id));
}

QGraphicsItem *sceneItem(QGraphicsScene *scene, const QString &id)
{
    const QList<QGraphicsItem *> items = scene->items();
    const auto it = std::ranges::find_if(
            items, [&](const QGraphicsItem *item) { return item->data(0).toString() == id; });
    return it == items.end() ? nullptr : *it;
}

int countItems(QGraphicsScene *scene, const QString &id)
{
    return int(std::ranges::count_if(scene->items(), [&](const QGraphicsItem *item) {
        return item->data(0).toString() == id;
    }));
}

QPoint viewportPoint(QGraphicsView *view, QPointF scenePos)
{
    return view->mapFromScene(scenePos);
}

void mouseDrag(QWidget *viewport, QPoint from, QPoint to, Qt::MouseButton button,
               Qt::KeyboardModifiers modifiers)
{
    QTest::mousePress(viewport, button, modifiers, from);
    constexpr int moves = 5;
    for (int i = 1; i <= moves; ++i) {
        const QPoint pos = from + (to - from) * i / moves;
        QMouseEvent move(QEvent::MouseMove, pos, viewport->mapToGlobal(pos), Qt::NoButton, button,
                         modifiers);
        QApplication::sendEvent(viewport, &move);
    }
    QTest::mouseRelease(viewport, button, modifiers, to);
}

void wheel(QWidget *viewport, QPoint pos, int angleDeltaY)
{
    QWheelEvent event(pos, viewport->mapToGlobal(pos), QPoint(), QPoint(0, angleDeltaY),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewport, &event);
}

bool fullyVisible(QGraphicsView *view, const QRectF &sceneRect)
{
    return view->viewport()->rect().contains(view->mapFromScene(sceneRect).boundingRect());
}

void dragPresetOver(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                    QPoint viewportPos)
{
    delete dragData;
    dragData = palette->mimeData({ presetItem(palette, presetId) });
    dragData->setParent(view);
    QDragEnterEvent enter(QPointF(viewportPos), Qt::CopyAction, dragData, Qt::LeftButton,
                          Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &enter);
    QDragMoveEvent move(QPointF(viewportPos), Qt::CopyAction, dragData, Qt::LeftButton,
                        Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
}

void leaveDrag(QGraphicsView *view)
{
    QDragLeaveEvent leave;
    QApplication::sendEvent(view->viewport(), &leave);
    delete dragData;
}

void dropPreset(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                QPoint viewportPos)
{
    dragPresetOver(view, palette, presetId, viewportPos);
    QDropEvent drop(viewportPos, Qt::CopyAction, dragData, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &drop);
    delete dragData;
}
