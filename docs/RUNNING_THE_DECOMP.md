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

## Upstream, and what we take from it

The decompilation we contribute to is the public `PrimeDecomp/echoes`, cloned read-only for
reference at `/tmp/opencode/upstream` (`git clone --depth 1 https://github.com/PrimeDecomp/echoes`).
Until 2026-09-25 this repository never looked at it. It is worth looking at, in both directions:

- **We are ahead in some units and it is ahead in others.** Our fork names unknown members `xNN_name`
  and keeps `fn_` prefixes with an owner annotation (`fn_80036200__13CStateManagerFv`); upstream uses
  `mName` members and real names for a different subset. In one measurement our `symbols.txt` had 434
  function names upstream lacks and upstream had 988 we lack - a mix of genuinely different progress
  and the two naming conventions.
- **There is no shared git history** (our commits are not in it and vice versa), so a merge is
  impossible. Syncing is file by file.

The decision taken (2026-09-25) is **sync where the gates pass, and say so**: a unit ported from
upstream is landed only when it is `Matching`, the DOL sha1 and all 86 REL hashes still reproduce
retail, the probe and symbol checks are green, and the commit says plainly that the code came from
upstream. Anything that does not hold there is left as a candidate, not merged.

What porting one actually costs, measured on eight candidate units from upstream `d83da79`:

