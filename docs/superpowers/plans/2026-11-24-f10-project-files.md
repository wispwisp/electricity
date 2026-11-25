# F10 Project Files Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The user keeps a panel in a `.panel` file and gets it back as it was left. The File menu has New, Open…, Open Recent, Save, Save As… and Quit. A "Save changes?" question comes before unsaved design edits would be lost. Files carry a version, and older versions get upgraded. Qt's file dialog speaks Russian and Kazakh. The local clang-tidy gate checks what the next push brings to CI.

**Architecture:**
- `panelcore`:
  - A new `projectfile` unit reads and writes ADR 0010's JSON.
  - Reading fills a fresh `Project` through the model's own add calls, so every editor rule applies to files too.
  - `Project::replace` then moves the whole contents into the window's project in one step.
  - `Simulation::reset` clears all live state and takes the file's switch settings just before that replace.
- App:
  - `MainWindow` owns the File menu, the title, the unsaved-changes question, the dialogs and the recent list. Recent files and the last folder are stored in `Settings`.
  - A new `src/app/qtstrings.cpp` registers Qt's own file dialog strings, so lupdate puts them into our `.ts` files.

**Tech Stack:** C++20, Qt 6.12.0 static (Core: `QJsonDocument`, `QSaveFile`; Widgets: `QFileDialog`, `QMessageBox`), Qt Test (core and offscreen GUI), Xvfb for the smoke run.

**Spec:** `docs/superpowers/specs/2026-11-24-f10-project-files-design.md`

## Global Constraints

- No new Qt modules, plugins or libraries.
- File format version 1, exactly the keys of the spec's example:
  - top level `format` (`"electricity-panel"`), `version` (1), `supply` `{type}`, `enclosures` (a list holding exactly one `{type}`), `devices`, `loads`, `wires`;
  - device `id`, `type`, `poles`, `parameters`, `row`, `module`;
  - load `id`, `type`, `x`, `y`, `power`, plus `socket` and `place` while plugged in;
  - wire `id`, `from`, `to`, `section`, `conductor` (`"L"`, `"N"`, `"PE"`), `length`.
  - `"on": true` is written only on devices and non-socket loads that are on. There is no `view` key.
- All of those keys are required (`devices`, `loads` and `wires` may be empty lists), except `on`, `socket` and `place`. Integers (`version`, `row`, `module`, `power`, `place`) must be JSON numbers with no fraction. Ids must match `d<n>`, `w<n>` and `l<n>` with n ≥ 1 for devices, wires and loads.
- Lists are written in `devices()`, `loads()` and `wires()` order. Reading keeps the file's order.
- Wire length must be in (0, `MaxWireLength` = 500] m, enforced by the model.
- Recent files: at most 8 (`kMaxRecentFiles`), newest first, stored as absolute paths. The last folder starts at `QStandardPaths::DocumentsLocation`.
- Shortcuts are set explicitly, because Windows has no standard SaveAs or Quit key: New Ctrl+N, Open Ctrl+O, Save Ctrl+S, Save As Ctrl+Shift+S, Quit Ctrl+Q.
- Window title is `<name>[*] — Electricity`, where `<name>` is the file name or `tr("Untitled")`. `windowModified` = `!undoStack.isClean()`. "Electricity" is not translated.
- Object names: `fileMenu`, `newAction`, `openAction`, `recentMenu`, `saveAction`, `saveAsAction`, `quitAction`; `openDialog` and `saveDialog` (QFileDialog); `saveChangesBox` with buttons `saveChangesSave`, `saveChangesDiscard`, `saveChangesCancel`; `fileErrorBox` (every open, save and missing-file message).
- Message boxes are titled "Electricity". Their buttons carry our own translated texts.
- Strings in context `MainWindow` (English source; enter these translations exactly):

  | English | Russian | Kazakh |
  |---|---|---|
  | File | Файл | Файл |
  | New | Создать | Жаңа |
  | Open… | Открыть… | Ашу… |
  | Open Recent | Недавние файлы | Соңғы файлдар |
  | Save | Сохранить | Сақтау |
  | Save As… | Сохранить как… | Басқаша сақтау… |
  | Quit | Выход | Шығу |
  | Untitled | Без имени | Атаусыз |
  | Open | Открыть | Ашу |
  | Save As | Сохранить как | Басқаша сақтау |
  | Panel projects (*.panel) | Проекты щитов (*.panel) | Қалқан жобалары (*.panel) |
  | Save changes to %1? | Сохранить изменения в %1? | %1 файлындағы өзгерістерді сақтау керек пе? |
  | Save changes to the new project? | Сохранить изменения в новом проекте? | Жаңа жобадағы өзгерістерді сақтау керек пе? |
  | Don't Save | Не сохранять | Сақтамау |
  | Cancel | Отмена | Бас тарту |
  | %1 is not an Electricity panel file, or it is damaged. | %1 — не файл щита Electricity, или он повреждён. | %1 — Electricity қалқанының файлы емес немесе ол бүлінген. |
  | %1 was saved by a newer version of Electricity. Update the app to open it. | %1 сохранён более новой версией Electricity. Обновите программу, чтобы открыть его. | %1 Electricity бағдарламасының жаңарақ нұсқасында сақталған. Оны ашу үшін бағдарламаны жаңартыңыз. |
  | Can't find %1; it was moved or deleted. | Не удаётся найти %1: файл перемещён или удалён. | %1 табылмады: файл жылжытылған немесе жойылған. |
  | Can't save %1: %2 | Не удалось сохранить %1: %2 | %1 сақталмады: %2 |

  `%1` is the file name and `%2` is `QSaveFile::errorString()`.
