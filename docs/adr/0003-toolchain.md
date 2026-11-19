# 0003. Toolchain: C++20, Qt 6.12.x static, CMake

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The app targets Windows 10/11 and Linux desktops as one statically linked
executable. Qt 6.12 is an LTS release and the last Qt release that supports
Windows 10 (Qt docs: Windows 10 1809+, x86_64, MSVC 2022 or MinGW-w64 15.1).
Qt 6 requires only C++17; C++20 is the newest standard that is production
ready on all compilers Qt 6.12 is tested with.

## Decision

- **Language:** C++20. `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED
  ON`, `CMAKE_CXX_EXTENSIONS OFF`. No C++23 features, no C++20 modules, no
  compiler extensions.
- **Qt:** 6.12.x, built from source as a static library. The exact patch
  version is pinned in one place per environment: the container (`QT_VER`,
  currently `6.12.0`), the CI workflow, and `cmake/QtVersion.cmake`, which
  fails configuration if `Qt6_VERSION` differs. Updating the patch version
  updates all three in one commit. Moving to another Qt minor version
  (6.13+) requires a new ADR because it drops Windows 10.
- **Build system:** CMake, minimum version 3.22 (Ubuntu 24.04 ships 3.28),
  Ninja generator. Use Qt's CMake API (`qt_standard_project_setup`,
  `qt_add_executable`, `qt_add_resources`, `qt_add_translations`).
- **Compilers:** GCC 13+ on Linux (container: GCC 13.3). MSVC 2022 on Windows
  for release builds; MinGW-w64 GCC 15.1 must also build (CI may check it
  later). Clang is used only for tooling (clang-tidy, clang-format).
- **Warnings:** `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang, `/W4 /WX
  /permissive-` on MSVC, for project targets only.
- **Windows runtime:** static MSVC runtime (`/MT`, via
  `CMAKE_MSVC_RUNTIME_LIBRARY`), matching a Qt built with `-static-runtime`.
- **Source layout:**
  ```
  CMakeLists.txt
  cmake/            QtVersion.cmake, helpers
  src/core/         simulation + model library (no GUI)
  src/app/          Qt Widgets application
  resources/        svg/, catalog/, tutorial/, fonts/
  translations/     *.ts
  tests/core/       unit tests
  tests/gui/        Qt Test GUI scenario tests
  scripts/          quality_gates.sh, helper scripts
  docs/
  ```

## Rejected alternatives

- **C++23** — not fully supported by the MSVC 2022 / GCC 13 baseline.
- **C++17** — loses `std::span`, designated initializers, `<=>`, concepts and
  ranges, all useful in the model code, and buys nothing on these compilers.
- **qmake** — Qt 6 treats CMake as the primary build system.
- **Prebuilt Qt from the online installer** — not static; the static
  single-binary requirement needs a source build.

## Consequences

- The Qt source build is slow (around an hour). CI must cache it.
- A Qt patch upgrade is a deliberate, single-commit change.