| outcome | units |
| --- | --- |
| flips clean, landed | `Kyoto/Animation/CSegId`, `Kyoto/Animation/CSegIdList`, `Kyoto/CTimeProvider` (1/1, 1/1, 2/2 - all three `Matching`) |
| compiles with fixups, +26 functions, unit still `NonMatching` | `Kyoto/CToken` (5/9), `Kyoto/Text/CStringTable` (11/14), `Kyoto/CDependencyGroup` (10/13) |
| blocked | `Kyoto/Animation/CAdditiveAnimPlayback` (header size 0x28 expected, local `rstl::rc_ptr` makes it 0x24), `Kyoto/CSimplePool` (upstream's `.data` split ends inside `lbl_803BAF90` - unsafe to guess) |

The fixups are the cost: member renames to our convention, external symbol renames, a header expanded
from its stub, and a constructor moved out of a header. Do **not** copy upstream's `config/`,
`configure.py`, `symbols.txt` or `splits.txt` wholesale - merge by hand, and never touch
`config/G2ME01/config.yml`. Which units are worth porting is a judgement call: prefer the ones whose
dependencies our tree already has.

### What a port costs, measured over two batches

**Four files per unit**: a `splits.txt` entry, a `configure.py` entry, `symbols.txt` renames, and the
source. The renames are the hidden cost and they are mechanical: `tools/fnmap.py` pairs retail's
functions with the ones our object emits byte-for-byte, `tools/apply_rename.py` writes those names
into `symbols.txt`, and `tools/autorename.py` does both - it turned nine `CPakFile` functions from 0%
to 100% in one call. Check a range before claiming it with `tools/range_owner.py` (is it already
claimed?) and `tools/range_bounds.py` (does it start and end on real symbols? a split that cuts a
function in half can never be reproduced).

Second batch, measured 2026-09-25: of eight candidates, **one landed** (`CCubeSurface`, `Matching`,
2/2) and four were carried as `NonMatching` partials worth 45 exact functions (`CObjectReference`
98.53%, `NMWException` 96.02%, `CPakFile` 77.75%, `CFontImageDef` 67.16%); three were dropped for
missing dependencies (`CPlayerGunBase` needs `CWorldShadow.hpp`/`CRainSplashGenerator.hpp`,
`CCubeMaterial` needs `CGX_Impl.hpp`, `DolphinCMemoryCardSys` needs `rstl::aligned_allocator` and a
replacement for our stub `CCardFileInfo`).

Two root causes came out of it, both shared-header divergences that block whole families:

- **Our `rstl::rc_ptr` is a 4-byte `CRefData*`; retail's is 8 bytes - a pointer plus a raw `int*`
  refcount.** That makes `CVParamTransfer` 0x4 instead of 0x8 and `CObjectReference` 0x20 instead of
  0x24, and it is the same reason `CAdditiveAnimPlayback` was blocked. Every unit holding either
  class is stuck behind it. Aligning `rc_ptr` is a whole-tree change and deserves its own lane.
- **`rstl/vector.hpp` and `construct.hpp` have diverged from upstream in ways that change inlining.**
  `vector(int)` does not set `x4_count`, and the three-argument fill constructor inlines here where
  retail keeps it out of line.

`tools/flip_test.sh` and `tools/unit_fit.sh` now accept `.cp` and `.c` sources as well as `.cpp`
(`Runtime/NMWException.cp` needs `extra_cflags=["-RTTI on","-Cpp_exceptions on"]`, and both tools had
assumed the suffix in two separate places - the source-path check and the `configure.py` entry match,
which also has to survive an entry carrying extra arguments).

## The measurement rig

| tool | question it answers |
| --- | --- |
| `tools/decomp_build.sh [unit]` | ninja + objdiff + that unit's unmatched functions with per-function percentages. The worklist is `build/report.json`. |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one, section by section. Stricter than the link. |
| `tools/flip_test.sh <unit>...` | **the acceptance test.** Flips a unit to `Matching`, rebuilds, checks the DOL and all 86 RELs, keeps the flip only if retail is still reproduced byte-for-byte. |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares inside a unit's `.text` ranges, checked against what the retail-derived object defines. |
| `tools/find_trivial_functions.py` | unmatched functions classified by the shape of their machine code - the cheap-work queue. |
| `tools/unit_fit.sh <unit>` | why a unit will not promote: claimed range vs our object's sections, and the functions we emit that the retail unit object does not define. |
| `tools/check_module_wiring.py` | is every module with sources in `src/` actually wired into the build - catches a `Rel(...)` block lost to a config clobber, and counts the modules that link our own code. |
| `tools/range_owner.py <section> <start> <end>` | which unit claims a split range, if any - before carving one for a new unit. |
| `tools/range_bounds.py <start> <end>` | does a proposed range start and end on real symbols in retail. |
| `tools/fnmap.py <unit>` | byte-identical function pairing between a unit's retail object and ours (the mechanical half of a port). |
| `tools/autorename.py <unit>` | rename every byte-identical `fn_` function after our own symbol, via the two above. |
| `tools/apply_rename.py` | apply `old=new` renames to `symbols.txt` from stdin, reporting any it could not find. |
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

### Pairing a function the retail symbol table has no name for

Four units were blocked in one session by the same thing, and it is solvable. `dtk` cannot name a
TU-local weak template instantiation, so the base object calls it `fn_803254FC`; objdiff pairs by
name, so the byte-identical function our compiler emits scores 0%, and hand-writing an `extern "C"`
body under that name only makes MWCC emit its own copy as well.

**Rename the retail symbol instead** - `config/G2ME01/symbols.txt` *is* the rename mechanism, and
`dtk` will name the base-object symbol accordingly:

```
__dt__Q24rstl47vector<10SAdsrDelta,Q24rstl17rmemory_allocator>Fv = .text:0x803254FC; // type:function size:0x84
```

Do not guess the name: write the function in the unit's source, compile, and read the name MWCC
emitted out of our own object with `build/binutils/powerpc-eabi-nm`. Measured 2026-09-25 on
`Kyoto/Input/CRumbleVoice`: six functions went from 0% to 100% this way, taking the unit from 8/16
to 13/16 and the project from 2759 to 2764.

Two limits, both measured: it **enables pairing, not matching** - the code still has to be
byte-exact - and it does not fix a unit that emits functions retail does not have
(`tools/unit_fit.sh` still lists `__dt__rstl::reserved_vector<ushort,4>` and a second fill
instantiation there, 180 bytes over, which is why the unit still cannot be promoted). Constructors
declared inline emit no standalone helper symbol, so there is no source shape that suppresses the
extra destructor while keeping these pairs.

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

### A technique that works: model the retail layout, do not fight it

`MetroidPrime/ScriptLoaderRel.cpp` reached 42/42 and became `Matching` on 2026-09-25 by naming the
problem correctly. Retail keeps each script-loader global as an 8-byte slot - a pointer plus a
padding word - and its setters are real mangled functions (`ScriptGUI_SetPtrs__FP10GUILoaders`,
`SetLoader_SafeZone`, ...), not the invented C++ names the file used. Modelling the slot as a
one-member template with an implicit conversion and an assignment operator reproduces both the BSS
layout and the code shape:

```cpp
template < typename T >
struct SLoaderSlot {
  T* value;
  uint padding;
  operator T*() const { return value; }
  SLoaderSlot& operator=(T* ptr) { value = ptr; return *this; }
};
```

Two lessons from the same session, both cheap and both general: **a BSS slot's padding is part of
the layout** - if a unit's `.bss`/`.sbss` is a few bytes off, look for a word the original kept for
alignment - and **name the function what the retail symbol says**, using `symbols.txt` to check,
rather than inventing a friendlier name; objdiff pairs by name, so an invented name scores 0%
however identical the code is.

The same lane moved `CScriptAreaProperties::LoadAreaProperties` from 0.00% to 81.25% with a
partially reconstructed body (the unit is still `NonMatching`).

### A technique that works: `const` on by-value parameters, and a `const` local

Three of four small leaf functions that a lane brought to 100% on 2026-09-25 were fixed by a change
that cannot affect the symbol or the callers:

```cpp
void CHealthInfo::SetCauseOfDeathWeapon(CWeaponMode mode, TUniqueId id) const { ... }  // retail's mask lands in r5
void CHealthInfo::SetCauseOfDeathWeapon(const CWeaponMode mode, const TUniqueId id) const { ... }  // ours does too
```

Top-level `const` on a by-value parameter does **not** appear in the mangled name, so it is invisible
to every other unit, and it is enough to move MWCC's register allocation. The same lever in a
function body is a one-statement `const` local of the value being compared - `const bool swap = flag;`
used throughout made retail mask the bool in place in `r4` instead of `r0` in
`CGameOptions::ToggleControls`; replacing a one-line ternary with two locals fixed
`InitSoundMode`. Confirm per function with the report: it can move a score the wrong way too.

### A negative result: constant-trip-count loops are always unrolled

`CPlayerState::InitializeScanTimes` sits at 97.63% and cannot be finished. Retail's first loop is a
`for`/`while` shape - the test block sits *after* the entry branch - and its iterator is in `r6`
where ours is in `r7`. But MWCC fully unrolls any `for`/`while` with a constant trip count in these
units (5-8 iterations, 38-42 instructions of difference), and the only shape it does *not* unroll,
`do...while`, is exactly the one that cannot produce retail's entry branch. Every form was tried.
Treat a retail loop with a test-after-entry and a constant count as blocked, not as unfinished.

### Rig trap, found and fixed 2026-09-25: a failed flip left a broken DOL behind

`tools/flip_test.sh` reverts `configure.py` when a flip fails, but it did not rebuild - so
`build/G2ME01/main.dol` went on holding the binary the failed flip had produced, and the next
`sha1sum` read that instead of retail. A lane hit it and reported `ee273df2...` as the DOL hash.
The script now rebuilds after reverting and prints the restored hash, or says loudly that the rebuild
failed. If you ever see a DOL hash that is not `6ef9b491...` with a clean `git status`, rebuild before
investigating anything else.

### Identifying an unnamed class from its vtable (the TypesMatch ids)

`TypesMatch.cpp` needs the classes behind 32 type ids, and the tree names none of them. The recipe
that worked (2026-09-25, worth 94 functions):

1. Find the vtable that holds the id. `dtk dol info config/G2ME01/config.yml` prints the section
   table; search the DOL's `.data`/`.rodata`/`.sdata` for the id as a word. A class's vtable starts
   with `[0][0][dtor][TypesMatch]`, then the flat `CEntity`/`CActor` slots, so the entry that points
   at the id's `TypesMatch` pins the whole table - and the table gives you the class's own virtuals,
   which is what its member helpers will be.
2. The class's **parent** is the class whose `::TypesMatch` its override calls. Read it off the
   call, not off a plausible-looking name.
3. Name the class, then rename the retail symbol: `config/G2ME01/symbols.txt` carries the
   `TypesMatch__<class>CFi` and both `TCastToPtr<...>` names, and dtk will name the base object from
   them. Take the mangled names from our own compiled object (`powerpc-eabi-nm`).

Two traps, both found the hard way:

- **A rename must replace its `fn_` line, never be inserted beside it.** Two symbols on one line is
  a parse error for dtk (`invalid digit found in string`) and the whole build dies, so it is caught
  immediately - unlike a rename that is *dropped*.
- **dtk rewrites `config/G2ME01/symbols.txt` on every build and drops a symbol that duplicates an
  address.** That is why the reference-form cast has to replace its `fn_` line: leave both and the
  reference form silently stays unnamed and scores 0.00%, which reads exactly like "the code is
  wrong".

`docs/research/TypesMatch_unnamed_ids.txt` is the table this produced - per id: parent, `TypesMatch`
address, both cast addresses, vtable address, the class's own virtuals. `docs/research/rename_typesmatch_ids.py`
regenerates the `CUnknown<id>` block from it; put a real name in `CLASS` and re-run when one is found.

### Writing a destructor whose class is only a type id (+16 functions, 2026-09-25)

Once the class exists, its own virtual destructor in the same unit is writable, and it is what
establishes the members. Two MWCC rules decide whether the derived destructor matches, and both cost
a build to find:

- **MWCC inlines a base destructor only when the base destructor is compiler-generated.** With
  `~Base();` declared in the class and defined out of line, the derived destructor emits
  `bl ~Base` (as it must for a base in another TU) and lands ~20% short. Delete the declaration *and*
  the out-of-line definition and MWCC still emits `__dt__<Base>Fv` for the vtable, but the derived
  destructor then inlines the base's body and matches byte for byte. `CUnknown50` is exactly that: an
  empty `CScriptDamageableTrigger` subclass.
- **MWCC emits a null guard (`addic. r0,rN,off; beq`) in front of a member's destructor when the
  member's destructor is defined in its class, and none when it is only declared.** One instruction
  apart, and getting it wrong costs the *outer* destructor its 100% while the member's own
  destructor still matches.

Other things that were true here, all measured:

- **objdiff pairs by symbol name and ignores relocation targets in an unlinked object**, so a retail
  `fn_8009D51C` that is our `SRefHolder::Release` is a naming problem, not a codegen problem.
- A destructor of a class with no members and a polymorphic base is `if (this) { vptr = ...;
  if (flag > 0) Free(this); }`.
- `rstl::optional_object<T>` puts its valid flag at `round4(sizeof(T))`, so the offset of the byte a
  destructor tests *pins* `sizeof(T)` - that is how id 63's member was identified as
  `optional_object<TCachedToken<T>>` rather than guessed.
- **Corrected here: ids 40, 46 and 68 derive from `CUnknown33`, not `CActor`** - their overrides all
  call `TypesMatch__10CUnknown33CFi`, which the first pass through this list got wrong.

### A DOL unit can be blocked by data, not by code

`Kyoto/Graphics/CGX` matches every function it can (51 of 54, 99.47%) and **is not promotable**,
because its *sections* cannot be reproduced by C++ source:

- `CGX::sGXState` is a COMMON symbol for us and a real `.bss` object in retail, so our `.bss` is 0
  bytes against a claimed 612.
- `SetAlphaCompare` and the state constructor reference five small-data words
  (`lbl_8041E4A0/A4/A8/AC`, `lbl_8041F8D8`) that dtk attributed to
  `auto_11_8041E278_sdata2.o` / `auto_10_80419828_sbss.o`, and our object defines them locally as
  anonymous constant-pool words with identical *values*. No C++ source shape makes a
  compiler-generated float-constant pool external.

`tools/unit_fit.sh` now prints sections our object carries that `splits.txt` never claims, which is
what surfaces this: CGX reports `.sdata2` and `.sbss2` unclaimed and `.bss` 612 bytes short. When a
unit's *functions* are all matched and it still will not promote, look there before looking at the
code again.

Two more negatives from the same lane, so nobody spends a session on them:

- Adding `operator=(const T*)` to `rstl::single_ptr` to match retail's `__as__...FPQ2...` mangling is
  not viable: it makes the `= nullptr` idiom ambiguous tree-wide (MWCC stops at `CActor.cpp:255`,
  `CCubeMoviePlayer.cpp:412`, `:536`, `:588`, `:909`) and MWCC inlines the 8-byte body at `-O4,p`
  anyway, so the out-of-line symbol never appears.
- `decomp_build.sh <unit>`'s per-function percentages are the ground truth. A bare two-object
  `objdiff-cli diff` disagrees on units that set `reverse_fn_order` (it reports 99.6x% for functions
  the project counts as matched). Score with the tool, not with the raw diff.

### MWCC's inlining and scheduling levers, and their limits

Collected from lanes on 2026-09-25, all measured:

- **`#pragma noinline` is the only working no-inline pragma** in this compiler; MWCC 2.7 (GC) rejects
  `__declspec(noinline)`. Putting it above an out-of-line destructor is what lets retail's
  out-of-line call site stay out of line (`auto_ptr<CDependencyGroup>::~auto_ptr` 69.84% -> 100%).
- **A 4x loop unroll needs a straight-line body, and `rstl::construct`'s placement-new form puts a
  branch in it** - MWCC then guards the store with the address arithmetic's flags (`add.`/`beq`) and
  the unroll is lost. A file-local `construct<T>` specialisation in assignment form restores it:
  `CDependencyGroup::ReadFromStream` went 34.97% -> 98.65% that way, unrolled 4x exactly as retail.
- **MWCC hands out callee-saved registers in declaration order.** In `rstl::algorithm.hpp`'s
  `lower_bound`, moving `It it;` inside the loop after `halfDist` is what puts `halfDist` in `r30` and
  `it` in `r29` like retail - a pure declaration-order difference.
- **A unit at 100% of its functions can still be unpromotable, and the tool that says so is the DOL,
  not the percentage.** `CStringTable` and `CDependencyGroup` are at 98.84% with every fixable
  function fixed; flipping them adds 384 and 800 bytes to the binary respectively because our objects
  emit container/COMDAT code retail does not have there. Measure it by flipping and diffing the DOL
  size, not by looking at the fuzzy number.

### A negative result that saves a family: `rstl::rc_ptr` is 8 bytes, and flipping it costs 18 functions

Measured 2026-09-25, and it is worth recording because the same wrong model was re-derived three
times. Retail's `rstl::rc_ptr<T>` **is 8 bytes**, and its word 1 is never initialised or read:

- `ReleaseData__Q24rstl20rc_ptr<10IVParamObj>Fv`: `lwz r4,0(r3); lwz r3,4(r4); addic. r0,r3,-1;
  stw r0,4(r4)` - word 0 is a `CRefData*`, and the count is **inside** the CRefData at +4.
- `rstl::CRefData` does exist in Echoes and is 8 bytes `{ptr, int}` with a static `sNull`
  (`R_PPC_EMB_SDA21 sNull__Q24rstl8CRefData` in `CStateManager.o`). Upstream's `CRefData` (4 bytes,
  count at +0) and its `rc_ptr {ptr, int*}` are both wrong.