- Every task that adds strings runs `cmake --build build --target update_translations` and enters its rows, so `translations_complete` passes after every task. Task 8 enters Qt's file dialog strings.
- Red/green TDD. Never weaken a test. Only the two old assertions named in Task 5 change, which the owner approved on 2026-11-24.

## Review Focus

- **Russian file names with spaces** (`щит кухня.panel`): they save, open, show in the title and in the recent list. Task 3 `savesAndOpensCyrillicPath`, Task 5 `cyrillicFileName`.
- **Save As with a typed name lacking `.panel` when `<name>.panel` exists:** the dialog itself adds the suffix and asks before replacing (`setDefaultSuffix`, never appended afterwards). Task 5 `saveAsAddsSuffixBeforeAskingToReplace`.
- **Don't Save, then the chosen file fails to open:** the edited project stays, with its `*` and its undo history. Task 6 `failedOpenAfterDontSaveKeepsEdits`.
- **Save fails while closing:** the window stays open with its `*`. Task 6 `failedSaveCancelsClose`.
- **New or Open after trips and edits:** no old status-bar condition, undo step or switch survives. Task 5 `newDropsOldConditionsAndUndo`.

---

### Task 1: Local clang-tidy gate, ADR 0031, roadmap

**Files:**
- Modify: `scripts/quality_gates.sh` (gate 4)
- Create: `docs/adr/0031-project-files.md`
- Modify: `docs/adr/README.md`, `docs/roadmap.md`

**Interfaces:**
- Produces: the gate prints the files it checks, one per line.

- [ ] **Step 1: Change gate 4.**
  - Outside CI, when `git rev-parse --verify --quiet origin/main` succeeds: take the `.cpp` files under `src/` and `tests/` from `git diff --name-only --diff-filter=d "$(git merge-base HEAD origin/main)"` and `git ls-files --others --exclude-standard`.
  - Add the `.cpp` beside each changed `.h` when that file exists.
  - In CI, or with no `origin/main`, check every file, as today.
  - Print the list before running clang-tidy.
- [ ] **Step 2: Verify.**
  Run the gate-4 block alone: `bash -c "$(sed -n '/== 4\/6/,/== 5\/6/p' scripts/quality_gates.sh | sed '$d')"`.
  Expected:
  - the list holds the `.cpp` files changed in `git diff --stat origin/main` (19 commits ahead today);
  - touching `src/core/version.h` alone adds `src/core/version.cpp`;
  - clang-tidy exits 0.
- [ ] **Step 3: Write ADR 0031**, "Project files", with the template's sections. It refines 0010 and 0018, and records:
  - the format details in Global Constraints, with `view` dropped and `enclosures` as a list of one;
  - files open whole or not at all, through the model's add calls on a separate project, then `Project::replace`;
  - ids `d<n>`, `w<n>`, `l<n>`, restarting per file;
  - plugged loads are added after the unplugged ones, at their file positions;
  - `Simulation::reset` before the replace;
  - switch settings saved as `on`, with a tripped device saved off;
  - unsaved changes follow the undo stack only;
  - the upgrade step list, and the two rules for later features: a new device parameter bumps the version and its step fills the parameter in; loads keep their full power;
  - system file dialogs, and Qt's dialog strings registered in `src/app/qtstrings.cpp`;
  - the gate change (13 minutes measured for every file, 6 jobs);
  - rejected: a fresh editor per file, loading into the live project, every file in the local gate.

  Add its row to `docs/adr/README.md`.
- [ ] **Step 4: Update the roadmap.**
  - Set F10 to ◐.
  - Under F11 add: "Allow filter: File → New, Open…, Open Recent and Quit during the tutorial also go through the filter."
