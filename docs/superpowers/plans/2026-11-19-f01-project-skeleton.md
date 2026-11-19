# F01 Project Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An empty, dark main window built as one static binary, with the core unit test, the offscreen GUI test with screenshots, the Xvfb smoke test and all six quality gates working.

**Architecture:** CMake project with three targets: `panelcore` (static lib, Qt Core only), `electricity_lib` (static lib with all app sources) and `electricity` (executable, `main.cpp` only). Tests are Qt Test executables registered with CTest through two helper functions, `add_core_test` and `add_gui_test`, which every later feature reuses.

**Tech Stack:** C++20, Qt 6.12.0 static (`/opt/qt`, found through the `CMAKE_PREFIX_PATH` environment variable already set in the container), CMake 3.28 + Ninja, GCC 13.3, Qt Test, clang-format/clang-tidy 18, cppcheck 2.13.

**Spec:** `docs/superpowers/specs/2026-11-19-f01-project-skeleton-design.md` (ADRs 0002–0006, 0013, 0018, 0019 behind it).

## Global Constraints

- C++20, `CMAKE_CXX_EXTENSIONS OFF`; `cmake_minimum_required(VERSION 3.22)`; Ninja generator.
- Qt exactly 6.12.0; configuration fails on any other version.
- Warnings on every project target: `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX /permissive-` (MSVC). `CMAKE_MSVC_RUNTIME_LIBRARY` = `MultiThreaded$<$<CONFIG:Debug>:Debug>` (verified on Windows in F02).
- `panelcore` links only `Qt6::Core` and never includes app headers.
- Includes are rooted at `src/`: `#include "core/version.h"`, `#include "app/mainwindow.h"`.
- Every C++ file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`; CMake, shell and YAML files start with `# SPDX-License-Identifier: GPL-3.0-or-later`. JSON and Markdown files are exempt.
- User-visible strings use `tr()`, except the product name "Electricity".
- Link only Qt6::Core, Qt6::Widgets and Qt6::Test (tests only). Svg, PrintSupport and LinguistTools arrive with F03/F04.
- No menus and no light theme (F03).
- Linux runtime dependencies of the release binary are only those in ADR 0004: no `libQt*`, `libstdc++`, `libgcc_s` or `libwayland-*`.
- Build directories: `build/` (preset `dev`), `build-asan/` (`asan`), `build-release/` (`release`).
- Run `clang-format -i` on touched C++ files before every commit. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Never edit `CLAUDE.md`. Ignore `build_claudecode_isolation_container.sh`, `run_claudecode_isolation_container.sh`, `claudecode.dockerfile`, `check_grammar.sh`.

## Review Focus

1. **Small laptop screen (1366×768, common in the target market).** A 1280×800 window plus its title bar does not fit, so the window must start maximized there. Pinned by `startsMaximizedOnlyOnSmallScreens` (Task 2).
2. **User closes the Components panel.** There is no View menu to bring it back yet, so the dock must not be closable or floatable. Pinned in `launchesMainWindow` (Task 2).
3. **A PC without DejaVu installed (any Windows machine).** The UI must still use DejaVu Sans from the embedded resource. Pinned in `appliesDarkFusionThemeAndEmbeddedFont` (Task 3): it checks that the resources exist and load as fonts, which a system-installed copy cannot fake.
4. **Kazakh text.** The app font must contain ә ғ қ ң ө ұ ү һ і (the reason DejaVu was chosen). Pinned in `appliesDarkFusionThemeAndEmbeddedFont` (Task 3).
5. **The binary on another Linux desktop.** It must not need Qt, libstdc++ or Wayland client libraries. Qt links its Wayland helper plugins by default, which pulls in `libwayland-*`, so they are excluded. Pinned by the `ldd` check (Task 4).

---

### Task 1: CMake skeleton, core library and core test

**Files:**
- Modify: `.gitignore` (line `*build-*`)
- Add to git: `docs/adr/0019-build-ci-distribution.md` (exists, currently ignored)
- Create: `CMakeLists.txt`, `CMakePresets.json`, `cmake/QtVersion.cmake`, `.clang-format`, `.clang-tidy`
- Create: `src/core/CMakeLists.txt`, `src/core/version.h`, `src/core/version.cpp`
- Create: `tests/CMakeLists.txt`, `tests/core/tst_version.cpp`

