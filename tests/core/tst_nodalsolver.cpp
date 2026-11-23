// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/nodalsolver.h"

#include <QTest>

#include <cmath>
#include <numbers>

class TestNodalSolver : public QObject
{
    Q_OBJECT

private:
    static QList<Admittance> divider()
    {
        return { { 0, ReferenceNode, 1 / 0.4 },
                 { 0, 1, 1 / 10.0 },
                 { 1, ReferenceNode, 1 / 100.0 } };
    }

private slots:
    void dividerFromNortonSource()
    {
        const QList<Complex> v = solveNodeVoltages(divider(), { 230 / 0.4, 0 });
        QCOMPARE(v.size(), 2);
        QVERIFY(std::abs(v[1] - 230.0 * 100 / 110.4) < 1e-4);
        QVERIFY(std::abs(v[0] - 230.0 * 110 / 110.4) < 1e-4);
    }

    void phaseCarriesThrough()
    {
        const double angle = -2 * std::numbers::pi / 3;
        const QList<Complex> v = solveNodeVoltages(divider(), { std::polar(230 / 0.4, angle), 0 });
        QVERIFY(std::abs(std::arg(v[1]) - angle) < 1e-9);
    }

    void floatingNodesSitAtReference()
    {
        QList<Admittance> branches = divider();
        branches.append({ 3, 4, 1.0 });
        const QList<Complex> v = solveNodeVoltages(branches, { 230 / 0.4, 0, 0, 0, 0 });
        QCOMPARE(v.size(), 5);
        QVERIFY(std::abs(v[1] - 230.0 * 100 / 110.4) < 1e-4);
        QVERIFY(std::abs(v[2]) < 1e-9);
        QVERIFY(std::abs(v[3]) < 1e-9);
        QVERIFY(std::abs(v[4]) < 1e-9);
    }

    void branchWithinOneNodeIsIgnored()
    {
        QList<Admittance> branches = divider();
        branches.append({ 1, 1, 5.0 });
        const QList<Complex> v = solveNodeVoltages(branches, { 230 / 0.4, 0 });
        QVERIFY(std::abs(v[1] - 230.0 * 100 / 110.4) < 1e-4);
        QVERIFY(std::abs(v[0] - 230.0 * 110 / 110.4) < 1e-4);
    }

    void noNodes() { QVERIFY(solveNodeVoltages({}, {}).isEmpty()); }
};

QTEST_APPLESS_MAIN(TestNodalSolver)
#include "tst_nodalsolver.moc"
