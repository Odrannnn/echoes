# Running the decompilation

**Starting fresh? Read `docs/HANDOFF.md` first** - it has the current position, the verdict
tools, the open blocker and what to do next. This file is the method; that one is the map.

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
| `tools/unit_fit.sh <unit>` | why a unit will not promote: claimed range vs our object's sections, and the functions we emit that the retail unit object does not define. |
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

### Why a unit will not promote: extra functions, not size

Measure it with `tools/unit_fit.sh <unit>`. Two facts, both measured on 2026-09-25, and the second
is the one that matters:

- **An object bigger than the claimed range is not by itself fatal.** `Kyoto/Basics/RAssertDolphin.cpp`
  is `Matching` today with `.text` 1964 bytes against a claimed 1852. The reason is that its one
  extra function, `hack__Fv` (112 bytes), is byte-identical to what retail has immediately after
  the claimed range - `dtk`'s split attributed those bytes to the neighbouring unit, so our object
  overlapping them changes nothing. The proof that our object really is in the link: editing one
  string inside it moves `build/G2ME01/main.dol`'s sha1 off retail immediately.
- **What actually blocks a flip is emitting functions the retail unit does not have *and* that are
  not retail's bytes there.** `Kyoto/Audio/CStaticAudioPlayer` emits eight of them, 868 bytes of
  weak container and destructor instantiations (`reserve<vector<auto_ptr<uchar>>>`, `__dt__CDvdRequest`,
  `__dt__basic_string`, `destroy<pointer_iterator...>`, ...) - the unit measured 4668 bytes against a
  claimed 3800, and `tools/flip_test.sh` showed the REL differing. `CRumbleVoice` is the same shape
  (132 bytes over, its six weak vector instantiations emitted out of line), and four units hit this
  one cause in a single session.

So: run the tool first, read the *extra function* list, and only then decide whether the remaining
work is matching (fixable) or codegen the source cannot express (report it as blocked). Do not
trust a percentage - a 99.9% unit with 868 bytes of extra emissions will never be `Matching`.

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

### A second rig defect, found the same day: `flip_test.sh` never ran configure

`tools/flip_test.sh` needs the arguments `configure.py` was last run with, which `build.ninja`
records as a ninja variable spanning several lines, each continuation ending in ` $`:

```
configure_args = --version G2ME01 --compilers $
    /path/to/compilers $
    ...
```

The old extraction (`sed -n 's/^configure_args = //p' build.ninja | tr -d '\\\n'`) read only the
first line, so it ran `python3 configure.py --version G2ME01 --compilers` - an argparse error -
and every single flip reported `configure.py failed` followed by `FAIL -> reverted`. **A lane that
trusted it would have concluded, wrongly, that nothing could be promoted.** It was found by
running the tool by hand on `CScriptCannonBall` and reading the output instead of the exit code.

The fix joins the continuation block first (see `CONFIGURE_ARGS` in the script) and prints the last
15 lines of the ninja log when the build genuinely fails, so a real failure is diagnosable from the
tool's own output. Two lessons worth keeping: a verification tool that fails *closed* is
indistinguishable from "nothing passes" unless you read its output, and any tool reading
`build.ninja` must handle multi-line values.

`tools/decomp_build.sh` had a smaller version of the same disease: its report tail indexed
`fuzzy_match_percent` directly, and objdiff omits that key for the data-only units in the report
(any `auto_*` region with no code), so the plain invocation died with `KeyError` before printing a
single unit. It now skips units with no `total_functions` and defaults the missing percentages, so
the worklist it prints is usable again.

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

### A technique that works on the generated loader structs

`scripts/generate_script_loaders.py` writes a constructor as `Name() : a(), b(), c() { a = 1.0f;
b = 2.0f; }` - every member default-constructed, then assigned in the body. Retail does not do
that: it initialises most members *in the mem-init list* and leaves only the ones needing a
statement in the body. Moving the assignments into the list is a pure source rearrangenent that
moves the score by tens of points (measured 2026-09-25):

| unit | before | after |
| --- | --- | --- |
| `SLdrTweakTargeting_Scan` | 78.04% | **99.52%**, `__ct__` 0.00 -> 100.00% |
| `SLdrTweakTargeting_VulnerabilityIndicator` | 86.88% | **99.52%**, `__ct__` 56.48 -> 100.00% |
| `SLdrTweakCameraBob` | 85.49% | 91.02% |
| `SLdrTweakSlideShow` | 76.23% | 81.87% |
| `SLdrTweakPlayerRes` | 54.90% | 57.18% |

