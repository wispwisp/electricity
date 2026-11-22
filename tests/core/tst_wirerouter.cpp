// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/wirerouter.h"

#include <QtTest>

#include <algorithm>

namespace {

// 2-module device bodies from y 48 to 138, with the 9-unit designation band above.
constexpr QRectF DeviceA(32, 39, 36, 99);
constexpr QRectF DeviceB(68, 39, 36, 99);
constexpr QRectF DeviceC(104, 39, 36, 99);
constexpr QRectF Preferred(12, 12, 256, 220);

RouteRequest request(QPointF from, Exit fromExit, QPointF to, Exit toExit)
{
    return { { from, fromExit }, { to, toExit } };
}

bool orthogonal(const QList<QPointF> &route)
{
    for (qsizetype i = 1; i < route.size(); ++i) {
        if (route[i].x() != route[i - 1].x() && route[i].y() != route[i - 1].y())
            return false;
    }
    return true;
}

// For orthogonal routes, where a segment's bounding rect is the segment itself. The first and the
// last segment are the stubs into the route's own terminals and are skipped.
bool crossesInside(const QList<QPointF> &route, const QRectF &rect)
{
    const QRectF inside = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    for (qsizetype i = 2; i + 1 < route.size(); ++i) {
        const QRectF segment = QRectF(route[i - 1], route[i]).normalized();
        if (segment.left() < inside.right() && segment.right() > inside.left()
            && segment.top() < inside.bottom() && segment.bottom() > inside.top())
            return true;
    }
    return false;
}

bool overlaps(const QList<QPointF> &a, const QList<QPointF> &b)
{
    for (qsizetype i = 1; i < a.size(); ++i) {
        const QRectF s = QRectF(a[i - 1], a[i]).normalized();
        for (qsizetype j = 1; j < b.size(); ++j) {
            const QRectF t = QRectF(b[j - 1], b[j]).normalized();
            const bool sameRow = s.height() == 0 && t.height() == 0 && s.top() == t.top()
                    && std::min(s.right(), t.right()) > std::max(s.left(), t.left());
            const bool sameColumn = s.width() == 0 && t.width() == 0 && s.left() == t.left()
                    && std::min(s.bottom(), t.bottom()) > std::max(s.top(), t.top());
            if (sameRow || sameColumn)
                return true;
        }
    }
    return false;
}

// From A's bottom-left terminal to B's top-left one: the route has to go around A.
QList<QPointF> bottomToTopRoute()
{
    return routeWires({ DeviceA, DeviceB }, Preferred,
                      { request({ 41, 130 }, Exit::Down, { 77, 56 }, Exit::Up) })
            .value(0);
}

} // namespace

class TestWireRouter : public QObject
{
    Q_OBJECT
private slots:
    void endsOnTerminalsAndIsOrthogonal();
    void leavesDownwardFromBottomTerminals();
    void avoidsObstacles();
    void routesAroundOwnObstacle();
    void parallelWiresDoNotOverlap();
    void staysInsidePreferredArea();
    void isDeterministic();
    void fallsBackWhenBoxedIn();
};

void TestWireRouter::endsOnTerminalsAndIsOrthogonal()
{
    const QList<QList<QPointF>> routes
            = routeWires({ DeviceA, DeviceB }, Preferred,
                         { request({ 41, 56 }, Exit::Up, { 77, 56 }, Exit::Up) });
    QCOMPARE(routes.size(), 1);
    const QList<QPointF> &route = routes.first();
    QVERIFY(route.size() >= 3);
    QCOMPARE(route.first(), QPointF(41, 56));
    QCOMPARE(route.last(), QPointF(77, 56));
    QVERIFY(orthogonal(route));
    QVERIFY(route[1].y() < 56);
    QVERIFY(route[route.size() - 2].y() < 56);
}

