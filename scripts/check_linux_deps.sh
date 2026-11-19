#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fails if the binary needs a shared library outside the allowlist (ADR 0004).
set -euo pipefail

allowed='^(libc|libm|libdl|libpthread|librt)\.so\.|^ld-linux-x86-64\.so\.|^libX11|^libxcb|^libxkbcommon|^lib(GL|EGL|GLX|OpenGL)\.so\.|^libfontconfig\.so\.|^libfreetype\.so\.'

needed=$(readelf -d "$1" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
if [ -z "$needed" ]; then
    echo "no dynamic dependencies found"
    exit 1
fi

bad=$(echo "$needed" | grep -Ev "$allowed" || true)
if [ -n "$bad" ]; then
    echo "$bad" | sed 's/^/not allowed: /'
    exit 1
fi

echo "System libraries needed:"
echo "$needed"
