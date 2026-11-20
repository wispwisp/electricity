# 0022. Translation check extracts strings first

- **Status:** Accepted
- **Date:** 2026-11-20

## Context

[0012](0012-internationalization.md) checks translations by scanning the `.ts`
files for unfinished entries. A string that was never extracted into the
`.ts` files has no entry there, so the scan passes while the text stays
untranslated.

## Decision

`cmake/CheckTranslations.cmake` copies every `.ts` file into a fresh work
directory, runs `lupdate` on each copy against the sources (with
`-pluralonly` for `_en.ts`), and counts `type="unfinished"` entries. It fails
with one line per file that has any, or when there are no `.ts` files. The
`translations_complete` test runs it on `translations/` and `src/`.

Developer workflow: after changing UI text, run
`cmake --build build --target update_translations`, then translate the ru and
kk entries in `translations/`.

## Rejected alternatives

- **Running `update_translations` in every build** — it rewrites tracked files
  during normal builds.
- **Scanning only** — misses new text that was never extracted.

## Consequences

Every feature ships its ru and kk text. A string that vanishes from the
sources stays in the `.ts` file until someone removes it.