void TestWireRouter::leavesDownwardFromBottomTerminals()
{
    const QList<QPointF> route = bottomToTopRoute();
    QVERIFY(route.size() >= 3);
    QVERIFY(route[1].y() > 130);
    QVERIFY(orthogonal(route));
}

void TestWireRouter::avoidsObstacles()
{
    const QList<QPointF> route = bottomToTopRoute();
    QVERIFY(route.size() >= 3);
    QVERIFY(!crossesInside(route, DeviceA));
    QVERIFY(!crossesInside(route, DeviceB));
    // crossesInside skips the stubs, so check that they end outside their own bodies.
    QVERIFY(!DeviceA.contains(route[1]));
    QVERIFY(!DeviceB.contains(route[route.size() - 2]));
}

void TestWireRouter::routesAroundOwnObstacle()
{
    const QList<QList<QPointF>> routes = routeWires(
            { DeviceA }, Preferred, { request({ 41, 56 }, Exit::Up, { 41, 130 }, Exit::Down) });
    QCOMPARE(routes.size(), 1);
    QVERIFY(routes.first().size() >= 3);
    QVERIFY(orthogonal(routes.first()));
    QVERIFY(!crossesInside(routes.first(), DeviceA));
    QVERIFY(!DeviceA.contains(routes.first()[1]));
    QVERIFY(!DeviceA.contains(routes.first()[routes.first().size() - 2]));
}

void TestWireRouter::parallelWiresDoNotOverlap()
{
    const QList<QList<QPointF>> routes
            = routeWires({ DeviceA, DeviceB, DeviceC }, Preferred,
                         { request({ 41, 56 }, Exit::Up, { 113, 56 }, Exit::Up),
                           request({ 59, 56 }, Exit::Up, { 131, 56 }, Exit::Up) });
    QCOMPARE(routes.size(), 2);
    QVERIFY(routes[0].size() >= 2);
    QVERIFY(routes[1].size() >= 2);
    QVERIFY(!overlaps(routes[0], routes[1]));
}

void TestWireRouter::staysInsidePreferredArea()
{
    const QList<QPointF> route = bottomToTopRoute();
    QVERIFY(!route.isEmpty());
    QVERIFY(std::ranges::all_of(route, [](QPointF p) { return Preferred.contains(p); }));
}

void TestWireRouter::isDeterministic()
{
    const QList<QRectF> obstacles{ DeviceA, DeviceB, DeviceC };
    const QList<RouteRequest> requests{
        request({ 41, 56 }, Exit::Up, { 113, 56 }, Exit::Up),
        request({ 59, 56 }, Exit::Up, { 131, 56 }, Exit::Up),
        request({ 41, 130 }, Exit::Down, { 95, 130 }, Exit::Down),
    };
    const QList<QList<QPointF>> first = routeWires(obstacles, Preferred, requests);
    QCOMPARE(first.size(), 3);
    QCOMPARE(routeWires(obstacles, Preferred, requests), first);
}

void TestWireRouter::fallsBackWhenBoxedIn()
{
    // The from terminal sits in its own small body inside a closed ring of walls, with free room
    // between the body and the ring: its end node is found, but no path leads out of the ring.
    const QList<QRectF> obstacles{
        QRectF(36, 50, 10, 10), // the terminal's own body
        QRectF(10, 20, 62, 6),  // top wall
        QRectF(10, 84, 62, 6),  // bottom wall
        QRectF(10, 20, 6, 70),  // left wall
        QRectF(66, 20, 6, 70),  // right wall
    };
    const QList<QList<QPointF>> routes = routeWires(
            obstacles, Preferred, { request({ 41, 56 }, Exit::Up, { 122, 110 }, Exit::Down) });
    const QList<QPointF> fallback{ QPointF(41, 56), QPointF(41, 110), QPointF(122, 110) };
    QCOMPARE(routes, QList<QList<QPointF>>{ fallback });
}

QTEST_APPLESS_MAIN(TestWireRouter)
#include "tst_wirerouter.moc"
