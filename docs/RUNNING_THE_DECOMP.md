# Running the decompilation

How the work is organised, what has been proven to work, and what keeps failing. This is
the operating companion to `PORT_NOTES.md` (which holds the techniques and mechanisms).
Update it when the strategy changes, not only when a fact is learned.

## Where the work is

Two repositories, both forks we own:

- **`MetroidPrime2Port`** (here) is the **port** and the **measurement rig**. It builds the
  decompiled sources against Aurora for a PC target, and it carries the matching build, the
  test suite and the tooling.
- The **decompilation itself** is what upstream `PrimeDecomp/echoes` is doing, and what we
  contribute to. Our fork's `master` is the shared tree: one branch, committed centrally by
  the orchestrator.

Everything that measures progress lives here: `build/report.json`, `tools/decomp_build.sh`,
`tools/flip_test.sh`, `tools/compare_unit.sh`, `tools/check_symbol_names.py`,
`tools/find_trivial_functions.py`, `tools/scaffold_rel_module.py`.

## The measurement rig

| tool | question it answers |
| --- | --- |
| `tools/decomp_build.sh [unit]` | ninja + objdiff + that unit's unmatched functions with per-function percentages. The worklist is `build/report.json`. |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one, section by section. Stricter than the link. |
| `tools/flip_test.sh <unit>...` | **the acceptance test.** Flips a unit to `Matching`, rebuilds, checks the DOL and all 86 RELs, keeps the flip only if retail is still reproduced byte-for-byte. |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares inside a unit's `.text` ranges, checked against what the retail-derived object defines. |
| `tools/find_trivial_functions.py` | unmatched functions classified by the shape of their machine code - the cheap-work queue. |
| `tools/scaffold_rel_module.py` | the three artifacts needed to start a REL module, printed or `--write`. |
| `tools/probe_sources.sh` | the port build's syntax sweep: 114 files, must stay 0 failures. |

## The one rule that decides completion

**A unit is done when the build still reproduces retail with the unit's own object in the
link.**

Marking a unit `Matching` in `configure.py` swaps its input from `build/G2ME01/obj/<unit>.o`
(bytes `dtk` split out of the retail binary) to `build/G2ME01/src/<unit>.o` (our compile).
Until then, objdiff percentages are the only signal, and the "DOL and RELs are identical"
check says nothing about the unit - it validates the untouched parts of the binary.

Two consequences that have each cost a session:

- **A `NonMatching` unit's code is not in the binary at all.** A lane that writes an empty
  source file and reports "the module hash matches" has proven nothing: the module linked the
  `obj/` copy. Always report the hash **both ways** - `NonMatching` and `Matching`.
- **The same applies to REL modules.** A module's `.rel` links `obj/` for `NonMatching` units
  and `src/` for `Matching` ones. `cmp` on the `.rel` is vacuous unless the unit is `Matching`
  and the linked object is the one compiled from source.

`configure.py` also refuses to run at all if a `Matching` object has no source file, which
takes `build.ninja` down with it.

## A defect found in the rig (2026-09-25)

`config/G2ME01/build.sha1` names its files with paths relative to `build/` - literally
`build/G2ME01/<Module>/<Module>.rel` - and `dtk shasum` reads those paths. A lane that
configures into `build-clone/` therefore has its integrity check read the **master tree's**
`build/`, not its own output. In practice `ninja` printed `87 files OK` while the lane's own
`build-clone/G2ME01/ScriptCoin/ScriptCoin.rel` had a different sha1 from the original. Every
REL lane's "cmp silent, 87 files OK" report was unproven, and one of them held a module whose
own code does not reproduce it.

Rules that follow, and they are not optional:

- **A lane must not symlink the master `build/`.** Give it its own directory, so the check's
  relative paths resolve to the lane's artifacts.
- **Verify against `config/G2ME01/config.yml`**, which records each module's expected hash -
  not against a copy of the file in another tree, and not against "the check passed".
