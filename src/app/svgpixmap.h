// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPixmap>
#include <QString>

class QSvgRenderer;

// Resource path of a catalog picture file, e.g. "breaker-1p.svg".
QString picturePath(const QString &picture);
// Renders the SVG at `height` logical pixels, keeping the view box aspect ratio, on a transparent
// background. Returns a null pixmap if the file is missing or not a valid SVG.
QPixmap svgPixmap(const QString &path, int height, qreal devicePixelRatio = 1.0);
// One renderer per path, created on first use and kept for the process.
QSvgRenderer &svgRenderer(const QString &path);
