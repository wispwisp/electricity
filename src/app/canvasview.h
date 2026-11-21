// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsView>

class EditorScene;

// The editor canvas: wheel zoom around the cursor, middle-button or Space+left-button pan, and
// fitting the panel into the view, which it redoes on resizes until the user zooms or pans.
class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CanvasView(EditorScene *scene, QWidget *parent = nullptr);
    // fits the supply and the enclosure, keeping the aspect ratio
    void fitToPanel();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void fitUntilViewChanged();
    // Starts a pan or takes part in one; true if the press belongs to a pan.
    bool panPress(QMouseEvent *event);
    void updateCursor();

    bool m_viewChanged = false; // the user zoomed or panned
    bool m_spaceHeld = false;
    Qt::MouseButton m_panButton = Qt::NoButton; // NoButton while not panning
    QPoint m_panFrom;
};