- **`87 files OK` from a lane is not evidence.** The acceptance test for a module is: apply the
  lane's source, set the unit `Matching`, rebuild *in the master tree*, and compare the module's
  sha1 to config.yml's.

## The recipe for decompiling a REL module

This works and is verified. It is the one arrangement that survives the module's hash check,
and every earlier attempt failed by not using it.

### The problem it solves

A module's `.rel` is linked from objects, and `dtk` fills every range **no unit claims** with
bytes split out of the retail module (the `auto_*` units in `build/report.json`). So a unit does
not have to own the whole module - and it must not, because a unit that claims a range its own
object does not reproduce removes those bytes from the link and the module stops matching.

### The arrangement

1. Scaffold the module: `python3 tools/scaffold_rel_module.py <Module> [Class] --write` prints a
   splits entry, a `Rel(...)` call and a source skeleton in address order. It is a starting
   point, not the answer - its default claims the whole module for one unit.
2. Write the functions you can.
3. **Re-split so the `Matching` unit claims only the ranges its own object actually reproduces.**
   Everything else stays unclaimed (or is claimed by `NonMatching` units with no source), and
   `dtk` fills it from retail.
4. Verify with the module's hash against `config/G2ME01/config.yml`, not with `87 files OK`.

Two worked examples, both committed:

`ScriptRiftPortal` - a three-way split, where only the middle unit is ours:

```
MetroidPrime/ScriptObjects/CScriptRiftPortalPrefix.cpp:   NonMatching, no source file
    .text 0x0..0xB4          (.bss 0x0..0x30)
MetroidPrime/ScriptObjects/CScriptRiftPortal.cpp:         Matching, and it exists
    .text 0xB4..0x128        (.bss 0x30..0x34)
MetroidPrime/ScriptObjects/CScriptRiftPortalTail.cpp:     NonMatching, no source file
    .text 0x128..0x2B38      (.rodata 0x0..0x78, .data 0x0..0x84)
REL/REL_Setup.cpp:          the module's prolog/epilog scaffolding
    .text 0x2B38..0x2CDC     (.rodata 0x78..0xFC)
```

`Metaree` - the same idea with the ranges left unclaimed instead of named:

```
MetroidPrime/ScriptObjects/CScriptMetaree.cpp:  Matching
    .text 0x324..0x460, .bss 0x0..0x4
REL/REL_Setup.cpp:
    .text 0x1F80..0x2124, .rodata 0x90..0x114
```

Everything between `0x460` and `0x1F80` is unclaimed and therefore retail bytes, which is where
the module's remaining functions live. The named units total 23 functions (18 ours plus the 5
`REL_Setup` ones, all exact); the module has 59 in total, so the unclaimed 36 are still retail
and show up in `build/report.json` as `Metaree/auto_*` entries. The module still hashes to what
`config.yml` records - that is the point of the arrangement: a module can be partly decompiled
and still correct.

### Why the `NonMatching`-with-no-source trick is legal

`configure.py` requires a source file only for `Matching` objects (it exits with
"Missing source file" otherwise, taking `build.ninja` with it). A `NonMatching` entry may name a
path that does not exist, which is how a module keeps retail bytes for a range while still giving
that range a name in the splits. It is also why the earlier `SkyRipple`-style scaffolds broke:
they marked the claimed ranges as the unit's own while the unit had nothing in them.

### The check that actually means something

```sh
python3 - <<'PY'
import re, hashlib
cfg = open('config/G2ME01/config.yml').read()
for name, expected in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg):
    mod = name[:-4]
    actual = hashlib.sha1(open(f'build/G2ME01/{mod}/{mod}.rel', 'rb').read()).hexdigest()
    print(('OK  ' if actual == expected else 'DIFF'), mod)
PY
```

`87 files OK` from a lane is not that check; see the rig defect above.

### Why the matched total can go *down* when module work lands

