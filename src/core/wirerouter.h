// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>

#include <cstdint>

enum class Exit : std::uint8_t { Up, Down };

struct RouteEnd
{
    QPointF point; // the terminal
    Exit exit;     // the side the wire leaves its own obstacle from
};

struct RouteRequest
{
    RouteEnd from;
    RouteEnd to;
};

// One polyline per request, in request order. Each starts at from.point, ends at to.point and
// has only horizontal and vertical segments. Earlier requests claim their lanes first, so the
// same input always gives the same routes. Steps outside `preferred` (the enclosure interior)
// cost more. Routes are point lists, not QPolygonF: that is a Gui type, and panelcore links only
// Qt Core (ADR 0006).
[[nodiscard]] QList<QList<QPointF>> routeWires(const QList<QRectF> &obstacles,
                                               const QRectF &preferred,
                                               const QList<RouteRequest> &requests);