Three things the lane proved the hard way: retail's *program order* is the mem-init order followed
by the body, so where the list stops matters; a member can genuinely be set twice -
`indicatorColor(CColor::Green())` with `indicatorColor = CColor(1.f, 1.f, 1.f)` in the body is
what retail does, the first call really executed and really overwritten; and `CColor(1.f,1.f,1.f)`
passes floats, not bytes. The retail default values are readable from the module's
`build/G2ME01/<Module>/asm/auto_*_rodata.s` plus its symbols file.

**Do not delete an initialisation to gain percent.** Two of that lane's seven files also dropped
real assignments - 27 beam-icon angles in `SLdrTweakTargeting`, and the whole weapon configuration
in `SLdrTweakPlayerGun_Weapons`. Both raised the *unit-level* fuzzy average and both were rejected
here: the function itself scored worse (`__ct__` 50.76 -> 21.33) and nothing was left setting the
members. Land only rearrangements that keep every value, and check the per-function score, not the
unit average.

### What still blocks most modules

- **A module's `.rodata` is not always splittable per unit.** `Tweaks` shows the shape of it:
  `config/G2ME01/rels/Tweaks/splits.txt` splits only `.text` and `.bss`, so all 0x408 bytes of the
  module's `.rodata` come from the base object `auto_03_00000000_rodata.s`, whose symbols are
  FORCEACTIVE. A `Matching` unit that contributes any `.rodata` therefore adds a second
  contribution and the module's hash breaks - and the constants a unit needs are not even
  contiguous (one unit wanted `.rodata` 0x28 and 0x30 but not 0x2C). Only a unit that owns the
  whole pool can claim it. Check the module's split before promising a unit there, and prefer the
  units whose gains are `.text` only.
- **A 99.2% wall that is not source-expressible (measured 2026-09-25, `Tweaks`).** Seven units
  sit at exactly the same two-instruction difference: retail moves the first stream pointer into
  `r4` and reuses `r4` for the switch's `propertyId`, ours uses `r3` and `r6`. Retail's own
  *Matching* units in the same module (`SLdrTweakPlayer`: 14 cases, `SLdrTweakGuiColors`: 15)
  emit the same shape ours does - the difference is the switch size. Ruled out by the lane:
  id/size type and constness, declaration order, all six case permutations, `default:` first, an
  if-chain, suffixed literals and casts. That is MWCC register allocation, and no source rewrite
  reaches it; treat these as blocked, not as unfinished.

- ~~**`UnkVtable20__6CActorFv` has no definition**~~ **Superseded, 2026-09-25** (commit `8f5b538`):
  retail's vtable slot +0x20 points at `0x8004B3E0`; the function clears the two reserved-vector
  counts at +0x110 and +0x11c and bit 7 of the byte at +0x128. It is named in `symbols.txt`, defined
  in `CActor.cpp`, and the linked DOL exports it. **Measured again on current `HEAD`** (with the
  fixed `flip_test.sh`, see the rig defects): promoting `CScriptCannonBall` no longer fails on a
  symbol at all - the DOL links and the module's REL differs
  (`build/G2ME01/ScriptCannonBall/ScriptCannonBall.rel: FAILED`), because its split claims the whole
  `.text` while only 12 of its 26 functions are at 100%. The `__ct__6CActorF...` failure recorded in
  the commit message does not reproduce on `HEAD`; the next real step for that module is the other
  14 functions, not a missing symbol.
- **`include/MetroidPrime/Enemies/` holds only the `SwarmBasics` layer** - `CSwarmBasics.hpp` and
  five `CSwarmBasics*` sources, landed with the module. There is still no `CPatterned` and no `CAi`.
  A creature module's *loader* can be reconstructed (and `Metaree` did, for the setup and accessor
  range), a swarm's accessors and hooks can be, but creature *behaviour* cannot be written until
  those base classes exist.
