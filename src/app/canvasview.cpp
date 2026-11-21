// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/canvasview.h"

#include "app/editorscene.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kZoomStep = 1.15; // per wheel notch
constexpr qreal kMinZoom = 0.5;
constexpr qreal kMaxZoom = 12;
constexpr qreal kFitMargin = 16;
} // namespace

CanvasView::CanvasView(EditorScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                   | QPainter::SmoothPixmapTransform);
    setResizeAnchor(AnchorViewCenter);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void CanvasView::fitToPanel()
{
    fitInView(m_scene->panelRect().adjusted(-kFitMargin, -kFitMargin, kFitMargin, kFitMargin),
              Qt::KeepAspectRatio);
}

void CanvasView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    fitUntilViewChanged();
}

void CanvasView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitUntilViewChanged();
}

// Small screens show the window at its default size and maximize it afterwards, so the panel stays
// fitted until the user zooms or pans.
void CanvasView::fitUntilViewChanged()
{
    if (!m_viewChanged && isVisible() && !viewport()->size().isEmpty())
        fitToPanel();
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    m_viewChanged = true;
    const qreal zoom = transform().m11();
    const qreal notches = event->angleDelta().y() / qreal(QWheelEvent::DefaultDeltasPerStep);
    const qreal factor = std::clamp(std::pow(kZoomStep, notches), kMinZoom / zoom, kMaxZoom / zoom);
    const QPointF cursor = event->position();
    const QPointF anchor = mapToScene(cursor.toPoint());
    scale(factor, factor);
    // Scroll back so that the scene point under the cursor stays under it.
    const QPointF shift = mapFromScene(anchor) - cursor;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + qRound(shift.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + qRound(shift.y()));
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (!panPress(event))
        QGraphicsView::mousePressEvent(event);
}

// The press of a quick second pan drag arrives as a double click.
void CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!panPress(event))
        QGraphicsView::mouseDoubleClickEvent(event);
}

bool CanvasView::panPress(QMouseEvent *event)
{
    const bool panButton = event->button() == Qt::MiddleButton
            || (event->button() == Qt::LeftButton && m_spaceHeld);
    if (m_panButton == Qt::NoButton && panButton) {
        m_viewChanged = true;
        m_panButton = event->button();
        m_panFrom = event->position().toPoint();
        updateCursor();
    }
    if (m_panButton == Qt::NoButton)
        return false;
    event->accept();
    return true;
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panButton == Qt::NoButton) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - m_panFrom;
    m_panFrom = pos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panButton == Qt::NoButton) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }
    if (event->button() == m_panButton) {
        m_panButton = Qt::NoButton;
        updateCursor();
    }
    event->accept();
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Space) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    m_spaceHeld = true;
    updateCursor();
    event->accept();
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Space) {
        QGraphicsView::keyReleaseEvent(event);
        return;
    }
    if (!event->isAutoRepeat()) {
        m_spaceHeld = false;
        updateCursor();
    }
    event->accept();
}

// The Space release may never arrive once focus is gone.
void CanvasView::focusOutEvent(QFocusEvent *event)
{
    m_spaceHeld = false;
    updateCursor();
    QGraphicsView::focusOutEvent(event);
}

void CanvasView::updateCursor()
{
    if (m_panButton != Qt::NoButton)
        viewport()->setCursor(Qt::ClosedHandCursor);
    else if (m_spaceHeld)
        viewport()->setCursor(Qt::OpenHandCursor);
    else
        viewport()->unsetCursor();
}
