// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

class QSvgRenderer;

// Resource path of a catalog picture file, e.g. "breaker-1p.svg".
QString picturePath(const QString &picture);
// Resource path of a load picture file, e.g. "kettle.svg".
QString loadPicturePath(const QString &picture);
// Renders the SVG at `height` logical pixels, keeping the view box aspect ratio, on a transparent
// background. Returns a null pixmap if the file is missing or not a valid SVG.
QPixmap svgPixmap(const QString &path, int height, qreal devicePixelRatio = 1.0);
// Square icon of `size` logical pixels with pixmaps at 1x and 2x.
QIcon svgIcon(const QString &path, int size);
// One renderer per path, created on first use and kept for the process.
QSvgRenderer &svgRenderer(const QString &path);

inline constexpr qreal HandleTravel = 12.5; // picture units a handle moves down when off
// The picture with its "handle" element moved down by HandleTravel; one renderer per path.
QSvgRenderer &switchedOffRenderer(const QString &path);
