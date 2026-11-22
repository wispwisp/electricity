// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/wirerouter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>
#include <queue>

namespace {

constexpr double Pitch = 4.5;       // grid spacing, so neighbouring wires run 4.5 apart
constexpr double Clearance = 1.5;   // kept free around every obstacle
constexpr int BoundsPadding = 10;   // pitches of free room around everything
constexpr double OutsideFactor = 3; // step cost multiplier outside the preferred area
constexpr double BendCost = 3 * Pitch;
constexpr double SharedLaneCost = 40 * Pitch; // along or bending on a node an earlier route used
constexpr double CrossingCost = Pitch;        // straight across a node an earlier route used

enum Axis : std::uint8_t { Horizontal, Vertical };

constexpr std::uint8_t bit(Axis axis)
{
    return std::uint8_t(1 << axis);
}

// Grid nodes are numbered row by row from 0; node n is column n % columns, row n / columns.
struct Grid
{
    double x0 = 0;
    int firstColumn = 0; // grid line index of column 0: x = x0 + (firstColumn + column) * Pitch
    int firstRow = 0;    // grid line index of row 0: y = (firstRow + row) * Pitch
    int columns = 0;
    int rows = 0;
    std::vector<bool> blocked;
    std::vector<std::uint8_t> used; // per node, bit(axis) for each axis earlier routes ran along