**Interfaces:**
- Produces: `QString appVersion();` in `src/core/version.h`. It returns `PROJECT_VERSION` (`project(electricity VERSION 0.1.0 LANGUAGES CXX)`), passed to `version.cpp` as a private compile definition.
- Produces: target `panelcore` with `PUBLIC` include directory `${PROJECT_SOURCE_DIR}/src`.
- Produces: CMake function `add_core_test(name source...)` in `tests/CMakeLists.txt`. It creates a Qt executable linked to `panelcore` and `Qt6::Test`, then registers a CTest test `name` with label `core`.

- [ ] **Step 1: Stop ignoring ADR 0019**

In `.gitignore`, replace the line `*build-*` with `/build-*/`.
Run: `git check-ignore -q docs/adr/0019-build-ci-distribution.md; echo $?` → `1` (not ignored).
Run: `git check-ignore -q build-asan/x; echo $?` → `0` (still ignored).

- [ ] **Step 2: Add formatting and static-analysis configs**

`.clang-format` follows Qt's own style file:

```yaml
# SPDX-License-Identifier: GPL-3.0-or-later
# Based on qtbase's _clang-format, 100 columns (ADR 0018).
Language: Cpp
BasedOnStyle: WebKit
Standard: c++20
ColumnLimit: 100
PointerAlignment: Right
AlignAfterOpenBracket: Align
AlignEscapedNewlines: DontAlign
AlignTrailingComments: true
AllowShortFunctionsOnASingleLine: Inline
AlwaysBreakTemplateDeclarations: Yes
BreakBeforeBinaryOperators: All
BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: true
  AfterFunction: true
  AfterStruct: true
  AfterEnum: false
  AfterNamespace: false
  AfterControlStatement: Never
  BeforeCatch: false
  BeforeElse: false
BreakConstructorInitializers: BeforeComma
ContinuationIndentWidth: 8
ForEachMacros: [forever, Q_FOREVER]
IncludeCategories:
  - Regex: '^<Q.*'
    Priority: 200
NamespaceIndentation: None
SpaceAfterTemplateKeyword: false
SpaceBeforeCpp11BracedList: false
SortIncludes: CaseSensitive
StatementMacros: [Q_UNUSED, Q_OBJECT]
```

`.clang-tidy`: `Checks` is `-*`, `bugprone-*`, `-bugprone-easily-swappable-parameters` (it flags every pair of adjacent `QString` parameters), `performance-*`, `modernize-*`, `-modernize-use-trailing-return-type`, plus these readability checks: `readability-container-size-empty`, `readability-else-after-return`, `readability-inconsistent-declaration-parameter-name`, `readability-misleading-indentation`, `readability-redundant-control-flow`, `readability-simplify-boolean-expr`. Set `WarningsAsErrors: '*'` and `HeaderFilterRegex: '/(src|tests)/.*\.h$'` (`.h` only, so moc output under `build/` is never matched). Begin with the YAML SPDX comment.

- [ ] **Step 3: Write the failing core test and the build files that register it**

`tests/core/tst_version.cpp`, class `TestVersion`, run with `QTEST_APPLESS_MAIN`:

```cpp
void TestVersion::versionIsSemantic()
{
    static const QRegularExpression semver(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    QVERIFY2(semver.match(appVersion()).hasMatch(), qPrintable(appVersion()));
}
```

`tests/CMakeLists.txt` defines `add_core_test` (Interfaces above), calls `add_core_test(tst_version core/tst_version.cpp)`, and registers the Qt pin checks. Each check runs the pin script in CMake script mode:

```cmake
add_test(NAME qt_version_pin_accepts_pinned
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=6.12.0 -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
add_test(NAME qt_version_pin_rejects_other
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=6.13.0 -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
set_tests_properties(qt_version_pin_rejects_other PROPERTIES
                     PASS_REGULAR_EXPRESSION "Qt 6.12.0 is required")
set_tests_properties(qt_version_pin_accepts_pinned qt_version_pin_rejects_other PROPERTIES LABELS cmake)
```

Root `CMakeLists.txt` at this step:
- `project(electricity VERSION 0.1.0 LANGUAGES CXX)`
- C++20 settings and the MSVC runtime
- `find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)`
- `qt_standard_project_setup()`
- warnings via `add_compile_options` with an `if(MSVC)` branch
- `enable_testing()`, `add_subdirectory(tests)`