- `CToken(IObj*)` does `li r3,36`, so `CObjectReference` is 0x24 - one word more than its members sum
  to with a 4-byte `rc_ptr`; `CAdditiveAnimPlayback` at 0x28 agrees.

**Do not flip the header on its own.** A lane did, with the gates green and the DOL unchanged, and
measured the whole tree: **0 functions gained, 18 lost at 100%** (13 `CStateManager`, 2 `CActor`,
1 `CPlayerState`, 1 `CPlayerGun`, 1 `CScriptCannonBall`), 30 more regressed, only 7 units' code moved
at all - and `CObjectReference` itself was byte-for-byte unchanged, still 8/10. The reason is that
every class embedding an `rc_ptr` has its later members shifted +4, and several of them carry filler
words written for the 4-byte model (`CStateManager` alone has four; `CAnimData` has six `rc_ptr`s).
So this is a **per-class offset-repair job** - fix the members of every rc_ptr-embedding class, then
move the header - not a shared-header change. As a header flip it is a regression, and the
`CObjectReference` constructors are stuck on something else entirely (the `Null()`/`GetFactory()`
call shape), not on `rc_ptr`'s size.

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
  five `CSwarmBasics*` sources, landed with the module - and now `CAi` (11/11) and `CPatterned`
  (10/10) as `Matching` units too. The hierarchy exists; what remains thin is the *behaviour*: the
  creature classes' own virtuals are largely unnamed and `CPatterned`'s constructor is unwritten.
