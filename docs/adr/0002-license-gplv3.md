# 0002. License: GPL-3.0-or-later, with statically linked LGPL Qt

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The app is open source and ships as a single statically linked executable
that contains Qt. Open-source Qt modules we use (qtbase, qtsvg) are available
under LGPLv3 / GPLv2 / GPLv3. Some Qt tools (Linguist tools in qttools) are
under GPL but run only at build time and are not linked into the binary.

LGPLv3 permits static linking only if the user can relink the application
against a modified Qt. Providing the complete corresponding source of the
application under a compatible license satisfies this. The repository already
contains a GPLv3 `LICENSE` file.

## Decision

- The project is licensed **GPL-3.0-or-later**. `LICENSE` stays as is.
- Qt is used under its open-source license (LGPLv3 for linked modules); since
  the whole application source is public under GPLv3, the relinking
  requirement is satisfied.
- Every source file carries an SPDX header:
  `// SPDX-License-Identifier: GPL-3.0-or-later`.
- The app has an **About** dialog that states the app license, states that it
  uses Qt under LGPLv3 with a link to the Qt source, and lists third-party
  assets (embedded font, see [0004](0004-dependencies.md)).
- Release artifacts link to the exact source tag and to the pinned Qt
  version's source archive.
- All SVG art is original (see CLAUDE.md, Visual assets) and falls under the
  project license.

## Rejected alternatives

- **MIT / BSD / Apache-2.0** — permissive licenses allow the app itself, but
  a closed fork could then ship static Qt without giving users the means to
  relink, putting distributors in breach of LGPL. GPL keeps every
  distribution of the static binary compliant by construction.
- **LGPLv3 for the app** — no benefit: nobody links against this app.
- **Commercial Qt license** — not needed for an open-source project; cost.
- **Dynamic linking to avoid the question** — contradicts the single-binary
  requirement.

## Consequences

- Contributions must be GPL-compatible. Third-party code or assets with
  GPL-incompatible licenses cannot be added.
- The About dialog and release notes are part of license compliance, not
  decoration; they are in the roadmap.
