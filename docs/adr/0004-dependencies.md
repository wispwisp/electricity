# 0004. Dependencies: Qt only, plus build and test tooling

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

A static single binary and GPL compliance both get harder with every third-party
library. The development container (`claudecode.dockerfile`, Ubuntu 24.04)
defines the concrete toolchain that sessions build and test with.

## Decision

### Runtime (linked into the binary)

Only Qt 6.12.x, static. No other third-party libraries.

| Qt module | Used for |
|-----------|----------|
| Qt6::Core | core library: strings, JSON (`QJsonDocument`), containers |
| Qt6::Gui | painting, `QUndoStack`, `QPdfWriter`, images |
| Qt6::Widgets | main window, Graphics View editor, dialogs |
| Qt6::Svg | `QSvgRenderer` for device and load pictures |
| Qt6::SvgWidgets | `QGraphicsSvgItem` on the editor scene |
| Qt6::PrintSupport | printing the parts list and diagrams |
| Qt6::Test | unit and GUI tests only, never in the app |

Qt plugins imported into the app: the platform plugin (`xcb` on Linux,
`windows` on Windows, added by Qt's static finalization), `qsvg` image format,
`qsvgicon` icon engine. Tests also import `QOffscreenIntegrationPlugin`.

The container's Qt build is configured with
`-static -release -submodules qtbase,qtsvg,qttools -qt-zlib -qt-libpng
-qt-libjpeg -qt-pcre -qt-harfbuzz`. It also pulls in qtdeclarative (Qt
Quick/QML) as a dependency of qttools; we do **not** use it (see
[0005](0005-ui-qt-widgets-graphics-view.md)). qttranslations is not built, so
Qt's own built-in strings (standard dialog buttons) are translated by our
own `.ts` files where they appear (see [0012](0012-internationalization.md)).

**Embedded font:** DejaVu Sans (Regular, Bold), from Ubuntu's
`fonts-dejavu-core` 2.37, embedded via Qt resources and set as the
application font. It covers Russian and Kazakh Cyrillic (ә ғ қ ң ө ұ ү һ і),
gives identical rendering on Windows, Linux and in offscreen tests, and its
license (Bitstream Vera / public domain changes) is GPL compatible. The
license text ships in `resources/fonts/LICENSE-DejaVu.txt` and is shown in
About.

### Build tools

| Tool | Version in container | Role |
|------|----------------------|------|
| GCC (build-essential) | 13.3 | Linux compiler |
| CMake | 3.28 | build system (min required 3.22) |
| Ninja | distro | generator |
| Qt LinguistTools | 6.12.0 | `lupdate`, `lrelease` |
| pkg-config | distro | Qt's Linux dependency discovery |

Linux system libraries the static Qt still links dynamically: glibc,
libstdc++ (static via `-static-libstdc++ -static-libgcc`), libGL/libEGL,
X11/xcb family (`libxcb*`, `libxkbcommon*`), fontconfig, freetype. These are
present on every desktop Linux; a fully static glibc + X11 + GL binary is not
feasible.

### Quality tools

| Tool | Role |
|------|------|
| clang-format | formatting check (`.clang-format` in repo) |
| clang-tidy | static analysis on project sources |
| cppcheck | second static analyzer |
| GCC/Clang sanitizers (`-fsanitize=address,undefined`, `libclang-rt-dev`) | `asan` build preset for tests |

The container also has `afl++`, `libboost-dev`, `libssl-dev`; we do not use
them. Fuzzing the project file loader with afl++ is possible later but not
planned.

### Test and inspection tools

| Tool | Role |
|------|------|
| Qt Test + CTest | unit and GUI tests |
| Xvfb, xauth | smoke-testing the real binary without a display |
| ImageMagick `import` | screenshot of the Xvfb screen |
| xdotool | ad-hoc exploration only, never in tests |
| gdb | backtraces on crashes |
| Mesa DRI (`libgl1-mesa-dri`) | software GL for Xvfb |

### Windows (CI only)

MSVC 2022 (Visual Studio Build Tools), CMake, Ninja, and Qt 6.12.x built
from source with `-static -static-runtime -release` and the same submodules.

## Rejected alternatives

- **nlohmann/json, fmt, spdlog, GoogleTest, Catch2** — each duplicates
  something Qt already provides (QJson, QString::arg, qDebug categories,
  Qt Test). Fewer dependencies keep the static build simple.
- **Eigen for the circuit solver** — matrices are small (tens to a few
  hundred nodes); a dense Gaussian elimination in ~60 lines is enough
  (see [0008](0008-simulation-engine.md)).
- **System fonts only** — Kazakh glyph coverage and rendering would differ by
  machine, making screenshots in tests unstable.

## Consequences

- Adding any new runtime dependency requires an ADR.
- The core library depends on Qt6::Core, so unit tests link Qt Core too.