Don't add `src/core` or the pin include yet.

`CMakePresets.json` (`"version": 3`, so it works with CMake 3.22):
- `dev`: Ninja, `binaryDir` `${sourceDir}/build`, `CMAKE_BUILD_TYPE=Debug`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
- `asan`: inherits `dev`, `binaryDir` `${sourceDir}/build-asan`, `CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`, `CMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`
- `release`: Ninja, `binaryDir` `${sourceDir}/build-release`, `CMAKE_BUILD_TYPE=Release`

- [ ] **Step 4: Run to verify it fails**

Run: `cmake --preset dev && cmake --build build`
Expected: build FAILS with `core/version.h: No such file or directory`.

- [ ] **Step 5: Implement the pin and the core library**

`cmake/QtVersion.cmake` sets `ELECTRICITY_QT_VERSION` to `6.12.0`. If `Qt6_VERSION` is not `VERSION_EQUAL` to it, it fails with `message(FATAL_ERROR "Qt ${ELECTRICITY_QT_VERSION} is required (ADR 0003), found ${Qt6_VERSION}")`. Add a comment saying the version is pinned together with the container and the CI workflow. Use only `set`/`if`/`message`, so it also runs with `cmake -P`. Root `CMakeLists.txt`: `include(cmake/QtVersion.cmake)` right after `find_package`, and `add_subdirectory(src/core)` before `tests`.

`src/core/CMakeLists.txt`: `add_library(panelcore STATIC version.cpp version.h)`, linking `PUBLIC Qt6::Core`. Add the include directory and the `ELECTRICITY_VERSION="${PROJECT_VERSION}"` definition from the Interfaces block.

- [ ] **Step 6: Run to verify it passes**

Run: `cmake --preset dev && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 3` (`tst_version`, `qt_version_pin_accepts_pinned`, `qt_version_pin_rejects_other`).

- [ ] **Step 7: Format and commit**

Run: `clang-format --dry-run --Werror src/core/*.{h,cpp} tests/core/*.cpp` → no output.

```bash
git add .gitignore .clang-format .clang-tidy CMakeLists.txt CMakePresets.json cmake src/core tests docs/adr/0019-build-ci-distribution.md
git commit -m "F01: CMake skeleton, panelcore and core test infrastructure"
```

---

### Task 2: Main window and GUI test infrastructure

**Files:**
- Create: `src/app/CMakeLists.txt`, `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `src/app/main.cpp`
- Create: `tests/gui/tst_mainwindow.cpp`, `tests/gui/screenshot.h`, `tests/gui/screenshot.cpp`, `tests/gui/offscreen.json`
- Modify: `CMakeLists.txt` (add `src/app`), `tests/CMakeLists.txt` (add `add_gui_test`)

**Interfaces:**
- Consumes: `panelcore` (Task 1).
- Produces: `class MainWindow : public QMainWindow` with `explicit MainWindow(QWidget *parent = nullptr);` in `src/app/mainwindow.h`. Object names: `mainWindow`; child `QGraphicsView` `canvasView` (central widget, owns a `QGraphicsScene`); `QDockWidget` `paletteDock`.
- Produces: `bool startsMaximized(const QSize &availableScreenSize);` in `src/app/mainwindow.h`. It returns true when the available screen area is smaller than 1300×860: the 1280×800 default plus a 20×60 allowance for the window frame.
- Produces: target `electricity_lib` (static, `PUBLIC` links `panelcore` and `Qt6::Widgets`) and executable `electricity`.
- Produces: `bool saveScreenshot(QWidget &widget, const QString &test, const QString &step);` in `tests/gui/screenshot.h`. It writes `widget.grab()` to `SCREENSHOT_DIR "/<test>-<step>.png"`, creating the folder if needed.
- Produces: CMake function `add_gui_test(name source...)`, which:
  - builds the sources plus `gui/screenshot.cpp`, linked to `electricity_lib` and `Qt6::Test`
  - defines `SCREENSHOT_DIR="${CMAKE_BINARY_DIR}/screenshots"`
  - calls `qt_import_plugins(<name> INCLUDE Qt::QOffscreenIntegrationPlugin)`
  - sets CTest label `gui`
  - sets `WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gui`
  - sets `ENVIRONMENT "QT_QPA_PLATFORM=offscreen:configfile=offscreen.json"`

  The config path is relative because Qt splits platform arguments on `:`, which would break a Windows drive letter.

- [ ] **Step 1: Write the failing GUI test**

`tests/gui/offscreen.json` gives the offscreen platform one 1600×900 screen at 96 dpi, dpr 1, the same size as the Xvfb smoke screen:
`{"screens":[{"name":"test","x":0,"y":0,"width":1600,"height":900,"logicalDpi":96,"logicalBaseDpi":96,"dpr":1}]}`.

`tests/gui/tst_mainwindow.cpp`, class `TestMainWindow`, run with `QTEST_MAIN`. It creates a `QApplication` because linking `Qt6::Widgets` defines `QT_WIDGETS_LIB`. End the file with `#include "tst_mainwindow.moc"`.

