# 0018. Testing strategy and quality gates

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

Development uses red/green TDD. The container has no display. A feature is
done only when its scenario test and the full suite pass and the screenshots
have been inspected (CLAUDE.md, headless testing).

## Decision

### Test layers (all run by CTest)

1. **Core unit tests** (`tests/core/`, Qt Test, one executable per area:
   model, catalog, solver, protection, file format, parts list, diagram
   trace, tutorial conditions). Fast and deterministic; simulated time only.
2. **GUI scenario tests** (`tests/gui/`, Qt Test with `QTest::mouseClick`,
   `QTest::mouseMove`, `QTest::keyClicks`), one per user-facing feature,
   driving real widgets found by `objectName` and scene items found by id
   ([0005](0005-ui-qt-widgets-graphics-view.md)), asserting on core model
   state.
   - Environment set via CTest: `QT_QPA_PLATFORM=offscreen`.
   - Each target: `qt_import_plugins(<target> INCLUDE
     Qt::QOffscreenIntegrationPlugin)`.
   - A shared helper `saveScreenshot(test, step)` writes `grab()` to
     `build/screenshots/<test>-<step>.png`.
   - The simulation timer is replaced in tests by explicit `step()` calls
     through a test hook, so tests do not sleep.
3. **Smoke test of the real binary** under Xvfb (manual per feature, as in
   CLAUDE.md), screenshot inspected.

### Quality gates

`scripts/quality_gates.sh` runs, and must pass, before a feature is reported
done:

1. Configure and build (`cmake --preset dev`, Ninja, warnings as errors).
2. `ctest --test-dir build --output-on-failure`.
3. `clang-format --dry-run --Werror` on all project sources
   (`.clang-format`: based on Qt style, 100 columns).
4. `clang-tidy` with the repository `.clang-tidy` (bugprone-*,
   performance-*, modernize-* minus trailing-return-type, readability
   basics) on changed files, using `compile_commands.json`.
5. `cppcheck --enable=warning,performance,portability --error-exitcode=1`
   on `src/`.
6. ASan/UBSan build preset (`asan`) running the core unit tests.

CMake presets (`CMakePresets.json`): `dev` (Debug), `asan`, `release`
(static release binary).

## Rejected alternatives

- **GoogleTest / Catch2** — new dependencies; Qt Test is needed for GUI tests
  anyway, so one framework covers both.
- **Pixel-comparison screenshot tests** — brittle across font/GL changes;
  screenshots are for human/agent inspection, assertions are on the model.
- **xdotool-driven tests** — fragile, require Xvfb; reserved for
  exploration.

## Consequences

- Every feature session ends with green gates and inspected screenshots.
- Sanitizers run only on core tests to keep GUI tests fast.
