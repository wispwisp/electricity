# 0021. Static xcb-util libraries on Linux

- **Status:** Accepted
- **Date:** 2026-11-20

## Context

[0004](0004-dependencies.md) and [0019](0019-build-ci-distribution.md) keep
the whole X11/xcb family dynamic, assuming `libxcb*` is present on every
desktop Linux. That holds only for libxcb itself and its extension libraries
(`libxcb-randr`, `-shm`, `-xkb`, ...), which come from one source package that
every X server and Mesa pull in. Qt's xcb plugin also needs the xcb-util
libraries (`libxcb-cursor`, `-icccm`, `-image`, `-keysyms`, `-render-util`,
and `-util` through `-image`). Those are separate packages that desktops
often do not install. The CI binary failed on a user's machine with
`libxcb-cursor.so.0: cannot open shared object file`. The dependency check
accepted it because its allowlist matched `^libxcb`.

## Decision

- The six xcb-util libraries are linked statically on Linux.
  `cmake/StaticXcbUtil.cmake` points Qt's `XCB::CURSOR`, `ICCCM`, `IMAGE`,
  `KEYSYMS`, `RENDERUTIL` and `UTIL` imported targets at the `.a` archives
  from the `-dev` packages (installed in the container and in CI). It also
  declares their inter-dependencies so static archives are ordered correctly
  (Qt lists `UTIL` before `IMAGE`, which uses it).
- `scripts/check_linux_deps.sh` lists the allowed libxcb libraries by name
  (`libxcb` plus the extensions the binary uses: glx, randr, render, shape,
  shm, sync, xfixes, xkb) instead of `^libxcb`. A new xcb library makes the
  check fail until someone decides which side it belongs to.
- This refines 0004 ("X11/xcb family stays dynamic") and 0019 (Linux release
  flags): libX11, libxcb and its extensions, and libxkbcommon stay dynamic.

## Rejected alternatives

- **Tell users to install `libxcb-cursor0` etc.** — the audience is
  non-experts, and the deliverable is one file that just runs.
- **Bundle the `.so` files next to the binary / AppImage** — no longer a
  single executable (0019).
- **Build Qt with bundled xcb-util** — Qt 6 removed that option (`-qt-xcb`
  existed only in Qt 5).

## Consequences

- The xcb-util libraries are under the MIT/X11 license; their copyright
  notices must be shown with the other third-party notices in About (F18).
- The Qt build and its CI cache are unchanged; only the app link changes.