```cpp
void TestMainWindow::startsMaximizedOnlyOnSmallScreens_data()
{
    QTest::addColumn<QSize>("available");
    QTest::addColumn<bool>("maximized");
    QTest::newRow("full HD") << QSize(1920, 1040) << false;
    QTest::newRow("1600x900 minus taskbar") << QSize(1600, 860) << false;
    QTest::newRow("1366x768 laptop") << QSize(1366, 728) << true;
    QTest::newRow("narrow 1280x1024") << QSize(1280, 984) << true;
}

void TestMainWindow::startsMaximizedOnlyOnSmallScreens()
{
    QFETCH(QSize, available);
    QFETCH(bool, maximized);
    QCOMPARE(startsMaximized(available), maximized);
}

void TestMainWindow::launchesMainWindow()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QCOMPARE(window.windowTitle(), QStringLiteral("Electricity"));
    QCOMPARE(window.size(), QSize(1280, 800));
    QVERIFY(!(window.windowState() & Qt::WindowMaximized));

    auto *canvas = window.findChild<QGraphicsView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas);
    QCOMPARE(window.centralWidget(), canvas);
    QVERIFY(canvas->scene());

    auto *dock = window.findChild<QDockWidget *>(QStringLiteral("paletteDock"));
    QVERIFY(dock);
    QVERIFY(dock->isVisible());
    QCOMPARE(window.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
    QCOMPARE(dock->windowTitle(), QStringLiteral("Components"));
    QVERIFY(!dock->features().testFlag(QDockWidget::DockWidgetClosable));
    QVERIFY(!dock->features().testFlag(QDockWidget::DockWidgetFloatable));
    QVERIFY(dock->width() >= 200);

    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Ready"));

    QVERIFY(saveScreenshot(window, QStringLiteral("f01"), QStringLiteral("main-window")));
}
```

In `tests/CMakeLists.txt`, define `add_gui_test` and call `add_gui_test(tst_mainwindow gui/tst_mainwindow.cpp)`. In root `CMakeLists.txt`, add `add_subdirectory(src/app)` before `tests`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --preset dev && cmake --build build`
Expected: FAILS. CMake configure stops because `src/app/CMakeLists.txt` doesn't exist.

- [ ] **Step 3: Implement the main window and the executable**

`MainWindow` constructor:
- `setObjectName("mainWindow")`, title `QStringLiteral("Electricity")`
- central `canvasView` with a scene parented to the window
- `paletteDock` titled `tr("Components")`, holding an empty `QWidget`, docked left, features `DockWidgetMovable` only, initial width 240 px
- `statusBar()->showMessage(tr("Ready"))`
- `resize(1280, 800)`, then `setWindowState(Qt::WindowMaximized)` if `startsMaximized(screen()->availableGeometry().size())`

Keep the 1280×800 size and the 20×60 allowance as named constants in `mainwindow.cpp`, so `startsMaximized` and `resize` share them.

`main.cpp` creates the `QApplication` and a `MainWindow`, calls `show()` and returns `app.exec()`.

`src/app/CMakeLists.txt`:

```cmake
add_library(electricity_lib STATIC mainwindow.cpp mainwindow.h)
target_link_libraries(electricity_lib PUBLIC panelcore Qt6::Widgets)

qt_add_executable(electricity WIN32 main.cpp)
target_link_libraries(electricity PRIVATE electricity_lib)
# Release binary carries its own C++ runtime (ADR 0019).
target_link_options(electricity PRIVATE
    "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-static-libstdc++;-static-libgcc>")