- [ ] **Step 5: Commit** `F10: local clang-tidy gate checks what the next push brings (ADR 0031)`.

### Task 2: Model support: wire length, replace, simulation reset

**Files:**
- Modify: `src/core/project.h`, `src/core/project.cpp`, `src/core/simulation.h`, `src/core/simulation.cpp`
- Test: `tests/core/tst_wires.cpp`, `tests/core/tst_project.cpp`, `tests/core/tst_simulation.cpp`

**Interfaces:**
- Produces:
  - `WireCheck::InvalidLength`, after `InvalidSection`: length not in (0, `MaxWireLength`]. `addWire` and `updateWire` refuse it.
  - `void Project::replace(const Project &source)`. The source uses the same catalog (`Q_ASSERT`). The project takes the source's enclosure, devices, wires, loads and the three id counters, then emits `changed()` and then `enclosureChanged()`, always.
  - `void Simulation::reset(const QStringList &on)`. It forgets every switch setting, trip, fault, heat value, wire heat state, RCD overload, danger and pending event, and sets `elapsed()` to 0. Then it stores exactly the ids in `on` as on, without checking them. It doesn't solve: the project's next `changed()` does. Callers call it right before `Project::replace`.
  - `QStringList Simulation::switchedOn() const`: the ids of the project's devices and non-socket loads for which `isOn` is true, in `devices()` then `loads()` order. Tripped devices are left out.

- [ ] **Step 1: Write the failing tests.**
  - `tst_wires::refusesWireLengthOutOfRange_data/…`: lengths 0, −1, 500.5, NaN give `InvalidLength`, and `addWire` returns `""`. 500 and 0.3 give `Ok`. `updateWire` to length 0 returns false.
  - `tst_project::replaceTakesEverything`:
    - The source has `enclosure-24`, `d7` (via `addDevice` with a kept id), a wire `w4` and a load `l3`.
    - After `replace`: `enclosure().id`, `devices()`, `wires()` and `loads()` equal the source's, and spies count one `changed` and one `enclosureChanged`.
    - The next `addDevice` without an id returns `"d8"`.
  - `tst_project::replaceWithNewProjectRestartsIds`: after building `d1`–`d3`, `replace(Project(catalog))` leaves no devices, `enclosure-12`, and the next ids `d1`, `w1`, `l1`.
  - `tst_simulation::resetForgetsLiveState`:
    - Build a circuit with a breaker feeding a lamp. Switch everything on, set a lamp fault, trip a breaker, `step(30)`.
    - `reset({"d2"})`, then `project.replace(copy)`, where the copy is a project with the same contents.
    - `isOn("d1")` is false and `isOn("d2")` is true. Every `tripCause` and `fault` is None, `elapsed()` is 0 and every `heatLevel` is 0.
    - A spy on `occurred` sees no `Cleared` event.
  - `tst_simulation::switchedOnListsWhatIsOn`: devices and loads switched on are listed in `devices()` then `loads()` order. A tripped breaker is not listed. A socket never is.
- [ ] **Step 2: Run them; they fail.**
  Run: `cmake --build build && ctest --test-dir build -R "tst_wires|tst_project|tst_simulation" --output-on-failure`
  Expected: build errors for the missing members, or the new tests FAIL.
- [ ] **Step 3: Implement** the four interfaces. `EditorScene`'s hint chain needs no case for `InvalidLength`, because the UI never makes such a wire.
- [ ] **Step 4: Run them; they pass,** with the whole core suite: `ctest --test-dir build -L core --output-on-failure`.
- [ ] **Step 5: Commit** `F10: wire length rule, Project::replace, Simulation::reset`.

### Task 3: Reading and writing `.panel` files (core)

**Files:**
- Create: `src/core/projectfile.h`, `src/core/projectfile.cpp` (add both to `panelcore` in `src/core/CMakeLists.txt`)
- Test: `tests/core/tst_projectfile.cpp`. In `tests/CMakeLists.txt`, add `add_core_test(tst_projectfile core/tst_projectfile.cpp)` and `target_compile_definitions(tst_projectfile PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/core/data")`.