- **There is no GUI hierarchy at all.** Both `src/GuiSys/` and `include/GuiSys/` are empty and
  neither is listed in `configure.py` or `files.cmake`. An earlier version of this entry claimed
  `src/GuiSys/` held the decompiled `CGui*` hierarchy and only the include tree was missing - that
  was wrong, and it was written here from recollection rather than checked. The available GUI header
  is a stub. Any GUI-dependent module (ScriptGui, ScriptFrontEndDataNetwork) can do its accessors
  and loader wiring but not its widget work.
- **`ScriptGui`'s loader table** is written and verified, but the loaders it registers are named
  only by address (`fn_60_6FF0` and friends) and their bodies are not written.

## Where a module can even be written

`include/MetroidPrime/Enemies/` has only the `SwarmBasics` layer: there is no `CPatterned`, no
`CAi`, no creature base class. Any module whose objects derive from those **cannot be written at
all** until upstream lands the base classes - the loader can be reconstructed, but the actor
cannot. That is not a delegation problem, and no number of lanes fixes it.

Of the 86 modules, **11 are `Script*` units** (script objects that lean on `CEntity`/`CActor`,
which do exist) and the other 75 are creatures, bosses and swarms that need the missing Enemy
hierarchy. Acknowledge this before assigning module work: check that the base classes a module
needs actually exist.

## A cyclic link-order dependency, and why CAi cannot be added yet

A lane reconstructed `CAi` completely - all 11 retail functions compiling to identical instructions,
with its layout (0x330 bytes: CPhysicsActor, then CHealthInfo at 0x2d0, CDamageVulnerability at 0x2f0,
a state-machine token at 0x320), its 46-slot vtable and its member names. None of it can be counted.

`objdiff` pairs functions by unit, and a unit only exists when `config/G2ME01/splits.txt` gives it a
range. CAi's `.text` is `0x80096C94..0x800972BC`, immediately before `TypesMatch.cpp`'s range, so it is
inside `dtk`'s `auto_03_8009..._text` unclaimed region. Adding the range should be routine, and it
fails on something structural:

```
Cyclic dependency encountered while resolving link order:
  MetroidPrime/Enemies/CAi.cpp -> MetroidPrime/TypesMatch.cpp -> auto_03_8009D644_text
  -> ... -> MetroidPrime/CPhysicsActor.cpp -> ... -> Kyoto/Alloc/CMediumAllocPool.cpp
  -> ... -> Kyoto/Graphics/CCubeMoviePlayer.cpp -> auto_10_80419C18_sbss
```

`CAi`'s constructor references `TypesMatch`, `TypesMatch` (via the auto code that follows it) references
`CPhysicsActor`, and the chain returns to the `auto_*` region CAi was carved out of. `dtk` resolves the
DOL's link order by dependency and rejects the cycle. `configure.py` has a `link_order_callback` hook
for exactly this, commented out:

```python
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    ...
# config.link_order_callback = link_order_callback
```

A matching build returns early and never consults it, so enabling it for this is not a one-liner: it
would mean changing how the DOL's link order is resolved for a *matching* build, which touches every
unit. That is a config-level change with project-wide consequences, not a per-unit one, and it is
where this now stands.

**State of the CAi work:** complete and verified by its author, uncommitted, and reproducible from the
lane's worktree at `/tmp/opencode/c2` (`src/MetroidPrime/Enemies/CAi.cpp`,
`include/MetroidPrime/Enemies/{CAi,CPatterned}.hpp`, and header fixes to SMoverData, CHealthInfo,
CMaterialList, CDamageVulnerability and CActor::Think that made other units match better - CPlayer's
constructor went 18.05% to 21.06%). None of it is in master, because landing it requires solving the
link order first.

**CPatterned** is further off: size 0x7c0 and an 82-slot vtable are established, its constructor is
about 0xB58 bytes and was not attempted, and its 36 own virtuals are unnamed. Trilogy's Wii build of
the same code is laid out differently (0xF00-scale constructor), so its names cannot be mapped onto
slots by position.

## Parallel lanes: running many Luna workers at once

The work is run as many agents in parallel, one **lane** per module or unit. This is worth doing
properly because spawning is free and immediate while **collecting is the dominant cost** - a
dozen lanes can be in flight at once, but each one has to be verified and merged by hand.

### Spawning a lane (the whole sequence)

