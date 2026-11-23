// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/nodalsolver.h"

#include <utility>
#include <vector>

QList<Complex> solveNodeVoltages(const QList<Admittance> &branches, const QList<Complex> &injected)
{
    const auto n = static_cast<std::size_t>(injected.size());
    std::vector<Complex> m(n * n);
    std::vector<Complex> rhs(injected.begin(), injected.end());
    const auto at
            = [&m, n](std::size_t row, std::size_t col) -> Complex & { return m[row * n + col]; };

    for (const Admittance &branch : branches) {
        if (branch.a == branch.b)
            continue;
        const bool hasA = branch.a != ReferenceNode;
        const bool hasB = branch.b != ReferenceNode;
        const auto a = static_cast<std::size_t>(branch.a);
        const auto b = static_cast<std::size_t>(branch.b);
        if (hasA)
            at(a, a) += branch.y;
        if (hasB)
            at(b, b) += branch.y;
        if (hasA && hasB) {
            at(a, b) -= branch.y;
            at(b, a) -= branch.y;
        }
    }
    for (std::size_t i = 0; i < n; ++i)
        at(i, i) += Gmin;

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < n; ++row) {
            if (std::abs(at(row, col)) > std::abs(at(pivot, col)))
                pivot = row;
        }
        if (pivot != col) {
            for (std::size_t k = col; k < n; ++k)
                std::swap(at(pivot, k), at(col, k));
            std::swap(rhs[pivot], rhs[col]);
        }
        for (std::size_t row = col + 1; row < n; ++row) {
            if (at(row, col) == Complex(0))
                continue;
            const Complex factor = at(row, col) / at(col, col);
            for (std::size_t k = col; k < n; ++k)
                at(row, k) -= factor * at(col, k);
            rhs[row] -= factor * rhs[col];
        }
    }

    QList<Complex> voltages(injected.size());
    for (std::size_t i = n; i-- > 0;) {
        Complex sum = rhs[i];
        for (std::size_t k = i + 1; k < n; ++k)
            sum -= at(i, k) * voltages[static_cast<qsizetype>(k)];
        voltages[static_cast<qsizetype>(i)] = sum / at(i, i);
    }
    return voltages;
}