- **There is no GUI hierarchy at all.** Both `src/GuiSys/` and `include/GuiSys/` are empty and
  neither is listed in `configure.py` or `files.cmake`. An earlier version of this entry claimed
  `src/GuiSys/` held the decompiled `CGui*` hierarchy and only the include tree was missing - that
  was wrong, and it was written here from recollection rather than checked. The available GUI header
  is a stub. Any GUI-dependent module (ScriptGui, ScriptFrontEndDataNetwork) can do its accessors
  and loader wiring but not its widget work.
- **`ScriptGui`'s loader table** is written and verified, but the loaders it registers are named
  only by address (`fn_60_6FF0` and friends) and their bodies are not written.

## Where a module can even be written

`include/MetroidPrime/Enemies/` now has `CAi` and `CPatterned` as `Matching` units, so a module
whose objects derive from them *can* be written - that was the blocker, and it is gone. What limits
those modules now is the behaviour inside the classes: most of the creature virtuals are unnamed,
`CPatterned`'s 0xB58-byte constructor is unwritten, and 75 modules' worth of actor code has to be
decompiled one function at a time like anything else.

Of the 86 modules, **11 are `Script*` units** (script objects that lean on `CEntity`/`CActor`,
which do exist) and the other 75 are creatures, bosses and swarms that need the missing Enemy
hierarchy. Acknowledge this before assigning module work: check that the base classes a module
needs actually exist.

