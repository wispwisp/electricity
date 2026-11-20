# F03 — Languages and settings

Date: 2026-11-20 · Status: approved by the owner

Roadmap item: [F03](../../roadmap.md#f03-languages-and-settings).
ADRs: [0012](../../adr/0012-internationalization.md),
[0013](../../adr/0013-dark-theme-and-svg-art.md),
[0018](../../adr/0018-testing-and-quality-gates.md).

## Intent

The app's users are non-experts in Kazakhstan and the CIS, so the UI ships in
Russian, Kazakh and English. The language switches at runtime without losing
anything on screen (a beginner exploring languages mid-tutorial keeps their
work), the theme switches dark/light, and both choices persist. On the first
launch a dialog asks for the language; the tutorial (F11) will start right
after it.

## Goals

- **Menus:** `Settings → Theme → Dark / Light` and
  `Language → Русский / Қазақша / English`. Language names always appear in
  their own script, so a user who lands in a language they can't read can
  still find theirs. The current choice is checked.
- **Instant switch:** picking a language retranslates every menu, the dock
  title and the status bar immediately; picking a theme repaints immediately.
- **Persistence:** both choices saved in `QSettings`, INI format in the user
  config folder (0013), and applied at startup.
- **First launch** (no saved language): before the main window, a dialog with
  a trilingual heading and one button per language:

  ```
  ┌───────────────────────────────────────────┐
  │ Выберите язык · Тілді таңдаңыз · Language │
  │                                           │
  │   [ Русский ]   [ Қазақша ]   [ English ] │
  └───────────────────────────────────────────┘
  ```

  The system language is preselected if it is ru or kk, otherwise Russian
  (0012). Closing the dialog keeps the preselected language, saves it and
  does not ask again.
- **Translations:** English source strings in code; `electricity_ru.ts` and
  `electricity_kk.ts` fully translated, `electricity_en.ts` for plural forms
  only; `.qm` files embedded (0012).
- **Untranslated-text check:** a CTest test fails when any `.ts` file has
  unfinished entries. It extracts strings from the sources first, so new text
  added without updating the `.ts` files also fails. From F03 on, every
  feature ships its Russian and Kazakh text.

## Non-goals

- Settings dialog (settings live in menus), "Show engineering details" (F12),
  tutorial and Help menu (F11), File menu (F10).
- Translating Qt's own standard strings (dialog buttons, text-field context
  menus): none appear yet; they are added to our `.ts` files by the feature
  that first shows one (0012).
- Kazakh in Latin script (0012).

## Constraints

- Tests never touch the developer's real settings: they use a temporary
  settings location.
- Only Qt modules already allowed by 0004 (LinguistTools is part of qttools,
  which is built locally and in CI).

## Approaches

- **Runtime switch** (`QTranslator` swap, `LanguageChange` →
  `retranslateUi()`) — **chosen**, as decided in 0012.
- **Restart to switch** — simpler, rejected in 0012: a beginner would lose
  state.

## Decided by the owner

Approved the design above as proposed.

## Builder's calls

- Window title stays "Electricity" in every language (product name; F18 may
  revisit).
- Theme as a two-item radio submenu rather than a single "Dark theme" toggle.
- The untranslated-text check extracts before scanning (refines 0012; recorded
  in an ADR if the mechanism is non-obvious).
- Russian and Kazakh wording written by the builder; Kazakh to be reviewed by
  a native speaker when possible (0012).

## Tests

- Scenario (GUI, offscreen): open the Language menu, pick Қазақша, assert menu
  texts change without restart; pick the light theme; recreate the window and
  assert both persisted. Drive the first-launch dialog by clicks and assert
  the saved language. Screenshots: each language, light theme, first-launch
  dialog.
- Unit: default-language choice from the system locale, settings round trip.
- Untranslated-text check in CTest.
