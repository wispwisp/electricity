# 0031. Project files

- **Status:** Accepted
- **Date:** 2026-11-24

## Context

F10 adds New, Open and Save for `.panel` files, and fixes the local
clang-tidy gate, which on `main` checked only uncommitted files while CI
checked every file. Design:
`docs/superpowers/specs/2026-11-24-f10-project-files-design.md`.

## Decision

This refines 0010 and 0018.

- The file's top level has `format` ("electricity-panel"), `version` (1),
  `supply` (`{type}`), `enclosures` (a list of exactly one `{type}`),
  `devices`, `loads` and `wires`. A device has `id`, `type`, `poles`,
  `parameters`, `row` and `module`. A load has `id`, `type`, `x`, `y` and
  `power`, plus `socket` and `place` while plugged in. A wire has `id`,
  `from`, `to`, `section`, `conductor` ("L", "N" or "PE") and `length`. All of
  these keys are required (`devices`, `loads` and `wires` may be empty lists),
  except `on`, `socket` and `place`. `version`, `row`, `module`, `power` and
  `place` are JSON integers without a fraction. `"on": true` is written only
  on devices and non-socket loads that are on. Lists keep the model's order.
  The model refuses a wire length outside (0, 500] m, in files and in the
  editor.
- Version 1 of the file is written indented, UTF-8, atomically. ADR 0010's
  `view` block is dropped (opening fits the panel to the window), and
  `enclosures` stays a list holding exactly one entry. Unknown fields are
  ignored when reading.
- A file opens whole or not at all. The core builds a separate project with
  the same catalog through the model's own add calls, so every editor rule
  applies to the file, then `Project::replace` gives its contents to the live
  project in one step.
- Ids are `d<n>` for devices, `w<n>` for wires and `l<n>` for loads, with
  n ≥ 1, and restart per file. Counters go past the highest id in the file.
- Plugged loads are added after the unplugged ones and inserted at their file
  positions, so an appliance may come before its socket and load names keep
  their order.
- `Simulation::reset` runs before the replace, so the old file's switch
  settings never show on the new one. Then the file's settings are applied.
- Switch settings are saved as `on`: a device's handle and a non-socket
  load's button. A tripped device is saved off, as its handle shows down.
- Unsaved changes follow the undo stack's clean state only. Switch flips,
  faults and time are not changes.
- Upgrades are one function per version step, each turning the JSON of one
  version into the next before reading. Version 1 has no step. Two rules for
  later features: a new device parameter bumps the version and its step fills
  the parameter in, because `Project::placement()` accepts only exactly the
  type's keys; loads keep their full power, so a changed catalog default does
  not touch saved files.
- File dialogs are the system's own. Qt's own dialog strings (`QFileDialog`,
  `QFileSystemModel` and related contexts) are registered in
  `src/app/qtstrings.cpp` and translated to ru and kk, as ADR 0012 does for
  Qt's standard strings. Qt's dialog adds the `.panel` default suffix only to
  names without a dot, so the app appends `.panel` after the dialog to a name
  with a dot ("ул. Абая 10") and asks itself before replacing that file.
- Gate 4 of `scripts/quality_gates.sh` checks every file in CI, and also
  locally when there is no `origin/main`. Otherwise it checks the `.cpp`
  files under `src/` and `tests/` changed since the merge-base with
  `origin/main` (unpushed, uncommitted and untracked) plus the `.cpp` beside
  each changed `.h`, and prints the list. Every file took 13 minutes here
  (50 files, 6 jobs).

## Rejected alternatives

- **A fresh editor per file** — every part of the window holds the one
  project, so all of it would be rewired for no visible gain.
- **Loading into the live project** — a bad file would leave a half-loaded
  panel and a solve per object on screen.
- **Every file in the local gate** — exact, but 13 minutes per run.

## Consequences

- A feature that adds a device parameter must add an upgrade step and a
  fixture; the v1 fixture must open in every later version.
- Wiring mistakes in a file trip their device on open, as a click would,
  because switches are saved on.
- The local gate can miss an unchanged `.cpp` broken by a header with a
  different name; CI still catches it.
