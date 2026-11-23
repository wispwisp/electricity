# CLAUDE.md

## Project
An open-source desktop app for designing and simulating residential electrical panels (single- and three-phase), aimed at non-experts in Kazakhstan and the wider CIS. It is a graphical editor written in C++ with Qt 6, built for Windows and Linux as a single statically linked executable. The app starts with a tutorial that walks the user through assembling a basic single-phase apartment panel; keeping that basic flow simple takes priority over features for complex cases. Components mirror what is sold on the local market; loads (lamps, sockets, appliances) are shown as pictures. Simulation is logical: switching, summed load currents, breaker trips on overload and short circuit, RCD trips on leakage. The UI is dark by default and ships in Russian, Kazakh and English. Architectural decisions live in docs/adr/: read the index before changing architecture and add an ADR for every new decision. Features are implemented one per session, in roadmap order.

#### Visual assets
All component and device pictures (breakers, RCDs, lamps, sockets, appliances) are original SVG files drawn for this project. Do not use manufacturer photos, logos, or images copied from catalogs or the web. Draw components as generic devices; brand and model are text data in the component catalog, not artwork. SVGs must stay readable on the dark theme and at small sizes, and are embedded into the binary through the Qt resource system.


## Claude directives about CLAUDE.md editing
Never edit this file - The most you can do if you need to write down something non-obvious is to create a document and write one line about it, but without automatic loading.


## Claude directives about code
* I don't want to go deep into code. Code architecture, code design, and microarchitecture are completely up to you. Just do your best and keep code simple and readable. When I ask for a high-level feature, you should ask only clarifying questions about those features, not code design decisions.
* Use docs/adr for non-obvious architectural decisions.
* Add comments to your code only where the logic is not obvious.
* Use red/green TDD.


## Quality gates
* Always run quality gates after finishing developement.


## Claude directives about tests
* Always test with headless mode the feature or fix after finishing developement.
* Never change a test just to make it pass, and if a test fails, say so. Correct and validate the logic, don't tweak values. Never tweak values in a test just to make it pass. Write a red test first, only after the code itself.
* Never turn off or delete tests without explicit approval

##### Use TDD
* Before writing code, write a test, run it, and verify that it fails;
* If fixing a bug, first write a test that reproduces the bug;
* Do not delete, skip, or weaken tests to make them pass;
* If a test seems wrong to you, stop and say so.

#### Claude directives about headless testing
The container has no display. Never report a feature as working without running it.

1. Logic. Simulation and model code lives in a core library with no GUI dependencies and is covered by unit tests. Run `ctest --test-dir build --output-on-failure`.
2. Feature scenario. Every user-facing feature gets a Qt Test GUI test that drives the real UI the way a user would (QTest::mouseClick, QTest::keyClicks, widgets found by objectName) and asserts on the resulting model state. GUI tests run with `QT_QPA_PLATFORM=offscreen`, set through CTest; no X server is needed. Qt is static, so each GUI test target must import the plugin: `qt_import_plugins(<target> INCLUDE Qt::QOffscreenIntegrationPlugin)`. For a Qt Quick UI also set `QT_QUICK_BACKEND=software`.
3. Look at it. Each GUI test saves a screenshot of the window (`grab()`) to `build/screenshots/<test>-<step>.png` at its key steps. Open the PNGs and check.
4. Smoke-test the real binary: `xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build/<app> & sleep 3; import -window root build/screenshots/app.png; kill $!'`, then inspect the screenshot. Use `xdotool` inside the same xvfb-run session only for ad-hoc exploration, not for tests.
5. On a crash, get a backtrace: `gdb -batch -ex run -ex bt --args <binary>`.

A feature is done only when its scenario test and the full suite pass and you have looked at the screenshots.


## Files to ignore
Ignore those files completely, as they are not your concern.
* `build_claudecode_isolation_container.sh`
* `run_claudecode_isolation_container.sh`
* `claudecode.dockerfile`
* `check_grammar.sh`

---

# Engineering Principles
- Touch only what is required. Match existing code style perfectly.
- Do not "improve" adjacent code, refactor unrelated sections, or change formatting/quotes.
- Preserve comments you don't fully understand; do not delete them.
- Every line changed must trace directly to the current request.
- Write the minimum code required to solve the task.
- Push back if a requested change is over-engineered or adds unnecessary complexity.
- Avoid speculative features, abstractions for single-use code, or "future-proofing."
- Do not "improve" adjacent code, refactor unrelated sections, or change formatting/quotes.
- Transform tasks into verifiable goals (e.g., "Write a failing test for [bug], then make it pass").
- Loop until verified: Do not declare success until you have run the relevant tests or verification steps.

