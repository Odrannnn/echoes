# AGENTS.md

For any agent or person working in this repository. Read `docs/HANDOFF.md` first — it has the
current position, the verdict tools and the open blocker. `docs/RUNNING_THE_DECOMP.md` is the
method (recipes, gates, lane mechanics, known-hard patterns). `PORT_NOTES.md` is how the port
itself works. Dated session narratives and per-item findings were moved verbatim to
`docs/history/` on 2026-09-29; both big docs end with an index of the moved headings. The goal
loop commits each item's notes as `docs/goal-notes/<id>.md`.

## General process lessons live in `docs/PROCESS_LESSONS.md`

Verification that cannot fail, tautological checks, stale derived inputs, inherited
reasons, sets versus orders, gross versus net, and eight more - each one paid for in
this project, each one general. It is deliberately **not** GameCube-specific and is
meant to be reusable. Read it before concluding that a green build means a correct
change; the failure it describes is three green checks agreeing on a broken change.

## Documentation is part of the work

**Run `python3 tools/check_docs_claims.py` before committing anything that moves a number.** It
derives the claims these files make - the state block, per-unit counts quoted in the prose, the list
of modules that link our own code, the pinned hashes - from `build/report.json` and
`tools/check_module_wiring.py`, and fails when a doc disagrees with the tree. Keeping the state block
current is not enough: it stayed current through a whole session while a paragraph listed sixteen
modules linking our code when three of them had no `Rel(...)` block at all, which reads as "landed"
to anyone planning from it.


These files are load-bearing. A session that trusts a stale handoff wastes its whole budget
re-deriving what the previous one already knew, and that has happened repeatedly here. So:

**If you changed the answer to a question one of these files answers, update that file in the
same commit as the change.** Not afterwards, not in a follow-up — the same commit, so the two
cannot drift apart.

Where things go:

- **`docs/HANDOFF.md`** — the measured position (counts, which units and modules are done), the
  open blockers, what to do next. If you matched functions or completed a module, the state
  block at the top is stale until you update it.
- **`docs/RUNNING_THE_DECOMP.md`** — techniques that worked, patterns that cannot work, the
  module recipe, the gate list, and the lane-spawning and lane-collection rules. Add a row to
  its "Attempted modules" table for **every** module you try, whatever the outcome.
- **`PORT_NOTES.md`** — how the port works: the matching build, the SDK shim queue, the REL
  runtime, `TARGET_PC` rules, the build instructions.

And specifically:

- **Measure numbers; never recall them.** Every wrong figure found in these files was written
  from memory. `build/report.json` is the source of truth; quote what `./tools/decomp_build.sh`
  and the config.yml hash check actually print.
- **Record negative results.** A blocker you found is worth a paragraph: the CAi link-order
  cycle, the assembly dead end, the verification that proves nothing — each cost a session and
  each is now a few lines that save the next one.
- **Annotate stale checkpoints rather than rewriting them**, if the figure was right when
  written. Mark it and point at the current source of truth.
- **Correct a superseded claim in place**, and say it was superseded. An earlier version of
  `RUNNING_THE_DECOMP.md` concluded no REL module could be decompiled; that was wrong two
  sessions later, and leaving it would have misled the next reader.

## The one rule that decides whether work counts

A unit is done when the build still reproduces retail **with that unit's own object in the
link** — `Matching` in `configure.py`, verified by `tools/flip_test.sh` for DOL units, or by
the module's sha1 against `config/G2ME01/config.yml` for REL units. objdiff percentages on a
`NonMatching` unit are a signal, not a result, and `87 files OK` says nothing about a unit that
is not `Matching`. Several sessions were spent learning this.

## Gates, every time, before committing

```sh
sha1sum build/G2ME01/main.dol                 # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      # 329 files, 0 failures
python3 tools/check_symbol_names.py           # 0 missing names
./tools/decomp_build.sh                       # the All: line must not fall
```

and all 86 RELs `cmp`-equal to `orig/G2ME01/files/RelProd/` with their sha1s matching
`config/G2ME01/config.yml`. If a change breaks any of these, revert it rather than commit it.

## Committing

Commit after each implemented feature, once its quality pass and gates are done. Stage only
what belongs to the change. Follow the existing message style: a `type: subject` line, then
what changed and why it matters, then what was verified. A commit that claims a result the
gates do not support is worse than no commit — several were made and later corrected.