## CAi: landed, and the "cyclic link-order dependency" was never real

`MetroidPrime/Enemies/CAi.cpp` is `Matching` and complete (11 of 11 functions, 100%) as of
2026-09-25. The DOL sha1 and all 86 RELs still reproduce retail with its own object in the link.

**The cycle did not exist.** The earlier session's conclusion - that adding CAi's range fails on a
cyclic dependency and needs a project-wide change to how the DOL's link order is resolved - is
wrong, and the `configure.py` `link_order_callback` question it raised was never the issue. Claiming
the ranges makes `dtk dol split` accept the graph immediately; the build goes straight to the link.
What looked like a cycle is **CodeWarrior COMDAT weak symbols** - inline virtuals and template
destructors emitted in many translation units. The retail linker kept one copy each and discarded
the rest; `mwldeppc` discards them too, so they are harmless. This is also why
`tools/unit_fit.sh` reports `CAi` as carrying 224 bytes of "extra" functions while the flip holds:
those bytes are weak copies that never reach the binary.

What the unit actually needed was ordinary work, in this order:

1. **Claim all four sections, not just `.text` and `.data`:**
   ```
   MetroidPrime/Enemies/CAi.cpp:
   	.text       start:0x80096C94 end:0x800972BC
   	.data       start:0x803B29E0 end:0x803B2A98
   	.sdata      start:0x80417FC8 end:0x80417FD8
   	.sdata2     start:0x8041AD38 end:0x8041AD50
   ```
   Leaving `.sdata`/`.sdata2` unclaimed moved every `lfs`/`bl` displacement and every REL import
   address (`0x8041B758` -> `0x8041B778`) and broke 71 RELs.