# Qt links its Wayland helper plugins by default; without the Wayland platform
# plugin they are dead weight that adds libwayland-* runtime dependencies.
qt_import_plugins(electricity EXCLUDE_BY_TYPE
    wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration)
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 4`.

- [ ] **Step 5: Inspect the screenshot**

Open `build/screenshots/f01-main-window.png` with the Read tool. Expected:
- 1280×800
- an empty "Components" dock about 240 px wide on the left, with no close or float buttons
- an empty canvas filling the rest
- "Ready" in the status bar

The colours are still the default light palette.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i src/app/*.{h,cpp} tests/gui/*.{h,cpp}
git add CMakeLists.txt src/app tests
git commit -m "F01: main window with canvas, palette dock, status bar; GUI test infrastructure"
```

---

### Task 3: Dark Fusion theme and embedded DejaVu Sans

**Files:**
- Create: `src/app/theme.h`, `src/app/theme.cpp`, `src/app/appsetup.h`, `src/app/appsetup.cpp`
- Create: `resources/fonts/DejaVuSans.ttf`, `resources/fonts/DejaVuSans-Bold.ttf` (copied from `/usr/share/fonts/truetype/dejavu/`, Ubuntu `fonts-dejavu-core` 2.37), `resources/fonts/LICENSE-DejaVu.txt` (copied from `/usr/share/doc/fonts-dejavu-core/copyright`, which holds the full license text)
- Modify: `src/app/CMakeLists.txt`, `src/app/main.cpp`, `tests/gui/tst_mainwindow.cpp`