**Interfaces:**
- Consumes: `Project::addDevice/addWire/addLoad`, `WireCheck::InvalidLength` (Task 2).
- Produces:
  ```cpp
  inline constexpr int ProjectFileVersion = 1;

  struct OpenResult
  {
      enum class Status : std::uint8_t { Opened, NotPanelFile, NewerVersion, Refused };
      Status status = Status::Opened;
      QString reason; // English, for tests and debugging, never shown; "" when Opened
      QStringList on; // ids saved with "on": true (devices and non-socket loads), in file order
  };

  // Reads a file's contents into project, which must be new. Unless Opened, project holds part of
  // the file and is to be thrown away.
  [[nodiscard]] OpenResult readProject(const QByteArray &data, Project &project);
  // The same from a file; a file that can't be read is NotPanelFile.
  [[nodiscard]] OpenResult openProject(const QString &path, Project &project);
  [[nodiscard]] QByteArray writeProject(const Project &project, const QStringList &on);
  // Atomic (QSaveFile). On failure: false, error = the system's reason, an existing file unchanged.
  bool saveProject(const QString &path, const Project &project, const QStringList &on,
                   QString &error);
  ```
  - `reason` starts with where the problem is: `"version"`, `"supply"`, `"enclosures"`, `"devices[<i>]"`, `"loads[<i>]"` or `"wires[<i>]"`, followed by `": "` and what is wrong.
  - Reading checks `format`, then `version`, then runs the upgrade steps, then reads supply, enclosure, devices, loads (two passes) and wires.
  - Upgrade steps: a list of functions, each turning the JSON object of version v into v+1. The list is empty while `ProjectFileVersion` is 1.
  - Loads, pass 1: add the unplugged loads in file order. Pass 2: insert each plugged load at its file index.
  - `on` on a socket is ignored.

- [ ] **Step 1: Write the failing tests.**
  - `roundTripKeepsDesignAndSwitches`:
    - Build in code: `setEnclosure("enclosure-24")`; preset `breaker-1p-c16` at row 1, module 2; a TV added unplugged first, then a socket, then the TV updated to plug into the socket at place 2 (so the appliance comes before its socket); a ceiling lamp; a wire from `d1:2` to the lamp's `L` with section 1.5 and length 22.5.
    - `on = {"d1", <lamp id>}`.
    - `readProject(writeProject(...))` into a new project gives `Opened`, equal `enclosure().id`, `devices()`, `wires()` and `loads()`, and `on` equal to the written list.
    - The next `addLoad` without an id gets the id after the highest `l<n>`.
  - `writesVersion1Format`: the written object has `format` `"electricity-panel"` and `version` 1, `supply.type` `"supply-1p"` and `enclosures` of size 1. `on` is present only on the on objects. There is no `view`. Plugged loads have `socket` and `place`, unplugged ones neither.
  - `refuses_data/refuses`: start from a small valid file (a breaker, a socket with a kettle, one wire) and change one thing per row. Expected status and reason prefix:

    | row | change | status | reason prefix |
    |---|---|---|---|
    | not JSON | `"{"` | NotPanelFile | `format` |
    | array | `[]` | NotPanelFile | `format` |
    | wrong format | `"format":"x"` | NotPanelFile | `format` |
    | no version | key removed | NotPanelFile | `version` |
    | version 0 | `0` | NotPanelFile | `version` |
    | version 1.5 | `1.5` | NotPanelFile | `version` |
    | newer | `2` | NewerVersion | `version` |
    | unknown supply | `"type":"supply-3p"` | Refused | `supply` |
    | two enclosures | list of 2 | Refused | `enclosures` |
    | unknown enclosure | `"enclosure-99"` | Refused | `enclosures` |
    | unknown device type | `"type":"relay"` | Refused | `devices[0]` |
    | row as text | `"row":"0"` | Refused | `devices[0]` |
    | row 0.5 | `0.5` | Refused | `devices[0]` |
    | device id x1 | `"id":"x1"` | Refused | `devices[0]` |
    | device id d0 | `"id":"d0"` | Refused | `devices[0]` |
    | overlapping device | second breaker on the same module | Refused | `devices[1]` |
    | duplicate id | second device with `d1` | Refused | `devices[1]` |
    | load id d9 | load `"id":"d9"` | Refused | `loads[0]` |
    | plug into non-socket | kettle's `socket` is the breaker's id | Refused | `loads[1]` |
    | socket without place | `place` removed | Refused | `loads[1]` |
    | no devices key | key removed | Refused | `devices` |
    | missing terminal | `"to":"d1:9"` | Refused | `wires[0]` |
    | length 0 | `0` | Refused | `wires[0]` |
    | length −1 | `-1` | Refused | `wires[0]` |
    | length 500.5 | `500.5` | Refused | `wires[0]` |
    | unknown conductor | `"conductor":"X"` | Refused | `wires[0]` |

  - `ignoresUnknownFields`: extra keys at the top level and in each object open fine.
  - `ignoresOnForSockets`: `"on": true` on the socket is not in `on`.
  - `savesAndOpensCyrillicPath`: `saveProject` to `<QTemporaryDir>/щит кухня.panel`, then `openProject` gives an equal model.
  - `saveFailsWithoutFolder`: a path in a folder that doesn't exist gives false and a non-empty `error`.
  - `saveFailureKeepsExistingFile`: write a file, make its folder read-only, and save again with different contents. That returns false and the bytes are unchanged. `QSKIP` on Windows, and when a file can still be created in the read-only folder (running as root).
  - `openMissingFileIsNotPanelFile`.