2. **Rename every function in the range** in `symbols.txt` to the mangled name our object emits,
   because retail's other units reference them by name. Eleven of them, e.g.
   `fn_80096EBC` -> `AcceptScriptMsg__3CAiFR13CStateManagerRC10CScriptMsg`. Read the names from our
   own object with `powerpc-eabi-nm`; pair by disassembly, not by size. `GetStateMachine2` emits
   *before* `GetStateMachine` even though the source is the other way round - their bodies are
   identical, so the pairing is by emission order and the names are a guess pinned to it.
3. **Rename the slots and callees the link complains about, one at a time**, each because `mwldeppc`
   printed `undefined:` for it: `fn_8004A0D8` -> `Think__6CActorFfR13CStateManager`, the four vtable
   copies the linker had kept elsewhere (`fn_800358D8`, `fn_80073CAC`, `fn_8003C59C`, `fn_80073CB4`,
   all `CAi` members whose bodies are identical to the CAi ones), and ten callees
   (`fn_8001C814` -> `__ct__10SMoverData...`, `fn_80070D60` -> `__ct__11CHealthInfoFRC...`,
   `fn_80072560` -> `GetTriggerBoundsWR__14CScriptTriggerCFv`, and so on). Deleting a dead rename is
   not optional: rename `fn_8003C59C` and `CStateManager` loses one *bookkeeping* match because our
   `CStateManager.o` does not define it - the bytes and the DOL are unchanged.
