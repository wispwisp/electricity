# 0020. CI pipeline and Qt build in CI

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

[0019](0019-build-ci-distribution.md) sets the shape of CI: GitHub Actions,
a Linux and a Windows job, cached static Qt. The runners have no static Qt,
so CI must build it. On Linux the runner has many `-dev` packages that Qt's
configure would pick up, which could add runtime dependencies the container
build does not have. Windows binaries can only be checked on a Windows
runner.

## Decision

- **Modules:** CI builds only qtbase, qtsvg and qttools, and qttools without
  clang (`-no-feature-clang`). This refines "same submodules" in
  [0004](0004-dependencies.md): the app needs nothing else.
- **Linux features:** the configure flags switch on xcb and switch off glib,
  icu, zstd, libudev, cups and gtk3, and pin dbus (runtime-loaded), xcb-sm and
  the system doubleconversion, libb2, textmarkdownreader and xcb-xinput
  libraries, all to the container's values. Wayland is off because the app
  excludes the Wayland plugins (F01), not to match the container. The explicit
  flags prevent the known runtime dependencies (for example `libdbus-1.so.3`
  or `libSM` from `-dev` packages on the runner); the `NEEDED` allowlist is
  the backstop for the rest.
- **Disk:** Linux builds with `-no-pch`: the work dir peaked at 17 GB, 16.3 GB
  of it precompiled headers, and standard runners have 14 GB. Windows keeps
  precompiled headers for build speed. On both OSes the script deletes each
  module's build dir after installing it.
- **One build script:** `scripts/build_qt.cmake` builds Qt on both OSes. It
  downloads qtbase, qtsvg and qttools 6.12.0 archives from
  `download.qt.io/archive` (Qt removes superseded patch releases from
  `official_releases`) with pinned SHA-256 hashes and extracts all of them
  first, so a mirror problem fails in minutes, not after the qtbase build.
  Then it configures, builds and installs each module. Windows adds
  `-static-runtime`, installs to `C:\qt-static` and builds in the short work
  dir `C:/qw`. The script is the
  place where CI pins the Qt version ("the CI workflow" in
  [0003](0003-toolchain.md)); the `qt_version_pin_matches_ci` test checks it
  against `cmake/QtVersion.cmake`.
- **Archive extraction:** `cmake -E env LC_ALL=C.UTF-8 cmake -E tar xf`.
  qttools contains a non-ASCII file name (`café.js`) that libarchive can only
  extract under a UTF-8 locale; the script failed in the C-locale container
  without this.
- **Cache:** `actions/cache/restore` and `actions/cache/save` are separate
  steps; the key is the runner image label (`ubuntu-24.04`, `windows-2022`, written
  literally) plus a hash of `scripts/build_qt.cmake`, so a `runs-on` change
  cannot restore a Qt built on another image. Save runs
  right after the Qt build, so a later failing step keeps the long build, and
  a failed Qt build is never cached.
- **Refinement of 0019:** 0019 says the exact patch is "pinned in the
  workflow" and the cache is "keyed on the Qt version and configure flags".
  The pin lives in the script, and the script contains both the version and
  the flags, so hashing it gives the key 0019 describes.
- **Dependency checks:** `scripts/check_linux_deps.sh` is an allowlist of the
  direct `NEEDED` entries (libc family, ld-linux, X11/xcb, xkbcommon, GL/EGL,
  fontconfig, freetype). `scripts/check_windows_deps.ps1` runs
  `dumpbin /dependents` and accepts only API sets and DLLs found in
  System32; the C/C++ runtime and Qt DLLs must be linked statically even if
  they exist in System32.
- **clang-tidy:** with `CI=true` (set by GitHub Actions) `quality_gates.sh`
  checks all `src` and `tests` `.cpp` files, not only those changed against
  `main`.
- **Linux job:** builds through `scripts/build_linux_executable.sh` (release
  preset, dependency check, copy to `dist/electricity-linux-x86_64`), the same
  script used locally. Then it runs CTest, uploads the executable and
  screenshots, and runs the quality gates.
- **Windows job:** `windows-2022` is the oldest Windows image available. If
  it is retired, the fallback is `windows-2025` with
  `vcvars64.bat -vcvars_ver=14.44`. The job finds Visual Studio with
  `vswhere`, runs `vcvars64.bat`, and exports only the variables it changed to
  `GITHUB_ENV`. Configure and build are separate steps because pwsh fails a
  step only on the exit code of its last native command.
- **Actions:** only `actions/checkout`, `cache`, `upload-artifact`.

## Rejected alternatives

- **qt-everywhere archive** — a larger download and harder to pin per module.
- **Inline YAML build steps for Qt** — two copies (Linux, Windows) that
  cannot be run locally.
- **Denylist dependency check** — misses new libraries that appear on a
  runner image.
- **Third-party MSVC setup action** — an extra dependency for what
  `vcvars64.bat` already does.

## Consequences

- Changing `scripts/build_qt.cmake` in any way invalidates the Qt cache and
  triggers a full rebuild.
- The workflow was linted by reading only: downloading actionlint and
  PowerShell was not permitted in the development environment. So
  `check_windows_deps.ps1` and the Windows job are first exercised on the
  real Windows runner, and may need fixes after the first push.
- Updating the Qt patch version means editing the script (version and
  hashes) and `cmake/QtVersion.cmake` in one commit, and the container.