```sh
SRC=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port
N=m7
git -C $SRC worktree add -f /tmp/opencode/$N -b mod-$N HEAD    # cut from current HEAD
mkdir -p /tmp/opencode/$N/build /tmp/opencode/$N/orig         # REAL dirs, see below
ln -sfn $SRC/orig/G2ME01 /tmp/opencode/$N/orig/G2ME01          # disc files, read-only
cp -r $SRC/build/binutils /tmp/opencode/$N/build/binutils      # symbol tools, see below
printf '\nbuild-clone/\n' >> /tmp/opencode/$N/.gitignore
cp $SRC/docs/LANE_BRIEFING.md /tmp/opencode/$N/LANE.md           # the briefing, versioned in the repo
```

`docs/LANE_BRIEFING.md` is the briefing itself, in the repo rather than in `/tmp` so it survives a
reboot and can be corrected in a commit. The old copies lived under `/tmp/opencode/n1/LANE.md` and
were lost with the boot.

`binutils` is not generated by `configure.py`, so a fresh lane has no
`build/binutils/powerpc-eabi-nm` and `tools/check_symbol_names.py` cannot run there - and that is the
check which catches a rename that breaks every REL link. Copy it in as above, or fetch it with
`python3 tools/download_tool.py binutils build/binutils --tag 2.42-2`.

Then spawn the agent with the brief pointing at that worktree, and tell it which module it owns.

### Non-negotiable details

- **The lane's `build/` must be a real directory, not a symlink to the master's.**
  `config/G2ME01/build.sha1` names files as `build/G2ME01/...`, and `dtk shasum` reads those
  literal paths - so a lane sharing the master's `build/` has its integrity check hash the
  *master's* files and will report `87 files OK` while its own output differs. This produced
  several false "verified" reports before it was found.
- **One lane per module or unit.** Two lanes on the same unit overwrite each other's source; that
  cost a whole lane's work early on.
- **Cut from current `HEAD` every time.** A lane carries its `config/` as of its commit, so an
  older worktree silently reverts a later `symbols.txt` fix when collected.
- **`LANE.md` is the lane's only briefing.** Update it when a failure mode is found - the
  no-assembly rule and the config.yml verification both live there now, and every new lane copies
  it.
- **Lanes write to their own worktree.** One lane wrote into the master tree instead; that is
  harmless only while its edits stay uncommitted, and it makes the tree ambiguous. Point them at
  the worktree explicitly.

### Collecting a lane

1. Read its report, then **verify it independently** - lane reports have been wrong in both
   directions (one understated its own result by 10 functions, one claimed a hash that did not
   hold).
2. Apply its `src/` and `include/` changes to the master tree by copying the files.
3. **Re-check its `config/` changes against the current tree** rather than copying them - its
   `symbols.txt` may predate a fix.
4. Run the gates, including all 86 module hashes against `config.yml`.
5. Commit, with the lane's findings in the message.

### Scale

Fourteen lanes ran in one wave without compute trouble (16 cores; builds are short and bursty).
The limit is not hardware - it is collection. A wave of six to eight is comfortable to verify
honestly in a turn; more than that and reports pile up unprocessed, which is how unverified
claims reach the tree.

### What fails, repeatedly

- **Stale `config/`** - described above.
- **Vague success criteria.** "cmp silent and 87 files OK" is satisfied by doing nothing, and the
  criterion must name the state in which the check is meaningful (the unit `Matching`).
- **Assembly as a shortcut.** A transcribed `.s` unit reproduces the bytes and scores 100% while
  decompiling nothing. One lane did this for a whole module (`FogOverlay`, 1,014 instructions)
  and it was rejected. A module that can only be reproduced that way is **blocked**, not done.
- **Claiming ranges the object does not reproduce.** Breaks the module's hash for every REL. The
  fix is to claim only what reproduces - see the recipe above.