4. **A `.sdata2` split may not end inside a dtk `lbl_` symbol.** `lbl_8041AD50` spans
   0x8041AD50..0x8041AFE0, so CAi can only carve 24 bytes while its object wants 28. The seventh
   float constant was declared `extern` and its pool address named `kCAiSplashDenom` in
   `symbols.txt` - exactly what retail's linker did.

**Correction to an earlier claim in this file.** The orchestrator's reading of the disassembly -
that `fn_80096F8C` "takes no arguments, so the accessor's signature is wrong" - was wrong. The
retail vtable relocates `+0x38` to `HealthInfo` and `+0x3c` to
`GetHealthInfo__6CActorCFRC13CStateManager`, so `+0x38` *is* `CActor::HealthInfo(CStateManager&)`'s
slot and the parameter has to stay; the override merely ignores it. A no-argument accessor would add
a vtable slot and break the 46-slot table. Read the slot's neighbours before changing a signature.

**`CPatterned` landed too** (2026-09-25, same day): a `Matching` unit, 10 of 10 functions, by
*not* attacking its 0xB58-byte constructor. The lane disassembled the vtable cluster instead and found
fifteen tiny accessors at `0x80073BF0..0x80073D14` - mostly `li r3,0; blr` - of which ten reproduce
exactly; those are claimed (`.text 0x80073C58..0x80073CB4`, 92 bytes) and the other five are left
retail. The class now exists, CPatterned's vtable relocations resolve, and the constructor is blocked
on nothing but the constructor.

Five things that cost that lane a build each, all general:

- **A translation unit is emitted in *reverse* source order.** The source had to be written bottom-up
  for its functions to land in retail address order. Anyone hand-writing a `Matching` DOL unit needs
  this; it is also why `GetStateMachine2` emits before `GetStateMachine` in `CAi`.
- **A function that belongs to another class is defined with that class's qualifier even when it lives
  in this TU**: `CAi::IsListening` sits among CPatterned's functions but mangles `__3CAi`; as
  `CPatterned::` it breaks the vtable relocation.