Claiming ranges in a named unit removes those bytes from the `auto_*` units that `dtk` builds
from the retail module, and an `auto_*` unit's functions count as matched by default - retail
bytes trivially match retail bytes. So moving worked-on functions out of `auto_*` into a named
unit at 100% can lower the headline total while the module is strictly better.

Puffer is the example: its 9 functions are now 6 + 3 in two named units, all exact, and the
project total went 2633 -> 2629. Nothing regressed; the 9 were previously counted for free and
the ranges they left behind are the ones now listed as unmatched `auto_*` entries. Judge module
work by the module's hash and by the named units' percentages, not by the global total.

### What still blocks most modules

- **`UnkVtable20__6CActorFv`** is declared in `CActor.hpp` (`// G2ME01 slot +0x20; original name
  unknown`) with no definition and no retail symbol. Anything derived from `CActor` that calls
  it cannot be linked, which is what stopped `CScriptCannonBall` and `CScriptForgottenObject`
  being promoted.
- **`include/MetroidPrime/Enemies/` is empty** - no `CPatterned`, no `CAi`. A creature module's
  *loader* can be reconstructed (and `Metaree` did, for the setup and accessor range), but its
  actor behaviour cannot be written until those base classes exist.

## Where a module can even be written

`include/MetroidPrime/Enemies/` is empty: there is no `CPatterned`, no `CAi`, no creature base
class. Any module whose objects derive from those **cannot be written at all** until upstream
lands the base classes - the loader can be reconstructed, but the actor cannot. That is not a
delegation problem, and no number of lanes fixes it.

Of the 86 modules, **11 are `Script*` units** (script objects that lean on `CEntity`/`CActor`,
which do exist) and the other 75 are creatures, bosses and swarms that need the missing Enemy
hierarchy. Acknowledge this before assigning module work: check that the base classes a module
needs actually exist.

## Parallel lanes

The work is run as several agents in parallel, one **lane** each. Lanes are cheap; **collecting
them is not**, and that is the dominant cost.

What works:

- One git worktree per lane (`/tmp/opencode/mN`, branch `mod-mN`), cut from current `HEAD`.
- The lane's own build directory (`configure.py --build-dir build-clone`), with the master
  tree's `build/` and `orig/G2ME01` symlinked read-only. `build-clone/` is gitignored.
- `LANE.md` in each worktree: the build commands, the completion rule, the hard gates, and
  the accumulated failure modes. **Keep it current** - it is the lane's only briefing.
- One lane per module, or one lane per unit. Lanes that share a unit overwrite each other.

What fails, repeatedly:

- **Stale `config/`.** A lane cut from an older commit carries older `symbols.txt` back with
  it, silently reverting a later fix. Collecting a lane means applying its **source** changes
  and re-checking its **config** against the current tree - never copying config verbatim.
- **Vague success criteria.** "cmp silent and 87 files OK" is satisfied by doing nothing.
  The criterion must name the state in which the check is meaningful.
- **Assembly as a shortcut.** A transcribed `.s` unit reproduces the bytes and scores 100%
  while decompiling nothing. One lane did this for a whole module (`FogOverlay`, 1,014
  instructions) and it was rejected. There are no `.s` units in this decompilation upstream;
  a module that can only be reproduced that way is **blocked**, not done.

## What to delegate, and how

- Cheapest lane that can do the job: `qwen27b`/`qwen` -> `worker` -> the `claude-code` tool.
- The **worker** lane produces volume on mechanical, reference-backed work (a matched Prime 1
  counterpart, script-unit scaffolding, name identification) and is weak at the last 1%
  (register allocation, instruction scheduling). It is also the lane that found the module
  recipe, by trying the `Matching` flip and reporting the exact symbol that blocked it rather
  than the check that passed.
- The **claude-code** tool is the one that converts near-misses and does multi-function units
  in one pass; it is also the one worth giving a whole unit and a long report.