- [ ] **Step 2: Run them; they fail.**
  Run: `cmake --build build && ctest --test-dir build -R tst_projectfile --output-on-failure`
  Expected: build errors (no `projectfile.h`).
- [ ] **Step 3: Implement** `projectfile.cpp` to the interfaces. Write with `QJsonDocument::Indented`.
- [ ] **Step 4: Run them; they pass,** together with `-L core`.
- [ ] **Step 5: Commit** `F10: read and write .panel files`.

### Task 4: Version 1 fixture and upgrade scaffold

**Files:**
- Create: `tests/core/data/v1-tutorial.panel`
- Test: `tests/core/tst_projectfile.cpp`

**Interfaces:**
- Consumes: `saveProject`, `openProject` (Task 3); `addTutorialDevices/Wires/Loads` (`tests/gui/editorhelpers.h`), only to generate the fixture.

- [ ] **Step 1: Generate the fixture once.**
  - Add a throwaway function to a GUI test. It builds a `MainWindow` with the three tutorial helpers, switches `d1`, `d2`, `d3` and `l1` on through `simulation().setOn`, and calls `saveProject("../core/data/v1-tutorial.panel", project, simulation().switchedOn(), error)`. GUI tests run in `tests/gui`.
  - Run it, then delete the function.
  - Check the file by hand against the helper comments in `editorhelpers.h`: 5 devices, 10 loads (`l7`–`l10` plugged), 27 wires with lengths 0.3 or 15, and `on` on exactly `d1`, `d2`, `d3`, `l1`.
- [ ] **Step 2: Write the failing test** `opensVersion1Fixture`:
  - `openProject(TEST_DATA_DIR "/v1-tutorial.panel")` gives `Opened` and `on == {"d1","d2","d3","l1"}`.
  - Devices: `d1` breaker 2P `{curve C, ratedCurrent 40}` at (0,0), `d2` rcd at (0,2), `d3` C10 at (0,4), `d4` and `d5` C16 at (0,5) and (0,6).
  - Loads: 10, with `l7` tv in `l3` place 1, `l8` air-conditioner in `l4` place 1, `l9` kettle in `l5` place 1 and `l10` microwave in `l5` place 2. `loadNumber("l2")` is 2.
  - Wires: 27, the first `supply:L`→`d1:1` with section 10, conductor L and length 0.3.
  - A comment on the test says the fixture must open in every later version, and each new version adds its own fixture and step test.
- [ ] **Step 3: Run it.** It passes at once, because Task 3 already reads version 1. To prove that it can fail, change the fixture's `"version": 1` to `3`, see the test FAIL with NewerVersion, and restore it.
  Run: `ctest --test-dir build -R tst_projectfile --output-on-failure`
- [ ] **Step 4: Commit** `F10: version 1 fixture`.

### Task 5: File menu: New, Open…, Save, Save As…, Quit, window title