**Interfaces:**
- Consumes: `MainWindow`, `add_gui_test` (Task 2); `appVersion()` (Task 1).
- Produces: in `src/app/theme.h`, `namespace Theme { QPalette darkPalette(); void apply(); }`. `apply()` sets the Fusion style first and then the dark palette, both application-wide, because `setStyle` can reset the palette.
- Produces: `void setupApplication(QApplication &app);` in `src/app/appsetup.h`. It registers both embedded fonts from `:/fonts/`, sets "DejaVu Sans" as the application font family (keeping the platform's default point size), calls `Theme::apply()` and `app.setApplicationVersion(appVersion())`. Later features add their startup steps here.

- [ ] **Step 1: Write the failing test**

Add `initTestCase()` to `TestMainWindow`, calling `setupApplication(*qApp);`, and add:

```cpp
void TestMainWindow::appliesDarkFusionThemeAndEmbeddedFont()
{
    QCOMPARE(QApplication::style()->name(), QStringLiteral("fusion"));

    MainWindow window;
    const QColor windowColor = window.palette().color(QPalette::Window);
    QCOMPARE(windowColor, Theme::darkPalette().color(QPalette::Window));
    QVERIFY(windowColor.lightness() < 80);
    QVERIFY(window.palette().color(QPalette::WindowText).lightness() > 180);
    auto *canvas = window.findChild<QGraphicsView *>(QStringLiteral("canvasView"));
    QVERIFY(canvas->palette().color(QPalette::Base).lightness() < 80);

    // Loading succeeds only from the embedded resource, never from a system copy.
    QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DejaVuSans.ttf")) != -1);
    QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/DejaVuSans-Bold.ttf")) != -1);
    QCOMPARE(QApplication::font().family(), QStringLiteral("DejaVu Sans"));
    QCOMPARE(QFontInfo(window.font()).family(), QStringLiteral("DejaVu Sans"));

    const QFontMetrics metrics(window.font());
    for (char32_t letter : std::u32string_view(U"әғқңөұүһіЁё"))
        QVERIFY2(metrics.inFontUcs4(letter), qPrintable(QString::number(letter, 16)));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build`
Expected: FAILS with `app/appsetup.h: No such file or directory`.

- [ ] **Step 3: Implement the theme, font registration and startup**

`theme.cpp` defines the dark colours once, as named constants for window, base, text, button, accent (highlight) and disabled text, and fills every `QPalette` colour role from them. That includes the Disabled group for text roles. Choose the exact colours yourself; the test requires window and base lightness below 80 and window text above 180.

`src/app/CMakeLists.txt` adds `theme.cpp theme.h appsetup.cpp appsetup.h` to `electricity_lib` and embeds the fonts:

```cmake
qt_add_resources(electricity_lib "fonts" PREFIX "/fonts"
    BASE "${PROJECT_SOURCE_DIR}/resources/fonts"
    FILES "${PROJECT_SOURCE_DIR}/resources/fonts/DejaVuSans.ttf"
          "${PROJECT_SOURCE_DIR}/resources/fonts/DejaVuSans-Bold.ttf")
```

`main.cpp` calls `setupApplication(app)` before creating `MainWindow`.

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 4` (all `TestMainWindow` functions PASS).

- [ ] **Step 5: Inspect the screenshot**

Open `build/screenshots/f01-main-window.png`. Expected: the layout from Task 2, now with a dark window, dark canvas and light DejaVu text in the dock title and status bar.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i src/app/*.{h,cpp} tests/gui/*.cpp
git add src/app resources tests
git commit -m "F01: dark Fusion theme and embedded DejaVu Sans"
```

---

### Task 4: Quality gates, release binary and smoke test

**Files:**
- Create: `scripts/quality_gates.sh` (executable)
- Modify: `docs/roadmap.md` (F01 status), and any source files the gates flag

**Interfaces:**
- Consumes: presets, targets and CTest labels `core` / `gui` / `cmake` (Tasks 1–3).
- Produces: `scripts/quality_gates.sh`. It runs from any working directory (it `cd`s to the repo root), prints `== <n>/6 <gate>` before each gate, exits non-zero on the first failure, and ends with `All quality gates passed`. Every later feature runs it.

- [ ] **Step 1: Write the gates script**

Use `#!/usr/bin/env bash`, then the SPDX comment, then `set -euo pipefail`. The gates, in order (ADR 0018):

1. `cmake --preset dev && cmake --build build`
2. `ctest --test-dir build --output-on-failure`
3. `clang-format --dry-run --Werror` on `git ls-files -co --exclude-standard '*.cpp' '*.h'`
4. `clang-tidy -p build --quiet` on the changed `.cpp` files under `src/` and `tests/`. "Changed" means `git diff --name-only --diff-filter=d $(git merge-base HEAD main)` plus untracked files. If there is no `main` ref, check all tracked `.cpp` files under `src/` and `tests/`. If nothing changed, skip the gate with a message.
5. `cppcheck --enable=warning,performance,portability --error-exitcode=1 --library=qt --inline-suppr --quiet src/`
6. `cmake --preset asan && cmake --build build-asan && ctest --test-dir build-asan -L core --output-on-failure`

- [ ] **Step 2: Run the gates and fix what they report**

Run: `scripts/quality_gates.sh`
Fix findings in the code. If a single check is pure noise for Qt code, exclude it in `.clang-tidy` with a one-line comment saying why. Never disable a whole category, never suppress a finding without such a comment, and never weaken a test. Re-run until it passes.
Expected: final line `All quality gates passed`, exit code 0.

- [ ] **Step 3: Build the release binary and check its runtime dependencies**

Run: `cmake --preset release && cmake --build build-release && ldd build-release/electricity | grep -E 'libQt|libstdc\+\+|libgcc_s|wayland'; echo "exit=$?"`
Expected: no matched lines, `exit=1`.

- [ ] **Step 4: Smoke-test both binaries under Xvfb**

Run:
```bash
LANG=C.UTF-8 xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build/electricity & sleep 3; import -window root build/screenshots/app.png; kill $!'
LANG=C.UTF-8 xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build-release/electricity & sleep 3; import -window root build/screenshots/app-release.png; kill $!'
```
Open both PNGs. Expected: the dark 1280×800 window on the 1600×900 screen, with the dock on the left and "Ready" in the status bar. On a crash, get a backtrace with `gdb -batch -ex run -ex bt --args ./build/electricity`.

- [ ] **Step 5: Check SPDX headers**

Run: `git ls-files -co --exclude-standard '*.cpp' '*.h' '*.cmake' '*CMakeLists.txt' '*.sh' .clang-format .clang-tidy | xargs grep -L 'SPDX-License-Identifier: GPL-3.0-or-later'`
Expected: no output.

- [ ] **Step 6: Mark F01 done and commit**

In `docs/roadmap.md`, change the F01 row status from `☐` to `☑`.

```bash
git add scripts docs/roadmap.md .clang-tidy src tests
git commit -m "F01: quality gates script; mark F01 done"
```