- **Assuming a module is writable.** `include/MetroidPrime/Enemies/` holds only the `SwarmBasics`
  layer, and `CPatterned`/`CAi` still do not exist (`CActor::UnkVtable20` is resolved, superseded
  above), so creature behaviour cannot be written however many
  lanes are pointed at it. Check the base classes exist before assigning a module.

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
- **The REL modules** (~11.3k functions across 86 modules). ~~The pipeline is proven for wiring
  (`AIMannedTurret` links our object and stays byte-identical) but no module has yet been
  *decompiled*~~ **Superseded, 2026-09-25:** several modules now link our own C++ and still hash
  to `config.yml` - `AIMannedTurret`, `ScriptRiftPortal`, `Metaree`, `ScriptGui`, `Puffer`,
  `WallCrawler`, `FlyerSwarm`, `ScriptSafeZone`, `SwarmBasics`, `ScriptPlayerActor`,
  `ScriptPlayerTurret`, `ScriptFrontEndDataNetwork` (the table at the end of this file is the
  current list). What remains blocked in most of them is *behaviour*, not wiring - see "What still
  blocks most modules".

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
| `ScriptRsfAudio` | wiring, 7 correct symbol names, and an empty source; hash "matched" only because the unit was `NonMatching`. The symbol names are worth keeping; the rest proves nothing. **Superseded, 2026-09-25**: a later lane added exact `RELMain`/`RELExit`, loader registration and the setup range - 8 functions, unit `Matching`, sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified against config.yml. The remaining 14 module functions stay retail/unclaimed. |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function and one field accessor) plus all 5 `REL_Setup` ones - unit `Matching`, sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified. The remaining 48 class functions stay retail/unclaimed; this is the cheapest module shape yet found (its writable code is all `.text` wiring). |
| `DarkSamusBattleStage` | scaffolded split produced a 5,184-byte REL and an assembly object would not link. Correctly reverted with 0 functions. |
| `ScriptCoin` | 3 real functions written (a class, `Render`, `GetTouchBounds`) - the first genuine C++ in a module. With the unit `Matching`, the module sha1 differs from config.yml, so its code does not reproduce it yet. |
| `Ripper` | blocked with evidence: no `CRipper`, no `CPatterned`, no `include/MetroidPrime/Enemies/` at all. Reverted the scaffold rather than claim ranges it could not fill. The range check passed, so the block is the missing base classes, not the splits. |
| `Tweaks` | 2 generated constructors brought to exactly 100% (`SLdrTweakTargeting_Scan`, `SLdrTweakTargeting_VulnerabilityIndicator`) and 3 more moved 5-40 points closer, by moving the member assignments from the constructor body into the mem-init list. Not promoted - the other 12 units are blocked (seven `LoadTypedef*` at a 99.2% register-allocation wall, three on float-literal pooling, and the module's `.rodata` cannot be split per unit). |
| `CRumbleVoice`, `CRumbleGenerator` | 0 of 10 matched, blocked: the six unmatched `fn_8032*` functions are TU-local weak `rstl::vector<SAdsrDelta>`/`<SAdsrData>` instantiations that the retail object has no symbol for. The lane wrote all of them instruction-identical by hand and objdiff still scored 0% (it pairs by name), and hand-writing `extern "C"` bodies makes the compiler emit its own copy as well. `tools/unit_fit.sh` shows 132 and 452 bytes over the claimed range from those duplicated emissions. |
| `CScriptStreamedMusic`, `CStaticAudioPlayer` | 0 of 4 matched, and both unmatched functions in each are *pure register allocation*: 54/54 and 74/74 instructions identical to retail, only the register choice (and consequent branch targets) differs - `lwz r5,0(r7)` vs `lwz r6,0(r31)`. Neither unit can flip anyway on extra emitted functions (4 and 9 of them, `CStaticAudioPlayer` 868 bytes over its range). |

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
| `CScriptCannonBall` | 12 of 26 matched, unit still `NonMatching` | was blocked on `UnkVtable20`, which is resolved; the link now fails on `__ct__6CActorF...` instead |
| `CScriptForgottenObject` | 9 of 12 at 95.86%, unit still `NonMatching` | .text/.rodata/.data a few bytes off |
| `ScriptCoin` | 3 functions written | does not hold its hash yet |
| `ScriptGui` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) + a 5-entry loader table | sha1 `2b58f6d3…` verified; widget bodies blocked, see below |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function, 1 accessor) + 5 setup | sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified; 48 class functions unclaimed |
| `ScriptRsfAudio` | 8 functions (loader registration, `RELMain`/`RELExit`) + 5 setup | sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified; 14 class functions unclaimed |
| `SkyRipple` | 7 exact of 15 named + fuzzy loader/constructor | unit kept `NonMatching` on purpose - promoting it would break the module |
| `Puffer` | 9 functions (6 + 3 in two named units) | sha1 `ab46667b…` verified |
| `WallCrawler` | 18 functions | verified; no `LoadWallCrawler` or Think to attach to yet |
