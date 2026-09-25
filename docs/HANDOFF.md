# Handoff

Orientation for whoever picks this up next, human or agent. Read this first, then
`docs/RUNNING_THE_DECOMP.md` for how the work is run and `PORT_NOTES.md` for how the port
itself works. This file is the map and the current position; those two are the detail.

## The state, measured

```
matched    3020 / 28465 functions        (7.95% fuzzy, 7.08% of code, 4.85% fully linked)
linked     1635 / 28465 functions        (the one rule's count: the unit is Matching and has a source)
DOL units  2659 / 16726 functions        (main/*, including the SDK's 882; 1334 of them linked)
REL units   361 / 11739 functions        (the 86 modules; 301 linked, 170 of those = REL_Setup)
```

Verify all of that yourself; do not trust this file's numbers over the report:

```sh
./tools/decomp_build.sh            # ninja + objdiff + the All: line
python3 - <<'PY'
import re, hashlib, os
cfg = open('config/G2ME01/config.yml').read()
ok = 0
for name, exp in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg):
    mod = name[:-4]; p = f'build/G2ME01/{mod}/{mod}.rel'
    a = hashlib.sha1(open(p,'rb').read()).hexdigest() if os.path.exists(p) else 'missing'
    ok += a == exp
print(f'{ok}/86 modules match config.yml')
PY
```

Last known good: the commit that last touched this file (`git log -1 --format=%h -- docs/HANDOFF.md`).
As of the numbers above: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs
byte-identical to `orig/G2ME01/files/RelProd/`, probe 114 files 0 failures, symbol check 0 missing.
(The old form of this line pinned a commit hash, which cannot be written down in the commit that
creates it.)

## If you are picking this up (2026-09-25, end of session)

Read this section, then the rest of this file, then `docs/RUNNING_THE_DECOMP.md`. Everything below is
measured; `python3 tools/gate.sh` is the single command that tells you whether the tree is sound, and
`python3 tools/check_docs_claims.py` tells you whether these files are still true.

**What landed today** (each with the gates run and a per-function report diff, and all of it in the
history with the reasoning):

- `CAi` is **done** - 11/11, `Matching` - and the "cyclic link-order dependency" that this file used
  to call the top blocker **was never real**: the split is accepted, and what looked like a cycle is
  COMDAT weak symbols that both linkers discard. Read "CAi: landed…" in `RUNNING_THE_DECOMP.md` before
  touching `include/MetroidPrime/Enemies/`.
- `CPatterned` is a `Matching` unit too (10/10), landed as the 92-byte accessor cluster rather than its
  0xB58-byte constructor, which is still unwritten and is the largest single known item left.
- `TypesMatch` went 398 -> **508 of 511**; the three that remain are characterised in this file.
- Three modules (`Puffer`, `WallCrawler`, `ScriptGui`) had lost their `Rel(...)` blocks to config
  clobbers and were restored; `Puffer` was then promoted for real. **16 modules now link our own code**
  (measured by `tools/check_module_wiring.py`, never from memory).
- Upstream `PrimeDecomp/echoes` is now a workstream: five units ported, `CCubeSurface` landed. The rule
  and the measured cost are in "Upstream, and what we take from it".
- Eight tools added or repaired, including `tools/gate.sh` (the whole acceptance test, ~1.9 s) and
  `tools/report_diff.py`; ripgrep the tools table for what each answers.

**The number to watch is the linked one, not the headline.** The report's `All:` line counts functions
at 100% inside units that are not in the binary; the state block above carries both. A change can
raise the headline while lowering what is linked, and `tools/report_diff.py` is what catches it.

