# Handoff

Orientation for whoever picks this up next, human or agent. Read this first, then
`docs/RUNNING_THE_DECOMP.md` for how the work is run and `PORT_NOTES.md` for how the port
itself works. This file is the map and the current position; those two are the detail.

## The state, measured

```
matched    2759 / 28465 functions        (7.57% fuzzy, 6.78% of code, 4.80% fully linked)
DOL units  2428 / 16726 functions        (main/* units, including the SDK's 882)
REL units   331 / 11739 functions        (the 86 modules)
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

## What this repository is

Two things at once, and it is easy to confuse them:

- **A port** of Metroid Prime 2: Echoes to PC, built on Aurora (the MIT GameCube SDK/GX
  replacement). The port layer is essentially complete: REL runtime, entry point, SDK shims,
  disc tools, tests. It cannot run because the decompilation is 6.75% done.
- **A contribution to the decompilation** (`PrimeDecomp/echoes`), which is what the remaining
  work actually is. Every rule about completion in `RUNNING_THE_DECOMP.md` comes from this half.

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

**1. The DOL** - 2421 of 16726 functions, ~14.3k left (that figure includes the SDK's 882,
which are essentially complete). Verified matches land here steadily
(`CStateManager` 64/239, `CPlayerGun` 62/135, `CPlayerState` 69/72, `TypesMatch` 398/511).
The reachable pools are thinning; what remains is dominated by FPU register allocation,
instruction scheduling, string-pool offsets, and weak rstl instantiations whose callers are
not decompiled. All of those are documented in `RUNNING_THE_DECOMP.md` - check it before
spending a session rediscovering one.

**`TypesMatch` is not the pool its 398/511 makes it look like** (measured 2026-09-25). All 113 of
its unmatched functions score exactly 0.00% - there is nothing partial to improve. They are 33
`TypesMatch` overrides plus 64 `TCastToPtr` specialisations for type IDs that are unnamed in this
tree's symbols and enum, so none can be written until those types are identified, plus 15 unnamed
destructor helpers (`fn_8009CD30` onward) whose member types are unknown. A lane spent its budget
there and correctly changed nothing: the cheap-looking pool is gated on naming, not on matching.

**2. The REL modules** - 331 of 11739 functions, 86 modules. That count is low partly because
claiming a range for a unit *removes* those bytes from the `auto_*` units that match for free -
see "why the matched total can go down" in `RUNNING_THE_DECOMP.md`. **The recipe works and is written
up**: a module may be partly decompiled, with the `Matching` unit claiming only the ranges its
own object reproduces and everything else unclaimed so `dtk` fills it from retail. Sixteen
modules currently link our code and keep their hashes: `AIMannedTurret`, `ScriptRiftPortal`,
`Metaree`, `SwarmBasics`, `Puffer`, `WallCrawler`, `FlyerSwarm`, `ScriptGui`, `ScriptSafeZone`,
`ScriptPlayerActor`, `ScriptPlayerTurret`, `ScriptFrontEndDataNetwork`, `ScriptPlayerProxy`,
`ScriptRsfAudio`, `ScriptStreamedMovie` and `RubiksPuzzle`.

**But 75 of the 86 modules cannot progress far without the creature base classes.** See below.

## The blocker, and the decision it needs

A lane reconstructed **`CAi` completely** - all 11 retail functions compiling to identical
instructions, the 0x330 layout, the 46-slot vtable, the member names. `CPatterned` has its size
(0x7c0) and 82-slot vtable established but its constructor (~0xB58 bytes) and 36 own virtuals
are not written.

None of the `CAi` work can be landed, because giving it a range in `config/G2ME01/splits.txt`
fails on a cyclic link-order dependency:

```
CAi.cpp -> TypesMatch.cpp -> auto_03_8009D644_text -> ... -> CPhysicsActor.cpp
  -> ... -> CMediumAllocPool.cpp -> CCubeMoviePlayer.cpp -> auto_10_80419C18_sbss
```

`CAi`'s constructor references `TypesMatch`, whose trailing auto code reaches `CPhysicsActor`,
and the chain returns to the `auto_*` region `CAi` was carved from. `dtk` resolves the DOL's
link order dependency-first and rejects the cycle. `configure.py` has a `link_order_callback`
hook for exactly this and it is commented out - but a matching build returns early from it, so
enabling it means changing how the DOL link order is resolved **for matching builds**, touching
every unit. That is a project-wide config decision, which is why it is documented here rather
than taken unilaterally.

**The work is preserved and reproducible**: `/tmp/opencode/c2` holds
`src/MetroidPrime/Enemies/CAi.cpp` and `include/MetroidPrime/Enemies/{CAi,CPatterned}.hpp`, plus
header fixes to SMoverData, CHealthInfo, CMaterialList, CDamageVulnerability and `CActor::Think`
that made other units match better (CPlayer's constructor 18.05% -> 21.06%). Copy it out of
`/tmp` before relying on it - `/tmp` does not survive reboots.

Resolving that decision is the highest-leverage step available: it unblocks `CAi`, then
`CPatterned`, then most of 75 modules.

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
