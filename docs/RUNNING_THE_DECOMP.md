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
  (register allocation, instruction scheduling).
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