- Every delegation ends with the lane stating its own verification result, and the
  orchestrator **re-measuring it independently** before committing. Lane reports have been
  wrong in both directions - understating and overstating.

## Where the remaining work is

- **The DOL tail** (~26k functions in `auto_*` units and the named `NonMatching` units). The
  named units that are close to complete are the cheapest; the rest is genuinely hard
  matching.
- **The REL modules** (~11.3k functions across 86 modules). The pipeline is proven for wiring
  (`AIMannedTurret` links our object and stays byte-identical) but no module has yet been
  *decompiled* - three lanes tried and hit the same compiler-level walls.

Record here which modules have been attempted and what blocked each one, so the next lane
does not rediscover it.

## Hard gates, every time

- `sha1sum build/G2ME01/main.dol` == `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- every built REL `cmp`s equal to `orig/G2ME01/files/RelProd/`
- `./tools/probe_sources.sh` green (114 files, 0 failures)
- `python3 tools/check_symbol_names.py` reports 0 missing names
- `All:` matched count from the report does not fall
- `config/G2ME01/splits.txt` and `configure.py` only change when the task is explicitly a
  config task (REL modules), never as a side effect

## Attempted modules (keep this list current)

| module | what happened |
| --- | --- |
| `AIMannedTurret` | wired; 3 getters matched; module byte-identical **with our object linked**. The working example. |
| `IngSwarm`, `WallCrawlerSwarm` | wired; no class code at all (all `REL_Setup`), so nothing to decompile. |
| `SkyRipple` | scaffold broke the hash (85/86 RELs) - claimed ranges did not match the object. Reverted. |
| `FogOverlay` | "completed" by transcribing 1,014 instructions into a `.s` unit. Rejected as not a decompilation. |
| `ScriptRsfAudio` | wiring, 7 correct symbol names, and an empty source; hash "matched" only because the unit was `NonMatching`. The symbol names are worth keeping; the rest proves nothing. |
| `DarkSamusBattleStage` | scaffolded split produced a 5,184-byte REL and an assembly object would not link. Correctly reverted with 0 functions. |
| `ScriptCoin` | 3 real functions written (a class, `Render`, `GetTouchBounds`) - the first genuine C++ in a module. With the unit `Matching`, the module sha1 differs from config.yml, so its code does not reproduce it yet. |
| `Ripper` | blocked with evidence: no `CRipper`, no `CPatterned`, no `include/MetroidPrime/Enemies/` at all. Reverted the scaffold rather than claim ranges it could not fill. The range check passed, so the block is the missing base classes, not the splits. |

**Superseded, 2026-09-25:** an earlier version of this table concluded that no module had been
decompiled and that the route was gated on the DOL hierarchy. Both halves were wrong in an
important way. `ScriptRiftPortal`, `Metaree` and `AIMannedTurret` now link our own C++ and hash to
what `config.yml` records, using the split described under "The recipe" - a module can be
*partly* decompiled and still correct, which is what makes the route viable before the actor
hierarchy exists. The hierarchy still gates the *behavioural* functions (see that section), but
accessors, predicates, loaders and setup can be taken now.

Current module status:

| module | our code in the link | notes |
| --- | --- | --- |
| `AIMannedTurret` | 3 functions | the first, and the simplest |
| `ScriptRiftPortal` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) | first with a three-way split; sha1 `a0fa6c69…` verified against config.yml |
| `Metaree` | 23 named functions exact (18 ours + 5 setup), of 59 total; the rest unclaimed | first creature-family module; ranges unclaimed rather than named |
| `CScriptCannonBall` | 12 of 26 matched, unit still `NonMatching` | blocked on `UnkVtable20` |
| `CScriptForgottenObject` | 9 of 12 at 95.86%, unit still `NonMatching` | .text/.rodata/.data a few bytes off |
| `ScriptCoin` | 3 functions written | does not hold its hash yet |
