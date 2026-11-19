#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

cd "$(dirname "$0")/.."

cmake --preset release
cmake --build build-release

scripts/check_linux_deps.sh build-release/electricity

mkdir -p dist
cp build-release/electricity dist/electricity-linux-x86_64
echo "Created dist/electricity-linux-x86_64"
