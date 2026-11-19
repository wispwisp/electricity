# 0012. Internationalization: Qt Linguist, English source, ru/kk/en, runtime switch

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The UI, catalog names and the tutorial ship in Russian, Kazakh and English.
Users may switch language without restarting. The container has Qt
LinguistTools; qttranslations (Qt's own strings) is not built.

## Decision

- **Source language is English** in code (`tr("Main breaker")`). Translation
  files: `translations/electricity_ru.ts`, `electricity_kk.ts`,
  `electricity_en.ts` (the English file only for plural forms).
- Build with `qt_add_translations` (lupdate + lrelease); `.qm` files are
  embedded as resources.
- Catalog and tutorial JSON hold **English source strings**, registered for
  extraction with `QT_TRANSLATE_NOOP("Catalog", "...")` in a generated
  or hand-maintained `catalog_strings.cpp`, and translated at display time
  via `QCoreApplication::translate`.
- **Runtime switch:** language menu (Русский / Қазақша / English). Switching
  installs the new `QTranslator` and every widget handles
  `QEvent::LanguageChange` by calling its `retranslateUi()`. Scene items
  redraw their labels.
- **Default language:** the saved setting; on first run the system locale
  if it is ru or kk, otherwise Russian. The first-run screen offers the
  choice before the tutorial starts.
- Qt standard strings (dialog buttons, context menu of text fields) are
  covered by adding the needed `QPlatformTheme` / `QDialogButtonBox`
  contexts to our own `.ts` files. Qt ships no Kazakh translations anyway.
- Kazakh uses Cyrillic script (current official use in software sold in
  Kazakhstan); a Latin variant is out of scope.
- Technical notation stays universal across languages: "C16", "30 mA",
  "2.5 mm²", device designations QF/QS/QD/FU per CIS convention.
- Units and numbers use the locale's decimal separator (`QLocale`).
- A test fails if any `.ts` file contains `type="unfinished"` entries
  (plain text scan of the files), so missing translations are caught in CI.

## Rejected alternatives

- **Russian as source language** — English source is the Qt norm and lets
  non-Russian contributors read code; translations are equally complete.
- **Restart to change language** — simpler, but a beginner exploring
  languages in the tutorial should not lose state.
- **Separate tutorial text files per language (Markdown)** — bypasses
  Linguist tooling and makes missing translations invisible.

## Consequences

- Every user-visible string must go through `tr()` / `translate()`.
- Translations of electrical terms need review by a native speaker; until
  then, Kazakh terms follow established usage in local product catalogs.
