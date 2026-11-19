# 0001. Record architecture decisions as ADRs

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

The application is built feature by feature, one feature per session, often
by an agent that starts each session without memory of the previous ones.
Decisions that are not written down get re-litigated or silently broken.

## Decision

We record every non-obvious architectural decision as an Architecture
Decision Record in `docs/adr/`.

- Files are named `NNNN-kebab-case-title.md`, numbered sequentially from
  `0001`. `0000-template.md` is the template.
- Each ADR has: title, status, date, context, decision, rejected
  alternatives, consequences.
- `docs/adr/README.md` is the index: number, link, one-line summary, status.
  Every new or changed ADR updates the index in the same commit.
- Accepted ADRs are not rewritten. A changed decision gets a new ADR; the old
  one becomes `Superseded by NNNN`.
- Read the index before changing architecture.

## Rejected alternatives

- **Design notes in a wiki or in CLAUDE.md** — CLAUDE.md must not be edited
  (project rule) and a wiki drifts from the code. ADRs are versioned with the
  code.
- **One big architecture document** — harder to supersede one decision
  without rewriting the whole document; history is lost.

## Consequences

- Each session can load the index cheaply and open only relevant ADRs.
- A small overhead per decision; trivial or obvious choices do not get ADRs.
