// SPDX-License-Identifier: GPL-3.0-or-later
#include "editorhelpers.h"

#include "app/palettewidget.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
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

QTreeWidgetItem *paletteItem(QTreeWidget *tree, int role, const QString &id)
{
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = tree->topLevelItem(g);
        for (int i = 0; i < group->childCount(); ++i) {
            if (group->child(i)->data(0, role).toString() == id)
                return group->child(i);
        }
    }
    return nullptr;
}

void dragItemOver(QGraphicsView *view, PaletteWidget *palette, QTreeWidgetItem *item,
                  QPoint viewportPos)
{
    delete dragData;
    QVERIFY2(item, "no such palette item");
    dragData = palette->mimeData({ item });
    dragData->setParent(view);
    QDragEnterEvent enter(QPointF(viewportPos), Qt::CopyAction, dragData, Qt::LeftButton,
                          Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &enter);
    QDragMoveEvent move(QPointF(viewportPos), Qt::CopyAction, dragData, Qt::LeftButton,
                        Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
}

// For an orthogonal segment, whose bounding rect is the segment itself.
bool segmentInside(QPointF from, QPointF to, const QRectF &rect)
{
    const QRectF inside = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    const QRectF segment = QRectF(from, to).normalized();
    return segment.left() < inside.right() && segment.right() > inside.left()
            && segment.top() < inside.bottom() && segment.bottom() > inside.top();
}

void dropDragged(QGraphicsView *view, QPoint viewportPos)
{
    QDropEvent drop(viewportPos, Qt::CopyAction, dragData, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &drop);
    delete dragData;
}
} // namespace

QTreeWidgetItem *presetItem(QTreeWidget *tree, const QString &id)
{
    return paletteItem(tree, PaletteWidget::PresetIdRole, id);
}

QTreeWidgetItem *loadItem(QTreeWidget *tree, const QString &typeId)
{
    return paletteItem(tree, PaletteWidget::LoadTypeRole, typeId);
}

const Preset &preset(const QString &id)
{
    for (const Preset &p : embeddedCatalog().presets) {
        if (p.id == id)
            return p;
    }
    qFatal("unknown preset %s", qPrintable(id));
}

void addTutorialDevices(Project &project)
{
    struct Placed
    {
        const char *preset;
        int module;
    };
    for (const Placed &device : { Placed{ "breaker-2p-c40", 0 }, Placed{ "rcd-2p-40-30", 2 },
                                  Placed{ "breaker-1p-c10", 4 }, Placed{ "breaker-1p-c16", 5 },
                                  Placed{ "breaker-1p-c16", 6 } })
        QVERIFY(!project.addDevice(deviceFromPreset(preset(device.preset), 0, device.module))
                         .isEmpty());
}

void addTutorialWires(Project &project)
{
    struct Ends
    {
        const char *from;
        const char *to;
        double section;
    };
    for (const Ends &ends :
         { Ends{ "supply:L", "d1:1", 10 }, Ends{ "supply:N", "d1:3", 10 },
           Ends{ "d1:2", "d2:1", 10 }, Ends{ "d1:4", "d2:N1", 10 }, Ends{ "d2:2", "d3:1", 6 },
           Ends{ "d3:1", "d4:1", 6 }, Ends{ "d4:1", "d5:1", 6 }, Ends{ "d2:N2", "nbar:1", 10 },
           Ends{ "supply:PE", "pebar:1", 10 } }) {
        const QString from = QString::fromLatin1(ends.from);
        const QString to = QString::fromLatin1(ends.to);
        QVERIFY(!project.addWire(Wire{ {}, from, to, ends.section, project.conductorFor(from, to) })
                         .isEmpty());
    }
}

