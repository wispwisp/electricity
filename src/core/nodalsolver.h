// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>

#include <complex>

using Complex = std::complex<double>;
inline constexpr int ReferenceNode = -1;
inline constexpr double Gmin = 1e-9; // S, from every node to the reference

struct Admittance
{
    int a = ReferenceNode;
    int b = ReferenceNode;
    Complex y; // S
};

// Node voltages solving Y·V = I for nodes 0..injected.size()-1, with Gmin from every node to the
// reference. A branch whose ends are the same node is ignored. injected[i] flows into node i.
[[nodiscard]] QList<Complex> solveNodeVoltages(const QList<Admittance> &branches,
                                               const QList<Complex> &injected);
