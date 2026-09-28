# One goal item

You are working **one item** in the Metroid Prime 2 decomp repo, in the worktree the driver named
(`../wt-mp2-goal` on `goal/decomp`, or a lane worktree `../wt-mp2-goal-L<k>` on `goal/lane-<k>`
when several items run at once - other agents may be working other worktrees; never touch them).
Everything you need is on disk. Nothing you were told in a chat session survives; this file plus
the repo is the whole briefing.

## Your item

The driver puts it in `build/goal/item.json` (`id`, `kind`, `target`, `reason`). Read it first.

- `kind: port` - define a missing symbol, or fill an empty body, **on the boot path**.
- `kind: match` - take a unit to `Matching`.
- `kind: progress` - raise `target`'s `matched_functions` in `build/report.json` without flipping
  it. This is for units too big to flip in one item (CStateManager, CGameState, CPlayerGun...).
  The unit stays `NonMatching`. The judge passes you only if the full gate is clean, the unit's
  matched count rises strictly, no function anywhere gets worse, and your diff adds no `asm`.
  Keep what you matched even if the rest of the item is blocked. The reviewer rejects a
  function matched by deleting real work (see the initialisation rule below).

`reason` says why this item is queued and what was measured when it was queued. Trust it as a
starting point, not as a measurement: **re-measure before you act.**

## The one rule

**A function counts only when its unit is `Matching` in `configure.py` and the build still
reproduces retail with our object in the link.** objdiff percentages on a `NonMatching` unit are a
signal, not a result. `87 files OK` is not evidence of anything - it validates the parts of the
binary nobody touched. (A `progress` item is judged on `report.json`'s per-function exact
matches, which are a real measurement; they only become a unit result when the unit flips.)

**Never mark a unit `Matching` without `tools/flip_test.sh`.** If a unit reaches 99% and stays
`NonMatching`, that is the correct outcome: note it and move on.

## Rules that have cost real time

- **Do not commit.** The driver commits, and only after `tools/goal_check.sh` passes. A commit you
  make will be reset.
- **Do not edit `tools/`, `docs/research/port_link_baseline.txt` or anything in `build/goal/`**
  other than your own notes file (the path the driver gave you, `.../build/goal/notes/<id>.md`). They are the judge and its baselines; a change
  that touches them fails the item outright, whatever else it did. Never run
  `tools/link_check.sh --record` or `tools/gate.sh --baseline`.
- **A `port` item passes only if the judge can see it.** If `item.json` has a `verify` field, that
  script under `tools/goal_verify/` is the acceptance test - read it before you start. Otherwise
  the target must be in the port's undefined-symbol list now and gone after your change. Either
  way the change must touch `src/` or `include/`.
- **`verify: boot-progress.sh` means "make the boot get further".** The loop booted the branch
  head under gdb before you started and recorded where the main thread stopped (a fault, or where
  it sits after a hang). You pass only if every boot of your tree gets further than every boot of
  the head: a new `boot: step` / `Initializing renderer` marker, or a later line in the same
  function. The fix must be the real one - an early return, a skipped call or a no-op body gets
  the change rejected by the reviewer - and you may not add, move or reword those marker lines.
  Try it yourself with `./tools/goal_verify/boot-progress.sh` (a few minutes; it restores the
  reach-stubs file it touches).
- **In the frame loop, each `frame: N` line is a marker too, and a `PORT_FRAME_STOP(...)` is a
  declared stop.** To pass, write that callee with retail's behaviour, then replace the stop with
  retail's call on the same line. The port's undefined count may not rise, so write any callee
  you add a call to. Do not edit the `PORT_FRAME_STOP` macro, the `frame:` print or
  `MP_PORT_FRAMES`.