void addTutorialLoads(Project &project)
{
    struct Load
    {
        const char *type;
        QPointF position;
        const char *socket;
        int place;
    };
    for (const Load &load :
         { Load{ "ceiling-lamp", { 0, 270 }, "", 0 }, Load{ "ceiling-lamp", { 45, 270 }, "", 0 },
           Load{ "socket", { 94.5, 270 }, "", 0 }, Load{ "socket", { 175.5, 270 }, "", 0 },
           Load{ "socket", { 256.5, 270 }, "", 0 }, Load{ "socket", { 337.5, 270 }, "", 0 },
           Load{ "tv", {}, "l3", 1 }, Load{ "air-conditioner", {}, "l4", 1 },
           Load{ "kettle", {}, "l5", 1 }, Load{ "microwave", {}, "l5", 2 } }) {
        const LoadType *type = project.catalog().loadType(QString::fromLatin1(load.type));
        QVERIFY(type);
        PlacedLoad placed = loadFromType(*type, load.position);
        placed.socket = QString::fromLatin1(load.socket);
        placed.place = load.place;
        QVERIFY(!project.addLoad(placed).isEmpty());
    }
    struct Ends
    {
        const char *from;
        const char *to;
        double section;
    };
    for (const Ends &ends :
         { Ends{ "d3:2", "l1:L", 1.5 }, Ends{ "nbar:2", "l1:N", 1.5 },
           Ends{ "pebar:2", "l1:PE", 1.5 }, Ends{ "l1:L", "l2:L", 1.5 },
           Ends{ "l1:N", "l2:N", 1.5 }, Ends{ "l1:PE", "l2:PE", 1.5 }, Ends{ "d4:2", "l3:L", 2.5 },
           Ends{ "nbar:3", "l3:N", 2.5 }, Ends{ "pebar:3", "l3:PE", 2.5 },
           Ends{ "l3:L", "l4:L", 2.5 }, Ends{ "l3:N", "l4:N", 2.5 }, Ends{ "l3:PE", "l4:PE", 2.5 },
           Ends{ "d5:2", "l5:L", 2.5 }, Ends{ "nbar:4", "l5:N", 2.5 },
           Ends{ "pebar:4", "l5:PE", 2.5 }, Ends{ "l5:L", "l6:L", 2.5 },
           Ends{ "l5:N", "l6:N", 2.5 }, Ends{ "l5:PE", "l6:PE", 2.5 } }) {
        const QString from = QString::fromLatin1(ends.from);
        const QString to = QString::fromLatin1(ends.to);
        Wire wire{ {}, from, to, ends.section, project.conductorFor(from, to) };
        wire.length = project.newWireLength(from, to);
        QVERIFY(!project.addWire(wire).isEmpty());
    }
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

bool crossesInside(const QPolygonF &route, const QRectF &rect)
{
    for (qsizetype i = 2; i + 1 < route.size(); ++i) {
        if (segmentInside(route[i - 1], route[i], rect))
            return true;
    }
    return false;
}

bool stubCrossesInside(const QPolygonF &route, const QRectF &rect)
{
    return route.size() >= 2 && segmentInside(route[route.size() - 2], route.last(), rect);
}

void dragPresetOver(QGraphicsView *view, PaletteWidget *palette, const QString &presetId,
                    QPoint viewportPos)
{
    dragItemOver(view, palette, presetItem(palette, presetId), viewportPos);
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
    dropDragged(view, viewportPos);
}

void dragLoadOver(QGraphicsView *view, PaletteWidget *palette, const QString &typeId,
                  QPoint viewportPos)
{
    dragItemOver(view, palette, loadItem(palette, typeId), viewportPos);
}

void dropLoad(QGraphicsView *view, PaletteWidget *palette, const QString &typeId,
              QPoint viewportPos)
{
    dragLoadOver(view, palette, typeId, viewportPos);
    dropDragged(view, viewportPos);
}

void chooseComboItem(QComboBox *combo, const QString &text)
{
    QTest::mouseClick(combo, Qt::LeftButton);
    QAbstractItemView *view = combo->view();
    QTRY_VERIFY(view->isVisible());
    const int row = combo->findText(text);
    QVERIFY(row >= 0);
    const QPoint itemPos = view->visualRect(view->model()->index(row, 0)).center();
    // The popup only accepts a click after the pointer has moved onto the item, and a move to
    // where the pointer already is does nothing.
    QTest::mouseMove(view->viewport(), itemPos + QPoint(1, 0));
    QTest::mouseMove(view->viewport(), itemPos);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, itemPos);
}
