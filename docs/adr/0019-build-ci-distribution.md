# 0019. Build, CI and distribution of the single static binary

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The deliverable is one statically linked executable for Windows and Linux.
The development container is Linux-only; Windows binaries need a Windows
build machine. Qt must be built from source as a static library.

## Decision

- **Linux build:** in the development container (Ubuntu 24.04, GCC 13,
  static Qt at `/opt/qt`). Release flags: `-static-libstdc++ -static-libgcc`;
  Qt and its bundled libs (zlib, libpng, libjpeg, pcre2, harfbuzz) are
  static. glibc, GL, X11/xcb, fontconfig and freetype stay dynamic
  ([0004](0004-dependencies.md)). Building on Ubuntu 24.04 sets the minimum
  glibc to 2.39; this is accepted for now (newer distros only) and revisited
  by ADR if users report older systems.
- **Windows build:** MSVC 2022, Qt built with `-static -static-runtime
  -release`, app built with `/MT`. Result: one `electricity.exe` with no
  DLLs beyond the OS. An application icon and version info resource are
  embedded.
- **CI:** GitHub Actions, two jobs (`ubuntu-24.04`, `windows-2022`):
  build Qt 6.12.x (exact patch pinned in the workflow) with
  `actions/cache` keyed on the Qt version and configure flags; build the app
  with the `release` preset; run the full CTest suite (offscreen on both);
  upload the binaries as artifacts. Linux CI also runs the quality gates
  script.
- **Release:** tag `vX.Y.Z` → CI attaches `electricity-linux-x86_64` and
  `electricity-windows-x86_64.exe` to a GitHub release, with notes linking
  the source tag and the Qt source archive ([0002](0002-license-gplv3.md)).
- **Version:** single source of truth in `CMakeLists.txt` `project(VERSION)`,
  shown in About.

## Rejected alternatives

- **AppImage / installer (NSIS, MSI)** — the requirement is a single
  executable; an installer adds a second artifact to maintain.
- **Cross-compiling Windows from Linux (MinGW in the container)** — possible,
  but MSVC is the primary Windows baseline and Qt's Windows CI targets it;
  a native Windows runner avoids cross-toolchain quirks.
- **Fully static Linux binary (musl)** — GL and X11 cannot reasonably be
  linked statically.

## Consequences

- The first Qt build in CI is long (≈1–2 h on Windows); later runs use the
  cache.
- Windows-specific problems are only visible through CI; sessions must push
  and check CI for features that touch platform code (only when the user
  asks to push).