**Files:**
- Modify: `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `src/app/settings.h`, `src/app/settings.cpp`, `translations/*.ts`
- Modify (approved): `tests/gui/tst_mainwindow.cpp:62` (expect `"Untitled[*] — Electricity"` and `!isWindowModified()`), `tests/gui/tst_editing.cpp:116` (expect `{ "File", "Edit", "View", "Settings", "Language" }`)
- Test: `tests/gui/tst_files.cpp` (new). Add `add_gui_test(tst_files gui/tst_files.cpp)` to `tests/CMakeLists.txt`.

**Interfaces:**
- Consumes: Tasks 2–3.
- Produces:
  - `QString MainWindow::filePath() const`, `""` while untitled.
  - `QString Settings::savedFileFolder()` (Documents when unset), `void Settings::saveFileFolder(const QString &)`.
  - Private flows that Tasks 6–7 extend:
    - `newProject()`: `simulation().reset({})`, `replace(Project(catalog))`, `undoStack.clear()`, conditions cleared, `setFilePath("")`.
    - `openFile(path)`: opens into a local `Project`. On failure it shows `fileErrorBox`. On success: `reset(result.on)`, `replace`, `undoStack.clear()`, conditions cleared, `setFilePath(path)`.
    - `bool save()`: `saveAs()` while untitled.
    - `bool saveAs()`.
    - `bool saveTo(path)`: `saveProject(path, project, simulation().switchedOn(), error)`; on success `setClean()` and `setFilePath(path)`.
    - `setFilePath(path)`, which updates the title.
  - Dialogs are `QFileDialog` instances run with `exec()`:
    - `openDialog`: `AcceptOpen`, filter `tr("Panel projects (*.panel)")`.
    - `saveDialog`: `AcceptSave`, `setDefaultSuffix("panel")`.
    - Both start in `Settings::savedFileFolder()`. A successful open or save stores the file's folder.
    - The system dialog is used where there is one; offscreen, Qt's own.
  - Successful opens and saves are where Task 7 adds recent files.

- [ ] **Step 1: Write the failing tests** in `tst_files`.
  - Copy the `init`/`cleanup` pattern of `tst_simulating`: temporary settings, English, the tutorial devices and wires on the model.
  - Add a helper `onModal<T>(callback)`, built like `onLanguageDialog` in `tst_preferences.cpp`, to act on the next modal `T`.
  - Add a helper `typeInFileDialog(path)`: it finds the `QFileDialog`, `keyClicks` the path into `fileNameEdit` and presses Return.
  - Click handles through `DeviceItem::handleRect()` and buttons through `LoadItem::buttonRect()`.
  - Tests:
    - `fileMenuComesFirst`: the menu bar titles are File, Edit, View, Settings, Language. The File actions are in the order of the spec's sketch, with the shortcuts in Global Constraints. `saveAction` is enabled with no edits.
    - `saveAsWritesFileAndNamesWindow`:
      - Add the tutorial loads. Click QF1, QD1, QF2 and Lamp 1 on.
      - File → Save As… and type `<tmp>/tutorial.panel`.
      - The file exists, and `openProject` of it equals the model with `on` = `{d1,d2,d3,l1}`.
      - The title is `"tutorial.panel[*] — Electricity"`, not modified, and `filePath()` is the path.
      - Screenshot `f10-saved`.
    - `newStartsEmpty`: after File → New there are no devices, loads or wires, `enclosure-12`, title `"Untitled[*] — Electricity"`, and `simulation().switchedOn()` is empty. Screenshot `f10-new`.
    - `openRestoresDesignAndSwitches`:
      - Save as above, then New, then File → Open… and type the path.
      - The model equals the saved one. Handles of `d1`–`d3` are drawn up and `isEnergized("l1")`. The enclosure is `fullyVisible` in the canvas (fitted).
      - The undo action is disabled, the window is not modified, and `simulation().elapsed()` is 0.
      - Screenshot `f10-reopened`.
    - `saveAsAddsSuffixBeforeAskingToReplace`:
      - Save to `<tmp>/a.panel`. Change the panel by a `RemoveDeviceCommand` push. Save As… and type `<tmp>/a`.
      - Qt's "already exists" question appears (`onModal<QMessageBox>`). Answer No, then press Escape on the dialog.
      - The bytes of `a.panel` are unchanged and no file `a` exists.
    - `cyrillicFileName`: Save As `<tmp>/щит кухня.panel`. The title is `"щит кухня.panel[*] — Electricity"`. New, then open it: equal model.
    - `openRefusesDamagedAndNewerFiles`:
      - Write `<tmp>/bad.panel` as `"{"`, and `<tmp>/new.panel` as a valid file with `version` 2.
      - Open each: `fileErrorBox` shows the damaged message or the newer-version message, with the file name.
      - After OK, devices, wires and title are unchanged. Screenshot of the box: `f10-damaged`.
    - `newDropsOldConditionsAndUndo`:
      - Make an undoable edit. Add the tutorial loads, switch QF1, QD1 and QF3 on, and `setFault("l3", Fault::ShortCircuit)`, so the status bar shows QF3's trip.
      - New: the status bar reads "Ready" and the undo action is disabled.
      - Add the tutorial devices again: `isOn("d1")` is false.
    - `quitClosesWindow`: triggering `quitAction` with no edits closes the window (`isVisible()` false).
- [ ] **Step 2: Run them and the two edited old tests; they fail.**
  Run: `cmake --build build && ctest --test-dir build -R "tst_files|tst_mainwindow|tst_editing" --output-on-failure`
  Expected: build errors for `filePath`, or FAIL on the missing menu and title.
- [ ] **Step 3: Implement.** Then run `update_translations` and enter the rows used so far.
- [ ] **Step 4: Run them; they pass,** and the whole suite passes: `ctest --test-dir build --output-on-failure`. Look at the screenshots.
- [ ] **Step 5: Commit** `F10: File menu with New, Open, Save, Save As, Quit`.

### Task 6: Unsaved changes question

**Files:**
- Modify: `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `translations/*.ts`
- Test: `tests/gui/tst_files.cpp`

**Interfaces:**
- Consumes: Task 5's flows.
- Produces:
  - `bool maybeSave()`. It returns true at once when the stack is clean. Otherwise it shows `saveChangesBox`: Save returns `save()`, Don't Save returns true, Cancel returns false.
  - It is called before New, before the Open dialog, and in `closeEvent`, where false ignores the event.
  - `windowModified` follows `QUndoStack::cleanChanged`.

- [ ] **Step 1: Write the failing tests.**
  - `editMarksWindowModified`: dragging a device to another module (`mouseDrag`) sets `isWindowModified()`. Undo clears it.
  - `switchingIsNotAChange`: on a saved file, clicking QF1's handle leaves the window unmodified. File → Save then writes `on` for `d1` (open the file into a new `Project`). After one more click, `close()` returns true with no box.
  - `closeAsksAboutUnsavedChanges`:
    - With an edit, `close()` shows `saveChangesBox` with the text "Save changes to the new project?". Screenshot `f10-question` of the box.
    - Cancel: `close()` returns false and the window stays.
    - Close again and choose Don't Save: it closes.
  - `saveFromQuestionOnUntitledOpensSaveAs`: with an edit, New, then Save. The save dialog opens; Escape. The devices are unchanged, the window is modified, and `filePath()` is `""`.
  - `saveFromQuestionSavesThenContinues`: save to a file, edit, New, then Save. The file now holds the edit (open it into a new `Project`), and the window shows an empty untitled project.
  - `failedOpenAfterDontSaveKeepsEdits`: edit, then File → Open…, Don't Save, and type a damaged file's path. After the error box, the edit is still in the model, the window is modified, and Undo is enabled and undoes it.
  - `failedSaveCancelsClose`:
    - Save to `<tmp>/ro/p.panel`, edit, and make `ro` read-only.
    - `close()` → Save → `fileErrorBox` with "Can't save p.panel: …".
    - The window is still visible and modified.
    - `QSKIP` on Windows and when the folder stays writable.
- [ ] **Step 2: Run them; they fail.**
  Run: `cmake --build build && ctest --test-dir build -R tst_files --output-on-failure`
- [ ] **Step 3: Implement.** Then run `update_translations` and enter the question rows.
- [ ] **Step 4: Run them; they pass,** together with the whole suite.
- [ ] **Step 5: Commit** `F10: ask before unsaved changes are lost`.

### Task 7: Open Recent

**Files:**
- Modify: `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `src/app/settings.h`, `src/app/settings.cpp`, `translations/*.ts`
- Test: `tests/gui/tst_files.cpp`

**Interfaces:**
- Consumes: Task 5's flows, Task 6's `maybeSave()`.
- Produces:
  - `QStringList Settings::savedRecentFiles()` and `void Settings::saveRecentFiles(const QStringList &)`.
  - `MainWindow::addRecentFile(path)`: moves the absolute path to the top and keeps 8. It is called by successful opens and saves.
  - `recentMenu` is rebuilt from settings on every change: one action per file, text = file name, `statusTip` = `QDir::toNativeSeparators(path)`. It is disabled when empty.
  - A recent action checks `QFileInfo::exists` first. If the file is missing: `fileErrorBox` "Can't find …", and it is removed from the list. Otherwise: `maybeSave()`, then `openFile`.

- [ ] **Step 1: Write the failing tests.**
  - `recentListsOpenedAndSavedFiles`:
    - Save `a`, `b`, then open `a`: the menu texts are `a.panel`, `b.panel`, and the status tips are the native paths.
    - After 9 different saves, there are 8 entries with the newest first.
    - A new `MainWindow` shows the same list.
    - A fresh settings file gives a disabled `recentMenu`.
  - `openRecentOpensTheFile`: save, New, then click the entry (`openSubmenu` + `clickMenuItem`). The model equals the saved one. Screenshot `f10-recent` with the submenu open.
  - `missingRecentFileIsRemoved`: save, delete the file on disk, and click its entry. `fileErrorBox` shows "Can't find x.panel; it was moved or deleted." The entry is gone, and the project is unchanged.
  - `damagedRecentFileStays`: overwrite a recent file with `"{"` and click it. The damaged message shows, and the entry stays.
  - `dialogsStartInLastFolder`: with fresh settings, the open dialog (caught with `onModal<QFileDialog>`) shows `directory()` = Documents. After saving into `<tmp>`, both dialogs show `<tmp>`.
- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement.** Then run `update_translations` and enter the "Can't find" and "Open Recent" rows.
- [ ] **Step 4: Run them; they pass,** together with the whole suite.
- [ ] **Step 5: Commit** `F10: Open Recent`.

### Task 8: Qt's file dialog in Russian and Kazakh

**Files:**
- Create: `src/app/qtstrings.cpp` (add it to `electricity_lib` in `src/app/CMakeLists.txt`)
- Modify: `translations/electricity_ru.ts`, `translations/electricity_kk.ts`
- Test: `tests/gui/tst_files.cpp`

**Interfaces:**
- Produces: `QStringList qtTranslationSources()`, declared in the same file and not called. It exists only so lupdate extracts Qt's contexts, like `catalogTranslationSources()`.

The strings:
- 75 messages from Qt 6.12.0, exactly as Qt's sources spell them, trailing space in `"Show "` included: QFileDialog 45, QFileSystemModel 5, QLineEdit 7 (the file name field's context menu), QPlatformTheme 18 (standard buttons, used by the "already exists" Yes/No).
- They are extracted in `/tmp/claude-1000/-home-claudeuser-workspace/de89c780-f68b-45bc-9959-9cac079155e3/scratchpad/qtfd.ts`. If that file is gone, get `qtbase-everywhere-src-6.12.0.tar.xz` (URL pattern and SHA-256 in `scripts/build_qt.cmake`) and run `/opt/qt/bin/lupdate` on `src/widgets/dialogs/{qfiledialog.cpp,qfiledialog.ui,qfiledialog_p.h,qsidebar.cpp}`, `src/gui/itemmodels/{qfilesystemmodel.cpp,qfileinfogatherer.cpp}`, `src/gui/kernel/qplatformtheme.cpp` and `src/widgets/widgets/qlineedit.cpp`.
- Two messages carry a disambiguation and need `QT_TRANSLATE_NOOP3`: QFileSystemModel "Kind" ("Match OS X Finder") and "Type" ("All other platforms").
- Translations:
  - Russian may follow Qt's own `qtbase_ru.ts` (qttranslations 6.12.0, same URL pattern) for wording.
  - Kazakh is written by hand in the app's vocabulary (файл, қалта for folder, Ашу, Сақтау, Бас тарту).
  - Keyboard shortcut texts (`Alt+Left`, `Alt+Right`, `Alt+Up`) keep their English text as the translation.

- [ ] **Step 1: Write the failing tests.**
  - `fileTextsFollowLanguage`: switch to Russian through the Language menu. The File menu title is "Файл", the title is "Без имени[*] — Electricity", and the question text after an edit is "Сохранить изменения в новом проекте?".
  - `fileDialogIsTranslated_data/fileDialogIsTranslated` (rows ru, kk):
    - Switch language, then File → Open….
    - In the dialog, `lookInLabel`, `fileNameLabel` and `fileTypeLabel` texts differ from `"&Look in:"`, `"File &name:"` and `"Files of &type:"`, and the button box's Open and Cancel buttons differ from `"&Open"` and `"Cancel"`.
    - Screenshot of the dialog `f10-dialog-<lang>`. Escape.
- [ ] **Step 2: Run them; they fail** (Russian and Kazakh labels in English).
  Run: `cmake --build build && ctest --test-dir build -R "tst_files|translations" --output-on-failure`
- [ ] **Step 3: Implement** `qtstrings.cpp`. Then run `update_translations` and enter all 75 rows in ru and kk.
- [ ] **Step 4: Run them; they pass.** `translations_complete` passes. Look at both dialog screenshots: no English left except paths and shortcut texts.
- [ ] **Step 5: Commit** `F10: Qt's file dialog in Russian and Kazakh`.

### Task 9: Roadmap scenario, smoke test, done

**Files:**
- Test: `tests/gui/tst_files.cpp`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Write** `roadmapScenario`. It runs the roadmap's steps in one function, by menu clicks and dialog typing:
  - Build the tutorial panel with loads and switch QF1, QD1, QF2 and Lamp 1 on.
  - Save to a temp file, New, and open it back. Assert equal devices, wires, loads and enclosure, and `switchedOn()` == `{d1,d2,d3,l1}`.
  - Move a device and close: `saveChangesBox` shows. Don't Save.
  - Screenshot each step as `f10-roadmap-<n>`.
- [ ] **Step 2: Run it,** then the full suite: `ctest --test-dir build --output-on-failure`. All pass.
- [ ] **Step 3: Smoke test the real binary.** Run `xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build/electricity & sleep 3; xdotool key ctrl+o; sleep 1; import -window root build/screenshots/app-open.png; kill $!'`. Inspect the screenshot: Qt's open dialog over the dark window, filter "Panel projects (*.panel)". Take a second screenshot with the File menu open (`xdotool key alt+f`).
- [ ] **Step 4: Run** `scripts/quality_gates.sh`. It must pass. Look at every `f10-*` screenshot.
- [ ] **Step 5: Set F10 to ☑** in the roadmap. **Commit** `F10: roadmap scenario, mark done`.