**Unlanded work, and where it is.** Three lanes' snapshots are preserved as branches (`git log
master..wave-w2` etc.), not because they should be merged wholesale - each predates later commits and
would revert them - but because their *sources* may still be worth mining: `wave-w2` (audio/input
tools and notes), `wave-w4` (a partial `CDamageInfo` reconstruction), `wave-w7` (upstream batch config,
superseded by the landed sync). Mine them file by file; never copy their `config/` or `configure.py`.

**What I would do next, in order:**

1. **Per-module config files.** Give each REL module its own object list instead of one shared
   `Rel(...)` hunk in `configure.py`; that is the structural fix for the clobbers that cost three
   modules today. `tools/check_module_wiring.py` only detects the damage afterwards.
2. **`collect.sh <lane>`**, per the review: three-way apply a lane's diff in a fresh HEAD worktree,
   run `tools/gate.sh`, print the diff stat and the per-function diff. Collection is the bottleneck
   now that a build is 3.8 s, and it is where my judgement was demonstrably fallible today.
3. **`CPatterned`'s constructor** (`fn_80079BE4`, 0xB58 bytes, 82-slot vtable) - the largest known
   item, and the last thing standing between the hierarchy and 75 creature modules.
4. **The blocked near-complete units**, each needing the same class of fix (container/COMDAT
   emission): `CStringTable` 12/14, `CDependencyGroup` 11/13, `CObjectReference` 8/10, `NMWException`
   10/11, `CPakFile` 22/33. `tools/unit_fit.sh` says exactly what is over, short or unclaimed.
5. **A policy on raw-offset code.** A review found 26 raw-offset sites in
   `ScriptFrontEndDataNetwork` and more in `Metaree`/`WallCrawler`/`Puffer`: they match bytes, but
   upstream would reject them and they are wrong on a 64-bit PC port. Somebody has to decide whether
   the port's goal tolerates them.

**The whole review is in the repository** - `docs/reviews/2026-09-25-rig-review.md` - with what was
adopted from it and the five items still open. It is worth reading before trusting any tool here,
including the ones added today: two of the tools I landed were empty stubs, and the acceptance test
passed on nothing twice over.

## What is not in git (check these before blaming the tree)

A fresh checkout is **not** self-sufficient. Three things live outside version control, and every
tool fails with a confusing error if one is missing:

1. **The retail data at `orig/G2ME01`** (7.5 MB, untracked and not ignored). `dtk` needs it to split
   the DOL and the 86 RELs, and `config/G2ME01/build.sha1` hashes what it produces. On a copy of this
   directory it is already there; on a `git clone` it is not, and it comes from an owned disc via
   `python3 tools/extract_disc_file.py`. `sha1sum orig/G2ME01/sys/main.dol` should be
   `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - the same hash everything else is measured against.
2. **The sibling toolchain tree `../MetroidPrimePort`** - MWCC compilers, `dtk`, `wibo`, and a ninja
   under `build/review-tools/bin/`. Nothing here builds it; `MP_TOOLCHAIN_DIR` points at it, and
   `tools/gate.sh`, `tools/decomp_build.sh` and `tools/flip_test.sh` all default to that sibling path
   and fail loudly if it is wrong. The lane briefing has the export line.
3. **`build/`** is ignored and regenerable - `tools/gate.sh` configures and builds it from nothing in
   about two seconds, given 1 and 2.

Also worth knowing, though nothing breaks without it: the upstream reference clone the sync
workstream reads (`git clone --depth 1 https://github.com/PrimeDecomp/echoes`), and the lane
worktrees under `/tmp/opencode`, which are disposable and can be removed (`git worktree list` currently
shows 61, holding ~11 GB of a tmpfs).

**The first command to run in a fresh session is `python3 tools/gate.sh`.** It configures, builds,
checks the DOL and all 86 RELs against `config.yml`, diffs the report per function, and checks the
module wiring, the docs' claims and the port probe - about two seconds, one verdict line, non-zero
exit on any failure. If it passes, the tree is sound and the documentation in this file is current;
if it fails, read the failing step before anything else.

## What this repository is

Two things at once, and it is easy to confuse them:

- **A port** of Metroid Prime 2: Echoes to PC, built on Aurora (the MIT GameCube SDK/GX
  replacement). The port layer is essentially complete: REL runtime, entry point, SDK shims,
  disc tools, tests. It cannot run because the decompilation is 7.95% done by fuzzy match and
  6.77% linked (1,626 functions of 28,465 are in a `Matching` unit that is really in the binary).