    [[nodiscard]] int nodeCount() const { return columns * rows; }
    [[nodiscard]] bool contains(int column, int row) const
    {
        return column >= 0 && column < columns && row >= 0 && row < rows;
    }
    [[nodiscard]] int node(int column, int row) const { return row * columns + column; }
    [[nodiscard]] QPointF point(int node) const
    {
        const int column = node % columns;
        const int row = node / columns;
        return { x0 + (firstColumn + column) * Pitch, (firstRow + row) * Pitch };
    }
    [[nodiscard]] Axis axisBetween(int a, int b) const
    {
        return a / columns == b / columns ? Horizontal : Vertical;
    }
};

bool strictlyInside(const QRectF &rect, QPointF p)
{
    return rect.left() < p.x() && p.x() < rect.right() && rect.top() < p.y()
            && p.y() < rect.bottom();
}

Grid makeGrid(const QList<QRectF> &obstacles, const QList<RouteRequest> &requests)
{
    const QPointF first = requests.first().from.point;
    QRectF bounds(first, first);
    const auto include = [&](QPointF p) {
        bounds.setLeft(std::min(bounds.left(), p.x()));
        bounds.setRight(std::max(bounds.right(), p.x()));
        bounds.setTop(std::min(bounds.top(), p.y()));
        bounds.setBottom(std::max(bounds.bottom(), p.y()));
    };
    for (const QRectF &obstacle : obstacles) {
        include(obstacle.topLeft());
        include(obstacle.bottomRight());
    }
    for (const RouteRequest &request : requests) {
        include(request.from.point);
        include(request.to.point);
    }
    const double padding = BoundsPadding * Pitch;
    bounds.adjust(-padding, -padding, padding, padding);

    Grid grid;
    grid.x0 = std::fmod(first.x(), Pitch);
    grid.firstColumn = int(std::ceil((bounds.left() - grid.x0) / Pitch));
    grid.firstRow = int(std::ceil(bounds.top() / Pitch));
    grid.columns = int(std::floor((bounds.right() - grid.x0) / Pitch)) - grid.firstColumn + 1;
    grid.rows = int(std::floor(bounds.bottom() / Pitch)) - grid.firstRow + 1;
    grid.blocked.assign(grid.nodeCount(), false);
    grid.used.assign(grid.nodeCount(), 0);
    for (const QRectF &obstacle : obstacles) {
        const QRectF grown = obstacle.adjusted(-Clearance, -Clearance, Clearance, Clearance);
        for (int node = 0; node < grid.nodeCount(); ++node) {
            if (strictlyInside(grown, grid.point(node)))
                grid.blocked[node] = true;
        }
    }
    return grid;
}

// On the grid line nearest to the terminal, the first free node from it in its exit direction.
std::optional<int> endNode(const Grid &grid, const RouteEnd &end)
{
    const int column = int(std::lround((end.point.x() - grid.x0) / Pitch)) - grid.firstColumn;
    const double row = end.point.y() / Pitch - grid.firstRow;
    const bool up = end.exit == Exit::Up;
    for (int r = int(up ? std::floor(row) : std::ceil(row)); grid.contains(column, r);
         r += up ? -1 : 1) {
        if (!grid.blocked[grid.node(column, r)])
            return grid.node(column, r);
    }
    return std::nullopt;
}

// A* over (node, axis of arrival) states, numbered node * 2 + axis. Returns the nodes from start
// to goal, or an empty list if the goal can't be reached.
std::vector<int> findPath(const Grid &grid, const QRectF &preferred, int start, int goal)
{
    struct Step
    {
        int dColumn;
        int dRow;
        Axis axis;
    };
    static constexpr std::array<Step, 4> Steps{
        { { 1, 0, Horizontal }, { -1, 0, Horizontal }, { 0, 1, Vertical }, { 0, -1, Vertical } }
    };
    const auto heuristic = [&](int node) {
        return (std::abs(node % grid.columns - goal % grid.columns)
                + std::abs(node / grid.columns - goal / grid.columns))
                * Pitch;
    };
    const auto ownEnd = [&](int node) { return node == start || node == goal; };

    const int states = grid.nodeCount() * 2;
    std::vector<double> cost(states, std::numeric_limits<double>::infinity());
    std::vector<int> parent(states, -1);
    std::vector<bool> done(states, false);
    // (f, insertion order, state): equal f costs pop in insertion order.
    using Entry = std::tuple<double, std::size_t, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    std::size_t order = 0;
    const auto reach = [&](int state, int from, double g) {
        cost[state] = g;
        parent[state] = from;
        open.emplace(g + heuristic(state / 2), order++, state);
    };

    reach(start * 2 + Vertical, -1, 0);
    while (!open.empty()) {
        const int state = std::get<2>(open.top());
        open.pop();
        if (done[state])
            continue;
        done[state] = true;
        const int node = state / 2;
        const auto axis = Axis(state % 2);
        if (node == goal) {
            std::vector<int> path;
            for (int s = state; s != -1; s = parent[s])
                path.push_back(s / 2);
            std::ranges::reverse(path);
            return path;
        }
        for (const Step &step : Steps) {
            const int column = node % grid.columns + step.dColumn;
            const int row = node / grid.columns + step.dRow;
            if (!grid.contains(column, row) || grid.blocked[grid.node(column, row)])
                continue;
            const int next = grid.node(column, row);
            double g = cost[state]
                    + Pitch * (preferred.contains(grid.point(next)) ? 1 : OutsideFactor);
            if (step.axis != axis) {
                g += BendCost;
                if (grid.used[node] != 0 && !ownEnd(node))
                    g += SharedLaneCost;
            }
            if (!ownEnd(next)) {
                if ((grid.used[next] & bit(step.axis)) != 0)
                    g += SharedLaneCost;
                else if (grid.used[next] != 0)
                    g += CrossingCost;
            }
            const int nextState = next * 2 + step.axis;
            if (g < cost[nextState])
                reach(nextState, state, g);
        }
    }
    return {};
}

// The stubs to the terminals run vertically through both end nodes.
void markUsed(Grid &grid, const std::vector<int> &path)
{
    for (std::size_t i = 0; i < path.size(); ++i) {
        const Axis in = i == 0 ? Vertical : grid.axisBetween(path[i - 1], path[i]);
        const Axis out = i + 1 == path.size() ? Vertical : grid.axisBetween(path[i], path[i + 1]);
        grid.used[path[i]] |= bit(in) | bit(out);
    }
}

bool collinear(QPointF a, QPointF b, QPointF c)
{
    return (a.x() == b.x() && b.x() == c.x()) || (a.y() == b.y() && b.y() == c.y());
}

// Drops repeated points and merges collinear runs into single segments. Exact comparisons keep
// every segment exactly horizontal or vertical.
QList<QPointF> simplified(const QList<QPointF> &points)
{
    QList<QPointF> result;
    for (const QPointF &p : points) {
        if (result.size() >= 2 && collinear(result[result.size() - 2], result.last(), p))
            result.removeLast();
        if (result.isEmpty() || result.last().x() != p.x() || result.last().y() != p.y())
            result << p;
    }
    return result;
}

QList<QPointF> polyline(const Grid &grid, const RouteRequest &request, const std::vector<int> &path)
{
    const QPointF first = grid.point(path.front());
    const QPointF last = grid.point(path.back());
    QList<QPointF> points{ request.from.point, QPointF(request.from.point.x(), first.y()) };
    for (int node : path)
        points << grid.point(node);
    points << QPointF(request.to.point.x(), last.y()) << request.to.point;
    return points;
}

} // namespace

QList<QList<QPointF>> routeWires(const QList<QRectF> &obstacles, const QRectF &preferred,
                                 const QList<RouteRequest> &requests)
{
    QList<QList<QPointF>> routes;
    if (requests.isEmpty())
        return routes;
    Grid grid = makeGrid(obstacles, requests);
    for (const RouteRequest &request : requests) {
        const std::optional<int> start = endNode(grid, request.from);
        const std::optional<int> goal = endNode(grid, request.to);
        const std::vector<int> path
                = start && goal ? findPath(grid, preferred, *start, *goal) : std::vector<int>();
        if (path.empty()) {
            routes << simplified({ request.from.point,
                                   QPointF(request.from.point.x(), request.to.point.y()),
                                   request.to.point });
            continue;
        }
        markUsed(grid, path);
        routes << simplified(polyline(grid, request, path));
    }
    return routes;
}