- **After the judge, a reviewer on a different model reads your diff** and can reject it
  (`docs/goal-review-prompt.md` lists exactly what it rejects). Keep the diff to what the item
  needs: an unrelated fix, a stub that makes the target symbol disappear without doing its work,
  a bypassed wall, or a doc claim nothing measured each gets the whole change rejected. If
  something else needs fixing, put a `NEW:` line in your notes rather than fixing it here. A
  rejection's reason is appended to your notes file for the next attempt.
- **Declare your functions in reverse.** mwcceppc emits definitions in reverse source order and
  mwldeppc keeps the object's `.text` order verbatim, so a unit's functions must be declared
  **descending by retail offset**. Ascending, the module's bytes come out permuted and its hash
  breaks on a few bytes - with objdiff still at 100%, `unit_fit.sh` still saying "fits", and the
  link still succeeding. **Only `flip_test.sh` catches it.** Check first with
  `python3 tools/check_decl_order.py --unit <yours>`.
- **A carve is four files or it is not a carve**: `configure.py`, `config/G2ME01/splits.txt`,
  `files.cmake`, and the source's own claim. All four in one change. **Never let a claim span an
  unclaimed gap**, and **check `total_functions` is still 28465** after every `splits.txt` edit.
- **`flip_test.sh` gives a vacuous PASS on a multi-line `Object(`.** One `Object(...)` per line.
- **Do not copy `configure.py` or a `config/` file** from another tree or an older commit. Report
  config changes as a list of intended changes.
- **No assembly.** A transcribed `.s` unit reproduces the bytes and scores 100% while decompiling
  nothing. Report such a thing as blocked instead.
- **Do not delete an initialisation to gain percent.** A rearrangement that drops real assignments
  can raise a unit's average while making the function worse. That is a regression, not progress.
- **Run `tools/unit_fit.sh <unit>.cpp` before promising a promotion.** A unit whose object emits
  functions the retail object does not define can never be `Matching`, however good it looks.
- **Adding a string literal to a unit someone else is decompiling can move an unrelated function**
  by 32 bytes. If you touch a shared unit, check its `.text` did not move.
- **Never fake a frame or an asset.** No cleared pixels, no placeholder draw, no plausible-looking
  return value. **A stub that announces itself is safe; a plausible one is not.** Every stand-in in
  this repo logs its own name for that reason.
- **A percentage is not a result, and a green build is not a correct change.** See
  `docs/PROCESS_LESSONS.md` before you conclude anything from passing checks.

## Build

    export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
    ./tools/decomp_build.sh                 # or: ./tools/decomp_build.sh <unit>
    ./tools/flip_test.sh <unit>.cpp         # the acceptance test; the trailing .cpp is required
    ./tools/unit_fit.sh <unit>.cpp
    python3 tools/check_symbol_names.py

`orig/G2ME01` is a symlink to the read-only disc. `build/binutils/` has `powerpc-eabi-nm` and
`powerpc-eabi-objdump`. This worktree has its **own** `build/` - never symlink another tree's.

## When you finish

1. **Add one dated line for this item to `docs/RUNNING_THE_DECOMP.md`** - what you did, what you
   measured, what blocked you. If the boot path changed, add a line to `docs/HANDOFF.md` too.
   `docs/AGENTS.md`'s rule applies: **measure numbers, never recall them**, and `build/report.json`
   is the source of truth.
2. **Do not commit.**

## If you cannot finish

Write what you learned to your notes file (the path the driver gave you) and stop. This is a **success**, not a
failure: a blocker characterised stops the next run repeating the work. Include:

- the exact command you ran and its output;
- what is blocked and the evidence for it;
- `NEW:` lines for any **new** blockers you found, one per line, in the form
  `NEW: <id> | <kind> | <target> | <one-line reason>`. The driver adds those to the queue. A
  `port` blocker that stops the boot may end in `| verify: boot-progress.sh`, which makes it
  judge itself as above; no other script may be named there.

Do not leave the tree in a half-edited state you cannot describe: the driver runs `git reset
--hard` plus a `git clean` of your files, so anything you want to keep must be in a note or in
`docs/`.
