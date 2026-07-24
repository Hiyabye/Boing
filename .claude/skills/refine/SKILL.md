---
name: refine
description: >-
  Whole-codebase refinement pass before starting new features — improve
  maintainability, readability, future expandability, and robustness (error-prone
  code, missing guards), then bring CLAUDE.md/README.md and inline comments back
  in sync with the code. Use when the user says "refine the codebase", "clean up
  before the next feature", "refactor pass", "tidy everything up", "polish the
  code", "level up the codebase", or similar broad quality passes. NOT for a
  single bug fix or one targeted change — this is a deliberate repo-wide sweep.
---

# Codebase refinement pass

Goal: leave the codebase measurably cleaner and the docs accurate, **without
changing behavior or blowing the 1.44MB budget**. The trap is churn — rewriting
clean code for its own sake. Resist it. The best refinement is often small.

## Rules of engagement

- **Understand before touching.** Read every affected module end to end first.
  A small diff in the wrong place is a second bug, not laziness.
- **Zero behavior change is the default.** Refactors must produce identical
  runtime behavior. If a change alters behavior, it's a feature/fix — call it
  out separately, don't smuggle it into a "refactor".
- **Be honest when it's already clean.** If there's little real rot, say so and
  keep the diff tiny. Don't invent work. (This repo is already well-architected:
  FSM + dispatch table, table-driven power-ups/themes/tracks, shared pool macros.)
- **No renames-for-taste.** Renaming public API used across files is high churn,
  low value, and risks regressions. Only rename when a name is actively wrong.
- Reuse what exists (`STATIC_ASSERT`, `clampf`, the pool macros, `persist`
  helpers) before adding anything new.

## Process

### 1. Read the whole thing
Read all of `src/*.c` + `include/*.h`, plus `CLAUDE.md`, `README.md`,
`CMakeLists.txt`. Trace the real control flow. Note the current shipped size
(`ls -l build/Boing`) as the baseline.

### 2. Catalog genuine findings, in these buckets
- **Robustness gaps** — the highest-value bucket. Look for:
  - Lookup tables with no compile-time completeness guard. Every enum-indexed
    table (`STATES[]`, `POWERUP_DEFS[]`, `sfxLen[]`, `TRACKS[]`, `ROW_LABELS[]`)
    should have a `STATIC_ASSERT(count == ENUM_COUNT_OR_LAST+1, name)` so a
    dropped row fails the build instead of a runtime divide-by-zero / NULL slot.
  - Divide-by-zero, unchecked mallocs, VLA on untrusted n, missing NULL guards.
- **Real duplication** — the *same* idiom/expression repeated 3+ times → one
  small file-local helper (e.g. `noise()`, `difficulty()`). Do NOT over-extract
  one-off patterns or near-but-not-identical code.
- **Doc/comment drift** — comments and `CLAUDE.md`/`README.md` that no longer
  match the code: stale counts (theme count, band height), examples that name
  things that don't exist, explanations of the wrong mechanism.

### 3. Ask the user for scope (this decision is theirs)
Use AskUserQuestion — the aggressiveness genuinely changes what you do:
- **Conservative** — robustness guards + tiny genuine DRY + comment/doc fixes.
- **Moderate** — the above + low-risk readability refactors (extract repeated
  inline patterns, add all missing table guards). *(Good default.)*
- **Aggressive** — the above + structural (split a large file, opaque handles,
  module reorg). Bigger diff, higher regression + size risk.

Second question: docs = **fix inaccuracies only** (default) vs **full rewrite**.

### 4. Apply the changes
Surgical edits per the chosen scope. Match surrounding comment density and
style. Keep behavior identical.

### 5. Verify end-to-end (all three)
1. **Build clean:** `cmake --build build` — no new `-Wall -Wextra` warnings, no
   clang-tidy regressions. A firing new `STATIC_ASSERT` means a table really is
   incomplete — fix the table, don't delete the guard.
2. **Size:** `ls -l build/Boing` — under 1,474,560 bytes and ~unchanged from
   baseline. Update the size numbers in `CLAUDE.md`/`README.md` to whatever the
   fresh build reports.
3. **Run:** launch briefly (`./build/Boing` for a few seconds, then kill) —
   confirm clean init, audio stream builds all loops (the startup
   `assert(isfinite(peak))` passing proves synth math is intact), no crash. For
   deeper changes, sanity-play the affected path.

### 6. Sync docs
Bring `CLAUDE.md`, `README.md`, and any drifted inline comments in line with the
final code. On any code/doc conflict, the code and `CMakeLists.txt` win. Fix
inaccuracies; don't rewrite prose that's already correct (unless full-rewrite
scope was chosen).

### 7. Commit + push
Per repo convention (see the `commit-straight-to-main` memory): commit directly
to `main`, no branch/PR, and push. Conventional-commits subject
(`refactor: ...`), body listing the buckets touched, and the "No behavior
change, size unchanged (N bytes)" line.