- **A contribution to the decompilation** (`PrimeDecomp/echoes`), which is what the remaining
  work actually is. Every rule about completion in `RUNNING_THE_DECOMP.md` comes from this half.
  The public upstream tree is reachable and **ahead of us in units we have not written** (and behind
  in others); from 2026-09-25 the rule is to port from it only where the gates pass, attributed in
  the commit. See "Upstream, and what we take from it" in `RUNNING_THE_DECOMP.md` for the measured
  cost of doing that and the units it has been done for.

## Tools, in the order you will want them

| | |
| --- | --- |
| `./tools/decomp_build.sh [unit]` | ninja, then objdiff, then that unit's unmatched functions |
| `tools/flip_test.sh <unit>` | **the acceptance test** - flip to `Matching`, rebuild, keep only if the DOL and all 86 RELs still reproduce retail |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares vs what the retail object defines |
| `tools/find_trivial_functions.py` | unmatched functions classified by machine-code shape - the cheap-work queue |
| `tools/scaffold_rel_module.py` | the three artifacts for starting a REL module |
| `tools/probe_sources.sh` | the port build's syntax sweep (114 files) |
| `build/binutils/powerpc-eabi-objdump`, `powerpc-eabi-nm` | disassemble / list symbols |

There is **no system cmake or ninja**. Use
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort/build/review-tools/bin/`
for cmake/ctest/ninja, and that port's `build/compilers` and `build/tools/{dtk,wibo}` for the
matching build.

## The one rule that decides whether work counts

A unit is done when the build still reproduces retail **with that unit's own object in the
link**. `configure.py` decides this: `NonMatching` links bytes `dtk` split out of the retail
binary, `Matching` links our compile. So objdiff percentages on a `NonMatching` unit are a
signal, not a result, and `87 files OK` proves nothing about a unit that is not `Matching` -
it validates the untouched parts of the binary. Two sessions were spent on this; see
"the one rule" in `RUNNING_THE_DECOMP.md`.

## Two independent workstreams, and where each stands

**1. The DOL** - 2659 of 16726 functions, ~14k left (that figure includes the SDK's 882, which are
essentially complete). Verified matches land here steadily, and the two units the whole port was
waiting on are in: `CAi` 11/11 and `CPatterned` 10/10, both `Matching`. Others:
`TypesMatch` 508/511, `CStateManager` 63/239, `CPlayerGun` 61/135, `CPlayerState` 69/72.
The reachable pools are thinning; what remains is dominated by FPU register allocation,
instruction scheduling, string-pool offsets, and weak rstl instantiations whose callers are
not decompiled. All of those are documented in `RUNNING_THE_DECOMP.md` - check it before
spending a session rediscovering one.

**`TypesMatch` was gated on naming, and that is now measured and paid off** (2026-09-25).
508 of 511 functions are exact: the first 94 (`30` `TypesMatch` overrides plus `64` `TCastToPtr`
casts) were landed under the placeholder class names `CUnknown<id>`, because every naming source
in the tree - the previous versions' configs, `symbols.txt`, the DOL's strings, the RELs - was
exhausted and none of them names those 32 classes. The *shape* is not a guess: each class's parent
is the class whose `::TypesMatch` the retail override calls, and the addresses come from the vtable
holding each id. `docs/research/TypesMatch_unnamed_ids.txt` is that table;
`docs/research/rename_typesmatch_ids.py` regenerates the block from it, so identifying a class later
is one line and a re-run, and it is now known what three of those classes are: id 76 has no members,
id 63 is `optional_object<TCachedToken<T>>` at 0x158, id 50 is an empty `CScriptDamageableTrigger`
subclass. **Three functions remain**, all characterised: `fn_8009D3D8` (93.27%) and `fn_8009D45C`
(79.70%) need a stack home and an outgoing-arg copy retail has and no source shape produced, and
`fn_80097520` is a 32-byte MWCC thunk to an unnamed function that nothing references.

**2. The REL modules** - 331 of 11739 functions, 86 modules. That count is low partly because
claiming a range for a unit *removes* those bytes from the `auto_*` units that match for free -
see "why the matched total can go down" in `RUNNING_THE_DECOMP.md`. **The recipe works and is written
up**: a module may be partly decompiled, with the `Matching` unit claiming only the ranges its
own object reproduces and everything else unclaimed so `dtk` fills it from retail.

**Measure this, never recall it**: `python3 tools/check_module_wiring.py`. As of the last commit it
reports **25 units of our own code in 16 modules** - `FlyerSwarm`, `Metaree`, `Puffer`,
`RubiksPuzzle`, `ScriptFrontEndDataNetwork`, `ScriptGui`, `ScriptPlayerActor`, `ScriptPlayerProxy`,
`ScriptPlayerTurret`, `ScriptRiftPortal`, `ScriptRsfAudio`, `ScriptSafeZone`, `ScriptStreamedMovie`,
`SwarmBasics`, `Tweaks`, `WallCrawler`. `Puffer` joined by being promoted rather than restored: with
its two units `Matching` the mutation check (change one byte of our source, the module hash must
break) proves our object really is in the link. The list this paragraph used to carry was wrong in both
directions and is exactly the kind of claim that must not be written from memory:

- `Puffer`, `WallCrawler` and `ScriptGui` had **lost their `Rel(...)` blocks** to later commits that
  copied an older `configure.py` (`33b73a3` replaced Puffer's block with WallCrawler's own; `f599488`
  dropped WallCrawler's and ScriptGui's). Their sources had been sitting in `src/` compiled by
  nothing, which is why the report showed those units at 0.00%. Restoring the blocks and the
  `Matching` states they had is worth **30 matched functions**, and `check_module_wiring.py` exists so
  the next one is caught the same day.
- `AIMannedTurret` was called "the working example" - it is not. Its unit is at 3/3 in the report, but
  promoting it to `Matching` **breaks the module's hash** (85/86), so its code is not in the link and
  never was. It stays `NonMatching`.
- `Puffer`'s units were `NonMatching` even in the commit that landed them, so its earlier "sha1
  verified" claim proved nothing - the vacuous-verification trap the rules warn about.

**The creature base classes now exist** (`CAi`, `CPatterned`), so the 75 creature and swarm modules
are no longer blocked on the hierarchy existing - only on their own code, and on `CPatterned`'s
0xB58-byte constructor if they need it.

## The blocker: CPatterned, and the base classes below it

**`CAi` is done** (2026-09-25): 11 of 11 functions, `Matching`, DOL sha1 and all 86 RELs still
reproducing retail. The "cyclic link-order dependency" that this section used to describe **was
never real** - the split is accepted, and what blocked it was ordinary symbol renaming. What looked
like a cycle was CodeWarrior COMDAT weak symbols that both linkers discard. The full account, the
rename list and the two traps (claim *all* four sections; a `.sdata2` split may not end inside a dtk
`lbl_`) are in `RUNNING_THE_DECOMP.md`, section "CAi: landed, and the cyclic link-order dependency
was never real". Read that before touching anything in `include/MetroidPrime/Enemies/`.

**`CPatterned` is landed as well** (2026-09-25): a `Matching` unit with 10 of 10 functions at 100%,
claimed as the 92 bytes of the small accessor cluster at `0x80073C58..0x80073CB4` rather than the
0xB58-byte constructor. The class exists, its vtable relocations resolve, and the creature modules
are no longer blocked on the hierarchy existing - only on their own code.

**What is left of the hierarchy:** `CPatterned`'s constructor at `0x80079BE4` (0xB58 bytes, 82-slot
vtable at `0x803B2458`, size 0x7c0) is untouched and is the next big single item; five of its sibling
accessors are unclaimed; its 36 own virtuals are unnamed. `fn_80079BE4` is still named `fn_` in
`symbols.txt`, deliberately - renaming it is the first step of writing it.

The other hierarchy gaps, unchanged: `include/MetroidPrime/Enemies/` holds the `SwarmBasics` layer
and now `CAi`/`CPatterned` headers; there is still no `CPatterned.cpp`, no GUI hierarchy
(`src/GuiSys/` and `include/GuiSys/` are empty and unlisted), and `UnkVtable20` is resolved.

## Running lanes

The work is done by many agents in parallel, one per module or unit, each in its own git
worktree with its own real `build/` directory (not a symlink - see the rig defect in
`RUNNING_THE_DECOMP.md`). The full spawn sequence, the non-negotiable details, how to collect a
lane, and the failures that recur are all in that file's "Parallel lanes" section. The two
things that cost the most time:

- **A lane's `config/` is its own view, never a patch.** Copying a lane's `symbols.txt` or
  `splits.txt` can silently revert a later rename, or delete an existing split block. Merge by
  hand against current `HEAD` and diff. This cost a lost `UnkVtable20` rename and two lost
  `configure.py` entries.
- **Verify a lane's report independently.** They have been wrong in both directions - one
  understated its own result by ten functions, several cited verification that proved nothing
  because their unit was `NonMatching`, and one reported its completed work as missing.

## Keeping this documentation true

These three files are load-bearing: a session that trusts a stale handoff wastes its whole
budget re-deriving what the last one knew. So updating them is **part of finishing a piece of
work**, not a separate chore. The rule is short:

**If you changed the answer to a question one of these files answers, update that file in the
same commit as the change.**

**A claim that can be derived must be derived, and that now has a checker.**
`python3 tools/check_docs_claims.py` verifies the numbers this file and `RUNNING_THE_DECOMP.md` quote -
the state block, the per-unit counts in the prose, the list of modules that link our code, and the
pinned hashes - against `build/report.json` and `tools/check_module_wiring.py`. Run it before
committing anything that moves a number, and after any config merge. It exists because the rule below
was in place for a whole session while a paragraph still listed sixteen modules linking our code when
three of them had no `Rel(...)` block at all: the state block was current and the prose was not, and
prose is where the reader forms their plan. Two more stale per-unit counts were found the moment the
checker was first run.

Concretely, after any turn that changes the position or the method:

| what changed | where it goes |
| --- | --- |
| matched counts, which units/modules are done | the state block at the top of this file |
| a unit or module is now `Matching` and verified | the state block, and the module table in `RUNNING_THE_DECOMP.md` |
| a new blocker found, or an old one cleared | the blocker section here, and the relevant one in `RUNNING_THE_DECOMP.md` |
| a technique that worked, or a pattern that cannot work | `RUNNING_THE_DECOMP.md` (recipe, known-hard, gates) |
| a module attempted, whatever the outcome | the "Attempted modules" table at the end of `RUNNING_THE_DECOMP.md` |
| how the port itself works | `PORT_NOTES.md` |
| a lane-collection or lane-spawning lesson | the "Parallel lanes" section of `RUNNING_THE_DECOMP.md` |

Rules for the writing itself, learned by getting it wrong:

- **Measure numbers, do not recall them.** Every wrong figure found in these files was written
  from memory. Run `./tools/decomp_build.sh` and the config.yml hash check and quote what they
  print. If a number in a doc disagrees with `build/report.json`, the report wins.
- **Annotate stale checkpoints, do not silently rewrite history.** If a figure was right at the
  time it was written, leave it and mark it as a checkpoint pointing at the current source of
  truth. `PORT_NOTES.md` line ~340 does this.
- **Say what is not known.** "CPatterned's constructor was not attempted" is worth more than a
  confident-sounding guess, and it is what stops the next session repeating the work.
- **A negative result belongs in the docs.** The CAi link-order cycle, the assembly dead end,
  the vacuous `NonMatching` verification - each cost a session, and each is now one paragraph
  that saves the next one.
- **Do not let a doc outlive its claim.** If a statement is superseded, correct it in place and
  note that it was superseded; an earlier version of `RUNNING_THE_DECOMP.md` concluded that no
  module could be decompiled, which was wrong two sessions later.

There is a repo `AGENTS.md` that repeats the short version of this, so an agent that never
opens this file still sees it.