- **Writing a float literal re-creates the pooled `.sdata2` entry** and shifts every address above it
  (the `kCAiSplashDenom` trap again). Reference the existing word - `extern const float
  lbl_8041B758; return lbl_8041B758;` - instead of typing the value.
- **A bitfield's position counts from the LSB, and MWCC emits shift *n+1* for a field named
  `x34c_n_`.** A `rlwinm r3,r0,29,31,31` that looks like `x34c_25_flyer` is actually
  `x34c_28_notFlyer`.
- **A private virtual can return by reference**: slot 73 is `addi r3,r3,1876; blr` - `&this+0x754`,
  not a pointer load.

Two of the cluster's functions are characterised rather than finished: `GetOrigin` (5 of 7
instructions - MWCC hoists the second and third `lfs` above the first `stfs`, and no source shape
tried stopped it) and `GetTouchBounds` (26 of 26 instructions, but the epilogue restores `r0` before
`r31` where retail restores `r31` first). Both are single-instruction-class walls, not logic.


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

**Reference the model explicitly, including its reasoning variant.** `Space Bunny Free` exposes
`low`/`medium`/`high`/`xhigh`/`max`, and a lane spawned without a variant runs at the provider's
default - which nothing in the lane's report reveals, so a weak result reads exactly like a hard
task. Space Bunny lanes are spawned as `opencode-go/space-bunny-free#max`.

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

- **Stale `config/`** - described above. **And stale `configure.py`, which is worse in one way:
  it fails silently.** Three modules (`Puffer`, `WallCrawler`, `ScriptGui`) lost their `Rel(...)`
  blocks to commits that copied an older `configure.py` (`33b73a3` replaced Puffer's block with
  WallCrawler's own; `f599488` dropped the other two). Their sources sat in `src/` compiled by
  nothing, their units still appeared in the report - because `config.yml` lists every retail module -
  and they read 0.00%, which looks like "not started" rather than "not wired". Restoring them was
  worth 30 matched functions. **Run `python3 tools/check_module_wiring.py` after any config merge**,
  and never copy `configure.py` from a lane.
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
| `CRumbleVoice`, `CRumbleGenerator` | `CRumbleVoice` now matches **five** of them (8/16 -> 13/16) after the fix below; 0 of `CRumbleGenerator`'s. The unmatched `fn_8032*` functions are TU-local weak `rstl::vector<SAdsrDelta>`/`<SAdsrData>` instantiations with no name in the retail object, so objdiff scored them 0% even when the bodies were byte-identical. **Solved for pairing** by writing explicit specialisations in the source and renaming the retail symbols in `symbols.txt` to the mangled names MWCC emits (read them from our own object with `nm`) - see "Pairing a function the retail symbol table has no name for". Neither unit can be promoted yet: `CRumbleVoice` emits 180 bytes the retail unit object does not have, `CRumbleGenerator` 452. |
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
| `ScriptStreamedMovie` | 6 functions: a local `CModelData` constructor wrapper (`__ct__10CModelDataFv`, which forwards to the DOL's routine) + `RELMain`/`RELExit` and the 3 setup functions | sha1 `d9b45eae…` verified; the rest of the module stays retail |
| `RubiksPuzzle` | 6 functions: `SLdrRubiksPuzzleData::SLdrRubiksPuzzleData()` (state machine `0xFFFFFFFF`, rotation speed from `.rodata`) + `RELMain`/`RELExit` and the 3 setup functions | sha1 `a29343f9…` verified; the rest of the module stays retail. The lane checked the base classes exist before starting, which is why this one was writable |
| `SkyRipple` | 7 exact of 15 named + fuzzy loader/constructor | unit kept `NonMatching` on purpose - promoting it would break the module |
| `Puffer` | 9 functions (6 + 3 in two named units) | sha1 `ab46667b…` verified |
| `WallCrawler` | 18 functions | verified; no `LoadWallCrawler` or Think to attach to yet |
