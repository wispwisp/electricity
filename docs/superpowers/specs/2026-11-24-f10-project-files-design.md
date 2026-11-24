# F10 — Project files

Date: 2026-11-24 · Status: approved by the owner (2026-11-24)

Roadmap item: [F10](../../roadmap.md#f10-project-files).
ADRs: [0006](../../adr/0006-core-and-app-layering.md),
[0010](../../adr/0010-project-file-format.md),
[0011](../../adr/0011-undo-redo.md),
[0012](../../adr/0012-internationalization.md),
[0018](../../adr/0018-testing-and-quality-gates.md),
[0024](../../adr/0024-placed-devices-and-designations.md),
[0027](../../adr/0027-wires-roles-colours-routes.md),
[0028](../../adr/0028-loads-plugging-cables.md),
[0029](../../adr/0029-live-simulation-state-and-contacts.md),
[0030](../../adr/0030-protection-faults-and-time.md).
Previous feature: [F09 design](2026-11-23-f09-protection-faults-design.md).

## Intent

One of the reasons the app exists is to "create a design for a future real
installation" (product design). Today a panel disappears when the window
closes. F10 lets the user keep a panel in a `.panel` file, reopen it later,
also in a newer version of the app, and carry on. A reopened panel looks the
way it was left: handles up or down, lamps on or off (owner's choice A).

The basic flow stays as simple as F05–F09: a File menu that works like the
one in every other desktop program, and a "Save changes?" question before
anything unsaved would be lost. Nothing else to learn.

F10 also fixes the local clang-tidy gate first (roadmap): on `main` it only
checks uncommitted files, while CI checks every file, so F09's committed code
passed locally and failed CI.

## Goals

### What the user sees

```
┌ File  Edit  View  Settings  Language ─────── kitchen.panel* — Electricity ┐
│┌───────────────────────────────┐                                        │
││ New                    Ctrl+N │                                        │
││ Open…                  Ctrl+O │                                        │
││ Open Recent                 ▸ │ kitchen.panel                          │
││───────────────────────────────│ flat-12.panel                          │
││ Save                   Ctrl+S │ dacha.panel                            │
││ Save As…         Ctrl+Shift+S │                                        │
││───────────────────────────────│                                        │
││ Quit                   Ctrl+Q │                                        │
│└───────────────────────────────┘                                        │
└ Ready ──────────────────────────────────────────────────────────────────┘

        ┌ Electricity ─────────────────────────────────┐
        │ Save changes to kitchen.panel?               │
        │                                              │
        │          [ Save ]  [ Don't Save ]  [ Cancel ]│
        └──────────────────────────────────────────────┘
```

- **Window title**: "Untitled — Electricity" for a project never saved,
  "kitchen.panel — Electricity" for a file, with `*` after the name while
  there are unsaved changes.
- **File** is the first menu.

### The File menu

- **New** (Ctrl+N): asks about unsaved changes, then starts a new project as
  the app does at launch: the default supply and the 12-module enclosure, no
  devices, loads or wires, everything off, simulated time 0, empty undo
  history, "Untitled" title.
- **Open…** (Ctrl+O): asks about unsaved changes, then a file dialog showing
  `*.panel` files ("Panel projects"). It starts in the folder last used for a
  file, or in Documents the first time.
- **Open Recent ▸**: up to 8 files, the most recent first, by file name; the
  full path shows in the status bar on hover. Every file opened or saved goes
  to the top. A file that no longer exists shows a message ("Can't find
  kitchen.panel; it was moved or deleted.") and leaves the list; a file that
  exists but can't be opened stays in it. With no files the submenu is
  disabled. The list is kept in the settings file.
- **Save** (Ctrl+S): writes the current file; for an untitled project it does
  Save As. Always enabled, also with no unsaved changes, because switch
  positions may have changed and Save writes them too.
- **Save As…** (Ctrl+Shift+S): a save dialog in the same starting folder;
  `.panel` is added when the name lacks it; the dialog itself asks before
  replacing an existing file. The new file becomes the current one.
- **Quit** (Ctrl+Q): closes the window, the same as its close button.

### Unsaved changes

- The project has **unsaved changes** when the design changed since it was
  last saved, opened or created: any edit that Undo can take back (ADR 0011:
  the undo stack's clean state). Undoing back to the saved state clears the
  `*`.
- **Playing with the simulation is not a change**: switching handles and
  loads, faults, resets and time are not undoable, so they never set the `*`
  and never cause the question. A non-expert who only tried the panel would
  otherwise be asked about changes they don't think they made. The next Save
  still writes the switch positions of that moment; closing without saving
  keeps the ones in the file.
- **The question** comes before New, Open…, Open Recent and closing the
  window (also by Quit) when there are unsaved changes: "Save changes to
  kitchen.panel?" (or "…to the new project?" when untitled) with Save /
  Don't Save / Cancel. Save on an untitled project opens Save As; cancelling
  that dialog cancels the whole action, as does Cancel. The buttons carry the
  app's own translated texts: Qt's standard button texts would stay English,
  as the static Qt ships no translations (ADR 0012).
- **A failed save** (no permission, disk full) shows a message ("Can't save
  kitchen.panel: …" with the system's reason), leaves the file on disk as it
  was (atomic save, ADR 0010), keeps the `*`, and cancels the New, Open or
  close that asked for it.

### What a reopened panel looks like (owner's choice A)

- **The design**: supply, enclosure size, devices with their ratings and
  rail positions, wires with section, colour and length, loads with power,
  positions and plugs. Designations, load names and wire routes come back
  because they are derived from these (ADRs 0024, 0027, 0028).
- **Switches**: each device's handle and each load's on/off button as shown
  at the save. A tripped device was showing its handle down, so it is saved
  off.
- **Not saved** (ADRs 0010, 0029, 0030): trips, faults, heat, the stairwell
  breaker, simulated time, the time speed (it is per launch already and
  stays as chosen across New and Open),
  selection, zoom and scroll, undo history. A file opens with everything
  reset, simulated time at 0, the panel fitted to the window, an empty undo
  history and no `*`. The status bar's conditions of the old project are
  dropped.
- Because switches are saved on, opening a file is like switching it all on
  at once: a wiring mistake (say a neutral that bypasses the RCD) trips its
  device as soon as the file opens, exactly as it would on a click.

### When a file can't be opened

The current project stays exactly as it was, and a message says why:

- **Not a panel file**: not readable, not JSON, or not `"format":
  "electricity-panel"` with a version ≥ 1. "kitchen.panel is not an
  Electricity panel file, or it is damaged."
- **Newer version**: `version` above what this app reads. "kitchen.panel
  was saved by a newer version of Electricity. Update the app to open it."
- **Contents the model refuses**: an unknown device, load, supply or
  enclosure type, a device off its rail or on another one, a wire to a
  terminal that doesn't exist or with a section or length out of range, a
  plug into something that isn't a socket, a missing required field, and so
  on. Shown with the same message as damaged files; the precise reason is
  kept by the core (tests assert on it) but not shown, because the people
  who would understand it edit files by hand and can find it with the tests.

A file is opened whole or not at all: never half a panel.

## The file (core)

ADR 0010's format, version 1, written indented, UTF-8, atomically:

```json
{
  "format": "electricity-panel",
  "version": 1,
  "supply": { "type": "supply-1p" },
  "enclosures": [ { "type": "enclosure-12" } ],
  "devices": [
    { "id": "d1", "type": "breaker", "poles": "2P",
      "parameters": { "curve": "C", "ratedCurrent": 40 },
      "row": 0, "module": 0, "on": true }
  ],
  "loads": [
    { "id": "l1", "type": "ceiling-lamp", "x": 0, "y": 270, "power": 10, "on": true },
    { "id": "l3", "type": "socket", "x": 94.5, "y": 270, "power": 0 },
    { "id": "l7", "type": "tv", "x": 0, "y": 0, "power": 120,
      "socket": "l3", "place": 1, "on": true }
  ],
  "wires": [
    { "id": "w1", "from": "supply:L", "to": "d1:1", "section": 10,
      "conductor": "L", "length": 0.3 }
  ]
}
```

- Devices, loads and wires are written in the model's order, which matters:
  load names ("Lamp 1") follow the order of `loads`.
- Ids are saved, because wires, plugs and switch settings refer to them.
  Only `d<n>` for devices, `w<n>` for wires and `l<n>` for loads are
  accepted, so a device and a load can never share an id. Ids are per file: after opening, new objects get ids past the highest `d<n>`,
  `w<n>` and `l<n>` in the file, so none is ever reused (ADR 0024; the
  model's add calls already advance their counters past a kept id).
- `"on"` is the switch setting from the simulation: a device's handle, a
  non-socket load's button. Missing means off.
- A plugged appliance's `x`/`y` are saved as stored, though unused while
  plugged in, so a round trip gives an identical model.
- ADR 0010's `"view"` block is dropped: opening fits the panel to the window.
  `"enclosures"` stays a list, as ADR 0010 shapes it, holding exactly one.
- Reading ignores unknown fields (ADR 0010).

### Versions and upgrades

- A file's version above 1 is "newer version"; missing or below 1 is "not a
  panel file".
- Upgrades are one function per version step (1→2, 2→3, …), each turning the
  JSON of one version into the next before it is read (ADR 0010). Version 1
  has no step yet. F10 builds the scaffold: the step list, and a fixture
  `tests/core/data/v1-tutorial.panel` (the tutorial panel with loads, written
  by F10 and checked by hand) with a test that asserts the model it opens to.
  That fixture must open in every later version; each future version adds its
  own fixture and step test.
- Two rules for later features, recorded in the F10 ADR:
  - `Project::placement()` accepts only parameters exactly matching the
    type's allowed keys, so a feature that adds a device parameter (F12, F16)
    bumps the version and its step fills the parameter in for older files.
  - Loads save their full power (ADR 0028), so a changed default in the
    catalog does not touch saved files. Only a narrower power range would,
    and that is a step like any other. (This answers the roadmap's "migrate
    power when a load type's default changes": nothing to do in F10.)

### Model and simulation support

- **Opening** builds a separate project with the same catalog from the file,
  using the model's own add calls, so every rule the editor obeys applies to
  the file too. Only if every object is accepted does the window's project
  take over the whole contents in one step (one `changed()`, and
  `enclosureChanged()`). An appliance can come before its socket in the
  load order (place a TV, then a socket, then plug the TV in), so the
  unplugged loads are added first and the plugged ones inserted at their
  file positions afterwards; the order, and with it the load names, survive. New does the same with an empty project, which also
  restarts the id counters at 1.
- **Wire length**: the model refuses a wire whose length is not in
  (0, MaxWireLength], as `setCableLength` already does. Today `addWire`
  accepts a length ≤ 0, and the simulation's `1/R` would turn the whole solve
  into NaN.
- **Simulation reset**: New and Open first clear every live setting — switch
  settings, trips, faults, heat, the stairwell breaker, time — then the
  project takes the new contents, then the file's switch settings are applied
  in one go. The old file's settings never show on the new one: without the
  reset, `d1` of the opened file would inherit the old `d1`'s handle (a
  switch flipping on its own; ids restart per file).

## The clang-tidy gate (first task)

Checking every file takes about 13 minutes here (measured: 50 files, 6
parallel jobs), too slow for every local run. The local gate checks the
`.cpp` files changed since the merge-base with `origin/main` (unpushed
commits, uncommitted and untracked files) plus the `.cpp` beside each changed header; with no
`origin/main` it checks every file. CI is unchanged: every file. That covers
what a push would bring to CI, except an unchanged `.cpp` broken by a header
it includes that has a different name; CI still catches that one. Recorded in
the F10 ADR as a refinement of ADR 0018's gate 4 (accepted ADRs are not
rewritten, ADR 0001).

## Non-goals

- Autosave, crash recovery, backup copies.
- Opening a file from the command line or by double-clicking it in a file
  manager. The app is one executable with no installer, so nothing ties
  `.panel` files to it; Open… and Open Recent are the ways in. (Cheap to add
  later if wanted.)
- Several projects at once: tabs, windows.
- Saving the view, undo history, simulated time, faults or trips.
- Toolbar buttons for the file actions: the menu and the shortcuts are where
  every desktop program has them, and the toolbar stays for editing and the
  simulation.
- Exports (F13–F15).

## Anti-goals

- Half a panel after a bad file.
- Losing work without being asked: no New, Open or close drops unsaved
  design changes silently, and a failed save never damages the file already
  on disk.
- A switch that flips on its own after Open.
- A "Save changes?" question after the user only played with switches.
- A newer version's file opened wrong, then saved back with data lost.

## Constraints

- File reading, writing and upgrades in `panelcore`, Qt Core only (ADR
  0006); dialogs, menus and settings in the app.
- ADR 0010's format and rules; ADR 0011's clean state for unsaved changes.
- Every new string translated to ru/kk/en (ADR 0012). Qt's own file dialog
  strings are covered the way ADR 0012 covers Qt's standard strings: the
  `QFileDialog`, `QFileSystemModel` and related Qt contexts the dialog shows
  are added to our `.ts` files with ru and kk translations (roughly 50–80
  strings each), and the translation check enforces them.
- Red/green TDD; a GUI scenario test driving the real menus and dialogs with
  screenshots (ADR 0018, CLAUDE.md).

## Approaches

**How a reopened panel looks** (owner chose A, 2026-11-24):
- *A. As left* — **chosen**: handles and load buttons saved; lamps light on
  open. Faults, trips, heat and time start fresh.
- *B. Handles only* — breakers up, every load off until clicked: a half-live
  panel.
- *C. Nothing* — everything off like a new panel; drops a line of ADR 0010.

**How opening replaces the panel** (builder's call):
- *Build off to the side, then replace in one step* — **chosen**: the model's
  own rules check the file, a bad file never touches the window, and the
  scene re-syncs once.
- *A fresh editor per file* (new project, scene, panel, simulation) — every
  part of the window holds the one project, so all of it would be rewired for
  no visible gain.
- *Load straight into the live project* — a bad file would leave a
  half-loaded panel and a solve per object on screen.

**Where file code lives** (builder's call): in the core beside the model
(**chosen**: unit-tested, the rules sit with the model's rules, ADR 0006 puts
files there), rather than in the main window.

**What counts as unsaved** (ADR 0011): the undo stack's clean state
(**chosen**). Counting switch flips too would prompt after mere trying.

**File dialogs** (builder's call): the system's own dialogs (**chosen**:
familiar Desktop, Documents and recent places on Windows). On the offscreen
test platform Qt falls back to its own dialog for the same calls, which the
GUI test drives like a user. Always using Qt's own dialog would match the
dark theme but feel foreign on Windows.

**The file dialog's language on Linux** (owner chose b, 2026-11-24): the
Linux Qt is built without GTK, so on Linux the dialog is always Qt's own, and
the static Qt carries no translations of its own strings (none for Kazakh
exist at all). Options were:
- *a. English on Linux for now* — nothing to add; translate at release.
- *b. Translate Qt's dialog in F10* — **chosen**: a Russian or Kazakh user on
  Linux gets the Open and Save dialogs, and the "replace it?" question, in
  their language.
- *c. The desktop's own dialog through the portal* — works on few desktops
  outside a sandbox; the binary's dependencies would need checking again.

**Local clang-tidy gate** (builder's call):
- *Files changed since `origin/main`, plus the `.cpp` beside a changed
  header* — **chosen**: covers what the next push brings to CI at a fraction
  of the time.
- *Every file, like CI* — exact, but about 13 minutes per local run.

## Decided by the owner

- F10 as the roadmap describes it (New / Open / Save / Save As, recent
  files, unsaved-changes question, version 1 with a migration scaffold).
- A reopened panel looks as it was left: handles and load buttons saved
  (choice A, 2026-11-24).
- Qt's file dialog is translated to Russian and Kazakh in F10 (choice b,
  2026-11-24).
- Two existing GUI test checks that F10 changes on purpose are updated as an
  explicit step (approved 2026-11-24): `tst_mainwindow` expects the title
  "Electricity" (now "Untitled — Electricity"), `tst_editing` expects the
  menus Edit, View, Settings, Language (now File first).

## Builder's calls

- Opening checks the file with the model's own rules on a separate project
  and replaces the window's project in one step; files open whole or not at
  all.
- The model refuses a wire length outside (0, 500] m everywhere, not only in
  files.
- Simulation reset on New and Open, before the new contents arrive.
- Switch flips are not unsaved changes (ADR 0011); Save is always enabled.
- A tripped device is saved off.
- `"view"` dropped from the format; the panel is fitted on open.
- One message for damaged and refused files; the precise reason stays in the
  core.
- 8 recent files, missing ones removed on click; last folder remembered,
  Documents first.
- System file dialogs; Qt's fallback in offscreen tests.
- Quit in the File menu.
- No file association or command-line opening; no toolbar buttons.
- Local clang-tidy gate on files changed since `origin/main`.
- One ADR for F10's decisions (format details, open-whole rule, simulation
  reset, upgrade rules, gate change), refining 0010 and 0018.
- Ids must be `d<n>`, `w<n>`, `l<n>`; plugged loads are added after the
  unplugged ones and inserted at their file positions.
- A recent file that exists but fails to open stays in the list.
- The time speed survives New and Open.
- The question's buttons use the app's own translated texts.

## Left to the builder

Exact message wording and translations, the file filter text, object names,
how the recent list labels two files with the same name, whether `"on":
false` is written or left out, the order of keys in the JSON, how the GUI test
reaches the modal dialogs.

## Tests

Core (`tst_projectfile`, plus additions to `tst_wires` and
`tst_simulation`):
- Round trip: the tutorial panel with loads, plugs and a mix of switch
  settings saves and opens to an identical model (devices, wires, loads,
  enclosure) and identical switch settings; new objects after opening get ids
  past the file's highest.
- Refused files, each with its reason and the target project untouched: not
  JSON, wrong `format`, missing or zero `version`, newer version, missing
  required field, unknown device / load / supply / enclosure type, an
  overlapping device, a wire to a missing terminal, wire length 0, negative
  and above 500 m, a plug into a non-socket, a duplicate id.
- Unknown fields are ignored.
- The v1 fixture opens to the expected tutorial model.
- A save to a path that can't be written fails and leaves an existing file
  unchanged.
- `addWire` refuses a length ≤ 0.
- Simulation reset: after it, `d1` is off, nothing is tripped or faulted,
  time is 0.

GUI scenario (`tst_files`), by real menu clicks and dialog typing, with
screenshots at each step:
1. Build a small panel (the tutorial fixture), switch QF1, QD1, QF2 on by
   clicking their handles and a lamp by its button.
2. File → Save As…, type a temp path: the file exists, the title shows its
   name without `*`.
3. File → New: empty panel, "Untitled", everything off.
4. File → Open Recent → the file: the model equals the saved one, the
   handles are up and the lamp glows, no `*`, empty undo history.
5. Move a device: `*` appears. Close the window: the question shows (dialog
   screenshot). Cancel: the window stays. Close again, Don't Save: it closes.
6. With unsaved changes on an untitled project, New → Save → the Save As
   dialog; cancel it: nothing changes.
7. Open a damaged file and a newer-version file: a message each, the panel
   unchanged.
8. A recent file deleted on disk: a message, and it leaves the list.
9. In Russian (and Kazakh), File → Open…: the dialog's labels and buttons
   are translated (dialog screenshot per language).

Smoke test of the real binary under Xvfb with the File menu open.
