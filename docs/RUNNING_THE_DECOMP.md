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
**A sync that takes upstream's version of a file drops the port's `#ifdef TARGET_PC` blocks in it,
and every gate stays green.** The 2026-09-28 base merge `aab3f15` did exactly that in 13 files (the
game allocator, `rmemory_allocator`, `CInputStream`, `CPakFile`, ...), plus host-width fixes that
were not under `TARGET_PC` at all (`kAllocatorPointerBits`, the small-pool index width, `sizeof`
in the pool placement-news). mwcceppc never sees those lines, so the DOL, REL and probe gates cannot
notice; only `tools/boot_probe.sh` does, and it crashed in `CGameAllocator::Initialize`. After any
sync, compare `git grep -c TARGET_PC` against the pre-sync parent and re-apply what vanished,
adapting to upstream's member names. Restored 2026-09-29; see `docs/research/boot_probe.md`.
**A sync that names a retail address breaks every `Matching` carve that referenced it by its old
label, and the fix is to spell the new name, not to keep the old one.** `symbols.txt` holds one name per
address, so once upstream names `0x801D5E9C` `AcceptScriptMsg__10CAuxWeapon...` a carve calling
`fn_801D5E9C` is an undefined symbol in the DOL link (2026-09-30, upstream 03bd14b: four `CAuxWeapon`
carves and `CConsoleOutputWindowCtor.cpp`). A C or `extern "C"` identifier can carry any mangled name
made of identifier characters, so the carves now declare `AcceptScriptMsg__10CAuxWeaponFR13CStateManagerRC10CScriptMsg`
directly. A name with `<` or `>` in it (`reserve__Q24rstl36vector<f,Q24rstl17rmemory_allocator>Fi`) cannot be
spelled that way; declare the explicit specialisation under `__MWERKS__`,
`template <> void rstl::vector< float >::reserve(int size);`, and call through it - mwcceppc mangles it
to exactly that name, and no body is emitted. Keep the old `lbl_` name instead when upstream's name is a
local one (`@stringBase0` at `0x803A89E8`), which nothing outside its own unit can reference.
**When upstream models a structure we had modelled differently, take upstream's and re-fit our
bodies to it** (decided 2026-10-01, eighth sync, 8bb7bd0f). Upstream is the base of every later
sync, so a private layout or name is a conflict that is paid again each time. In practice: take
upstream's header, re-add our out-of-line declarations, keep our `.cpp` bodies, and rename members
driven by compile errors; delete carves and port-only files whose range or job an upstream unit now
covers (`PortModuleManager.cpp` -> `CRelFile.cpp`), moving their `TARGET_PC` parts into it. Three
things that sync measured:
- **Judge a sync against the pre-merge head's report, not the worktree's own
  `build/report.base.json`.** The scratch worktree's was days old (9270 matched) and reported
  thirteen losses; against the judge's (`../wt-mp2-goal-L1/build/goal/judge/report.base.json`,
  passed as `tools/gate.sh <path>`) there were two.
- **A renamed function whose body became the implicit one reads as `GONE`, and it is a real loss.**
  `__dt__CGameGlobalObjects_80006518` became `__dt__18CGameGlobalObjectsFv`; an empty
  `~CGameGlobalObjects() {}` scores 92.09%. An `extern "C"` function may carry the mangled name of a
  member the class declares, so the hand-written teardown went back under the real name at 100%.
- **`tools/boot_probe.sh` only exits by itself with `MP_PORT_FRAMES` set**; without it the frame
  loop runs until the timeout kill (exit 137), which is not a failure. The judge sets 300.
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
| `tools/decomp_build.sh [unit]` | ninja + objdiff + that unit's unmatched functions with per-function percentages. The worklist is `build/report.json`. |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one, section by section. Stricter than the link. |
| `tools/flip_test.sh <unit>...` | **the acceptance test.** Flips a unit to `Matching`, rebuilds, checks the DOL and all 86 RELs, keeps the flip only if retail is still reproduced byte-for-byte. |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares inside a unit's `.text` ranges, checked against what the retail-derived object defines. |
| `tools/find_trivial_functions.py` | unmatched functions classified by the shape of their machine code - the cheap-work queue. |
| `tools/unit_fit.sh <unit>` | why a unit will not promote: claimed range vs our object's sections, and the functions we emit that the retail unit object does not define. |
| `tools/gate.sh [--baseline]` | **the whole acceptance test in one command**: configure, ninja (whose exit status *is* the hash check), an independent re-hash against `config.yml`, the per-function report diff, wiring, docs claims and the probe. Non-zero exit on any failure. `--baseline` records `build/report.base.json` from a clean tree. |
| `tools/report_diff.py <base> <new>` | per-function diff of two reports: `WORSE`, `GONE`, `UNLINKED`, and a fell linked total. Replaces the hand-typed comparison that missed things. |
| `tools/check_module_wiring.py` | is every module with sources in `src/` actually wired into the build - catches a `Rel(...)` block lost to a config clobber, and counts the modules that link our own code. |
| `tools/range_owner.py <section> <start> <end>` | which unit claims a split range, if any - before carving one for a new unit. |
| `tools/range_bounds.py <start> <end>` | does a proposed range start and end on real symbols in retail. |
| `tools/fnmap.py <unit>` | byte-identical function pairing between a unit's retail object and ours (the mechanical half of a port). |
| `tools/autorename.py <unit>` | rename every byte-identical `fn_` function after our own symbol, via the two above. |
| `tools/apply_rename.py` | apply `old=new` renames to `symbols.txt` from stdin, reporting any it could not find. |
| `tools/scaffold_rel_module.py` | the three artifacts needed to start a REL module, printed or `--write`. |
| `tools/probe_sources.sh` | the port build's **compile and link** sweep: 819 files, must stay 0 failures. |
| `tools/sync_files_cmake_excluded.py` | derives `check_files_cmake.py`'s `EXCLUDED` list from the tree: prunes entries for sources that are now listed, reports `Matching` objects in neither list. `--check` for a gate step. A hand-maintained list describing a tree that changes every commit will be wrong. |
| `tools/probe_cc.sh <src> <out.o>` | compile **one** scratch source with the exact `MWCC GC/2.7` flags a DOL unit gets - the fastest way to ask what mwcceppc does with a body before giving it a unit. The argument order is `wibo sjiswrap.exe mwcceppc.exe <cflags> -c <src> -o <out.o>` and the two `-pragma` options need their quotes kept, or the compiler reports `Specified file 'off' not found` and silently produces an unrelated object. |
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
**`configure.py` does *not* refuse a `Matching` object with no source file** - an earlier version of
this paragraph said it did, and that was wrong. `tools/project.py` prints `Missing source file
<path>`, sets `link_built_obj = False` and links the **retail** object instead, so the unit looks
`Matching` and is not ours. That is how `flip_test.sh` used to report `PASS` for units that proved
nothing; it now refuses, and `tools/gate.sh` greps the configure log for that line. It only prints it
when `warn_missing_source` is set or the unit is `completed`, so silence means nothing either.
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
**Correction, 2026-09-25: the `CStaticAudioPlayer` half of that bullet was wrong.** It is the one
thing in this file most likely to be copied forward, so it is corrected in place. Those 868 bytes
are **not** what blocks that unit: the DOL link passes `-strip_partial`, mwldeppc deletes the
duplicate weak copies from the middle of the section, and the flipped `main.dol` comes out the
**same size as retail**. Re-measured: the unit is 23/24 and its flip now fails on the *order* of
those instantiations, not their presence - see "An emission-order wall: out-of-line template
instantiations" below, which has the ELF-symbol proof. The other half of the bullet stands: a
strong (non-weak) extra definition, or a weak one that no other object also defines, is real and
does block the flip. The way to tell the two apart in one build is the ELF symbol check in that
new section; `unit_fit.sh`'s list alone cannot.
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
## A REL unit that defines a function nothing calls cannot be flipped (2026-09-25)
`ForgottenObject`'s unit cannot promote, and one of the two reasons is not a source problem at all.
With the unit `Matching`, the linked module comes out **2736 bytes instead of retail's 2832** -
96 short - because `fn_24_1E4` (module `.text` 0x1E4..0x238, 84 bytes) is referenced by nothing in
the module and **mwldeppc dead-strips it**, along with the 8-byte `.rodata` and 4-byte `.text` gaps
that leaves. `build/G2ME01/<Module>/ldscript.lcf` is written by `dtk dol split`, and its FORCEACTIVE
block holds only the module roots (`_prolog`, `_epilog`, `_unresolved`, `_ctors`, `_dtors`),
everything reachable **from data** - the vtable's functions, the rodata objects - and the data
objects themselves. An orphan that only code called is not in that set.
`config/` cannot influence it: putting `scope:global` on `fn_24_1E4` in `symbols.txt` and re-running
`dtk dol split` leaves the lcf byte-identical, and the file lives under `build/` and is regenerated
by the `split` ninja rule, so no `configure.py` edit can carry it. Adding the one line `fn_24_1E4`
to the FORCEACTIVE block is enough - nothing else needs force-active - and with it the module is
**2832/2832 bytes with 55 differing bytes in 19 runs**.
**So the fix is in dtk, or a post-split hook in `tools/project.py`** taking a per-module extra
list, e.g. `config/G2ME01/rels/<Module>/forceactive.txt`, whose content for `ForgottenObject` is
one line. Until that lands, **no REL unit that defines an uncalled function can be flipped** -
a class of module, not one module. It is the same shape as the build-clone defect below: a rig
property silently deciding whether a unit's work counts.
**Superseded 2026-09-29: the dead-strip is fixable from `config/`.** dtk 1.8.4 accepts a per-module `force_active:` list in `config/G2ME01/config.yml` and writes it into the generated `ldscript.lcf`'s FORCEACTIVE block; tested with `force_active: [fn_24_1E4]` on `ForgottenObject`, which kept the function in the link. The `scope:global` result above still stands (that does nothing), but no dtk patch or `tools/project.py` hook is needed. `ForgottenObject` stays unflipped for its other reasons: 3 functions still differ (55 bytes) and `__vt__22CScriptForgottenObject` is not emitted by our unit.
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
**Every module's `REL_Setup` tail is free, and `tools/wire_rel_setup.py` claims it (2026-09-28).**
The shared "REL" lib compiles `_unresolved`/`_epilog`/`_prolog`/`ModuleDestructors`/
`ModuleConstructors` for every module; a module links them once its `splits.txt` claims the
range. 27 modules had no claim at all and went 0 -> 5 each (+135 matched, +135 linked, all 86
hashes held) with no C++ written. The claim alone breaks the hash, by 48 bytes of relocations:
`_epilog`/`_prolog` call `RELExit`/`RELMain`, and in these modules those were unnamed
(`fn_55_104`) or named without `scope:global` (AtomicAlpha), so the reference stayed unresolved.
The tool names all four, reading the entry points from dtk's disassembly between two builds.
It judges nothing - build, check the hash, and revert a module that moved.
### Four structural facts about a REL split, learned wiring `ScriptCoin` (2026-09-25)
Each of these cost a lane something, and all four are properties of the arrangement rather than
of the module:
1. **One unit cannot claim two discontiguous ranges.** `dtk dol split` fails with
   `Cyclic dependency encountered while resolving link order: ...Rest.cpp -> ...Coin.cpp`. So a
   module with k separated claims needs k files and k unit entries - which is why `ScriptCoin`
   has six. (`CAi` gets away with four ranges because they are four *sections*, not two ranges
   within one section.)
2. **The scaffold's name for the REL tail is the wrong unit name, not the wrong range.**
   `REL/REL_Setup.cpp` claiming `.text 0x36A4..0x3848` plus `.rodata 0x68..0xEC` builds and links
   and still **breaks the module hash**: the GOT grows 40 bytes and the `bl` in `_epilog`/`_prolog`
   gets a real displacement where retail holds a placeholder. The *same ranges* under a
   module-unique name (`CScriptCoinTail.cpp`) hash correctly. Naming the tail `REL/REL_Setup.cpp`
   at any other start fails to link outright with `multiply-defined: '_unresolved'`.
3. **A `Matching` unit's `.text` is dead-stripped unless something in FORCEACTIVE references it.**
   dtk's generated `ldscript.lcf` FORCEACTIVE list holds the module's entry points and whatever its
   own data and code reference. A unit claiming `.text 0x27C8..0x27D0` for an 8-byte accessor links
   and produces a `.text` **8 bytes short**, because nothing references `fn_58_27C8` and it is simply
   dropped. Adding `scope:global` to the symbol in `symbols.txt` does **not** get it into
   FORCEACTIVE - tested, rejected, reverted. **Superseded 2026-09-29:** a module's `force_active:`
   list in `config/G2ME01/config.yml` does (dtk 1.8.4), so this no longer blocks anything by itself.
   It **blocked the whole tail of `ScriptCoin`**
   (`0x2600..0x36A4`: four functions and their neighbours are all unreferenced), so no unit there
   can be promoted without a source that also reproduces an adjacent *referenced* function. It will
   apply to any module whose remaining functions are unreferenced helpers.
4. **A non-`Matching` object can contribute bytes the split does not claim.**
   `CScriptCoinTouchBounds.o` emits 1 byte of local `.bss` for no claimed range; the module still
   hashes because mwld absorbed it, but it is a latent hazard in the same family as "a DOL unit can
   be blocked by data". Related trap: `CScriptCoinRel.o` puts its 4-byte slot in `.comm`, not
   `.bss`, so `unit_fit.sh` prints `.bss claimed 4 ours 0 SHORT by 4` on a module that is in fact
   correct - one more reason its REL column cannot be trusted (see below).
The shape that works, then: **one contiguous range per unit, one file per unit, the module's own
name for every unit, and the `Matching` units only where the object reproduces the range exactly.**
`ScriptCoin` is the worked example: three `Matching` units at `0x0..0xA0`, `0x1350..0x1370` and
`0x1B24..0x1BA4`, three `NonMatching` units carrying the rest, and `0xA0..0x1350` left unclaimed
as a single `auto_00_000000A0_text.o`.
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
Claiming ranges in a named unit removes those bytes from the `auto_*` units that `dtk` builds from
the retail module. The old explanation here - that an `auto_*` unit's functions count as matched by
default - is **wrong**: measured 2026-09-25, the report holds 791 `auto_*` units with 24,456
functions and **none of them matched**. What moves the headline is attribution: a rename can change
which unit owns a function, and a unit can stop being `Matching`. The Puffer 2633 -> 2629 anecdote
below needs re-deriving in that light.
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
### Read the loader record's size off *both* of the DOL's readers, not the one in `ScriptLoader/`
Landed 2026-09-29 for `Metroid`'s head (`CMetroidRel.cpp`, `.text 0x0..0x17C`, 18/18 at 100.00%),
and it is the general trap in the recipe above: **almost every module head in this family stores a
four-byte loader slot, so the spelling transfers from sibling to sibling - until one module's record
is bigger, and nothing in the copied spelling says so.**
The setter is always the same two instructions (`stw r3, gLoader_X; blr`), and the
`src/MetroidPrime/ScriptLoader/<X>.cpp` reader is always the same shape
(`(*gLoader_X.value)(mgr, input, info)`), which is what makes every head look four bytes wide. But
the record is only four bytes if **the loader is the only thing the DOL reads out of it**. For
`Metroid` it is not: the setter `fn_80218B68` stores the *address* of a record, `LoadMetroidAlpha`
calls word 0 as the loader, and the DOL's own
`OnDockTouch__13CMetroidAlphaFR13CStateManager` at 0x80218B10 does
```
lwz r5, gLoader_MetroidAlpha@sda21(r0)
addi r12, r5, 0x4
bl __ptmf_scall
```
so **words 4..15 are a CodeWarrior pointer-to-member-function** - `0 / 0xFFFFFFFF / fn_40_FB4` in
`.data:0x358` - and the record is 0x10 bytes. Two consequences, both measured here:
- the registration is **0x50 bytes, not 0x30**, and it *copies* the member-function pointer out of
  `.data` instead of building it. That is `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` (which copies two
  of them, 0x6C bytes) with one, so the spelling is the SnakeWeed one, not the four-byte one every
  other head uses;
- `.bss` shows it too: `lbl_40_bss_10` is `size:0x10` where the family's slots are `size:0x4`
  (`build/G2ME01/Metroid/asm/auto_05_00000000_bss.s`), so the `.bss` dump is the cheap check and the
  `__ptmf_scall` call is the one that explains it.
So the recipe is: **before writing a module head, read the `.bss` size of the slot the registration
stores to, and if it is not 4, `grep` the DOL for every reader of that `gLoader_*` symbol.** A
second reader means a bigger record, and a bigger record means a pmf, and a pmf is copied from
`.data` rather than assigned - three wrong steps if you copy a sibling's spelling.
The accessor block was the family's, so nothing else had to be worked out, and it is a sixth
instance of the same measured point the `MediumIng` row above makes: over the 0x17C claimed,
`Metroid` and `CMysteryFlyerRel.cpp`'s 0x170 agree on where the `GetBoundingBox` wrapper sits and
on every body, but this module **swaps the two leading eight-byte accessors** (it opens
`addi r3,r3,0x8c8` then `li r3,1`, MysteryFlyer `li r3,1` then `addi r3,r3,0x818`), runs **three**
`li r3,0` predicates to MysteryFlyer's two, and has **no three-float copy** - `fn_40_B4` is an
eight-byte `li r3,0` where MysteryFlyer has a 0x1C-byte accessor. That last difference also has a
second effect worth knowing: **it drops the file to zero sites in `tools/check_raw_offsets.py`**,
because the three-float copy is the one accessor the checker sees, so no `##` section can be
written for the file at all. See `docs/research/raw_offsets.md`, "The debt, measured".
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
### A static initialiser has to go through a function to reach `__sinit`
`CGX::__sinit_CGX_cpp` was 76.92% because it was missing a store: retail computes
`0x1C807` into `lbl_80419910` at run time (`lis r3,2; addi r0,r3,-14329; stw r0,lbl_80419910`) and
the symbol lives in **`.sbss`**, unclaimed. Every obvious spelling puts it in `.sdata` with a static
initializer and leaves `__sinit` unchanged - measured: a plain `= 0x1C807`, a `static` one, a
`volatile` one, and the same value written out as an enum/shift expression all give
`.sdata=0x8` and `store_in_sinit=0`.
**MWCC routes an initializer into `__sinit` (and the object into `.bss`) when the initializer is not a
constant expression to the front end, even when the back end folds it.** Putting the value behind a
function is what does it:
```cpp
// __sinit_CGX_cpp then emits exactly retail's lis/addi/stw, and the object is `B` in .sbss
static inline uint alphaCompareAlways() {
  return GX_ALWAYS | (0 << 3) | (GX_AOP_OR << 11) | (GX_ALWAYS << 14) | (0 << 17);
}
extern "C" uint lbl_80419910 = alphaCompareAlways();
```
`inline` is required: without it MWCC emits a real `bl` to the helper (87.31%, and an extra function
in the object). The same trick applied to `CGX::sGXState` does not help - it has no initializer to
move, which is why it stays COMMON (fixed on 2026-10-01 by making it a file `static`; see below). Worth knowing before concluding "MWCC put my global in the wrong
section": check whether the initializer is a constant expression first.
Two more negatives from the lane that first hit this, so nobody spends a session on them:
- Adding `operator=(const T*)` to `rstl::single_ptr` to match retail's `__as__...FPQ2...` mangling is
  not viable: it makes the `= nullptr` idiom ambiguous tree-wide (MWCC stops at `CActor.cpp:255`,
  `CCubeMoviePlayer.cpp:412`, `:536`, `:588`, `:909`) and MWCC inlines the 8-byte body at `-O4,p`
  anyway, so the out-of-line symbol never appears.
- `decomp_build.sh <unit>`'s per-function percentages are the ground truth. A bare two-object
  `objdiff-cli diff` disagrees on units that set `reverse_fn_order` (it reports 99.6x% for functions
  the project counts as matched). Score with the tool, not with the raw diff.
### An unnamed retail vtable keeps our weak copy alive - name it, and claim stray `.sdata` (measured 2026-09-29)
`MetroidPrime/BodyState/CBodyStateCmdMgr` was 33/33 at 100% and still failed `flip_test.sh`:
DOL and every REL off. `compare_unit.sh` showed `.data` 44 bytes over and a 1-byte `.sdata`
section the split did not claim. The causes, and why neither shows in objdiff:
- **Retail's constructor stores three vtables that live in earlier auto-split objects under `lbl_`
  names** (`lbl_803B263C`, `lbl_803B2648`, `lbl_803B37E0`). Our object defines them weak as
  `__vt__15CBCKnockDownCmd`, `__vt__12CBCHurledCmd`, `__vt__14CBCScriptedCmd`. MWLD only drops a
  weak duplicate when an earlier object defines the *same name*, so with `lbl_` there, ours stayed
  in the section. Renaming the three in `symbols.txt` (and the destructors in their vtables'
  slot 2, `fn_80078CBC`/`fn_80078D18`/`fn_800D5EA8`, as `scope:weak`) makes the linker discard
  ours. objdiff tolerated the name difference, which is why the unit scored 100%.
- **A compiler-generated `false` for `reserved_vector<bool,34>`'s fill (`@199`) is retail's
  `lbl_80418248`, in no split.** Adding `.sdata start:0x80418248 end:0x80418250` to the unit put it
  back; the link order agrees (CDecalManager's `.sdata` and `.text` precede, CMapWorldInfo's follow).
**The same holds for weak *functions* named `fn_`** (`CABSIdle`, 2026-09-29): our object emitted
`CBodyState`'s constant thunks, `~CAdditiveBodyState` and `~CABSIdle` weak, and retail keeps them
earlier (`0x800F0A6C`..`0x800F127C`, `0x800766CC`) under `fn_` names, so ours stayed in the
section. Mapping each weak symbol to retail by relocation (same-named functions' `bl`s, then the
words of each vtable whose address is known) and naming the 13 `scope:weak` gave 0 diff bytes. If a
NonMatching unit defines the old `fn_` placeholder, rename its body too (`extern "C"` under the
mangled name, as `CBodyStateInfo.cpp` and `CPatterned.cpp` do), or the gate reports it `GONE`.
`CFluidPlane` needed the same for 4 copies. Its other weak copies (the unreferenced destructors) were
stripped by the linker and needed nothing. Map only what the flip diff shows as moved or new, and follow
`bl` chains in the retail DOL for callees no same-named function reaches (`fn_800D747C` -> `fn_800D74C8`
-> `fn_800D750C`).
How to find it: disassemble retail's function with `-dr` and list the `R_PPC_ADDR16_LO` targets
that are `lbl_`; any that ours names `__vt__...` needs the rename. The unit also needed
`#pragma inline_max_size(127)` (window 127..160) to inline the 0x58-byte `CBCHurledCmd`/`CBCCoverCmd`
constructors but not the 0x7C `CBCJumpCmd` one, as retail does.
### A `Matching` unit's weak instantiations can steal a symbol retail has somewhere else (measured 2026-09-26)
Found promoting `FStringTableFactory` (retail 0x80312320, 0x64) out of the `NonMatching`
`src/Kyoto/Text/CStringTable.cpp` into a `Matching` unit of its own. It was already at 100.00%,
the new unit came out at **100.00% on both its functions, `flip_test.sh` PASS, DOL sha1 held** -
and the gate still reported
```
matched  3131 -> 3129   linked  1754 -> 1756
  WORSE  main/Kyoto/Text/CStringTable :: GetIObjObjectFor__22TToken<12CStringTable>...  100.00% -> 0.00%
  WORSE  main/Kyoto/Text/CStringTable :: GetNewDerivedObject__40TObjOwnerDerivedFromIObj<12CStringTable>...  100.00% -> 0.00%
```
**The mechanism.** `CFactoryFnReturn`'s converting constructor is defined in the header, so any
translation unit that builds one emits it - and, through it, the weak inline template members
`TToken<T>::GetIObjObjectFor` and `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject`. Those two
were *also* being emitted by `src/Kyoto/Text/CStringTable.o`, which is where retail's copies came
from, and retail has them at 0x80312434/0x80312460 while the new object puts them at +0x2F0 and
+0x31C (three weak `__dt__` instantiations land between). Two owners for one symbol.
**`flip_test.sh` cannot see this, and neither can the sha1.** The linked ELF still has both
symbols at retail's addresses and the DOL is byte-identical, because the new object's copies sit
past the claimed range and are dead-stripped. What breaks is the *report*: objdiff pairs the
vanilla function with the dropped copy. So the acceptance test passes and the gate fails, and the
only thing that catches it is a per-function baseline recorded on a clean tree.
Three rules out of it:
- **Before promoting a unit, list what its object emits that another object also emits.**
  `powerpc-eabi-nm -n build/G2ME01/src/<new>.o` against `build/G2ME01/obj/<other>.o`. Any symbol
  in both is a coin toss, and the extras past the claim are not harmless.
- **A new `Matching` unit cannot be a subset of an existing unit's claim if the two objects would
  both define a symbol the existing claim also covers.** The split has to go the other way round,
  or the new unit has to take the whole run.
- **MWCC 2.7 has no `extern template`**, so there is no source-level way to suppress the extra
  instantiation; the only fixes are a wider claim with a matching order, or accepting the loss.
  `docs/research/paks.md` records the worked example and both attempts (`FStringTableFactory` and
  `FRuleSetFactory`, the second blocked for an unrelated reason: its `operator new` names a
  `scope:local` symbol, and claiming it breaks the link with
  `undefined: '@stringBase0_803AC548'`).
### Adding a *string literal* to a unit can move an unrelated function (measured 2026-09-25)
Found while defining the port's retail globals in `main.cpp`. The DOL is unaffected either way -
the unit is `NonMatching` - but `report_diff.py` is a ratchet on per-function percentages, so an
unrelated function going from 96% to 95.97% is a red gate and blocks the change.
**Adding one string literal to `main.cpp` grew `CGameArchitectureSupport`'s constructor by 32
bytes and gave it a `__cvt_dbl_usll` call, and cost `AddWorldPaks` a fraction of a point** - while
`StreamNewGameState` in the same unit went *up* 6.6 points, so the unit average improved and the
gate still failed. `main.o`'s `.text` grew 0x2094 -> 0x20b8. The same two symbols defined in
`CPowerBeam.cpp` cost `Update` and `EnableSecondaryFx` 100% -> 98.56% and 100% -> 97.87%, and
`matched` fell 3032 -> 3030. It is the literal, not the symbol: 17 other globals in `main.cpp`,
including relocated pointer words in `.sdata2`, leave `.text` at 0x2094 byte for byte, and a
`static char k[] = "..."` buffer that the pointer then refers to is also inert. The workaround is
to spell string storage as a named mutable buffer and point the pointer at that.
The general lesson: **mwcceppc's codegen is not stable under additions that look like data.** Before
adding a definition to a `NonMatching` unit someone is actively decompiling, check
```sh
$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja -f build.ninja build/G2ME01/src/<unit>.o
build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o | grep ' .text'   # must be unchanged
```
Two other host-compiler facts, both of which silently delete the definition you just wrote (the
link then fails on a symbol that looks defined in the source):
- **GCC drops an uninitialised tentative definition that nothing in the translation unit reads.**
  `extern "C" int x;` in a TU that never mentions `x` again produces *no symbol at all*, at every
  optimisation level. Every port-side definition of a retail global needs an explicit `= 0`.
- **Inside `extern "C" { }`, GCC gives a `const` declaration internal linkage** unless it also says
  `extern`, and an unreferenced internal object is then dropped the same way. Six of the 19 needed
  the redundant `extern`.
### One instruction of register allocation, fixed by assigning the widened local back
`CGX::SetDstAlpha` was 99.43% - 140 bytes, every instruction in the right order, and one
register-allocation difference in the tail: retail materialised the constant `1` in `r0` and the
masked alpha in `r7`, we used `r5` and `r0`. **Removing the `const uint` local made it worse**
(three instructions short instead - MWCC then elides the second mask entirely), and every spelling of
the mask expression was a wash. What fixed it was assigning the *widened* local back to the member
rather than the original `uchar` parameter:
```cpp
gpGXState->x24c_fogParams.x14_ = 1;
const uint normalizedAlpha = alpha;
gpGXState->x24c_fogParams.x15_ = normalizedAlpha;   // was: = alpha
GXSetDstAlpha(enable, normalizedAlpha);
```
Same value, same instruction count, and the allocator stops needing a second temporary. Worth trying
on any last-percent function where the only difference is which register a value lands in: making the
*store* consume the widened temporary is a lever that costs nothing semantically. Ruled out in the
same batch, all 4-5 differing instructions: `static_cast<uint>`, `alpha & 0xff`, a `uchar` local, a
`const` local, swapping the two stores, and rewriting the `if (!enable)` as `if (enable)`.
## What still blocks most modules
- **A module's `.rodata` is not always splittable per unit.** `Tweaks` shows the shape of it:
  `config/G2ME01/rels/Tweaks/splits.txt` splits only `.text` and `.bss`, so all 0x408 bytes of the
  module's `.rodata` come from the base object `auto_03_00000000_rodata.s`, whose symbols are
  FORCEACTIVE. A `Matching` unit that contributes any `.rodata` therefore adds a second
  contribution and the module's hash breaks - and the constants a unit needs are not even
  contiguous (one unit wanted `.rodata` 0x28 and 0x30 but not 0x2C). Only a unit that owns the
  whole pool can claim it. Check the module's split before promising a unit there, and prefer the
  units whose gains are `.text` only.
- **`__nw__` vs `__nwa__` decides whether objdiff pairs a `new` at all (measured 2026-09-25,
  `Tweaks`).** mwcceppc mangles the *array* operator as `__nwa__FUlPCcPCc` and the *scalar*
  one as `__nw__FUlPCcPCc`, and the suffix is part of the call target, so writing
  `new (file, line) T[1]` where retail used `new (file, line) T` makes every allocation in the
  function fail to pair. Read the suffix off the retail symbol table before writing the first
  `new`: it also tells you whether the source had an array subscript, which decides whether an
  array-construction loop belongs in the body. Dropping the `[1]` in `REL_CreateTweakGlobals`
  moved it 140 bytes closer to retail in one edit.
- **MWCC common-subexpression-eliminates the `__FILE__` argument of `new`, retail does not
  (measured 2026-09-25, `Tweaks`).** Retail's `REL_CreateTweakGlobals` re-materialises
  `lis r4, lbl_82_section4_3F0@ha; addi r4, r4, lbl@l; addi r4, r4, 0xe` at all 15 of its
  allocation sites - 30 references to the symbol. Ours emits `addi r31,r4,14; mr r4,r31` once
  and reuses `r31`, which is 2 instructions short per site. There is no source expression that
  stops the hoist, and the only way to get the literal back is the `NEW` macro - which adds
  `.rodata`, i.e. exactly the wall above. Treat a unit that allocates with `new` against an
  unsplittable `.rodata` pool as capped, and say so instead of chasing the percentage.
- **A 99.2% wall that is not source-expressible (measured 2026-09-25, `Tweaks`).** Seven units
  sit at exactly the same two-instruction difference: retail moves the first stream pointer into
  `r4` and reuses `r4` for the switch's `propertyId`, ours uses `r3` and `r6`. Retail's own
  *Matching* units in the same module (`SLdrTweakPlayer`: 14 cases, `SLdrTweakGuiColors`: 15)
  emit the same shape ours does - the difference is the switch size. Ruled out by the lane:
  id/size type and constness, declaration order, all six case permutations, `default:` first, an
  if-chain, suffixed literals and casts. That is MWCC register allocation, and no source rewrite
  reaches it; treat these as blocked, not as unfinished.
- **Superseded, 2026-10-01: `CGX::SetVtxDescv_Compressed` matches, and it was never a wall.** The
  lever is a **body-local named variable for the shifted mask**: `uint shift = idx * 2;
  uint mask = 3 << shift;` inside the loop body, then `(flags & mask) == (gpGXState->mDescList & mask)`
  (and `1 << shift` in the second loop). Written inline twice, `3 << shift` becomes a late CSE
  temporary; MWCC hands out volatile registers by virtual-register age, so a temporary born late
  takes a different register from a user variable born at its declaration. The same `mask`
  declared at *function* scope does not work - it has to be created inside the body, after `shift`.
  The "named-mask locals" in the list below named the constant `3`, not the shifted value, which is
  why ~200 variants across three sessions missed it. Before calling a register difference a wall,
  ask which value retail allocates *first* and give that value a declaration at that point.
  The original entry follows as written.
- **`CGX::SetVtxDescv_Compressed` is the same wall at 95.78% (436 bytes, 2026-09-25).** The logic
  is identical instruction for instruction - same two loops, same unrolling (11, then 2 x 4), same
  `slw`/`srw`/`clrlwi` sequence, same early-out - and the *only* difference is which of `r4`..`r9`
  each value lands in. Retail fills them in the order mask-`3`, `gpGXState`, shift, `list`, index;
  we fill them in the order `list`, mask-`3`, `gpGXState`, shift, scratch. Both use exactly
  `r0, r3..r9, r31` and neither spills, so it is one allocation-order decision, not pressure.
  **45 source variants failed to move it** (best 61 differing instructions from 63, by putting
  `idx` and `shift` in one `for` header): loop variable `uint`/`int`/`u32`/`uchar`, `<` vs `<=` vs
  `!=` bounds, `idx * 2` vs `idx + idx` vs an explicit `shift` induction variable, `continue` vs a
  positive `if`, both store orders, `const` and named-mask locals, the class's own
  `MaskAndShiftLeft`/`ShiftRightAndMask` helpers, `reinterpret_cast<uint*>` stores, swapping the
  two loops' bodies, merging them into one, hoisting `idx` above `list`, `static const GXColor`
  initialisers, and writing through `list++` instead of `++list`. Treat it as blocked; the unit's
  remaining blockers are its data sections anyway (see above), so this is not where the value is.
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
`include/MetroidPrime/Enemies/` now has `CAi` and `CPatterned` as `Matching` units (`CPatterned` is
`NonMatching` again since the 2026-09-28 upstream merge widened it to 103 functions), so a module
whose objects derive from them *can* be written - that was the blocker, and it is gone. What limits
those modules now is the behaviour inside the classes: most of the creature virtuals are unnamed,
`CPatterned`'s 0xB58-byte constructor is unwritten, and 75 modules' worth of actor code has to be
decompiled one function at a time like anything else.
Of the 86 modules, **11 are `Script*` units** (script objects that lean on `CEntity`/`CActor`,
which do exist) and the other 75 are creatures, bosses and swarms that need the missing Enemy
hierarchy. Acknowledge this before assigning module work: check that the base classes a module
needs actually exist.
### A DOL unit can be blocked by data, not by code
`Kyoto/Graphics/CGX` matched every function it could (51 of 54, 99.47%) and **is not promotable**,
because its *sections* cannot be reproduced by C++ source. **Updated 2026-09-25: the functions are
now 53 of 54 (99.69%) and the data blocker is fully characterised - but it is not the
"unsourceable constant pool" this section used to claim.** The corrected version is below, because
the correction is the useful part: the constants are not compiler-generated at all.
**What retail's object actually does** (`build/G2ME01/obj/Kyoto/Graphics/CGX.o`, six symbols):
| symbol | section, address | referenced from | defined in |
| `lbl_8041E4A0` | `.sdata2` 0x8041E4A0, `0xffffffff` | `__ct__SGXState` (the white `GXColor`) | `auto_11_8041E278_sdata2.o` |
| `lbl_8041E4A4` | `.sdata2` 0x8041E4A4, `0.0f` | `__ct__SGXState` (fog start Z) | same |
| `lbl_8041E4A8` | `.sdata2` 0x8041E4A8, `1.0f` | `__ct__SGXState` (fog end Z) | same |
| `lbl_8041E4AC` | `.sdata2` 0x8041E4AC, `0.1f` | `__ct__SGXState` (fog near Z) | same |
| `lbl_8041F8D8` | `.sbss2` 0x8041F8D8, 8 bytes of zero | `__ct__SGXState`, `__ct__SFogParams` (the clear `GXColor`) | `auto_10_80419828_sbss2.o` / `auto_12_8041F880_sbss2.o` |
| `black$localstatic3$apply_fog__3CGXFv` | `.sdata2` 0x8041B018 | `SetFog` (the `black` in the header's `apply_fog`) | `auto_11_8041AD50_sdata2.o` |
All six are **imports** in retail's object: `powerpc-eabi-nm --undefined-only` on it lists every
one, and none is defined. So retail's `CGX.cpp` *referenced* globals that live in other translation
units, and dtk moved each definition's bytes into the unclaimed `.sdata2`/`.sbss2` blobs
(`auto_*` objects named for the start of the gap they fill), leaving an import behind. Our object
instead **defines** them, as anonymous compiler-generated words (`@358`, `@359`, `@746..@748`) and as
the local static in `apply_fog`.
**Superseded, 2026-10-01: no `extern` is needed, and no shared-header change.** With the three
small-data ranges claimed in `splits.txt` (`.sbss 0x80419910-18`, `.sdata2 0x8041E4A0-B0`,
`.sbss2 0x8041F8D8-E0`) our object's own compiler-generated constants land on retail's addresses,
and the weak `black$localstatic3$apply_fog__3CGXFv` copy in our `.sdata2` is merged by the linker
into the first definition at 0x8041B018, so the 20-bytes-against-16 that `unit_fit.sh` reports
("over by 4") is harmless. `unit_fit.sh`'s "SHORT by 4" on `.bss`/`.sbss`/`.sbss2` is alignment
padding and equally harmless. CGX is `Matching` with exactly that; the paragraph below is kept as
written.
**So the fix is `extern`, not cleverer source.** Declaring the five `lbl_*` objects at file scope
and using them in the header's `SFogParams()`/`SGXState()` constructors is legal and reproduces retail
exactly; the naming is not a guess either, because nothing else in the DOL references them (checked
across every object), so the dtk placeholder name is the name the link needs. The sixth,
`black$localstatic3$apply_fog__3CGXFv`, is a `static` local in the header's inline `apply_fog`, and
its 4 bytes sit in a **third** blob, 0x1218 bytes below the other four - so it cannot be claimed by
CGX's split at all (one input section cannot land in two output ranges) and has to stay an import
too, which means an `extern "C"` declaration carrying MWCC's own generated name, `$` included. That
last step is the one with a real cost, and it is a **shared-header** change: `apply_fog` is inlined
into `Kyoto/Graphics/CCubeMoviePlayer.cpp` too, which currently emits its own copy of the symbol.
**The measured consequence.** Flipping CGX by hand (not with `flip_test.sh`, to see the shape of the
damage) makes `main.dol` **32 bytes longer** and shifts everything after the first insertion:
`lbl_8041E4A0` lands at 0x8041E480 instead of 0x8041E4A0, `lbl_8041F8D8` at 0x8041F8C8 instead of
0x8041F8D8, and `sGXState` at 0x804170E0 instead of the claimed 0x803DF828. There is no partial
credit: the module hash is a single comparison. Note that once the *code* is at 100% the failure
moves earlier than the hash - it becomes a **link** error, `multiply-defined`, because our object
defines `lbl_80419910` and so does the blob (see below for the fix).
**Superseded, 2026-10-01: `sGXState` is a file `static`.** `static CGX::SGXState sGXState;` in
`CGX.cpp` (and the member declaration removed from the header) is a real `.bss` object at
0x803DF828 with its constructor call still in `__sinit_CGX_cpp`. The "class type versus POD" theory
below is wrong: the distinction is linkage. Retail is built with `-common on`, which makes every
*external* uninitialised object COMMON whatever its type, and a COMMON symbol cannot sit in the
middle of a unit's `.bss`; so any retail object found inside its unit's own `.bss` range had
internal linkage, and the `Class::member` name in `symbols.txt` is only a project label. The same
applies to the descriptor list at 0x803DFA8C: `static GXVtxDescList sVtxDescList[GX_MAX_VTXDESCLIST_SZ]`
as in Prime 1 (0xD8 bytes in the 0xDC slot), with CGX's `.bss` claim extended to 0x803DFB68.
`CStopwatch::mData` and `CCubeSurface::skDefaultNormal` are **not** candidates (checked 2026-10-01): both
units are already `Matching` and their `.bss` splits say `align:4 common`, so retail's copies really are
COMMON. The test is the split line - a `.bss` range without `common` whose object has `C` symbols in
`nm`. Three units fail it today, none code-complete yet: `MetroidPrime/CAnimData.cpp` (101/216),
`Kyoto/Audio/CSfxManager.cpp` (132/159) and `Kyoto/Graphics/DolphinCGraphics.cpp` (96/102).
**`sGXState` is the second, independent problem.** Ours is a **COMMON** symbol (`C`, 0x264) and
retail's is a real `.bss` object (`B`); a common symbol is placed by mwldeppc in a later section, so
`CGX::sGXState` lands 0x178B8 bytes past its claimed range. No source form tried moves it: a plain
`CGX::SGXState CGX::sGXState;` and every spelling of the out-of-class definition stay `C`,
`= CGX::SGXState()` is much worse (it turns `__sinit_CGX_cpp` into a 736-byte frame with a temporary
and a copy), and both `__attribute__((aligned(32)))` and `alignas(32)` **fail to compile** under
MWCC 2.6.2. The distinction looks like class type versus POD: every `.bss` symbol in our whole object
set is a POD static or array (`main.cpp`'s `static uchar sMainSpace[...]`, `CFrameDelayedKiller`'s
`sFrameDelayedList`, the C runtime's), and every class static with a user constructor comes out
COMMON (`CGX::sGXState`, `CStopwatch::mData`, `CCubeSurface::skDefaultNormal`). Note that a
`Matching` unit *can* hold COMMON symbols - `MetroidPrime/CAxisAngle.cpp` is `Matching` with
`.bss ... align:4 common` - so this is a placement problem, not a legality one, and the `align:4
common` spelling is the thing to try next.
**One of the three needed `splits.txt` lines is verified to work.** Giving CGX
`.sbss start:0x80419910 end:0x80419918` (the exact size dtk records for `lbl_80419910`) both fixes a
link error and places the symbol correctly:
```
### The positive form of the section above: name retail's constant, do not write a literal (measured 2026-09-26)
The CGX section above characterises the data blocker and does not get past it. The way out **is**
`extern "C" const T lbl_<addr>;` at file scope and using the name - and it is now measured on three
constants rather than argued, by the frame-loop lane writing five new `Matching` units.
The trap that makes this worth its own entry: **a `Matching` object's `.rodata`, `.sdata2` and
`.data` are linked into the DOL.** Any byte they add that `splits.txt` does not claim for that unit
grows the section, moves every address above it, and breaks the DOL's sha1 **with every function in
every unit still reading 100%**. Measured, for four bytes:
| | |
| what was written | `x10_timerPeriod = 1.0f / static_cast<float>(x0_timerFreq);` |
| what the object grew | a 4-byte `.sdata2` |
| what objdiff said | `100.00% fuzzy, 100.00% matched code, 1/1 functions` |
| what `unit_fit.sh` said | fits |
| what the linked ELF's section sizes said | all correct - `.text` 0x3a1c54, `.rodata` 0xb530, `.data` 0x14e10 |
| what actually happened | `.sdata2` went 0x54C0 -> 0x54E0, the BSS address moved, `main.dol` grew 32 bytes, `dtk shasum -c` printed `main.dol: FAILED`, and **all 86 RELs failed too** because they depend on that check |
Only the sha1 catches it. Neither `fast_try.sh` nor `unit_fit.sh` does, so a new unit is not
believed until `ninja build/G2ME01/main.dol && sha1sum build/G2ME01/main.dol` prints
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
The fix is byte-identical, because the linker fills the small-data offset in the relocation:
```cpp
extern "C" const float lbl_8041E258;   // .sdata2 0x8041E258, 0x3F800000 = 1.0f
...
x10_timerPeriod = lbl_8041E258 / static_cast< float >(x0_timerFreq);
```
which emits `lfs f0,-16744(r2)` against `R_PPC_EMB_SDA21 lbl_8041E258` - retail's own instruction.
**To find the offender in one step** when the sha1 breaks and every function reads 100%:
```sh
build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o | grep -E 'sdata2|rodata|data'
```
Anything that prints which `splits.txt` does not claim for that unit is the cause. Three such
constants turned up in one lane's five units, and all three are the same shape - a value retail
already has a name and an address for:
| name | section, address | value | why a source literal cannot be used |
| `lbl_8041E258` | `.sdata2` 0x8041E258 | `0x3F800000` = 1.0f | `CStopwatch::CSWData::Initialize` divides by it |
| `lbl_8041E260` | `.sdata2` 0x8041E260 | `0x4330000000000000` = 2^52 | `CSWData::Wait` adds and subtracts it |
| `lbl_803A60A0` | `.rodata` 0x803A60A0 | `"??(??)\0MainFlow"` | `CMainFlow::CMainFlow` points **seven bytes into** it, because retail's linker merged `"MainFlow"` with the tail of a longer literal; the `+7` is a separate `addi` and the source has to say so |
Two things follow that are easy to get wrong. First, **these names must be defined somewhere on the
port**, with the *value* and not the address - a 64-bit host cannot hold 0x8041E258, and undefined
they are zero fills, so `lbl_8041E258` being 0.0f would make `GetElapsedTime()` return 0.0 for the
whole game. `src/MetroidPrime/PortGlobals.cpp` is the place, because it is a unit `configure.py`
never claims; putting them in the `Matching` unit itself would collide with the retail object. Second,
**a `NonMatching` unit is exempt**, because its object is not in the link at all: that is why a
`NonMatching` unit can claim a range and still be safe, and also why the *weak* template
instantiations a `Matching` object emits past its claimed range (`__dt__rstl::list<...>`,
`ReleaseData__...rc_ptr<24IArchitectureMessageParm>`, `__vt__24IArchitectureMessageParm` - 0xF0 bytes
past 0x8C in `CIOWinManagerCtor.o`) are harmless: dtk drops them. They would not be, in a unit whose
claimed range they fell inside.
Worked examples, all `Matching` and all verified byte-exact:
`src/MetroidPrime/CIOWinManagerCtor.cpp`, `src/MetroidPrime/CIOWinCtor.cpp`,
`src/MetroidPrime/CMainFlowCtor.cpp`, `src/MetroidPrime/CInputGeneratorCtor.cpp`,
`src/Kyoto/Basics/CStopwatchCSWData.cpp`. `docs/research/frame_loop.md` has the per-function detail.
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
### Declare in reverse: the rule that keeps a module's hash from breaking invisibly
**mwcceppc emits function definitions in reverse source order, and mwldeppc places an input
object's `.text` in that object's own section order.** So a unit's functions must be *declared
descending by retail offset* or the module's bytes come out permuted.
This is not a theory. `AIMannedTurret`'s unit declared its three functions ascending
(`fn_1_0`, `fn_1_8`, `fn_1_10`); the object came out as `fn_1_10@0, fn_1_8@8, fn_1_0@0x10` and
the module hash broke. Reversing the declarations gives `fn_1_0@0, fn_1_8@8, fn_1_10@0x10`,
which is retail, and the flip passes. The bodies were never wrong and no symbol was wrong.
**Nothing else reports it.** objdiff pairs functions by name, so all three stayed at 100%;
`tools/unit_fit.sh` compares sizes, and the sizes were identical; the link succeeded, because a
permutation does not change the module's size. The failure was 4 bytes of `.text` plus two
relocation offsets. **Only `tools/flip_test.sh` catches it**, which is the whole argument for
that tool being the acceptance test rather than a percentage.
It is also visible after the fact, cheaply: `powerpc-eabi-nm -n` the object and compare the
address order with the source order reversed.
**18 units are permuted right now** (18 before `CGX` was reordered), all of them `NonMatching` -
which is the point, since a `Matching` unit cannot be permuted without the hash already having
broken. The list, with a reason and a note of what else blocks each, is
`docs/research/decl_order.md`, and `python3 tools/check_decl_order.py` measures it and checks the
list, in `tools/gate.sh`. It finds the defect in a `NonMatching` unit, which is the point: the
alternative is spending a lane discovering it at the end. The one worth doing next is
`CStaticAudioPlayer` (22/24); `CPakFile` is permuted too and is the largest unmatched pool in the
tree. **`CGX` was reordered and is no longer on the list, but it is still not promotable** - the
reorder removes the *silent* blocker, not the real one, which turned out to be its data sections
("A DOL unit can be blocked by data, not by code"). **Read that before spending a lane on the next
unit `check_decl_order.py` names**: being off the list is not the same as being ready to flip.
**`CGX`'s permutation was five local moves, not a rewrite** - about 15 lines moved, and
`check_decl_order.py` went from "would break on a flip" to ok. Its definition order was already
descending for 47 of the 52 functions; only `fn_802BCC74`/`fn_802BCC80` (which belong after
`SetAlphaCompare`, not in the middle), the swapped `fn_802BDFC8`/`fn_802BDF20`, `fn_802BE0E8`
(one position out), `CallDisplayList` (before `Begin` rather than after it) and `SetAlphaCompare`
itself were out of place. So **read the retail `nm -n` list and move only the positions that
disagree** - diffing the two orderings and splicing the misplaced blocks is exact and takes
minutes, where reordering the whole file by hand is where mistakes come from. A useful trick: a
lane can compare the two `nm -n` orderings directly and print the mismatched positions.
```sh
build/binutils/powerpc-eabi-nm -n --defined-only build/G2ME01/src/<unit>.o | grep ' [tT] '
```
`Puffer` and `WallCrawler` already write their sources in this order, and so does `CPatterned`
(`TakeDamage` last in the source, first at `0x0`) - which is why those modules hold their hashes.
The idiom was there without being written down.
### An emission-order wall: out-of-line template instantiations
Measured on `CStaticAudioPlayer` (2026-09-25), and it is the reason a unit can be 23/24 with
every function at 100% and still not flip. **"Declare in reverse" only orders the functions you
write.** The out-of-line copies of `rstl::vector<T>::reserve`, `operator=`, `clear`, `~vector`,
`destroy`, `uninitialized_copy`, `rstl::reserved_vector::erase` and the implicit `__dt__`
instantiations are emitted by mwcceppc in a **trailing pool**, after every source-defined
function, in an order that is *not* the order they are used:
```
retail ascending : ... StartMixOut  as  clear  destroy  dt_vector  IsReady  __dt__  __ct__
                    reserve  uninit_copy  Cancel  erase  Run  AICb  Install ...
ours             : ... StartMixOut  IsReady  __dt__  __ct__  Cancel  Run  AICb  Install
                    as  reserve  dt_vector  destroy  erase  clear  uninit_copy ...
```
Retail's order is the source functions descending *with each function followed by the
instantiations it needs*; ours is the source functions descending and then one pool. No
`#pragma inline_max_size` value, no `inline` marker, no reordering of the declarations in
`rstl/vector.hpp` and no reordering of the source statements moves it. `Kyoto/Streams/CFilePreload`
is `Matching` and *does* have a trailing pool - so retail's own sources do it both ways, and the
difference is per-translation-unit, not per-header. **Treat it as a wall and stop**: it costs
more builds than the last two functions of a unit are worth.
**Superseded 2026-09-29: it is not a wall, and there are two ways through, both per-TU.** Seven
all-100% units flipped on them (`CWorldLayerState`, `CAnimTreeSequence`, `CGameHintInfo`,
`CIOWinManager`, `DolphinCDvdFile`, plus `DolphinCColor`/`CSfxHandle` for other reasons). The
rule the retail objects show: mwcceppc places an **inline function it did not inline** right
after the function that called it, and a **non-inline template instantiation** in the trailing
pool. So, for an instantiation retail has *interleaved*:
- **If retail has it right after an inline caller** (typically `clear` after `vector::operator=`,
  which is `inline` in `rstl/vector.hpp`): in the unit, before first use, add
  `template <> inline void rstl::vector< T >::clear() { ... }` with the header's body. It stays
  out of line (the caller is too big to take it) and moves up behind its caller.
  `CWorldLayerState.cpp`, `CAnimTreeSequence.cpp`.
- **Otherwise** (after a source-defined function, or at `.text+0x0`): declare a non-inline
  explicit specialization before first use and **define it where reverse source order puts it** -
  in the source, just *before* the function that retail's text shows it after.
  `CGameHintInfo.cpp` (`assign`), `CIOWinManager.cpp` (`list::do_erase`, defined last so it lands
  at `0x0`), `DolphinCDvdFile.cpp` (`single_ptr`). Making `assign` an inline specialization does
  not work - it is small enough to be inlined, and neither `dont_inline` nor `inline_max_size`
  around the caller or the specialization stops that.
- **Both at once** (`CCollisionResponseData`, 2026-09-29): retail's pool was `assign<Decal>`,
  `push_back_unsafe`, `clear`, `assign<int>`, `assign<Gen>`, `push_back_unsafe`, `clear`, the
  reverse of the constructor's calls. Defining the three `assign`s before the constructor (Gen,
  int, Decal) fixed their order. Inline `clear` specialisations for the two token vectors then
  moved each `clear` behind its `assign`, after the inline `push_back_unsafe`. The specialised
  `assign`s come out strong (`T`), not weak, and the DOL still matched.
Neither touches the shared header, so no other unit moves. mwcceppc 2.7 rejects an explicit
instantiation of a single member (`illegal explicit template instantiation`), so it has to be a
specialization. `CPASDatabase` then flipped the same way (`vector::insert` defined just before
`AddAnimState`), plus one thing the order alone did not fix: an out-of-line copy of an `inline`
function template is **local** (`t`) unless the template was first declared without `inline`, and
only weak copies are deduplicated by the linker. Retail's `destroy_impl<CPASAnimState>` lives in an
early unit (`0x8002C984`), so ours had to become weak; `rstl/construct.hpp` now forward-declares
`destroy_impl(T*)` non-inline, as it already did `construct_impl` (all RELs and every other unit
unchanged). If a retail object lacks a helper yours keeps as `t`, look for this first.
`CFontRenderState` took both at once (inline `vector<CTextColor>::clear` behind `operator=`, and
`list::do_erase` defined above the constructor). `CStaticAudioPlayer` flipped the same way
(inline `clear` behind `operator=`; `erase` defined just before `CancelDMACallback`, `reserve` just
before the constructor) once `DecodeMonoAndMix` matched - see the correction below. Explicit
specializations come out strong (`T`) where retail's copies are weak; for these units that has not
changed a byte, since ours is the copy the linker keeps either way.
**Re-measured 2026-09-28 (goal item `match-cstaticaudioplayer`): the wall stands, and there is a
second, independent blocker behind it.** The unit is unchanged at 99.87369% / 23 of 24 functions,
`unit_fit.sh` still says 868 bytes over with the same 8 extras, and `flip_test.sh` FAILs. Two
things are now pinned rather than inferred.
*The permutation is exactly the pool, and it is 10 of 24, not "the two functions".* Numbering
retail's 24 functions 1..24 by ascending offset and listing our emission in that numbering gives
    1 2 3 4 5 6 7 8 9 | 12 11 13 14 15 18 20 21 22 | 10 16 17 19 | 23 24
- the first nine are the source-defined functions, all in place (`MixToMono` is already after
  `Decode`, which is what the 2026-09-25 reorder bought);
- position 10 is a pure adjacent transposition, `__dt__vector` (retail 12) emitted before
  `destroy` (retail 11);
- positions 19-22 are the trailing pool: `clear` (10), `reserve` (16), `uninitialized_copy` (17),
  `erase` (19) - the four pool members whose retail offsets fall *inside* the source-function run.
  Their order **relative to each other already matches retail's**; only their position does not, and
  a single trailing pool has one position.
*`DecodeMonoAndMix` is a second blocker, and it is two register tie-breaks, not 18 instructions of
logic.* **Superseded 2026-09-29: it matches.** Two changes, measured with `tools/try_batch.py`:
declaring `clamped1`/`clamped2` together at the top of the inner loop body (instead of each just
before its `if`) took it from 32 to 10 differing instructions and fixed the clamp registers; then
hoisting `remBytes` out of the `for` into a `while` and declaring the four locals in the order
`remBytes, curSample, inCursor, outCursor` fixed the r26/r29 pair (1 of the 24 orders matches).
The "all six permutations" below were of three locals only, which is why they missed it. The function is 92 instructions; 18 differ and every one is a register choice, with the
same opcodes in the same order:
- retail `outCursor` = r26 and the outer loop counter `remBytes` = r29; ours has r29 and r26
  (so every `sth`/`addi`/`subf`/`cmpw` on those two differs);
- retail computes the second sample into r3 (`mullw r3,r22,r3` / `add r3,r0,r3`) and the first
  clamp's result into r0; ours computes it into r0 and the clamp into r3. The inner clamp uses r0
  in both.
Note what the swapped pair actually is: `this` is r3, so the parameters are r3=this, r4=out,
r5=in, r6=numSamples, r7=startSample, r8=sampleEnd, r9=sampleStart, r10=vol and `state` on the
stack at 72(r1). The two variables fighting for r26/r29 are **`outCursor` and `remBytes`**, not
`outCursor` and `curSample` - `curSample` is r28 in both. All six permutations of the three
function-scope locals (`outCursor`/`curSample`/`inCursor`) were measured and the one in the file
is the best of them: 98.695656% (ABC), 98.532610% (CBA), 98.315216% (BAC), 98.206520% (BCA),
98.097824% (ACB), 98.043480% (CAB). Hoisting `remBytes` out of the `for`-init changes nothing
(98.695656%), and reversing the two operands of `samp2`'s `+` changes nothing either. So
**do not spend a lane re-ordering these three declarations** (superseded: four locals, plus the
`clamped` declarations, did it - see above) - it is already the optimum, and a
lane that tries will burn builds to arrive back here.
**How to tell it apart from a real size problem, in one build.** `unit_fit.sh` reports this
unit "868 bytes over" with 8 extra emitted functions, which reads as fatal. It is not. The DOL
link flags are `-lcf build/G2ME01/ldscript.lcf -m _prolog -strip_partial`, and `-strip_partial`
makes mwldeppc *delete* the duplicate weak copies out of the middle of the section and pack the
rest, so the bytes come back out of whichever object held retail's copy. Proof, from one flipped
build:
- the flipped `main.dol` and the retail-reproducing one are **the same size, 3 969 024 bytes**;
- the 8 extra symbols are **absent from `build/G2ME01/main.elf`** entirely;
- the symbol addresses in the flipped ELF are exactly *our object minus the 8 stripped
  functions* - `IsReady` at our `+0x4c4`, `CancelDMACallback` at our `+0x9d4 - 0x170`, `__sinit`
  at our `+0x11c4 - 0x364` = `0x80327474`, retail's address exactly;
- the diffs are not confined to the unit: `CFilePreload`, `CCubeMoviePlayer` and
  `auto_03_8018A188_text` also change, because their copies of those functions are the ones
  that got stripped.
So **`unit_fit.sh`'s extra-function list is not a verdict** - `flip_test.sh` is, and for a DOL
unit the cheap intermediate measurement is: flip it by hand, then compare
`powerpc-eabi-nm -n build/G2ME01/main.elf` against the report's `virtual_address`es. If the
sizes match and only the *order* inside the unit is wrong, it is this wall.
Two smaller things that flip turns up and are not faults: `.rodata` "SHORT by 1" is alignment
padding (our section is 7 bytes with `2**3` alignment against a claimed 8, so the linker pads
it identically), and `FORCEACTIVE symbol '__sinit_<unit>_cpp' is either not a global symbol` is
because our `__sinit` is local (`t`) where retail's is global - the `.ctors` entry still comes
out at the right size.
### An `inline` in a shared header costs whole functions, silently
Found on `CPakFile` (2026-09-25), and the symptom points nowhere near the cause.
**`rstl::vector::resize` is not `inline` in retail.** Our header had it `inline`, so MWCC inlined
it into every caller; retail emits it out of line and calls it. The visible damage was
`CPakFile::Warmup` stuck at 44% with a 796-byte `InitialHeaderLoad` at 84% - both of which are
*callers*, and neither of which mentions `resize` - plus two whole functions at 0.00% that were
`resize` instantiations our object never emitted, so objdiff had nothing to pair. Dropping the
one keyword: `Warmup` 44.53% -> **100%**, `InitialHeaderLoad` 83.95% -> 99.72%, one function in
`MetroidPrime/main` to 100%, and three unpaired functions became pairable.
**`rstl::vector(int count)` was a real bug, not a codegen difference.** It called `reserve(count)`
and left `x4_count` at 0, so `vector<T> v(n)` produced n elements' worth of uninitialised storage
and a size of 0. Retail's constructor stores the count after the `bl reserve` (`stw r30,12(r1)`).
`CPakFile::EnsureWorldPakReady` depends on it and went 65.65% -> 76.21%.
**`is_trivially_destructible` is specialised in retail, and not uniformly.** With it specialised
for `unsigned int` and `unsigned char`, `clear<vector<unsigned int>>` becomes the 12 bytes retail
has (`li`/`stw`/`blr`) instead of our 68 with a live element loop, and five extra emitted
functions disappear. It is scoped to those two types *on purpose*: adding `unsigned short` sends
`CStateManager::__dt__` from 18.39% to 13.07%, so the trait is not uniform in retail's codegen.
**Widen it one type at a time and re-gate** - the other arithmetic types are individually safe on
`CStateManager` but untested tree-wide.
**What was tried and rejected:** changing `rstl::construct<T>` from `new (dest) T(src)` to
`*static_cast<T*>(dest) = src` removes the null guard MWCC puts on placement new, and it was worth
**+5.44 points on the unit (88.30% -> 93.74%) and 25/33 functions**. It also broke the build:
`main.dol` -> `954faa0d…` and the report fell to 3001 functions. The guard is required by the units
that currently reproduce retail, and it is also the whole remaining gap in
`resize<vector<unsigned char>>`, in `RebuildResourceLists`' zero-fill loop and in
`EnsureWorldPakReady`'s `depList` fill. If anyone revisits it, it has to be **per call site**, not
in the shared header.
### MWCC 2.7's bit-field granularity, and the `rlwimi` trap
Both measured on `CPatterned`'s constructor (2026-09-25), and both will mislead anyone reasoning
from the encoding alone.
**`rlwimi`'s shift in a one-bit field is `31-p`, and the value comes from source bit 0.** Read
`rlwimi r0,r6,7,24,24` with the textbook mask semantics - destination bit 24 takes source bit 17 -
and every one of the 39 one-bit writes in this constructor stores **0**, which would make a
handful of correct header comments wrong. They are not wrong. The project's own MWCC emits
`li r0,1; rlwimi r4,r0,7,24,24` for a `true` bit-field, and `clrlwi r4,r4,24; rlwimi r0,r4,7,24,24`
for a bool variable. So for a 1-bit field at position `p` the shift is `31-p` and the bit taken is
source bit 0. **Do not "correct" a header's bit-field values from the encoding.**
**Word granularity is unreachable for a run of one-bit fields, at least in MWCC 2.7.** Retail writes
the word at `0x420` twenty-six times as `lwz`/`rlwimi`/`stw` - word granularity, shift always
`31-bit`, destination bits 0..25 in order. A standalone test compiled with the project's own
`GC/2.7/mwcceppc.exe` and the flags from `build.ninja`: MWCC 2.7 emits `lbz`/`rlwimi`/`stb` - **byte**
granularity - for a run of one-bit fields in a 4-byte-aligned struct member, and does so identically
for `bool`, `uint` and `short` declarations. Assigning all 26 fields `false` in turn reproduces the
shape exactly (78 instructions) but always byte-wise. **No declaration tried reproduces those 312
bytes.** By contrast the 11-field group at `0x34c`/`0x34d` *is* byte-wise in retail and *is*
reproduced by `bool x34c_24_ : 1;` members assigned one at a time - so the two groups in the same
constructor need different shapes, and one of them is currently unreachable.
The next thing to try, if someone picks it up: a named struct with a whole-word `uint` plus
bitfields, or a union, rather than a bare run of `bool : 1`.
### MWCC rotates a loop only when it cannot count it
Measured on `CPlayerState::InitializeScanTimes` (2026-09-25), and it is a rule rather than an
accident: **MWCC rotates a pre-test loop into `preheader; b latch; body; latch; br body` if and only
if it cannot compute the trip count.** A pre-test loop with a computable constant trip count is fully
unrolled instead, and a post-test loop is never rotated.
The in-tree evidence is three functions in one translation unit that are already matched at 100%:
`GetBitCount(uint)` (`for (; val != 0; val >>= 1) bits += 1;`), and every pointer loop -
`reserve<vector>`, `clear<vector>`, `uninitialized_copy<...>`, `__as__<...>` - all emit exactly
`preheader; b cmp; body; cmp; br body`, and all of them have a trip count the compiler cannot
compute. Against that, every pre-test loop written with a computable count is fully unrolled:
`for (i=0;i<4;++i)`, `while (i<4)`, `i != 4`, `i <= 3`, a `static const uint` bound, a non-const
local bound, `continue`, a dead `break` or `if`, a nested scope, a `switch`, a comma. About fifty
variants, none both.
**The practical consequence: you cannot get the `b` by writing a counted `for`, however you spell
it.** The near-miss that proves the mechanism is worth keeping in mind - `uint i =
static_cast<uint>(-1); while (++i < 4) { ... }` produces the `b` and every other instruction, and
differs from retail by exactly one: `li r6,-1` where retail has `li r6,0`. Reaching retail's shape
would need MWCC to hoist the first `++i` into the preheader, which it does not do. A unit blocked on
one missing `b` is blocked on the compiler, not on the source.
### mwcceppc allocates r30, r29, r28 to the first, second and third local (2026-09-26)
**The register a local gets is decided by its position in the declaration list, counting down from
r30, not up from r28.** This is the mechanism behind "A named temporary can move a register without
changing semantics" above, stated as a rule, and it is worth knowing before spending a lane on
body variants that are all one register swap away.
Measured on `CResLoader::AsyncIdlePakLoading` (0x802FCCF4, 0x9C bytes), whose five live values are
`this`, a bool latch, a node cursor, a `CPakFile*` and the pak's ARAM-file bit, and which retail
holds in **r27, r28, r29, r30, r31** in that order:
| declaration order | emitted |
| `latch, node, pak` (the order the code reads in) | `pak`=r28, `node`=r29, `latch`=r30 |
| `node, pak, latch` | `latch`=r28, `pak`=r29, `node`=r30 |
| `pak, node, latch` | `latch`=r28, `node`=r29, **`pak`=r30** - retail's |
So to reproduce retail's registers the declaration order is the **reverse** of the reading order,
and in the winning shape `pak` is declared **uninitialised, before the cursor it is derived from**,
and assigned inside the loop body. Everything else - the flag read into a local rather than
re-read, the end test written inside the loop condition rather than hoisted into an `end` local, the
cursor as a raw `rstl::list<T>::node*` rather than an `iterator` - follows from keeping the live
set at five, which is what makes retail's 32-byte frame and `stmw r27,12(r1)` come out at all.
Two shapes that look equivalent and are not: hoisting `end` into a local costs a sixth live value
and the frame becomes 48 bytes with seven saved registers, and taking `SPakLoadEntry& entry = *it`
before the body needs the item's address in a register throughout, which costs the same.
**How to find it without a variant search:** read retail's register numbers off the disassembly
(`this` is r27, and the rest are r28.. in order), then permute *only the declaration order* of the
locals - the bodies do not have to change at all. Two instructions per rebuild.
### Declaring a unit's functions ascending is right when the lower one is emitted first
"Declare in reverse" (above) says a unit's functions must be declared **descending** by retail
offset, because mwcceppc emits in reverse source order. `src/Kyoto/CResLoaderPakPump.cpp` is the
case that makes the rule mechanical rather than memorable: it holds two functions and they are
declared **ascending** - `AsyncIdlePakLoading` (0x802FCCF4) first, `AreAllPaksLoaded` (0x802FCCE4)
second - because the *lower* offset has to be emitted *first*, and emitting in reverse source order
means the *higher* offset has to be declared first. Declaring them the readable way round
(`AreAllPaksLoaded`, the predicate, first) gives an object that is still **exactly 0xAC bytes**,
`unit_fit.sh` reports "fits", objdiff still pairs by name and reads **100.00%** on both functions -
and the object then lands **0x200 bytes early** in the DOL, so `dtk shasum` fails and all 86 RELs
go with it. The only instrument that sees it is `flip_test.sh`, and the symptom reads like a
wildly wrong body rather than a transposition.
Two smaller things that came with it, both in the same unit and both worth knowing:
- **`CResLoader` is 0x60 bytes, not 0x58, and the +0x5C the tree assumed is a `rstl::list`'s
  `x14_count`.** The evidence is three counts read at +0x2C, +0x44 and +0x5C plus the erase at
  0x802fd1f4 decrementing the *same* word `AreAllPaksLoaded` reads; written out in
  `docs/research/paks.md`. Every offset downstream of it moved by 8, including `CFactoryMgr`
  (0x5C -> 0x64) and `CResFactory`'s size (0xC8 -> 0xD0), so this is a header change with a
  `CHECK_SIZEOF` blast radius, not a one-line fix.
- **Renaming an unnamed DOL symbol needs the *mangled* name, and `mwcceppc` will not accept
  `friend extern "C"`.** `fn_802FCCE4`/`fn_802FCCF4` are referenced by `main.o` and two
  `auto_*` objects, so claiming their bytes without renaming leaves the link undefined. The
  mangled names are `AreAllPaksLoaded__10CResLoaderCFv` / `AsyncIdlePakLoading__10CResLoaderFv`
  (read them off our own object with `nm`, not off a C++ compiler's mangling). And a port-side
  `extern "C"` copy of a retail helper that has to reach a private member cannot be a friend
  declared as `friend extern "C" void* f(void*, void*);` - mwcceppc reads the `extern` as a storage
  class and stops. Declare it `extern "C"` at namespace scope above the class and then write a
  plain `friend` declaration, which binds the same entity.
### An unnamed function is often a template instantiation you can identify by diffing it
This is the technique that landed `fn_802FC350`/`fn_802FC378` (2026-09-26, lane `k4`), and it is
worth trying on **any** unnamed function before writing a body, because when it works the body is
already written somewhere in the tree at 100%.
`rstl::list< rstl::auto_ptr< CFilePreloadData > >::do_insert_before` is a **`Matching` unit**
(`src/Kyoto/Streams/CFilePreload.cpp`, 100.00%, `scope:weak` in `symbols.txt` at 0x803445DC,
0xA8). `fn_802FC378` is unnamed, 0xA8, and in the same loader. Disassemble both and diff:
```sh
tools/dis.sh 0x803445DC 0xA8 > /tmp/a; tools/dis.sh 0x802FC378 0xA8 > /tmp/b
sed -E 's/^[0-9a-f]+ <[^>]*>:/\n/' /tmp/a   # strip addresses, keep mnemonics and operands
```
They are **identical instruction for instruction and register for register**, apart from the two
`bl` displacements. That is not a coincidence to be explained - it is the identification. The
function you are looking at *is* that instantiation with a different template argument, so:
* the **element type** is whatever the two instantiations have in common, and the `addic. r5,r3,8`
  / `beq` / three stores inside the copy are **that element's copy constructor**, not statements
  in the function. Here they are `rstl::auto_ptr`'s auto-relinquishing constructor, and the
  erasure side (`fn_802FD174`: `lbz` the byte, then `bl __dt__CPakFileFv` on `*(item+4)`) is
  that class's destructor, which is what confirms it.
* you can then write the function as **the container's own member** - `do_insert_before` called
  through the public `node*` - rather than a transcription, and it comes out byte-identical on
  the first build. The only thing left is the `extern "C"` wrapper for retail's dtk name.
The diagnostic generalises: **grep `symbols.txt` for a `size:` that equals your function's**, and
prefer a `scope:weak` template member over a named function. `do_insert_before` appears three
times in `symbols.txt` (0x8026D088 0x28, 0x803277C4 0x90, 0x803445DC 0xA8) and the third was the
one to compare against.
The corollary is a trap: **if the match is a template member, mwcceppc emits it out of line and
calls it** unless `#pragma inline_max_size` is large enough. Left at the default, `fn_802FC378`
came out as a 0x20-byte forwarder to a separate COMDAT - 0x58 of the 0xA8 missing and a symbol
retail does not have, with `unit_fit.sh` reporting a third function and "over by 32".
`#pragma inline_max_size(0)` is the opposite mistake: it stops *every* inline, so
`rstl::construct` stops being a placement `new` and becomes `__nw__FUlPv` plus a null test
plus a call.
**And the threshold is not a constant of the compiler - it moves when a header does.** 125 was
measured working for that unit, and stopped working when `Kyoto/CResLoader.hpp` started
including `Kyoto/CPakFile.hpp`; 190 is the new floor and the unit uses 200. Nothing about the
source or the body changed. So when a unit that has flipped before suddenly reports an extra
COMDAT template member, raise the pragma first - that is a cheaper hypothesis than "the body
regressed" and the symptom looks nothing like it.
### Four codegen rules that are not about register allocation, and one that is
Measured 2026-09-26 on four `CResLoader` units that went to 100% (see the Attempted modules table
and `docs/research/paks.md`). The first four are general and cheap; the fifth is the wall.
1. **Declare the return type MWCC can see is dead.** A function whose result every caller ignores
   must be declared `void`, not `void*`: `fn_802FC420` as `void*` emits a trailing `li r3,0` and is
   8 bytes longer than retail's 0xB8. Same polarity rule as `fn_802FCAE8`'s
   `IsCompressed() ? 1 : 0` and as `if (found) { return ...; } return 0;` - MWCC normalises what it
   can prove dead, so the *declaration* has to be the dead one.
2. **A named local decides the evaluation order of an expression's callees.** mwcceppc does not
   evaluate arguments left to right. `AsyncSeekRead(buf, (res->GetSize() + 31) & ~31, kSO_Set,
   res->GetOffset())` gave 87.03% with the accessors in the wrong order; hoisting one into a named
   local gave retail's exact order and 100.00%. **This is the cheapest experiment on any unit that
   is near 100% and whose diff is "the same calls, the wrong order".**
3. **A temporary passed straight into a call is not the same as a named local, and retail's
   instruction sequence tells you which it was.** `CMemory::Alloc(n, h, s, t, CCallStack(...))` in a
   temporary emits `addi r3,r1,8` / `bl <ctor>` / `mr r7,r3`; the same object in a named local emits
   `addi r7,r1,8`. Retail has the first, and the reason is that its 12-byte constructor
   (`stw r5,0(r3)` / `stw r6,4(r3)` / `blr`) leaves r3 alone, so the reference *is* r3. 91.70% vs
   100.00%. When retail's `mr rX,r3` immediately follows a call, the argument was the callee's own
   register, not a recomputed address.
4. **A private parameter with a default can still be the answer, and the default is usually a named
   retail constant.** `CCallStack`'s third parameter is `static const char kUnknownType[]`, private -
   unreachable from out here - and retail's relocation is `kUnknownType__10CCallStack` at
   0x803AEAB8. The two-argument call reaches it *through the default*. Before reaching for a
   `friend` or a `#define private public`, check whether the default argument is already the
   symbol. (This one also had the wrong address in the previous lane's table, which is the general
   point: **read the address out of `symbols.txt`, not out of the previous lane's prose.**)
5. **The wall: mwcceppc's choice among two free callee-saved registers is not reachable from the
   source.** `fn_802FC4D8` is 99.10% and `fn_802FC63C` 98.04% with every instruction right; the whole
   residual is that the compressed arm gets r6/r7 and r29/r30 where retail uses r7/r6 and r30/r29 (20
   instructions), and r27/r28/r29 against r28/r29/r30 (16). Roughly forty body shapes - cursor
   spelling, cursor type, size type, naming the temporaries, `Get(4)` vs the member, `get()` vs
   `operator->`, a `CMemoryInStream*` cast, an `if` vs `?:`, hoisted and sunk declarations, a named
   `owns` bool, a named `resSize` - moved it by nothing. **Report that as a register-allocation
   blocker and leave the unit `NonMatching` with its range claimed** (which is what
   `CResLoaderGetPakFile.cpp` and `MetroidPrime/Player/CPlayerState.cpp` do), rather than deleting a
   real initialisation to raise the average. The generalised form of rule 2's companion note in
   "A named temporary can move a register" applies here too, with the opposite outcome: if the whole
   diff is a register *number*, try naming things, but **stop after one systematic sweep and say so** -
   forty variants is a measurement, not a search.
### A named temporary can move a register without changing semantics
`InitializeScanTimes`, 97.63% -> 98.25% with one line and no logic change: `push_back_unsafe(
SScanState(it->first))` became
```cpp
const CAssetId id = it->first;
unkStruct.vec.push_back_unsafe(SPersistentState::SScanState(id));
```
MWCC had allocated the source iterator to r7 and the loaded id to r6; retail has them the other way
round. Five instructions moved. **Worth trying on any unit sitting near 99% with a "wrong register"
complaint** - it is cheaper than the body-variant search, because it is a single naming decision
rather than a control-flow experiment.
### Reading an address in the DOL, and a module's `.text` out of its `.rel`
Both cost a lane real time, and both end in a wrong answer rather than an error, so they are
worth writing down. `docs/research/port_globals.md` is the worked example.
**Which base register a small-data address uses.** `readelf -s` on `build/G2ME01/main.elf`
says `_SDA_BASE_ = 0x8041FD80` and `_SDA2_BASE_ = 0x804223C0`, and the naming implies r2 gets
the first. It is the other way round for the code that matters: **r13 is 0x8041FD80** and is
what `.sbss` is addressed through, **r2 is 0x804223C0** and is what `.sdata2`/`.rodata` are
addressed through. Compute the offset for both and grep for both before concluding a symbol is
unreferenced - `gpTweakGame` (`.sbss:0x80418F30`) is 10 hits at `-28240(r13)` and zero at any
`r2` offset.
**Section file offsets are not the VMAs,** in the DOL *or* the ELF, and the two disagree with
each other: `.rodata` is VMA 0x803A56C0 at file offset 0x3A27A0, and `.sdata2` is
0x8041A3C0 at 0x3C3C20 in the ELF but 0x3C3B40 in the DOL. Read the offset out of
`objdump -h` and validate it on a known string before believing anything you read back -
`0x803AEAF2` really is `"%s\n"`, which is what makes the mapping trustworthy. Getting this
wrong produces a *plausible* wrong value rather than a failure: the eight bytes at
`.sdata2:0x8041D550` read as `0xC6C33A80` under the wrong mapping and as `0x803AC3C6` - a
perfectly good pointer into `.rodata` - under the right one, and only the second is real.
**A REL module's `.text` is at file offset 0xA4 of its `.rel`**, not at the end of the 0x4C
header. Find it from the module's own map: `RELExit` / `RELMain` / `TweaksInit` have prologues
at 0xA4, 0xC8 and 0xE8, which pins module offset 0 to 0xA4. Then:
```sh
python3 -c "d=open('orig/G2ME01/files/RelProd/Tweaks.rel','rb').read(); \
  open('/tmp/t.bin','wb').write(d[0xa4:])"
./build/binutils/powerpc-eabi-objdump -D -b binary -EB -m powerpc:common \
  --start-address=0x508 --stop-address=0xab4 /tmp/t.bin
```
**`-EB` is not optional.** Without it objdump decodes the words byte-swapped, and the output
looks like plausible PowerPC with occasional garbage rather than an error. It is the reason
`REL_CreateTweakGlobals` first came out as nonsense.
The lesson is the one this file keeps making: **a check that cannot fail is not a check.** Both
tools still work where they are pointed at the right thing; the trap is that they report
success where they measure nothing.
### The port's link gap is 652 symbols, and most of it is bulk work, not decompilation
**Superseded, 2026-09-25, after the loader thunks landed:** the figure in the heading was 724, and
the measurement below still stands except for the counts. `docs/research/port_link_gap.md` has
the current table; 72 of the 234 "REL module loaders" closed as 64 `Matching` DOL units, and all
159 of the entity loaders are now identified (`docs/research/rel_loaders.md`) - they were never
the unknowns this section implied.
Measured 2026-09-25 with `tools/link_gap.py`; the work list is `docs/research/port_link_gap.md`
and the checker is in `tools/gate.sh`. This is the decompilation's half of the port's blocking
path, and it was unquantified until now - the port builds its game sources as an OBJECT library,
so no link step exists to fail and nothing ever reported what was missing.
**Of 1376 undefined symbols in `mp_game`, 44 are genuinely unaccounted for** (63 when first
measured; the 19 that closed are the retail globals below). The rest are the C++ runtime (722),
libc (23), and 107 that appear somewhere in Aurora's own trees. Those four kinds of missing symbol
are, in order of interest:
1. **29 functions nobody has written.** The port's own sources declare them `extern "C"` and
   call them. Where retail names the function, the port is calling it under its `fn_` name and
   the rename is the first step. **Two are on the port's blocking path by name:**
   `CreateFrameEnd__7MakeMsgF14EArchMsgTargetRCi` (0x800489AC, 204 bytes) is called by
   `CGameArchitectureSupport::Update`, and `SolveQuadratic__5CMathFfffRfRf` (0x802CC064, 188
   bytes) by `CMayaSpline`. Sizes run from 8 bytes to **604** (`fn_80038624`, in
   `CStateManager`).
2. **19 retail globals declared `extern` and never defined - CLOSED.** This was the class worth
   understanding, because it is *correct* in the decompilation and *impossible* in a PC link:
   ```cpp
   extern "C" int lbl_80419A10;   // CStateManager.cpp:33 - a declaration, not a definition
   lbl_80419A10 = x16a8;          // and assigned at :501
   ```
   For the decompilation that is right - retail's own objects define those symbols and the DOL
   links against them. **A standalone PC link is what finally forces this repository's data to
   be complete**, and this list is where it was not. Note also that dtk renames retail's
   `kInvalidUniqueId`-style constants and the `.rodata` float pools to `lbl_*`, so the value a
   definition needs has to come out of the DOL's data, not out of a header.
   All 19 are defined in `src/MetroidPrime/main.cpp` under one `extern "C"` block. Four things
   the reading needs, each of which cost a build:
   - **The width comes from the retail instruction, not dtk's `size:`.** dtk's `size:` is the gap
     to the next symbol, so `lbl_80419A10` claims 8 bytes and `lbl_8041A8BC` is the only one whose
     gap equals its type. `objdump -d -r build/G2ME01/obj/<unit>.o` is the arbiter: `lhz`/`lwz`/
     `lfs`/`stb`/`stw` pin `lbl_8041E2E6` to two bytes and `lbl_80419A98` to one.
   - **`.bss`/`.sbss` symbols have no contents in the ELF**, so their value at load is 0. That is
     13 of the 19, and it is a fact rather than a guess.
   - **Two GCC traps.** An uninitialised tentative definition that nothing in the translation unit
     reads is *dropped*, so every one of these needs an explicit `= 0` or the link is no better
     off; and inside an `extern "C" { }` block GCC gives a `const` declaration **internal**
     linkage without an explicit `extern`, so six of the 19 vanish unless it is written.
   - **`lbl_8041D394`/`lbl_8041D398` are guest addresses** (0x803AADF2/0x803AADFC, the `.rodata`
     strings `"ShotSmoke"` and `"Power2nd_1"`) and a 64-bit link cannot hold them, so the
     definition is the string.
3. **8 game globals and constants** - `gpRender`, four `gpTweak*` pointers, and the three
   `kInvalid*Id` values.
4. **6 REL module symbols** - `REL_loader_CannonBall` and five `lbl_57_rodata_*` labels. Port
   code (`platform/rel.cpp`), not decompilation.
**What the number does not prove.** The 107 attributed to Aurora are attributed because the
identifier appears in a file under `extern/aurora`; a name in a source is not a definition in an
object. The one symbol where the distinction is already known to bite is `AIStartDMA`, which
appears in an Aurora *header* and in none of its sources. **The authoritative answer is an actual
link**, and until one succeeds the Aurora half of the gap is unverified.
### A `static` on a namespace-scope declaration is internal linkage, and it hides a missing body
Found 2026-09-25 closing the three symbols `CGameArchitectureSupport::Update` needed, and it
is a trap because the build *does* tell you, in a warning most people read as noise:
```cpp
namespace MakeMsg {
  static CArchitectureMessage CreateFrameBegin(EArchMsgTarget, int);   // never satisfiable
};
```
`static` at namespace scope is internal linkage, so no other translation unit can ever define
or call it, and the port's build said so for every call site: `warning: 'CArchitectureMessage
MakeMsg::CreateFrameBegin(EArchMsgTarget, int)' used but never defined`. `main.cpp` calls both
factories from `UpdateTicks`, so the warning named the two declarations and nothing else. Drop
the `static` and define them, and the warning goes with it. The same shape hides a
class-scope `static` member function that is never defined out of line.
### Retail's `CArchitectureMessage` has one parm, not two - the fourth word is the refcount
Worth writing down because the disassembly invites the wrong reading and then the header gets
"fixed" to match. `MakeMsg::CreateFrameEnd` (0x800489AC), `CreateFrameBegin` (0x80048A80) and
`CreateTimerTick` (0x80048DC8) are 0xCC bytes each and end with four stores into the message
they return - at +0x00 target, +0x04 type, +0x08 and +0x0c - which looks like two parameters.
It is not: +0x08 and +0x0c are the two words of retail's `rstl::rc_ptr` (`{ T* x0_ptr;
u32* x4_refCount; }`). The evidence is the same three lines in all three factories - a
`new(4)` whose first word is set to 1 and stored at +0x0c, then `*(refCount) += 1` and a
`ReleaseData` on the stack copy - and it is the same pattern `CGameState`'s constructor uses at
0x80144140 for its own `rc_ptr`. Retail's message is 16 bytes with one parm, and
`CArchitectureMessage`'s three-argument constructor was already right.
Two more from the same job, both about writing an accessor retail keeps out of line:
- **An accessor retail calls is not inlinable, and inlining it costs the caller real
  percent.** `CGameArchitectureSupport::Update` is 100% against retail 0x80007A14 and stays
  that way only because `CGameState::GetWorldState` is out of line: retail's definition is in
  `CGameState.cpp`, a different translation unit. Inline it and the call folds into
  `lwz r4,gpGameState; lwz r3,60(r4)`, dropping the function to **95.89%**; add a null test on
  the result and it is **84.78%**. The port has no `CGameState.cpp`, so the definition sits at
  the bottom of `src/MetroidPrime/main.cpp` under `#pragma inline_max_size(0)` - the pattern
  `src/Kyoto/Input/CRumbleVoice.cpp` already uses.
- **Returning a reference to the pointer, not the pointer, is what reproduces the pair.** The
  accessor's whole body is `addi r3,r3,60; blr` and its caller then does one `lwz r3,0(r3)`, so
  `CWorldState*& GetWorldState()` is the signature that generates both halves. Returning
  `CWorldState*` puts the load inside the accessor and the caller's stream is one instruction
  short; returning `rstl::rc_ptr<CWorldState>&` needs two dependent loads on this port, whose
  `rc_ptr` is one word wide (see the negative result above).
## Two `fn_` functions that are `rstl` template members retail left out of line
Measured 2026-09-25 on `main/MetroidPrime/CStateManager`. Both matched on the first or second
attempt, and both lessons are reusable.
**1. A `fn_` name with four arguments and no `this` read is a non-static member.** `fn_8003C0C4`
(0x8003C0C4, 236 bytes) takes `r3` (the list, never read), `r4` (prev), `r5` (next), `r6` (the
value). `rstl::list<T>::create_node(node* prev, node* next, const T& val)` has exactly that
register signature, and `li r3,140` is `sizeof(node) = 8 + sizeof(T)`, so `sizeof(T) = 132` -
which is `sizeof(rstl::reserved_vector<CEntity*, 32>)`, the element type of
`CStateManager::m_graveyard`. Its caller `fn_8003C054` is then
`do_insert_before(node* n, const T&)`, and **`<rstl/list.hpp>` already contains that body
verbatim**, member for member: `if (n == x4_start) x4_start = nn; nn->prev->next = nn;
nn->next->prev = nn; ++x14_count;`. dtk gives no name to an out-of-line template member
here, so objdiff scored both 0% and there was nothing to rename *to* - an `extern "C"` copy over
a local POD mirror of the layout is what pairs them.
Three things make that work and are worth repeating:
- **Model the list object and its node locally, with `CHECK_SIZEOF`, and keep the real type
  only in the signatures that already match.** `fn_8003C02C` was at 100% and had to stay
  there; it takes `rstl::list<...>&`, so the list view is a `reinterpret_cast` and nothing
  else changes.
- **`rstl::construct` is what produces MWCC's unrolled block move.** A hand-written
  `uninitialized_copy_n` gives a naive one-word loop, and `memcpy` gives a `bl memcpy`. The
  8-word-unrolled `srwi r0,rX,3` body with the 4-byte tail is what the *copy constructor* of
  the element type compiles to, and only that.
- **`rmemory_allocator::allocate(size)` vs `allocate(out, count)` is 9 instructions of 59.**
  The templated out-parameter form gives the pointer two definitions; MWCC then spills `r3`
  across the copy loop and reloads it at the end, and the copy length lands in `r3` instead of
  the register that held `&item`. Writing
  `T* n = reinterpret_cast<T*>(rmemory_allocator::allocate(sizeof(T)));`
  is byte-exact. **When a function is one spill away, try the two spellings of the allocation
  before touching anything else** - this was 6 of the 9 differing instructions.
**2. Take a function's size from `report.json` or `nm`, never from subtracting addresses.**
See the correction in "What the port still needs in order to link" above. `0x8003ABF0 -
0x80038624` is 9,676 bytes and was quoted as one 604-byte function; it is 39 functions. The
subtraction is only a function's size when the next symbol is the next *function*, which in a
unit with 239 of them mostly is not. This cost a lane a whole turn of hunting a mystery that
was 604 bytes of dead code.
### Where a module can even be written
`include/MetroidPrime/Enemies/` now has `CAi` and `CPatterned` as `Matching` units (`CPatterned` is
`NonMatching` again since the 2026-09-28 upstream merge widened it to 103 functions), so a module
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
**`CPatterned` landed too** (2026-09-25, same day; superseded 2026-09-28, when upstream's unit took
the whole class and it became `NonMatching` 27/103 with these ten still matching): a `Matching` unit, 10 of 10 functions, by
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
**`tools/collect.sh <lane>`** does steps 1-4 below in one command, in about seven seconds:
```sh
tools/collect.sh a1            # one lane, or several: tools/collect.sh a1 a3 a5
```
It builds a fresh worktree at **current** HEAD with its own real `build/`, records a report
**baseline from unmodified HEAD** (`tools/gate.sh --baseline`), exports the lane's diff over
`src include config configure.py libc tools docs`, applies it with `git apply --3way`, runs
`tools/gate.sh` on the merged result and prints the diff stat. The merged worktree is left at
`/tmp/opencode/collect-<lane>` for reading, hand-fixing, or copying files across.
Two things it does that hand collection does not. A lane's `config/` is its own view, so a
**stale file becomes a visible conflict instead of a silent revert** - and the silent revert is
what cost three modules their `Rel(...)` blocks. And the baseline is built from HEAD rather than
from the lane, so the per-function diff compares like with like: the lane's own numbers are
measured against a tree that never had its changes.
What is still yours: **read the lane's report and check its claims against the gate's output.**
A passing gate means the tree is sound, not that the lane did what it said. Then commit, with
the lane's findings in the message.
By hand, the steps are:
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
The limit is not hardware, and since `tools/collect.sh` it is not the mechanics of collection
either - applying a lane and gating the result is one command and about seven seconds. The limit
is **judgement**: a gate that passes says the tree is sound, not that the lane's claim is true,
and every lane still has to be read against it. A wave of six to eight is comfortable to judge
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
  layer, and `CPatterned`/`CAi` now exist as `Matching` units (`CActor::UnkVtable20` is resolved, superseded
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
- **"All 86 RELs differ" is one fact, not 87.** `makerel` runs `dtk rel make ... @$rspfile` with
  `build/G2ME01/main.elf` as its first input, so *any* change to the linked DOL shifts every REL.
  A lane spent a bisect proving this. The corollary is the one that matters: `ninja`'s exit status
  **is** the hash gate, and `main.dol` must never be read after a failed `ninja` - it is the
  previous build's file
- `./tools/probe_sources.sh` green (819 files, 0 failures)
- `python3 tools/check_symbol_names.py` reports 0 missing names- `All:` matched count from the report does not fall
- `config/G2ME01/splits.txt` and `configure.py` only change when the task is explicitly a
  config task (REL modules), never as a side effect
- `python3 tools/check_docs_claims.py` reports no disagreement (it derives the docs' numbers from the
  report; a doc claim that cannot be checked is a claim that will drift)
- `python3 tools/check_module_wiring.py` reports nothing UNWIRED (a module whose sources no
  `configure.py` entry declares is in no link, whatever the report shows)
- `python3 tools/link_check.sh` reports no rise in undefined symbols and **zero** duplicate
  definitions, against `docs/research/port_link_baseline.txt`. This is the *slow* gate - it
  configures Aurora, fetches its SDL3 and Dawn, and builds 118 game units - so it is not in
  `gate.sh`, and it is the only check that can see a change to `CMakeLists.txt`, `files.cmake`
  or `platform/`. Run it before committing any of those. A **duplicate definition** is the
  signal to watch: it means two translation units claim one name, which is how the fourteen
  `RELMain`s surfaced.
- `python3 tools/check_files_cmake.py` reports no omission. **A configured, on-disk unit that
  `files.cmake` does not name is in no port binary, and `link_gap.py` cannot see it** - the tool
  derives the gap from the objects `files.cmake` produces, so the omission is invisible to the
  instrument meant to measure it. 96 units were in that state, including all 72 `Matching` loader
  thunks, which were `Matching` in the DOL and `MISSING` in the port's link for a release. Only
  `tools/link_check.sh`, which asks the real linker, found it. **A gap number from a tool is a
  statement about the tool's inputs.** Exclusions are declared in the tool with a one-line reason
  and a *stale* one fails too, so the list cannot become a place where things go to die. Note what it
  does not claim: being listed is not the same as being a win. Defining a default constructor
  constructs its members and can *open* a gap - one unit measured net -1 in one configuration and +1
  in another, so **measure the net in the configuration you are in.**
- `python3 tools/check_raw_offsets.py` is clean (a raw offset is a documented stopgap, not a
  decompilation - see `docs/research/raw_offsets.md`)
- `python3 tools/check_decl_order.py` agrees with `docs/research/decl_order.md` (a permuted unit
  compiles, links, scores 100% and breaks the hash on a few bytes - see "Declare in reverse")
## Attempted modules (keep this list current)
| module | what happened |
| ninth upstream sync (`5f97267f`) | **Landed, 2026-10-02** - matched 12699 -> 13012, linked 6058 -> 6118, DOL and 86 RELs bit-identical. 36 of our `Matching` carves deleted for upstream whole-file units; 26 upstream units added to `files.cmake`, 17 excluded with measured reasons. **Two things the gate caught that a green build did not**: `CScriptRepulsor`'s constructor fell 80.25% -> 67.86% because upstream's `CActorParameters` has no user destructor (now 100%: `CActorParameters::None()` plus `CMaterialList((flags & 1) ? kMT_Pillar : kMT_Pillar)`; re-adding the destructor broke 87 checksums), and two `PortLinkStubs.cpp` stubs became reachable through `CAi.cpp` and had to go. Detail in `docs/HANDOFF.md`, "The ninth sync". |
| `CCubeRenderer::EndScene` | **Landed, 2026-09-27** - `Matching` 100.00% 1/1, retail 0x8026FB80, 0x7C = 124 B, `flip_test` PASS, `linked` 2556 -> 2557, DOL bit-identical. **124 bytes and it abuts `BeginScene` exactly**: 0x8026FB80 + 0x7C = 0x8026FBFC. Two traps cleared, both measured: retail's `r13` is **`_SDA_BASE_` (0x8041FD80), not `_SDA2_BASE_`**, and `lbl_80418AE4` **must be declared non-`const`** - declared `const` it compiled, linked, and hoisted the `lbz` above the frame stores, leaving 2 differing instructions at the **same length**. `tools/sda.py` hardcodes the `.sdata` base and is silently wrong for `.sdata2`. **No pixels**: the draw methods are still logging stubs. |
| `CCubeRenderer` (four units) | **Landed, 2026-09-27** - `matched` 3977 -> 3979, `linked` 2555 -> 2556, DOL bit-identical, 87/87. **`Carve80272958.c` is `Matching` 100.00%** (0x80272958, 0x30) and is the only one of the four that counts as linked. **`BeginScene` is also 100.00% and must stay `NonMatching`** - mwldeppc attributes 20 bytes of `.sdata2` to its object that retail does not have, which costs 32 bytes of `main.dol` and breaks 43 REL hashes. See "A percentage is not a link result" below. The vtable is now **real**: `Carve80270848.cpp` is the key function and the only thing that emits `vtable for CCubeRenderer`, and the port gap 309 -> 391 is the known cost of listing it (76 arriving symbols are `CCubeRenderer::` methods with no body yet). |
| `CMain` (header) | **Landed, 2026-09-27** - `sizeof(CMain)` was 0x94 and is **0x98**, proved by retail's own `sMainSpace` (`.bss:0x803C5A20; size:0x98`, next object at 0x803C5AB8), not inferred. The `+0x18..+0x48` region stopped being `char x10_pad[0x38]` and became a `double`, two 20-byte `SFrameTimeHistory` and their two **sums**. **Adds 0 to `matched` and 0 to `linked`** - and is still worth landing, because it is the port's type model being right about a boot-path object. See "`sizeof(CMain)` is 0x98" below. |
| `CCallStack` | **Landed, 2026-09-27** - retail's `RAssert` call-stack scaffolding, and **it formats nothing.** The class is eight bytes (two `char const*`), the constructor discards its `uint` argument, and the two accessors are plain `lwz`/`blr`. `include/Kyoto/Alloc/CCallStack.hpp` is right about the layout and wrong about the names: `x0_line`/`x4_type` are the *second* and *third* arguments. Which accessor is which is **not guessed** - `CGameAllocator::FixupAllocPtrs`, the only caller, stores the +0 read into `SGameMemInfo::x8_fileAndLine` and the +4 read into `xc_type`, which settles both names at once. New `src/MetroidPrime/CCallStack.cpp`, 0x8028BFD8..0x8028BFF4, 0x1C = 28 B, 3 functions, **`Matching` 100.00% (3/3)**, `flip_test` PASS, port gap 318 -> 315 MISSING. **And the required follow-up was the fourth instance of its class:** stubs 28/29/30 had to be deleted from `PortReachStubs.cpp` by hand, because `boot_probe.sh` builds `-DMP_BOOT_STUBS=ON` and `gate.sh`'s duplicate count cannot see that configuration. |
| `main.cpp` (three-way split) | **Landed, 2026-09-27** - the split is **not free**, and the reason is structural rather than tunable. See "The `@stringBase0` pool is PER TRANSLATION UNIT" below. `CMain::FillInAssetIDs` (0x80006B38, 0x48 = 72 B) is now an isolated **`Matching` 100.00% 1/1** unit and `linked` rose 2554 -> 2555; the cost is -5.113 on `__ct__24CGameArchitectureSupport`, a 1-of-11 function that contributes 0 to both counts and whose behaviour is unchanged. `CMain::AsyncIdle` was **declined** on the mirror-image reasoning: 1-of-11, contributing 0 to both, for no `Matching` unit. |
| `AIMannedTurret` | **Landed, 2026-09-25** - the first module whose unit genuinely flips, and the failure this table recorded for several sessions was real but was not a blocked module. Declared ascending, the unit broke the module's hash (85/86, exactly as measured); the cause was **declaration order**, not a rename, a symbol, a data section or extra functions. See "Declare in reverse" below. With the order fixed: unit `Matching`, `flip_test.sh` PASS, sha1 `949b8c21caf1112b10d07748dbe8c32d3bd7efac` verified against `config.yml`, DOL and all 86 RELs unchanged. The first modules to link our own code are still `ScriptRiftPortal` and `Metaree`; `AIMannedTurret` is the first whose unit **flips**. |
| `Tweaks` | **Partly landed, 2026-09-26 (lane `e1`)** - the module's 76 `LoadTypedef<T>` bodies are **not** 68 distinct functions: 56 are in `Tweaks`, 7 in the DOL, and 5 of the port's names are retail's `UnknownStruct1/2`. The generated bodies are already **99.1-100%**; seven of them are at exactly 100% and three more landed as `Matching` units by **re-splitting the existing `[LoadTypedef, ~T, T]` triples** so each new unit claims only its `LoadTypedef` - see "A `Matching` unit may claim one function of a three-function triple" below. The retail member layout of all 79 `SLdr*`/`CTweak*` structs is now in `docs/research/sldr_tweak_sizes.md`, and it **overturns** the 1,500-byte `CTweakContents` drift in `docs/research/tweak_globals.md`: that figure is an LP64 artifact of a host probe (`sizeof(rstl::string)` is 24 there, 16 in the MWCC build), and with retail's widths the headers reproduce retail's layout exactly except for **one** struct, `SLdrTweakPlayerRes_AutoMapperIcons`, which carries five members that are not properties of it (+0x50). |
| `Krocuss` | **Landed, 2026-09-29, lane 2** - the module head extended to `.text 0x0..0xD8`: **`fn_38_0`**, the `GetBoundingBox` wrapper in front of the accessor block, on top of the 14 accessors, 15/15 at 100.00%, `flip_test.sh` PASS, module sha1 `fec35d1d4bd7e6815c37af398be8c1b0ff2864fa` unchanged against `config/G2ME01/config.yml`, `.rel` `cmp`-identical, all 86 holding, `main.dol` still `6ef9b491...`, `matched` 9177 -> 9178, `linked` 4003 -> 4004, the module's own count 14 -> **15 of 65** (`audit_rel_claim.py`: 65 text symbols in the preplf, 0 dropped by `-strip_partial`). `fn_38_0` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, and the transfer needed nothing re-found. The `#ifdef __MWERKS__` guard is `ShredderAccessors.cpp`'s, and it is load-bearing here for the same reason: the port's undefined count measured **259 -> 259, 0 duplicates**, with `link_gap` and not `build.sha1` the step that would otherwise have failed. See "A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1`" below. |
| `Shredder` | **Landed, 2026-09-29, lane 2** - the module head extended to `.text 0x0..0xC8`: **`fn_68_0`**, the `GetBoundingBox` wrapper in front of the accessor block, on top of the 12 accessors, 13/13 at 100.00%, `flip_test.sh` PASS, module sha1 `a70ac4a192c4ea785af74c09179b6fd64c8ed382` unchanged against `config/G2ME01/config.yml`, `.rel` `cmp`-identical, all 86 holding, `main.dol` still `6ef9b491...`, `matched` 9176 -> 9177, `linked` 4002 -> 4003, the module's own count 12 -> **13 of 62**. `fn_68_0` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`. See "A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1`" below. |
| `IngSpiderballGuardian` | **Landed, 2026-09-29, lane 2** - the module head extended to `.text 0x0..0xD8`: **`fn_35_0`** (`li r3,1; blr`) and **`fn_35_8`**, the `GetBoundingBox` wrapper, in front of the 13 accessors, 15/15 at 100.00%, `flip_test.sh` PASS, module sha1 `2c171d03c7ee30a71249350731ad43256c96098d` unchanged against `config/G2ME01/config.yml`, `.rel` `cmp`-identical, all 86 holding, `main.dol` still `6ef9b491...`, `matched` 9178 -> 9180, `linked` 4004 -> 4006, the module's own count 13 -> **15 of 87**. Unlike `Shredder` and `Krocuss` there are **two** functions below the accessor block, not one, so the wrapper is at 0x8 and not at 0x0 - the dtk `fn_<id>_<off>` name is not a clue to which. `fn_35_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with the module's own out-of-line `optional_object<CAABox>` constructor `fn_35_4030` as its callee. Port link measured **259 -> 259 undefined, 0 duplicates**. See "A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1`" below. |
| `Ripper` | **Head extended to 0x0, 2026-09-29 (lane 1) - `.text 0x0..0xD8`, 15/15 at 100.00%, the unit was already `Matching` and stays `Matching`, `main.dol` still `6ef9b491...`, `matched` 9178 -> 9179, `linked` 4004 -> 4005, the module's own count 14 -> **15 of 56** (`audit_rel_claim.py Ripper`: 56 text symbols in the preplf, 56 in the plf, 0 dropped by `-strip_partial`, 0 problem claims).** Module 54; the claim grew from `0x3C..0xD8` (the 14 accessors) to `0x0..0xD8` by writing `fn_54_0`, the 0x3C-byte `GetBoundingBox` wrapper in front of them - instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10` and `KrocussAccessors.cpp`'s `fn_38_0`, so nothing had to be re-derived. Module sha1 `f3ab11c967c58f4483a4264fbeb1ba4a837e8719` unchanged against `config/G2ME01/config.yml`, `.rel` `cmp`-identical to `orig/G2ME01/files/RelProd/Ripper.rel`, all 86 holding. `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra functions**. The `#ifdef __MWERKS__` guard is `ShredderAccessors.cpp`'s and is load-bearing here for the same reason - this file *is* in `files.cmake`, so a host `fn_54_0` would make the port link `fn_54_158C` and a host-mangled `CPhysicsActor::GetBoundingBox`; with the guard the port's undefined count measured **259 -> 259, 0 duplicates**. `flip_test.sh` was not run: the unit's `Matching`/`NonMatching` state does not change here, and `AGENTS.md` names the module sha1 plus the `cmp` as the acceptance test for a REL unit. **The 41 functions still unclaimed** (56 in the module, 15 claimed) start at `fn_54_D8` (0xD8), and the first of them is only 0x2C bytes - `lwz r12,0(r3); lwz r12,0x38(r12); mtctr; bctrl`, a vtable dispatch, which `CMysteryFlyerRel.cpp`'s `fn_45_D0` already reproduces against a thirteen-virtual stand-in. **The wall is `fn_54_178`** (0x178, **0x35C** bytes), the module's entity loader: a 0x790 frame whose first act is `bl __ct__20SLdrEditorPropertiesFv`, i.e. class code needing the CActor/CPatterned hierarchy. See "`Ripper` is the fourth `*Accessors.cpp` head extended past the wrapper" below. |
| `EyeBall` | **Landed, 2026-09-29, lane 2** - the module head extended to `.text 0x0..0xD8`: **`fn_19_0`**, the `GetBoundingBox` wrapper, in front of the 14 accessors, **15/15 at 100.00%**, `flip_test.sh` PASS ("kept as Matching"), module sha1 `96c3406ac17b2e275b05b890070c4eeac75baa72` unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/EyeBall.rel`, `main.dol` still `6ef9b491...`, `matched` 9180 -> 9181, `linked` 4006 -> 4007, the module's own count 14 -> **15 of 68** (`audit_rel_claim.py`: 68 text symbols in the preplf, 68 in the plf, 0 dropped by `-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra functions**. `fn_19_0` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`; its callee is this module's own out-of-line `optional_object<CAABox>` constructor `fn_19_2590`, which sits in `auto_00_000000D8_text` and stays unclaimed. Port link measured **259 -> 259 undefined, 0 duplicates**. See "A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1`" below. |
| `IngSwarm`, `WallCrawlerSwarm` | wired; no class code at all (all `REL_Setup`), so nothing to decompile. |
| `DigitalGuardian` | **Head extended to 0x0, 2026-09-29 (lane 1) - `.text 0x0..0x10C`, 16/16 at 100.00%, the unit was already `Matching` and stays `Matching`, `main.dol` still `6ef9b491...`, `matched` 9181 -> 9184, `linked` 4007 -> 4010, the module's own count 13 -> **16 of 420** (`audit_rel_claim.py DigitalGuardian`: 420 text symbols in the preplf, 420 in the plf, 0 dropped by `-strip_partial`, 0 problem claims).** Module 14; the claim grew from `0x78..0x10C` (the 13 accessors) to `0x0..0x10C` by writing `fn_14_0` and `fn_14_8` (`li r3,1; blr` each) and **`fn_14_10`, the one wrapper in this family that does NOT take the recipe's spelling**: 0x68 bytes with the `optional_object<CAABox>` conversion **inlined**, where `fn_45_10`, `fn_81_10`, `fn_35_8`, `fn_38_0`, `fn_54_0` and `fn_68_0` are 0x3C bytes and each ends in `bl <module>_ctor`. So the `fn_XX_ctor(out, box)` form in the FN_XX_10 HINT is wrong for this module, and the short spelling - the real return type - reproduces the bytes - see "`DigitalGuardian` is the sixth head past the wrapper, and the only one that inlines it" below. Module sha1 `a3798856ec6b175272529f6a6295a29140662bcc` unchanged against `config/G2ME01/config.yml`, `.rel` `cmp`-identical to `orig/G2ME01/files/RelProd/DigitalGuardian.rel`, all 86 holding. `unit_fit.sh`: claimed 268, ours 268, retail 268, **no extra functions**. Port link measured **259 -> 259 undefined, 0 duplicates** with the `#ifdef __MWERKS__` guard in place. `flip_test.sh` was not run: the unit's `Matching` state does not change here, and `AGENTS.md` names the module sha1 plus the `cmp` as the acceptance test for a REL unit. **404 functions remain unclaimed**, starting at `fn_14_10C` (0x10C, 0x2C, the vtable-0x38 dispatch this family opens with). |
| 27 modules that were unwired on 2026-09-28 (`AtomicAlpha` `BacteriaSwarm` `Blogg` `DarkTrooper` `DestructibleBarrier` `ElitePirate` `EmperorIngStage3` `FishCloud` `GeomBlobV2` `IngBlobSwarm` `IngPuddle` `IngSnatchingSwarm` `IngSpaceJumpGuardian` `MediumIng` `MetareeSwarm` `Metroid` `MysteryFlyer` `Parasite` `PillBug` `PlantScarabSwarm` `Rezbit` `SandBoss` `SnakeWeedSwarm` `Splitter` `SwampBossStage1` `SwampBossStage2` `Tryclops`) | **Landed, 2026-09-28 - `REL_Setup` tail claimed in each, 5/5 exact, +135 matched, +135 linked, 87/87 hashes.** No C++: `tools/wire_rel_setup.py` (see "The recipe"). `audit_rel_claim.py` reports 0 problem claims on all 27. **Superseded in part, 2026-09-29: ten of the 27 now have class code too** - `MetareeSwarm`, `IngPuddle`, `IngSnatchingSwarm`, `PlantScarabSwarm`, `SnakeWeedSwarm`, `AtomicAlpha`, `MysteryFlyer`, `BacteriaSwarm`, `FishCloud` and `Tryclops`, one row each below - and `EmperorIngStage3` joined later the same day with its head at `.text 0x0..0xF8` (14 functions), `IngSpaceJumpGuardian` with its head at `.text 0x0..0x170` (18 functions), `MediumIng` with its head at `.text 0x0..0x150` (15 functions) and `Metroid` with its head at `.text 0x0..0x17C` (18 functions), so this list is a 2026-09-28 checkpoint, not the current set. `Rezbit` joined the same day with its head at `.text 0x0..0x168` (17 functions) and `SwampBossStage2` later the same day with its head at `.text 0x0..0x170` (18 functions), and `Parasite` (`.text 0x0..0x148`, 17) and `ElitePirate` (`.text 0x0..0x178`, 19) after that, and `Splitter` (`.text 0x0..0xFC`, 15, plus `0x81F8..0x82B0`, 6, as a second unit) last, so **8** are left with only the `REL_Setup` tail. `python3 tools/check_module_wiring.py` is the source of truth: **81 units of our own code in 64 modules** as measured on 2026-09-29, `Splitter` having joined last, `Parasite` and `ElitePirate` before it (79 in 63), `SwampBossStage2` before them, `Metroid` before it (this row's 72 in 56 is superseded by that, and `MediumIng`'s 71 in 55 by that, its earlier 70 in 54 by `GeomBlobV2` and `IngSpaceJumpGuardian` the same day, 67 in 53 by the former, and 61 in 47 by the heads of that day; the figure the third upstream sync left it at, with `Tweaks` dropped, was 60 in 46). |
| `MetareeSwarm` | **Head landed, 2026-09-29 - `CMetareeSwarmRel.cpp`, `.text 0x0..0xD8`, 5/5 at 100.00%, module sha1 `e9b5a7bd…` unchanged, `audit_rel_claim.py` 0 problems, 0 of 61 symbols dropped by `-strip_partial`, `flip_test.sh` PASS.** Module 43, and the first of those 27 to get class code. `fn_43_0`, `fn_43_3C`, `RELExit`, `RELMain` and the loader registration `fn_43_A8`, written as `CScriptPlayerProxy.cpp` is. Two measurements worth keeping: **the registration hands the setter the *address* of a four-byte `.bss` slot, not a loader** - `fn_8022D5A8` is the DOL's `stw r3, gLoader_MetareeSwarm; blr` and `LoadMetareeSwarm` reads it as `lwz r6,slot; lwz r12,0(r6); mtctr r12` - and **three floats 0x10 apart have to be built, not indexed**: `out[0]=v[3]; out[1]=v[7]; out[2]=v[11];` is the same ten instructions interleaved and 58.30%, while `*out = CVector3f(v[3], v[7], v[11])` is 100.00%. `fn_43_0`'s flag byte is `>> 7 & 1`, **not** the bit-24 test an earlier reading of dtk's `extrwi` spelling claimed - the section below has the measurement and what it supersedes. The rest of the module (51 functions, `fn_43_D8` first) is left unclaimed: it is class code and needs the CActor/CPatterned hierarchy. **Not added to `files.cmake`**: a host body would reference `fn_43_D8` and `fn_8022D5A8`, which the port cannot link yet, and the probe's regression gate is a hard failure on a growing undefined count (measured: 314 -> 316). The port keeps reading `MetareeSwarm.rel` off the disc, which is the correct arrangement for a module whose code is not in `mp_game`. |
| `IngPuddle` | **Head landed, 2026-09-29 - `CIngPuddleRel.cpp`, `.text 0x0..0xA8`, 5/5 at 100.00%, module sha1 `312b87ac…` unchanged, `audit_rel_claim.py` 0 problems, 0 of 68 symbols dropped by `-strip_partial`, `flip_test.sh` PASS.** Module 32, and the second of those 27 to get class code, in the same arrangement as `MetareeSwarm` above. `fn_32_0`, `fn_32_8`, `RELExit`, `RELMain` and the loader registration `fn_32_78`. The measurement worth keeping: **the two head functions are vtable entries and have to be written as a member call** - `fn_32_8` reads vtable offset 0x38, and loading the vtable by hand gives `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, which is 99.09% on the function; a stand-in class with thirteen virtuals puts its last one at 0x38 and gives retail's seven instructions byte for byte. See "`CIngPuddleRel` is a module head, and a vtable call needs a class" below. The rest of the module (57 functions, `fn_32_A8` first) is left unclaimed: it is class code and needs the CActor/CPhysicsActor hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. |
| `IngSnatchingSwarm` | **Head landed, 2026-09-29 - `CIngSnatchingSwarmRel.cpp`, `.text 0x0..0xA8`, 5/5 at 100.00%, module sha1 `c8483963…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems, 0 of 102 symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `168 claimed / 168 ours / 168 retail, fits`.** Module 33, the sixth of those 27 to get class code, and **the second head that is another module's instruction for instruction** (after `CPlantScarabSwarmRel`/`CMetareeSwarmRel`): the same 0xA8 bytes as **IngPuddle's**, 42 instructions differing in 3 - the `addi` immediate and the two `bl` displacements - the two accessors at the *same two words of the same 31-word vtable* (0x38 and 0x3C, measured in `auto_04_00000000_data.s` for both), and the same four-byte `.bss` loader slot at `+0x0`. `fn_33_0`, `fn_33_8`, `RELExit`, `RELMain`, `fn_33_78`. The one thing that did not carry over for free is **the import's name**: IngPuddle's setter is the unnamed DOL symbol `fn_80229EE0`, so the C++ identifier was the same string; IngSnatchingSwarm's is `config/G2ME01/symbols.txt:9529`'s long MWCC-mangled form, and inside `extern "C"` the identifier has to be written out in full or the REL step fails with `Failed to find symbol SetLoader_IngSnatchingSwarm in any module`. Its parent is **`CActor`, not `CPhysicsActor`** as IngPuddle's is (`TypesMatch__18CIngSnatchingSwarmCFi`, 0x8009C5F4, is `cmpwi r4,0x1e` against `TypesMatch__6CActorCFi`). The rest of the module (91 functions, `fn_33_A8` at 0xA8/0x5D4 first) is left unclaimed: it is class code and needs the CActor hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. See "`CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out" below. |
| `PlantScarabSwarm` | **Head landed, 2026-09-29 - `CPlantScarabSwarmRel.cpp`, `.text 0x0..0xD8`, 5/5 at 100.00%, module sha1 `67240808…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems, 0 of 71 symbols dropped by `-strip_partial`, `flip_test.sh` PASS.** Module 49, the third of those 27 to get class code, and **its head is `MetareeSwarm`'s instruction for instruction** - the same 0xB8-byte record at the same four offsets, the two accessors at the *same two words of the same 50-word vtable* (0x98 and 0x9C, measured in `auto_04_00000000_data.s` for both modules), and only the two `bl` targets differ because each module registers its own loader. So none of the three spellings `CMetareeSwarmRel.cpp` measures had to be rediscovered: `CVector3f`'s constructor rather than three index assignments, `index > -1` rather than `>= 0`, and a pointer dereference rather than a subscript. `fn_49_0`, `fn_49_3C`, `RELExit`, `RELMain` and `fn_49_A8`. The rest of the module (61 functions, `fn_49_D8` first) is left unclaimed: it is class code and needs the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. See "`CPlantScarabSwarmRel` is `CMetareeSwarmRel` with another module number" below. |
| `AtomicAlpha` | **Head landed, 2026-09-29 - `CAtomicAlphaRel.cpp`, `.text 0x0..0x13C`, 18/18 at 100.00%, module sha1 `ade8972e…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems (`18/18 functions` in the claim), 0 of 71 symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `316 claimed / 316 ours / 316 retail, fits`.** Module 2, and **the first of the four heads that is not just the loader trio**: the fourteen-accessor block the REL loader generator emits at the head of a scripted-actor module comes *before* the trio here, so the claim reaches from 0x0 and is 18 functions, the largest single step of the four. **Twelve of the fourteen accessors are the bodies `AtomicBetaAccessors.cpp` already reproduces at 100%** - same three DOL relocations (`lbl_8041AAB8`, `kInvalidUniqueId`, `lbl_8041B758`) - and the other two are AtomicAlpha's *leading* pair, extra, at +0x8C8 and +0x7D8 where AtomicBeta opens with the float store. **So the block is not byte for byte identical to AtomicBeta's; twelve of fourteen is the measured number**, and a doc that says otherwise is describing bytes the disc does not have. So still no spelling had to be discovered. `fn_2_9C` is a vtable entry and is written as a member call against a thirteen-virtual stand-in class, the same trick `CIngPuddleRel.cpp` measures. The rest of the module (47 functions, `fn_2_13C` at 0x13C/0x420 first) is left unclaimed: class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. See "`CAtomicAlphaRel` is a module head, and twelve of its fourteen accessors are shared" below. |
| `SnakeWeedSwarm` | **Head landed, 2026-09-29 - `CSnakeWeedSwarmRel.cpp`, `.text 0x0..0xDC`, 4/4 at 100.00%, module sha1 `f59a2a74…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems, 0 of 74 symbols dropped by `-strip_partial`, `flip_test.sh` PASS.** Module 71, the fourth of those 27 to get class code, and **the first whose head is not shaped like the other three**: there is no index-guarded record accessor, so the head is four functions and its registration fills a **0x1C-byte** record rather than a four-byte loader slot. `fn_71_0`, `RELExit`, `RELMain`, `fn_71_70`. See "`CSnakeWeedSwarmRel` is a module head, and a pmf is 12 bytes" below. The rest of the module (65 functions, `fn_71_DC` first) is left unclaimed: it is class code and needs the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. |
| `BacteriaSwarm` | **Head landed, 2026-09-29 - `CBacteriaSwarmRel.cpp`, `.text 0x0..0xA0`, 4/4 at 100.00%, module sha1 `11859125…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems (`4/4 functions`), 0 of 114 symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `160 claimed / 160 ours / 160 retail, fits`, `check_decl_order.py` ok.** Module 6, one of the nine of those 27 to get class code. `fn_6_0`, `RELExit`, `RELMain`, `fn_6_70`. **It is the shortest head of the eight landed heads that start at the module's first byte** - 0xA0 = 160 bytes against IngPuddle's and IngSnatchingSwarm's 0xA8 = 168, FishCloud's 0xAC = 172, MetareeSwarm's and PlantScarabSwarm's 0xD8 = 216, SnakeWeedSwarm's 0xDC = 220 and AtomicAlpha's 0x13C = 316. MysteryFlyer is shorter still (0x74 = 116 bytes) but its claim starts at 0xFC, not 0x0, so it is not a head-from-zero. **`fn_6_0` is IngPuddle's `fn_32_8` and no spelling had to be discovered**: aligning `fn_6_0` on `fn_32_8` - dropping IngPuddle's leading 8-byte `fn_32_0` - leaves 40 instructions against 40, and exactly two of them differ in encoding, both `bl`, to each module's own loader-setter import. (A third `bl` differs only in the symbol dtk prints, `bl fn_6_70` against `bl fn_32_78`: both encode `48000015`, each branching to the function immediately after it.) Two details a copy of `CIngPuddleRel.cpp` would have got wrong, both measured: `fn_6_0` is vtable entry **0x3C** (its call target is 0x38, which is what the stand-in class has to place), and the loader slot is **`lbl_6_bss_10` at `.bss:0x10`**, not `+0x0`. **And the import needed no rename, which is worth saying because IngSnatchingSwarm's did**: the setter is the plain DOL symbol `fn_8022A5AC`, read out of the module's own `BacteriaSwarm.preplf` import table, so `symbols.txt` and the DOL are untouched - the long MWCC-mangled `SetLoader_...` form `CIngSnatchingSwarmRel` needed is the exception, not the rule. `fn_6_A0` (0xA0, 0x74C) is the module's entity loader and stays retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the six above. See "`CBacteriaSwarmRel` is a module head, and the shortest head from 0x0 is four functions" below |
| `MysteryFlyer` | **Head landed, 2026-09-29 - `CMysteryFlyerRel.cpp`, `.text 0xFC..0x170`, 3/3 at 100.00%, module sha1 `2770bc03…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems, 0 of 83 symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `116 claimed / 116 ours / 116 retail, fits`.** Module 45, the fifth of those 27 to get class code, and **the first head that claims only the loader trio**: `RELExit` (0xFC), `RELMain` (0x120) and the registration `fn_45_140` (0x140). The claim starts at 0xFC rather than at 0x0 because the fifteen functions below `RELExit` are this entity's own members and **one contiguous claim cannot skip them** - see "`CMysteryFlyerRel` is a module head, and `fn_45_10` is a hidden-return `optional_object`" below for what `fn_45_10` is and why it is the one thing between this head and 18 functions. `fn_45_140` is instruction-for-instruction `fn_49_A8` and `fn_43_A8`, only the two `bl` targets differing. The rest of the module (74 unclaimed functions, `fn_45_0` first) is left to dtk. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. **Extended to `.text 0x0..0x170` later on 2026-09-29: 18/18 at 100.00%, sha1 unchanged, `matched` 9102 -> 9117** - `fn_45_10` turned out to be a plain call to the out-of-line `fn_45_2BBC`; see "Solved" in the `CMysteryFlyerRel` section below. |
| `Tryclops` | **Superseded the same day: the claim is `.text 0x0..0x178`, 19/19 at 100.00%, `flip_test.sh` PASS, module sha1 unchanged - `fn_81_10` is `fn_81_4FEC(out, GetBoundingBox())`, the MysteryFlyer `fn_45_10` fix; the "cannot start at 0x0" reasoning below is wrong.** Head landed, 2026-09-29 (lane 1) - `CTryclopsRel.cpp`, `.text 0x4C..0x178`, 16/16 at 100.00%, module sha1 `535aee66…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems (`16/16 functions` in the claim), 0 of 114 text symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `300 claimed / 300 ours / 300 retail, fits`, `check_decl_order.py` ok.** Module 81, and the **first of these heads whose claim does not start at 0x0**. One unit cannot claim two discontiguous ranges, so the block above takes the three functions in front of it too, and `fn_81_10` (0x10, 0x3C) is the one that blocks the next step: it is MysteryFlyer's `fn_45_10` instruction for instruction - a member function with a hidden return pointer in r3 returning `rstl::optional_object<CAABox>`, whose converting ctor sets `m_valid` in the mem-init where retail copies the box and then sets the flag, and whose template instantiation leaves a trailing pool. `fn_81_0` and `fn_81_8` (8 bytes each, vtable entries of the module's second vtable at 0x458 and 0x424) are the `fn_2_0` / `fn_2_68` shape and would follow from it for free. **Its thirteen-accessor block is AtomicAlpha's, and that is measured rather than assumed**: both `.text 0x4C..0xD8` ranges are 0x8C = 140 bytes and 35 instructions with an identical instruction multiset, and the `diff` moves two lines and adds none - Tryclops runs three `li r3,0; blr` predicates immediately after the byte read where AtomicAlpha runs two, its third sitting later beside the `li r3,0x1`. So no spelling had to be discovered and the run took minutes. `fn_81_D8` is vtable entry 0x3C of the 82-word table at `.data:0x378` and calls slot 0x38, `HealthInfo__3CAiFv`, so the same thirteen-virtual stand-in class as IngPuddle and AtomicAlpha reproduces it. **The import needed no rename, as with BacteriaSwarm**: the setter is the plain DOL symbol `fn_80218D58`, read out of the module's own `Tryclops.preplf`, so `symbols.txt` and the DOL are untouched. The loader slot is **`lbl_81_bss_30` at `.bss:0x30`**, not `+0x0`. `fn_81_178` (0x178, 0x30C) is the module's entity loader and the 89 functions from there to `fn_81_5028` are its methods; all stay retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the eight above. See "`CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict" below. |
| `FishCloud` | **Head landed, 2026-09-29 (lane 1) - `CFishCloudRel.cpp`, `.text 0x0..0xAC`, 4/4 at 100.00%, module sha1 `79ae4b2e…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems (`4/4 functions` in the claim), 0 of 106 symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 172 ours 172 retail 172, fits` and `no extra functions`.** Module 20, the **eighth** head claimed, the second of four functions, and **the cheapest of the eight: nothing had to be discovered** - no new spelling, no new stand-in class, and **no locally spelled struct**, because `SFishCloud_FuncPtrs` is already in `ScriptLoaderRel.hpp`. `fn_20_0`, `RELExit`, `RELMain`, `fn_20_70`. **Its `.text:0x0` is `fn_71_0` byte for byte** - a `diff` of the two disassembly listings is empty over all eleven instructions, checked non-empty so the `diff` cannot be vacuous - and it is stored in **both** of the module's vtables (`lbl_20_data_8` and `lbl_20_data_84`, 0x7C bytes each), so the CActor `GetHealthInfo` entry and its 29-virtual stand-in class are the ones `CSnakeWeedSwarmRel.cpp` already measures. Its registration fills an **8-byte** record (two `FScriptLoader`s), so it is the only one of these heads with **no member-function pointer in it**: no `__ptmf_scall` reading three words, and nothing copied out of `.data`. The rest of the module (97 functions, `fn_20_AC` first) is left unclaimed: it is class code and needs the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. See "`CFishCloudRel` is a module head, and the header already had the record" below. |
| `IngBlobSwarm` | **Landed, 2026-09-29 (lane 1, `progress-rel-head-ingblobswarm`) - the module head only: 5 functions in one `Matching` unit, `matched` 8828 -> 8833, `linked` 3875 -> 3880, DOL sha1 and all 86 REL hashes held.** `src/MetroidPrime/ScriptObjects/CScriptIngBlobSwarmRel.cpp` claims `.text 0x0..0xD8` and reproduces it byte for byte: `fn_31_0`, `fn_31_3C`, `RELExit`, `RELMain`, `fn_31_A8`. `tools/audit_rel_claim.py IngBlobSwarm` reports 5/5 and no filled gap, `unit_fit.sh` says "fits, no extra functions", `check_decl_order.py` is ok. The 53 remaining functions need the `CIngBlobSwarm`/`CActor`/`CPatterned` hierarchy and are still retail in the unclaimed `auto_00_000000D8_text` range. Three things a lane does not have to rediscover: **(a)** the module's loader setter is the **DOL**'s `fn_8022E134` (8 bytes at 0x8022E134, `stw r3,gLoader_IngBlobSwarm@sda21(r0)`), and the module's import table names it exactly that - a friendlier name will not link; **(b)** the `.bss` loader slot `lbl_31_bss_20` must be `extern` under `__MWERKS__` and a host definition otherwise, as in `CScriptPlayerProxy.cpp`, or mwldeppc's internal linker error comes back; **(c)** this source is **deliberately not in `files.cmake`**, because `fn_31_D8` - the module's own 0x40C loader - has no body, and listing it would take the port's undefined count 314 -> 315, which `tools/link_check.sh --strict` fails. `tools/check_files_cmake.py` accepts it because it is a `MODULE_ENTRY` source, the same way `CSwarmBasicsREL.cpp`, `CScriptPlayerActor.cpp` and `CScriptPlayerTurretRel.cpp` are out. Two mwcceppc facts, both measured here: `*out = CVector3f(a,b,c)` is what produces retail's **load-all-three-then-store** float order (three `SetX/SetY/SetZ` statements interleave and score 58.30%), and a `bool : 1` bitfield is what produces `rlwinm. r0,r0,25,31,31` + `beq` + `li r5,1` where `result = (byte & 1) != 0` gives `rlwinm` + `mr r5,r0` (78.27%). |
| `PillBug` | **Head landed, 2026-09-29 - `CPillBugRel.cpp`, `.text 0x0..0x130`, 17/17 at 100.00%, module sha1 `261c9127…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems, 0 of 82 symbols dropped by `-strip_partial`, `flip_test.sh` PASS.** Module 48, one of those 27, and its head carries the loader generator's thirteen-accessor block - `grep -l lbl_8041AAB8 src/` finds the 24 `*Accessors.cpp` units that already reproduce it in the DOL at 100%, so the bodies transfer and only the offsets and the fourteenth function are the module's own. `fn_48_90` at `0x90` is a vtable call where Glowbug's block has a `bool : 1` flag test instead, so the blocks share **thirteen** functions and then differ, and every offset past `0x38` moves by 0x20. `fn_48_74` is a copy (interleaved load/store) and not a `CVector3f` constructor call, the opposite of `fn_43_3C`. See "`CPillBugRel` is a module head, and the accessor block is already in the DOL" below. The rest of the module (59 functions, `fn_48_130` first) is left unclaimed: it is class code and needs the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above. |
| `Blogg` | **Head landed, 2026-09-29 (lane 2, rescued from the review queue) - `CBloggRel.cpp`, `.text 0x94..0x108`, 3/3 at 100.00%, module sha1 `2def4cc1…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/`, `audit_rel_claim.py` 0 problems, 0 of 217 text symbols dropped by `-strip_partial`.** Module 7. `RELExit`, `RELMain` and `fn_7_D8`, which is `CTryclopsRel.cpp`'s `fn_81_148` with Blogg's names; the setter is the plain DOL symbol `fn_80218B08` and the slot is `lbl_7_bss_10` (`.bss:0x10`), so no `symbols.txt` edit and no DOL change. **The claim cannot start at 0x0**: the module has no accessor block, and `fn_7_0` (0x0, 0x94) is a destructor - vtable `lbl_7_data_540`, `__dt__20CDamageVulnerabilityFv` on `this+0x34` and `this+4`, vtable `lbl_7_data_5A0`, `Free__7CMemoryFPCv` - i.e. class code. Read a module's first `.text` function before planning its head. `fn_7_108` (0x108, 0x96C) is the entity loader and it and the 207 functions above it stay retail. **Not added to `files.cmake`**, for the same reason as the heads above. The lane's code was rejected once over a false "the only head with no accessor block" claim (MetareeSwarm, IngPuddle and several swarms have none either), then failed a re-judge on `goal_check.sh`'s asm guard matching a comment - both fixed in the rescue. |
| `SkyRipple` | scaffold broke the hash (85/86 RELs) - claimed ranges did not match the object. Reverted. |
| `DarkTrooper` | **Head landed, 2026-09-29 (goal item `progress-rel-head-darktrooper`) - `CDarkTrooperRel.cpp`, `.text 0x0..0x12C`, **16/16 at 100.00%**, module sha1 `f216a5cd…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/DarkTrooper.rel`, `audit_rel_claim.py` 0 problems (`16/16 functions` in the claim), 0 of 172 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 300 ours 300 retail 300, fits` and `no extra functions`, `check_decl_order.py` ok.** Module 12, and a head claimed from 0x0 the way `MetareeSwarm`'s and `IngPuddle`'s are - except that here the loader trio is at 0xB8 rather than 0x64, behind a full accessor block, so the claim is 0x0..0x12C and not the trio. Sixteen functions. `fn_12_0`, `fn_12_8`, `fn_12_18`, `fn_12_20`, `fn_12_28`, `fn_12_38`, `fn_12_44`, `fn_12_50`, `fn_12_58`, `fn_12_60`, `fn_12_68`, `fn_12_70`, `fn_12_8C`, `RELExit`, `RELMain`, `fn_12_FC`. **The thirteen-function block is PillBug's in a different order, and that is measured rather than assumed**: `diff` of the two `auto_00_00000000_text` listings over 0x0..0xBC shows this module opening with an 8-byte member-address accessor (`addi r3,r3,0x7c0`) where PillBug opens with the 0x10-byte float store, one `li r3,0; blr` predicate in the run where PillBug has four (3 against 6 over the whole block), and one function PillBug has not - `fn_12_38`, the `+0x34C` flag bit, `lbz r0,0x34c(r3) / extrwi r3,r0,1,28 / blr`. So no spelling had to be discovered: every body is the one `CPillBugRel.cpp` and `AtomicBetaAccessors.cpp` already reproduce at 100%. **The flag bit's encoding transferred with the rest of it, verbatim**: `fn_12_38` and `AtomicBetaAccessors.cpp`'s `fn_5_48` are the same three instructions, `88 03 03 4C / 54 03 EF FE / 4E 80 00 20` in both objects and in `orig/G2ME01/files/RelProd/AtomicBeta.rel`. The `rlwinm r3,r0,29,31,31` in the sibling's own comment is stale and superseded - that word appears nowhere in `build/`. `fn_12_8C` is vtable entry **0x3C** of the 106-word table at `.data:0x2BC` calling slot 0x38 (`HealthInfo__3CAiFv`), so the same thirteen-virtual stand-in class as IngPuddle, AtomicAlpha, BacteriaSwarm and PillBug reproduces it. The import is the plain DOL symbol `fn_80218DF4`, read out of the module's own `DarkTrooper.preplf`, so **no `symbols.txt` rename and no DOL change** - IngSnatchingSwarm's long `SetLoader_…` form is the exception, not the rule. The loader slot is `lbl_12_bss_0` at `.bss:0x0`. `fn_12_12C` (0x12C, 0x614) is the module's entity loader and stays retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the same reason as the other module heads. Project: `matched` 9185 -> **9201**, `linked` 4011 -> **4027**, REL units 1138 -> **1154**, `main.dol` still `6ef9b491…`, all 86 hashes held, port link measured **259 -> 259 undefined, 0 duplicates**, `check_module_wiring.py` **63 units in 49 modules -> 64 in 50**. See "`CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order" below. |
| `DestructibleBarrier` | **Head landed, 2026-09-29 (goal item `progress-rel-head-destructiblebarrier`) - `CDestructibleBarrierRel.cpp`, `.text 0x0..0xA0`, **4/4 at 100.00%**, module sha1 `e1744b2c…` unchanged and `cmp`-equal to `orig/G2ME01/files/RelProd/DestructibleBarrier.rel`, `audit_rel_claim.py` 0 problems (`4/4 functions`), 0 of 117 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 160 ours 160 retail 160, fits` and `no extra functions`, `check_decl_order.py` ok.** Module 13, one of the 27, and **the second head to be a sibling's instruction for instruction with nothing to discover** after BacteriaSwarm: diffing dtk's `auto_00_00000000_text.s` over the 0xA0 each claims, this module is 40 instructions against BacteriaSwarm's 40, in the same order, with 7 differing lines and **every one of them a symbol name** - the two setter imports (`fn_8022EBFC` against `fn_8022A5AC`), the registration function's own name (`bl fn_13_70` / `bl fn_6_70`, both encoding `48000015` because each branches to the function immediately after it), the loader address and the slot name. `fn_13_0`, `RELExit`, `RELMain`, `fn_13_70`. **The import needed no rename, as with BacteriaSwarm and Tryclops**: the setter is the plain DOL symbol `fn_8022EBFC` (0x8022EBFC, 8 bytes, immediately after `LoadDestructableBarrier__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022EBD0, so `config/G2ME01/symbols.txt:9939` already names it), and its body is `stw r3, gLoader_DestructableBarrier; blr` - `build/G2ME01/asm/auto_03_8022EBFC_text.s`. So the store hands it the *address* of a four-byte slot, as everywhere in this family, and `src/MetroidPrime/ScriptLoader/DestructableBarrier.cpp` (a `Matching` unit) already records why the setter is deliberately unclaimed in the DOL. The loader slot is **`lbl_13_bss_0` at `.bss:0x0`**, not BacteriaSwarm's `.bss:0x10`. `fn_13_0` is vtable entry 0x3C of the 38-word table at `.data:0x38` calling slot 0x38, so the same thirteen-virtual stand-in class as IngPuddle, BacteriaSwarm and AtomicAlpha reproduces it. `fn_13_A0` (0xA0, **0x8A0**) is the module's entity loader and the 108 functions from there to `fn_13_72B4` are its methods; all stay retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the other module heads (a host body would reference `fn_13_A0` and `fn_8022EBFC`, which the port cannot link). Project: `matched` 9206 -> **9210**, `linked` 4108 -> **4112**, REL units 1157 -> **1161**, `main.dol` still `6ef9b491…`, all 86 hashes held, port link measured **259 -> 259 undefined, 0 duplicates**, `check_module_wiring.py` **65 units in 51 modules -> 66 in 52**. |
| `MediumIng` | **Head landed, 2026-09-29 (goal item `progress-rel-head-mediuming`) - `CMediumIngRel.cpp`, `.text 0x0..0x150`, **15/15 at 100.00%**, module sha1 `4ff29124bdf74564c3d33e6ae5948071479d9e61` unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/MediumIng.rel`, `audit_rel_claim.py` 0 problems (`15/15 functions`), 0 of 184 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 336 ours 336 retail 336, fits` and `no extra functions`, `check_decl_order.py` ok, `check_docs_claims.py` clean.** Module 41, one of the 27. **The accessor block is the family in a *different order*, and that is measured rather than read off the names**: the `fn_<id>_<off>` dtk labels say nothing about which function is which, so the only way to know where the `GetBoundingBox` wrapper sits is to diff the disassembly. Over the 0x150 this claims, `MediumIng` and `CMysteryFlyerRel.cpp`'s 0x170 share **no two-accessor prefix at all**: this module opens `addi r3,r3,0x7c0` and then the wrapper at 0x8 where MysteryFlyer opens `li r3,1` and `addi r3,r3,0x818`; it runs **three** `li r3,0` predicates in a row where MysteryFlyer runs two; it has **no** `lbl_8041AAB8` store at +0x448 and **no** `li r3,1` anywhere, so it covers one member fewer than the rest of the family; and its three-float copy `fn_41_94` sits at 0x94 where the family usually puts the `lbl_8041B758` accessor. So nothing had to be re-derived - every body below is the one `CMysteryFlyerRel.cpp` or `CIngSpaceJumpGuardianRel.cpp` already reproduces at 100% - but the point is worth keeping: **a module head is not a copy of a sibling's head until the bytes say so.** `fn_41_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_41_A708` (0xA708, 0x3C: six word copies and `stb 1, 0x18(r3)`) as its callee; `fn_41_B0` is vtable entry 0x3C of the 0x148-byte table at `.data:0x5C8` calling slot 0x38, reproduced by the same thirteen-virtual stand-in class. **No dead-strip hazard**: the module's `ldscript.lcf` puts all twelve of `fn_41_0`..`fn_41_B0` in FORCEACTIVE **and** `.data:0x5C8` stores every one of them, so nothing needs a `force_active:` entry (the trap `CGeomBlobV2` hit). The import is the plain DOL symbol `fn_80218A6C` (0x80218A6C, 8 bytes, `stw r3, gLoader_MediumIng@sda21(r0)`, immediately after `LoadMediumIng__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218A40, so `config/G2ME01/symbols.txt:9476` already names it), so **no `symbols.txt` rename and no DOL change**; the loader slot is **`lbl_41_bss_10` at `.bss:0x10`**, *not* `.bss:0x0` - this module's `.bss:0x0` is a different 4-byte slot, read and written by code far above the head. `fn_41_150` (0x150, **0x868**) is the module's own entity loader and the 160 functions above it are its methods; all stay retail - behavioural class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **184 is this module's complete text symbol count** on the `audit_rel_claim.py` convention: 15 ours + 164 unclaimed + 5 setup. |
| `Metroid` | **Head landed, 2026-09-29 (goal item `progress-rel-head-metroid`) - `CMetroidRel.cpp`, `.text 0x0..0x17C`, **18/18 at 100.00%**, module sha1 `c46c7d6e7b24bef74ae279a0f97cbaddfe1ad97f` unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/Metroid.rel`, `audit_rel_claim.py` 0 problems (`18/18 functions`), 0 of 167 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 380 ours 380 retail 380, fits` and `no extra functions`, `check_decl_order.py` ok, `check_docs_claims.py` clean.** Module 40, one of the 27. **The loader record is 0x10 bytes, not the four bytes almost every other head in this family uses, and that is the one thing here that had to be worked out rather than copied.** The setter `fn_80218B68` (0x80218B68, 8 bytes, `stw r3, gLoader_MetroidAlpha@sda21(r0); blr`, immediately after `LoadMetroidAlpha__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218B3C, so `config/G2ME01/symbols.txt:9485` already names it - **no `symbols.txt` rename and no DOL change**) stores the *address* of a record, and the DOL reads that record **twice**: `src/MetroidPrime/ScriptLoader/MetroidAlpha.cpp` - a `Matching` unit, and the file that records why the setter is deliberately unclaimed in the DOL - calls word 0 as the loader, and the DOL's own `OnDockTouch__13CMetroidAlphaFR13CStateManager` at 0x80218B10 does `lwz r5, gLoader_MetroidAlpha@sda21(r0); addi r12, r5, 0x4; bl __ptmf_scall` (`build/G2ME01/asm/auto_03_80218B08_text.s`). So **words 4..15 are a CodeWarrier pointer-to-member-function**, which is what makes the record 16 bytes and the registration 0x50 bytes rather than 0x30 - and the three words it holds are not built in place but **copied out of `.data:0x358`** (`0 / 0xFFFFFFFF / fn_40_FB4`, the same twelve-byte non-virtual pmf object as `CSnakeWeedSwarmRel.cpp`'s `lbl_71_data_18`, vtable offset -1 meaning "call it directly"). So `fn_40_12C` is `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` with one member-function pointer instead of two, the assignments in the field order above it. `fn_40_FB4` (0xFB4, 0x8C) is the module's own `OnDockTouch` body and stays unclaimed. The record is spelled locally rather than taken from `MetroidPrime/ScriptLoaderRel.hpp` for the reason `CSnakeWeedSwarmRel.cpp` gives: that header models `SPlayerActor_FuncPtrs`, which has this exact shape but is named for another module, and correcting a port-side model is a different item. The loader slot is **`lbl_40_bss_10` at `.bss:0x10`**, *not* `.bss:0x0` - this module's `.bss:0x0` is a different 0x10-byte object, read and written by `fn_40_17C`'s code above the head. **The accessor block is the family close to MysteryFlyer's but not identical, and only a diff of the bytes establishes that**: both put the `GetBoundingBox` wrapper third and both carry the `lbl_8041AAB8` store, the `0x44f` byte, the `kInvalidUniqueId` store, the `+0x34c` bit test, the `lbl_8041B758` accessor, the `+0x754` address and a `li r3,1`, but this module **swaps the two leading accessors** (it opens `addi r3,r3,0x8c8` then `li r3,1`, MysteryFlyer `li r3,1` then `addi r3,r3,0x818`), runs **three** `li r3,0` predicates to MysteryFlyer's two, and has **no three-float copy** - `fn_40_B4` is an eight-byte `li r3,0` where MysteryFlyer has a 0x1C-byte accessor - so it covers one member fewer than the rest of the family. `fn_40_10` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_40_85F4` (0x85F4) as its callee; `fn_40_BC` is vtable entry 0x3C of the 85-word table at `.data:0x364` - and of CBabyMetroid's identical table at `.data:0x914` - calling slot 0x38, reproduced by the same thirteen-virtual stand-in class. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fifteen of `fn_40_0`..`fn_40_BC` in FORCEACTIVE **and** `.data:0x364` stores every one of them, so nothing needs a `force_active:` entry. `fn_40_17C` (0x17C, **0x79C**) is the module's own entity loader and the 149 functions above it are its methods; all stay retail - behavioural class code needing the CMetroidAlpha/CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **167 is this module's complete text symbol count** on the `audit_rel_claim.py` convention: 18 ours + 144 unclaimed + 5 setup. |
| `Rezbit` | **Head landed, 2026-09-29 (lane 2, goal item `progress-rel-head-rezbit`) - `CRezbitRel.cpp`, `.text 0x0..0x168`, **17/17 at 100.00%**, module sha1 unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/Rezbit.rel`, `audit_rel_claim.py` 0 problems (`17/17 functions`), 0 of 169 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 360 ours 360 retail 360, fits` and `no extra functions`, `check_decl_order.py` ok, `check_raw_offsets.py` ok, `check_docs_claims.py` clean.** Module 53, one of the 27. **The record is four bytes - the family's usual size - and the `.bss` dump is what establishes it, cheaply and before any C++ is written**: `build/G2ME01/Rezbit/asm/auto_05_00000000_bss.s` gives `lbl_53_bss_0` `size:0x4` at `.bss:0x0`, where `CMetroidRel.cpp` has to place its slot at `.bss:0x10` because MetroidAlpha's record is also a `__ptmf_scall` target. The only reader of the slot is `LoadRezbit` in the `Matching` unit `src/MetroidPrime/ScriptLoader/Rezbit.cpp`, so there is no second reader and no pmf, and the registration is the family's 0x30-byte `stwu` shape rather than `CSnakeWeedSwarmRel.cpp`'s 0x6C. The setter is the plain DOL symbol `fn_80227AF8` (0x80227AF8, 8 bytes, `stw r3, gLoader_Rezbit@sda21(r0); blr`, immediately after `LoadRezbit__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80227ACC, which is 44 bytes and so ends exactly there), so **no `symbols.txt` rename and no DOL change**. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fifteen of `fn_53_0`..`fn_53_C8` in FORCEACTIVE, so nothing needs a `force_active:` entry. **The accessor block is MysteryFlyer's with two measured differences**, established by diffing `build/G2ME01/Rezbit/asm/auto_00_00000000_text.s` over the 0x168 this claims against `CMysteryFlyerRel.cpp`'s 0x170 rather than from the `fn_<id>_<off>` names: the two leading eight-byte accessors are `addi r3,r3,0xac0` then `li r3,1` where MysteryFlyer opens `li r3,1` then `addi r3,r3,0x818` (Metroid is `addi r3,r3,0x8c8` then `li r3,1`, so this module shares Metroid's order and not its second offset), and the block runs **two** `li r3,0` predicates where MysteryFlyer runs three - so the three-float copy sits at 0xAC rather than 0xB4, the whole tail is eight bytes lower, and the claim ends at 0x168 rather than 0x170. That is why this head is 17 functions and MysteryFlyer's 18: nothing is missing, the block is shorter. `fn_53_10` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_53_88A4` (0x88A4, 0x3C) as its callee; `fn_53_C8` is a vtable dispatch reproduced by the same thirteen-virtual stand-in class. `fn_53_168` (0x168, 0x330) is the module's entity loader and the 146 functions above it are its methods; all stay retail - behavioural class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **169 is this module's complete text symbol count** on the `audit_rel_claim.py` convention: 17 ours + 146 unclaimed + 1 (`auto_fn_53_88E0_text`) + 5 setup, which is exactly the sum of its units' `total_functions` in `build/report.json`. |
| `SandBoss` | **Head landed, 2026-09-29 (lane 2, goal item `progress-rel-head-sandboss`) - `CSandBossRel.cpp`, `.text 0x0..0x178`, **19/19 at 100.00%**, module sha1 `0a28cddbe1a04c8c4ca2bf6a6750d0467b9e805e` unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/SandBoss.rel`, `audit_rel_claim.py` 0 problems (`19/19 functions`), 0 of 322 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 376 ours 376 retail 376, fits` and `no extra functions`, `check_decl_order.py` ok, `check_raw_offsets.py` ok, `check_docs_claims.py` clean, `gate.sh` PASS.** Module 55, one of the 27. **The record is four bytes - the family's usual size - and the `.bss` dump is what establishes it, cheaply and before any C++ is written**: `build/G2ME01/SandBoss/asm/auto_05_00000000_bss.s` gives three objects, `lbl_55_bss_0` (`size:0x4`) at `.bss:0x0`, **`lbl_55_bss_4` (`size:0x4`) at `.bss:0x4`** and `lbl_55_bss_8` (`size:0xC`) at `.bss:0x8` - and it is the **middle** one that `fn_55_148` stores through (`lis r3, lbl_55_bss_4@ha; stwu r0, lbl_55_bss_4@l(r3)`), not `.bss:0x0` as in MysteryFlyer. Only one reader of the slot exists, `LoadSandBoss` in the `Matching` unit `src/MetroidPrime/ScriptLoader/SandBoss.cpp`, so there is no second reader and no `__ptmf_scall` pmf, and the registration is the family's 0x30-byte `stwu` shape rather than `CSnakeWeedSwarmRel.cpp`'s 0x6C. The setter is the plain DOL symbol `fn_802189D0` (0x802189D0, 8 bytes, `stw r3, gLoader_SandBoss@sda21(r0); blr`, immediately after `LoadSandBoss__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x802189A4, which is 44 bytes and so ends exactly there), so **no `symbols.txt` rename and no DOL change** - unlike MysteryFlyer's `fn_80232868`, which is a mangled `SetLoader_*` name that earlier heads had to declare the same way. **No dead-strip hazard**: the module's `ldscript.lcf` puts all sixteen of `fn_55_0`..`fn_55_D8` in FORCEACTIVE, so nothing needs a `force_active:` entry. **The accessor block is the family with two measured differences**, established by diffing `build/G2ME01/SandBoss/asm/auto_00_00000000_text.s` over the 0x178 this claims against `CMysteryFlyerRel.cpp`'s 0x170 rather than from the `fn_<id>_<off>` names: it opens with the always-true predicate **twice** (`fn_55_0`, `fn_55_8`) where MysteryFlyer opens with the always-true predicate and then `+0x818`, so it covers one member fewer at the front, and it runs **three** `li r3,0` predicates in a row (`fn_55_64`, `fn_55_6C`, `fn_55_74`) where MysteryFlyer runs two - which pushes `kInvalidUniqueId` from 0x74 to 0x7C and every accessor above it by 8 bytes, and the block runs 0x0..0x104. So this head is 19 functions and MysteryFlyer's 18, with nothing missing. `fn_55_10` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_55_10C78` (0x10C78, 0x3C: six word copies then `stb 1, 0x18(r3)`) as its callee; `fn_55_D8` is vtable entry 0x3C of **both** `.data:0x760` (CSandBoss's) and `.data:0x9AC` (CPlasmaProjectile's) - slot 0x38 is `HealthInfo__3CAiFv` and `HealthInfo__6CActorFv` respectively - calling slot 0x38, reproduced by the same thirteen-virtual stand-in class. **`GetBoundingBox` still needs the one-method local stand-in**, and for the reason `CMysteryFlyerRel.cpp` gives: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`, whose file-scope `static EMaterialTypes SolidMaterial` puts 0x28 bytes of `.data` in the object and breaks the module sha1 with every function at 100%. `fn_55_178` (0x178, **0x33C**) is the module's entity loader and the 298 functions above it are its methods; all stay retail - behavioural class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **322 is the module's complete text symbol count**: 19 ours + 298 unclaimed + 5 setup. |
| `SwampBossStage1` | **Head landed, 2026-09-29 (lane 2, goal item `progress-rel-head-swampbossstage1`) - `CSwampBossStage1Rel.cpp`, `.text 0x0..0x160`, **17/17 at 100.00%**, module sha1 `ea208a1a6484a28df1261b9418c831a0b00bfc0d` unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/SwampBossStage1.rel`, `audit_rel_claim.py` 0 problems (`17/17 functions`), 0 of 251 text symbols dropped by `-strip_partial`, `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 352 ours 352 retail 352, fits` and `no extra functions`, `check_decl_order.py` ok, `check_raw_offsets.py` ok, `check_docs_claims.py` clean.** Module 78, one of the 27. **The record is four bytes - the family's usual size - and the `.bss` dump is what establishes it, cheaply and before any C++ is written**: `build/G2ME01/SwampBossStage1/asm/auto_05_00000000_bss.s` gives three objects, `lbl_78_bss_0` (`size:0x8`) at `.bss:0x0`, `lbl_78_bss_8` (`size:0x18`) at `.bss:0x8` and **`lbl_78_bss_20` (`size:0x4`) at `.bss:0x20`** - and it is the **last** one that `fn_78_130` stores through (`lis r3, lbl_78_bss_20@ha; stwu r0, lbl_78_bss_20@l(r3)`), not `.bss:0x0` as in MysteryFlyer nor the middle one as in SandBoss. Only one reader of the slot exists, `LoadSwampBossStage1` in the `Matching` unit `src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp` (which also records that the 0x8022EC64 setter is deliberately unclaimed in the DOL, because REL modules import it by name), so there is no second reader and no `__ptmf_scall` pmf, and the registration is the family's 0x30-byte `stwu` shape rather than `CSnakeWeedSwarmRel.cpp`'s 0x6C. The setter is the plain DOL symbol `fn_8022EC64` (0x8022EC64, 8 bytes, `stw r3, gLoader_SwampBossStage1@sda21(r0); blr`, immediately after `LoadSwampBossStage1__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022EC38, which is 44 bytes and so ends exactly there), so **no `symbols.txt` rename and no DOL change**. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fourteen of `fn_78_0`..`fn_78_C0` in FORCEACTIVE and `.data:0x510` stores every one of them as a vtable entry, so nothing needs a `force_active:` entry. **The accessor block is MysteryFlyer's with two members dropped and one predicate gained**, established by diffing `build/G2ME01/SwampBossStage1/asm/auto_00_00000000_text.s` over the 0x160 this claims against `CMysteryFlyerRel.cpp`'s 0x170 rather than from the `fn_<id>_<off>` names, which say nothing about which function is which: it opens `li r3,1` and goes **straight to the `GetBoundingBox` wrapper** (MysteryFlyer opens `li r3,1` then `addi r3,r3,0x818`, so this block has no `+0x818` accessor), it has **no** `lbl_8041AAB8` store at +0x448, and it runs **three** `li r3,0` predicates in a row to MysteryFlyer's two. Counted rather than eyeballed: **55 instructions over 14 accessors here against 59 over 15 there**, and the two multisets differ by exactly one `li r3,0` gained (+2), one `addi r3,r3,0x818` lost (-2) and the four-instruction float store lost (-4) - the two differences above and nothing else. So the block runs 0x0..0xEC where MysteryFlyer's runs 0x0..0xFC, and the head is 17 functions against MysteryFlyer's 18, with nothing missing. `fn_78_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_78_BCD0` (0xBCD0, 0x3C: `stb 1, 0x18(r3)` then six words copied) as its callee; `fn_78_C0` is vtable entry 0x3C of the 0x148-byte table at `.data:0x510` calling slot 0x38 (`HealthInfo__3CAiFv`), reproduced by the same thirteen-virtual stand-in class. **`GetBoundingBox` still needs the one-method local stand-in**, and for the reason `CMysteryFlyerRel.cpp` gives: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`, whose file-scope `static EMaterialTypes SolidMaterial` puts 0x28 bytes of `.data` in the object and breaks the module sha1 with every function at 100%. `fn_78_160` (0x160, 0x314) is the module's entity loader and the 229 functions above it are its methods; all stay retail - behavioural class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **251 is the module's complete text symbol count** on the `audit_rel_claim.py` convention: 17 ours + 229 unclaimed + 5 setup, which is exactly the sum of its units' `total_functions` in `build/report.json` - `auto_00_00000160_text` (228) and the one-function `auto_fn_78_BD0C_text` unit included. |
| `SwampBossStage2` | **Head landed, 2026-09-29 (lane 2, goal item `progress-rel-head-swampbossstage2`) - `CSwampBossStage2Rel.cpp`, `.text 0x0..0x170`, **18/18 at 100.00%**, module sha1 `a6c4d72b549d4dfa4713fb1470b5d8afe9ecde40` unchanged against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/SwampBossStage2.rel`, all 86 RELs holding, `main.dol` still `6ef9b491...`, `audit_rel_claim.py` 0 problems (`18/18 functions`, 243 preplf text symbols / 243 in the plf, 0 dropped by `-strip_partial`), `flip_test.sh` `PASS -> kept as Matching`, `unit_fit.sh` `.text claimed 368 ours 368 retail 368, fits` and `no extra functions`, `check_decl_order.py` ok, `check_raw_offsets.py` ok (145 sites in 56 files), `check_docs_claims.py` clean, and `tools/goal_check.sh` a clean pass.** Module 79, one of the 27. **This head is MysteryFlyer's, unchanged in size** - 0x170 and eighteen functions, against SwampBossStage1's 0x160 and seventeen - and the reason is one accessor traded for one predicate, measured by diffing `build/G2ME01/SwampBossStage2/asm/auto_00_00000000_text.s` over 0x0..0xFC against `CMysteryFlyerRel.cpp`'s over the same range rather than from the `fn_<id>_<off>` names, which say nothing about which function is which: **63 instructions over 15 accessors on both sides**, and the two multisets differ by exactly one `addi r3, r3, 0x818` lost and one `li r3, 0x0` gained. So the `+0x818` member accessor is swapped for a fourth `li r3, 0x0` predicate (`fn_79_5C`, `fn_79_64`, `fn_79_6C` after `fn_79_0`'s `li r3,1`) and nothing else moves. Against `CSwampBossStage1Rel.cpp` (59 over 14, 0x0..0xEC) the only difference is this module's four-instruction `lbl_8041AAB8` store at +0x448, which SwampBossStage1 does not have - five instructions, which is the whole of the size gap. `fn_79_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_79_D4E8` as its callee; `fn_79_D0` is vtable entry 0x3C of the 0x148-byte table at `.data:0x424` calling slot 0x38, reproduced by the same thirteen-virtual stand-in class. **`GetBoundingBox` still needs the one-method local stand-in**, for the reason `CMysteryFlyerRel.cpp` gives: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`, whose file-scope `static EMaterialTypes SolidMaterial` puts 0x28 bytes of `.data` in the object and breaks the module sha1 with every function at 100%. **The record is four bytes and the `.bss` dump is the cheap check that says so before any C++ is written**: this module's `.bss` holds four objects (`lbl_79_bss_0` size 0x4 at `.bss:0x0`, `lbl_79_bss_4` size 0x1 at 0x4, an unnamed 3-byte gap at 0x5, and **`lbl_79_bss_8` size 0x4 at `.bss:0x8`**) - the **second** of the four is the loader slot, where MysteryFlyer's is the first and SwampBossStage1's the last, so the offset is the one thing in this family that has to be read rather than copied. Only one reader of the slot exists, `LoadSwampBossStage2` in the `Matching` unit `src/MetroidPrime/ScriptLoader/SwampBossStage2.cpp` (which also records that the setter is deliberately unclaimed in the DOL, because REL modules import it by name), so there is no second reader and no `__ptmf_scall` pmf, and the registration is the family's 0x30-byte `stwu` shape. The setter is the plain DOL symbol `fn_8022EC30` (0x8022EC30, 8 bytes, `stw r3, gLoader_SwampBossStage2@sda21(r0); blr`, in `build/G2ME01/asm/auto_03_8022EC30_text.s`), immediately **before** `LoadSwampBossStage2__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022EC04, which is 44 bytes and so ends exactly there - the mirrored layout of SwampBossStage1, whose setter sits *after* its loader - so **no `symbols.txt` rename and no DOL change**. **No dead-strip hazard**: `.data:0x424` stores all fifteen of `fn_79_0`..`fn_79_D0` as vtable entries and `config.yml` carries no `force_active:` list for this module, so nothing needs one. `fn_79_170` (0x170, 0x2F0) is the module's entity loader and the 220 functions above it are its methods; all stay retail - behavioural class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`** (`git status` shows it untouched), for the reason the other heads measure. **243 is the module's complete text symbol count**: 18 ours + 220 unclaimed + 5 setup, which is exactly the sum of its units' `total_functions` in `build/report.json` - `auto_00_00000170_text` (219) and the one-function `auto_fn_79_D60C_text` unit included. |
| `Parasite` | **Head landed, 2026-09-29 (goal item `progress-rel-head-parasite`, rescued 2026-09-29 by the orchestrator from the goal loop's review queue - the lane's code was right and the item failed on something else; the code was re-applied to master unchanged apart from comments, and the docs were written fresh) - `CParasiteRel.cpp`, `.text 0x0..0x148`, **17/17 at 100.00%**, module sha1 unchanged against `config/G2ME01/config.yml`, all 86 RELs holding, `main.dol` unchanged.** Module 47, one of the 27. **Why it was set aside**: both lane runs matched 17/17 and failed only `gate.sh`'s `raw-offsets` step, because neither added a `docs/research/raw_offsets.md` section for the new file - the check is per file, so a new head with a raw offset fails until its section exists. The lane's notes said DONE both times; the loop's `run.log` (`GATE FAIL: raw-offsets`) is where the real reason was. The block is AtomicAlpha's accessor set minus its two leading member-address accessors and the `+0x34C` bit-3 test, plus two `li r3,0` predicates; the loader record is 0xC bytes (three `FScriptLoader`), so the registration `fn_47_100` is 0x48 bytes, not the family's 0x30. The import is the plain DOL symbol `fn_80200EFC`. `fn_47_148` (0x148, 0x618), the entity loader, stays retail. Not added to `files.cmake`. |
| `ElitePirate` | **Head landed, 2026-09-29 (goal item `progress-rel-head-elitepirate`, rescued 2026-09-29 by the orchestrator from the goal loop's review queue - the lane's code was right and the item failed on something else; the code was re-applied to master unchanged apart from comments, and the docs were written fresh) - `CElitePirateRel.cpp`, `.text 0x0..0x178`, **19/19 at 100.00%**, module sha1 unchanged, all 86 RELs holding, `main.dol` unchanged.** Module 15, one of the 27. **Why it was set aside**: the reviewer rejected the second run for documentation wording only ("No source or config change is needed") - a module count in this file's 27-module row, the list of differences from MysteryFlyer, which omitted the four `bl` displacements, and a `CAABox` described as 0x24 bytes when it is 0x18 (two `CVector3f`; `fn_15_C094` sets the `optional_object` flag at 0x18). All three are corrected here. Separately, its `raw_offsets.md` section named `+0x448` as a counted site where the checker counts `+0x54`; rewritten from `check_raw_offsets.py --list`. The head is `CMysteryFlyerRel.cpp`'s re-ordered: 94 instructions against 92, with two leading member-address accessors (`+0xA50`, `+0x9C0`) and one extra `li r3,0` predicate. `fn_15_10` calls the module's out-of-line `optional_object<CAABox>` ctor `fn_15_C094`. The import is the plain `fn_80218AD4`. `fn_15_178` (0x178, 0xACC), the entity loader, stays retail. Not added to `files.cmake`. |
| `Splitter` | **Head landed, 2026-09-29, as two units (goal item `progress-rel-head-splitter`, rescued 2026-09-29 by the orchestrator from the goal loop's review queue; the lane's edits were replayed from its transcript, comments and `configure.py` block rewritten, docs written fresh) - `CSplitterRel.cpp`, `.text 0x0..0xFC`, **15/15 at 100.00%**, and `CSplitterRelMain.cpp`, `.text 0x81F8..0x82B0`, **6/6 at 100.00%**; module sha1 unchanged against `config/G2ME01/config.yml`, all 86 RELs holding, `main.dol` unchanged.** Module 75, one of the 27. **Why it was set aside**: the lane matched both units (`flip_test.sh` PASS on each, `unit_fit.sh` fits, `audit_rel_claim.py` 0 problems) and failed two `gate.sh` steps - `raw-offsets` (no section for the new file) and `files-cmake` (`CSplitterRel.cpp` has no module entry point, so `check_files_cmake.py` requires it in `files.cmake`). Fixed by adding the section and listing the file with its REL-internal pieces (`CPhysicsActor` stand-in, `fn_75_7C44`, `fn_75_18`) under `#ifdef __MWERKS__`, the `RipperAccessors.cpp` pattern. **Why two units**: this is the first head whose RELExit/RELMain are not directly above the accessor block - `fn_75_FC` (0xFC, 0x370), the entity loader, follows the head and RELMain is at 0x8234 - so one object cannot cover both. The block opens with two member-address accessors (+0xD6C, +0xE5C). The registration record is `lbl_75_bss_20` (0x14 bytes in a 0x18-byte object: two loaders named from `ScriptLoader/SplitterMainChassis.cpp`, then a pmf copied from `.data`), handed to the plain DOL setter `fn_80218CF0`. `CSplitterRelMain.cpp` is not in `files.cmake`. |
| `CGraphicsTimeProvider` | **Landed, 2026-09-26 (lane `h2`)** - `CGraphics::SetExternalTimeProvider` (0x802BF618, 0x8) and `CGraphics::GetSecondsMod900` (0x802BF620, 0x20) in one `Matching` unit claiming the contiguous 0x802BF618..0x802BF640, both at **100.00%**, `flip_test.sh` `PASS -> kept as Matching`, DOL sha1 held. The technique worth keeping: **`CGraphics` has no `.cpp` at all**, so a `Matching` unit can only reach its statics by retail's *unnamed* dtk labels (`lbl_804199DC`, `lbl_804199D8`), never by the invented C++ member names in `CGraphics.hpp` - a reference to `CGraphics::mpExternalTimeProvider` mangles to a symbol nothing defines in the DOL. The port-side definitions of the `lbl_` objects are in `PortGlobals.cpp`, and the C++-named members are deliberately left undefined so there is only ever one object per concept |
| `CGraphicsScreenPosition` | **Landed, 2026-09-26 (lane `h2`)** - `CGraphics::GetScreenPosition` (0x802BE9A4, 0x34) in one `Matching` unit, **100.00%**, `flip_test.sh` `PASS`. **And the trap, which cost this lane two builds: the SDA21 field is the *full* signed displacement, so `field = (address - 0x8041FD80) & 0xFFFF`.** Two wrong answers (0x804199D0/D4/D8, then 0x804199E4/E8/EC, against the right 0x804199E0/E4/E8) each produced an object that was byte-identical, paired at 100% under objdiff and passed `unit_fit.sh` as *fits, no extra functions* - and each broke the DOL's sha1 on exactly three bytes. `flip_test.sh`'s "the REBUILD FAILED - do not trust build/ until it is green again" is the message to read first, and `cmp -l` against `orig/G2ME01/sys/main.dol` names the bytes. Do the subtraction in a script |
| `CGraphicsSetScreenPosition` / `SetUseVideoFilter` / `SetModelMatrix` / `SetViewPointMatrix` | **Not attempted, 2026-09-26 (lane `h2`), each for a measured reason.** `SetScreenPosition` (0x802BE8F0, 0xB4) is 180 bytes of register-allocated arithmetic over the 0x3C-byte render-mode object at 0x80417264, whose layout this tree does not model. `SetUseVideoFilter` (0x802BEC24, 0x48) **cannot be Matching at all**: retail never writes `r7`, so it passes an uninitialised fifth argument to `GXSetCopyFilter`, and no source expression reproduces an argument the compiler invents - Aurora's own `GXSetCopyFilter` takes four parameters, so the fifth is dead on the host and the behaviour is genuinely undefined on the cube. `SetModelMatrix` (0x802C24AC, 0x60) and `SetViewPointMatrix` (0x802C2534, 0xE0) both call `fn_802C2614`, which has no body in the tree, so a `Matching` unit closes one port-gap symbol and opens one - the trap `check_files_cmake.py`'s own header warns about |
| `FogOverlay` | "completed" by transcribing 1,014 instructions into a `.s` unit. Rejected as not a decompilation. |
| `ScriptRsfAudio` | wiring, 7 correct symbol names, and an empty source; hash "matched" only because the unit was `NonMatching`. The symbol names are worth keeping; the rest proves nothing. **Superseded, 2026-09-25**: a later lane added exact `RELMain`/`RELExit`, loader registration and the setup range - 8 functions, unit `Matching`, sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified against config.yml. The remaining 14 module functions stay retail/unclaimed. |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function and one field accessor) plus all 5 `REL_Setup` ones - unit `Matching`, sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified. The remaining 48 class functions stay retail/unclaimed; this is the cheapest module shape yet found (its writable code is all `.text` wiring). |
| `DarkSamusBattleStage` | scaffolded split produced a 5,184-byte REL and an assembly object would not link. Correctly reverted with 0 functions. |
| `ScriptCoin` | **Landed, 2026-09-25** - 6 functions in 3 `Matching` units, module hash held, **+6 linked**. *Superseded:* the row above used to say "3 real functions written (a class, `Render`, `GetTouchBounds`) ... does not hold its hash yet", and **no `CScriptCoin.cpp` and no `Rel("ScriptCoin", ...)` block existed in the tree at all** - the claim was written from memory about a lane that never landed. The 6 functions are `RegisterCoinLoader`, `RELMain`, `RELExit` and a vtable slot in `CScriptCoinRel.cpp`, `CScriptCoin::Render` in `CScriptCoin.cpp`, and `CActor::GetTouchBounds` in `CScriptCoinTouchBounds.cpp`; all 100%, all with the unit `Matching`. The rest of the module stays retail and unclaimed. |
| `Ripper` | **Superseded 2026-09-29 (lane 1)** by the `Ripper` row above: the head *has* now landed, `.text 0x0..0xD8`, 15/15, sha1 held. The rest of the row stands and is the reason the other 41 functions are still retail: blocked with evidence: no `CRipper`, no `CPatterned`, no `include/MetroidPrime/Enemies/` at the time; `include/MetroidPrime/Enemies/CPatterned.hpp` exists now, but `fn_54_178` - the 0x35C-byte entity loader that constructs an `SLdrEditorProperties` in a 0x790 frame - is still class code this tree does not model. The range check passed back then, so the block was the missing base classes, not the splits, and it still is. |
| `Tweaks` | 2 generated constructors brought to exactly 100% (`SLdrTweakTargeting_Scan`, `SLdrTweakTargeting_VulnerabilityIndicator`) and 3 more moved 5-40 points closer, by moving the member assignments from the constructor body into the mem-init list. Not promoted - the other 12 units are blocked (seven `LoadTypedef*` at a 99.2% register-allocation wall, three on float-literal pooling, and the module's `.rodata` cannot be split per unit). |
| `Tweaks` (2nd pass, `REL_CreateTweakGlobals`) | `REL_CreateTweakGlobals` (module `.text:0x508`, 1,452 bytes) went from `{}` to a full body at **68.29%**, 1,184 bytes. Not promoted and **no range claimed** - the unit stays `NonMatching` over the whole `0x0..0x1338`, so `Tweaks.rel` still hashes to `config.yml` and the gate is unmoved (`matched 3043 -> 3043, linked 1653 -> 1653`, port link gap 721 before and after). Two blockers, both measured: the module's `.rodata` is unsplittable, so mwcceppc's CSE of the `__FILE__` argument cannot be undone; and retail re-materialises that argument at all 15 sites while ours hoists it. The pass's real output is `docs/research/tweak_globals.md`, a store-by-store map of all 1,452 bytes, which is what establishes that `gpTweakPlayerA` ends up pointing at a 4-byte heap cell and **not** at a `CTweakPlayer` - so the function is *not* what unblocks the frame loop. |
| `CRumbleVoice`, `CRumbleGenerator` | `CRumbleVoice` now matches **five** of them (8/16 -> 13/16) after the fix below; 0 of `CRumbleGenerator`'s. The unmatched `fn_8032*` functions are TU-local weak `rstl::vector<SAdsrDelta>`/`<SAdsrData>` instantiations with no name in the retail object, so objdiff scored them 0% even when the bodies were byte-identical. **Solved for pairing** by writing explicit specialisations in the source and renaming the retail symbols in `symbols.txt` to the mangled names MWCC emits (read them from our own object with `nm`) - see "Pairing a function the retail symbol table has no name for". Neither unit can be promoted yet: `CRumbleVoice` emits 180 bytes the retail unit object does not have, `CRumbleGenerator` 452. |
| `CScriptStreamedMusic`, `CStaticAudioPlayer` | **`CStaticAudioPlayer` is `Matching` as of 2026-09-29** (24/24, `flip_test` PASS; the two blockers below were both solved, see "An emission-order wall"). **Superseded for `CStaticAudioPlayer`, re-measured 2026-09-25.** The old reading - "pure register allocation, and 868 bytes of extra emitted functions on top" - was half right and has been corrected. `CStaticAudioPlayer` is now **23/24 at 99.87%**, and the "extra functions" are *not* the blocker: the DOL link passes `-strip_partial`, so mwldeppc deletes the 8 duplicate weak copies out of the middle of our `.text` and the flipped DOL comes out **exactly the same size as retail** (3 969 024 bytes both), with the bytes coming back out of the three objects that hold retail's copies (`CFilePreload`, `CCubeMoviePlayer`, `auto_03_8018A188_text`). What now blocks the flip is the **emission order of the out-of-line template instantiations** - see the new section "An emission-order wall: out-of-line template instantiations". `Decode` went 99.39% -> 100% on a one-statement `const` local; `DecodeMonoAndMix` 97.50% -> 98.70% and is stopped at 18 differing instructions. `CScriptStreamedMusic` was not re-measured. |
| `CGX` (DOL, not a module) | **`Matching` since 2026-10-01, 54 of 54.** Four changes closed it: a body-local `uint mask = 3 << shift;` in `SetVtxDescv_Compressed` (the "register wall"), `sGXState` and the descriptor list as file statics (COMMON -> `.bss`), three small-data claims in `splits.txt`, and `CallDisplayList` moved after `GetFog` in the source (function order - invisible to objdiff, visible only in the link). None of the `extern` imports proposed below was needed. The earlier entry, kept as written: **53 of 54 and still not promotable, and the reason is data, not code.** The permutation went first (five local moves, ~15 lines - it was the unit `docs/research/decl_order.md` called the best value per line moved, and that is now paid out), then `SetDstAlpha` 99.43% -> 100% by assigning a widened local back to a `uchar` member, and `__sinit_CGX_cpp` 76.92% -> 100% by routing a constant initializer through an `inline` function. `.text` now measures 5936 against a claimed 5936, "fits", no extra functions. **Not flipped**, and a hand flip was measured rather than assumed: `main.dol` grows 32 bytes, `lbl_8041E4A0` moves to 0x8041E480, and `sGXState` (COMMON for us, `.bss` in retail) lands at 0x804170E0 against a claimed 0x803DF828. Three separate problems remain - six data symbols that must be *imports* rather than compiler-generated constants, `sGXState`'s COMMON-vs-`.bss` placement, and `SetVtxDescv_Compressed` on the register-allocation wall. Full symbol/address table and the DOL evidence in "A DOL unit can be blocked by data, not by code". The intended config changes, not applied here, are three `splits.txt` lines plus the six `extern` declarations - see the report. |
| `DolphinCGraphics` (DOL, not a module) | **`NonMatching`, 96 of 102 (2026-10-01, was 91).** Fixed: `ConfigureVideo`'s second parameter is `uchar`, not `bool` (retail's caller does `clrlwi rX,rY,24` before the call; symbol renamed `ConfigureVideo__9CGraphicsFbUc`), with `if/else` instead of ternaries for the render mode and the `!initial` block written twice; `ConfigureFrameBuffer` `if/else` around `GXSetPixelFmt`; `LoadLight` switches on `info.GetType()` directly; the ctor tests `mGraphicsInitialized != true`. `TexRegionCallback` is instruction-identical (signed `static char` counters, the `GX_TF_C4/C8/C14X2` test repeated per branch) and scores 92.7% only because its local statics are unnamed `lbl_804199FC..FF` in `symbols.txt`. **Open:** `StreamBegin` (retail really calls `GetLockedCacheAllocationBase__Fv`; ours folds `0xE0000000`), `CalculatePerspectiveMatrix` (FP register order only - **matched 2026-10-02**: `tan` goes through a file-local inline float wrapper, which is what puts `t` in f7; unit is 100 of 102, see `docs/goal-notes/progress-prime1-dolphincgraphics.md`), `EndScene`, `FullRenderWithVertexDelay`, `ClipScreenQuadFromVS`; then the data: `.rodata/.data/.bss/.sdata/.sbss/.sdata2` are all unclaimed in `splits.txt`, our object has COMMON symbols (`kDefaultDirectionVector`, `kEnvBlend`, ...) against a non-common `.bss`, and it emits three weak functions retail does not (`CTevPass` ctor, two `~reserved_vector`). |
| `CPlayerGun` / `CGunWeapon` / `CPlayer` (DOL, not modules) | **The 40 boot-path candidates are all gameplay-only, and one tool run establishes it.** `docs/research/port_link_stubs.md` lists 342 unstubbable symbols because they are *referenced by a reachable object*; lane `h3` crossed 40 of them (`CPlayerGun` 22, `CGunWeapon` 6, `CGunStateMachine` 5, `CPlayer` 7) against `docs/research/boot_path.md` and **0 are reached before the first frame, 0 during initialisation, all 40 in gameplay**. Not one is referenced from a static initialiser: of 69 relocation sites, 7 are static data in `sStateFuncs`/`sTriggerFuncs` and 62 are calls inside `CPlayerGun`, `CStateManager` or `CScriptCannonBall` methods. The gate is one function - `CPlayer::CPlayer` (0x8001B018, 0x15C8) is the only creator of `CPlayerGun` and has one caller chain (`fn_8001EE58` / `fn_801F42A0` -> `fn_800401D8` -> `fn_80040D88` -> `CPlayer::CPlayer`), and it needs a loaded world, which step 13 cannot yet provide. Two landed anyway as `Matching` units, `CPlayerGetPlayerIndex` (0x8000D084, 8 bytes) and `CPlayerGetTweakPlayer` (0x8000BF94, 0x18), both `flip_test.sh` PASS, and both needed `CPlayer.hpp` to name two fields that were inside one `char m_pad_6[0x1A8]` - which turned up that the header's own comment put that pad at 0x1320 when mwcceppc puts it at 0x131C. New tool `tools/link_fn_reach.py` does the partition at referencing-site granularity; over the whole 342 it reports 457 `call` sites, 41 `data` and **0 `pre-main`**, so `link_reach.py`'s static-initialiser roots are not the reason those 342 look dangerous. Full table in `docs/research/gun_boot_path.md`. |
| 72 REL entity-loader thunks (DOL, not modules) | **Landed, 2026-09-25, +72 matched and +72 linked, port link gap 724 -> 652.** 64 new `Matching` DOL units, 3168 bytes of `.text` and 512 of `.sbss`, and the **port link gap** closed 72 symbols. The trigger was a *static initialiser*: retail builds the same 184-entry `{FourCC, FScriptLoader}` table the port's `ScriptLoader.cpp` has, in `__sinit_ScriptLoader_cpp` (0x80242894, 5696 bytes, already `Matching`), so decoding its `lis`/`addi`/`stw` dataflow gives **every** loader's retail address and the two tables then match position for position. See the new section below and `docs/research/rel_loaders.md`. |
| `CAudioSys`, `CStreamAudioManager` (DOL, not modules) | **Landed, 2026-09-26 (lane `h1`), +12 matched and +12 linked, port link gap 311 -> 297, `link_check.sh` 342 -> 328 undefined.** Seven new `Matching` units in `src/Kyoto/Audio/`, 392 bytes of `.text`, `.text`-only claims, every one `flip_test.sh` PASS with `unit_fit.sh` "fits / no extra functions". The route split and, more usefully, **the finding that only 11 of the port's 29 audio symbols are reached before a first frame** - they come from two objects and two call sites - are in `docs/research/audio_stack.md`. Three traps, all measured there: **`dtk dol split` refuses a `.sdata` claim that is not eight-byte aligned**, and a `Matching` unit that owns such a slot then has to define all of it under retail's *names*, because retail text outside the unit reads them; **`clrlwi` at a call site comes from a conversion written in the source, not from the callee's declared prototype**; and **`cmplwi` vs `cmpwi` is decided by the left operand's type and is not cosmetic** (`static_cast<int>(x) > 0x7F` gives `cmpwi`, and the two disagree above 0x8000). |
| `rstl::CRcPtrData` (DOL, not a module) | **The out-of-line `rc_ptr` copy constructor, half-landed 2026-09-26 (lane `g4`)** - `fn_80049010` (0x80049010, 0x24) is retail's copy constructor, calls it 15 times, and the map gives it **no mangled name** while `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` 0x24 bytes away *is* named: one copy constructor for every `T` is only possible if the words live in a **non-template** class. `rstl::CRcPtrData` is that class now, `src/rstl/rc_ptr_copy.cpp` defines its copy constructor out of line, and `CIOWinManager::RemoveAllIOWins` reached **100.00%** (was 51.88%) and became byte-exact. Retail's asymmetry - inline at six sites, a call at fifteen - is reproduced with a **tag** (`rstl::CRcPtrData::OutOfLine`) rather than one definition, so `IOWinPQNode::IOWinPQNode` keeps its 100% and `AddIOWin` its 95.74%. **Not promotable: the copy constructor is 97.22%**, because mwcceppc allocates the AddRef to r5/r4 where retail uses r4/r3 - the *out-of-line* allocator differs from the one it uses for an inlined expansion, and twenty body spellings, four class shapes, a template base and every `-O`/`-pragma` combination leave it alone. So `RemoveAllIOWins` stays `NonMatching` although it is byte-exact. Two traps: the base's default constructor must be **empty** (an initialising one is not eliminated and cost `IOWinPQNode` 36 points and both `CObjectReference` constructors ~17), and MWCC does not encode base classes, so no mangled name in the tree changed. `docs/research/rc_ptr.md`. |
| `CArchitectureQueue::Pop`, `CIOWinManager::PumpMessages` (DOL, not modules) | **Landed 2026-09-26 (lane `g4`), both 100.00%, 372 bytes** - `PumpMessages` (0x800496A0, 196 B) and `CArchitectureQueue::Pop` (`fn_800495F0`, 0x800495F0, 176 B), one contiguous range, `NonMatching`. Two things make them writable: mwcceppc 2.7 **does not elide the copy out of a return value**, so `CArchitectureMessage msg = queue.Pop();` is a return slot *plus* a copy-initialised local and the slot's destructor runs between the copy and the use - which is exactly retail's order and is why there are two 16-byte messages and two **dead** `beq`s per function; and `Pop()` had to be moved **out of line** in the header, or it is inlined and `PumpMessages` carries a 176-byte expansion where retail has one call. Not promotable for two independent reasons, both measured: `rstl::list<CArchitectureMessage>::do_erase` is a template member so mwcceppc emits it `W` where retail has `fn_80048F78` as a strong `T`, and the unit emits a 12-byte `__vt__24IArchitectureMessageParm` in `.data` that `splits.txt` does not claim. `tools/flip_test.sh` FAILs it and reverts. |
| `CInputGenerator::Update` (DOL, not a module) | **Written 98.27%, 2026-09-26 (lane `g4`)** - 0x8001D888, 0x1FC = 508 bytes, the largest single symbol in the frame loop, `NonMatching`. Every instruction of the body is retail's; the whole 1.72% is **mwcceppc reserving 16 bytes of stack slack**: it allocates a 0x40-byte slot for the 0x30-byte parm aggregate where retail allocated 0x30, which shifts the prologue and epilogue's saved-area offsets by 16 (`stwu r1,-192` against `-176`, `stmw r23,140` against `124`, `stfd f31,176` against `160`) and replaces retail's `psq_st` with an `xxsel`. Compiling the *identical* source with a 0x28 parm makes the prologue **byte-identical** to retail's, which is the proof that nothing else is wrong - and 0x28 is wrong, because `fn_80306BB0` writes a byte at +41 and `fn_80048CF4` allocates 48. The three virtual calls need no renaming: `__vt__18CDolphinController` at 0x803BB068 reads `[3],[4],[5] = 0x8030bd1c, 0x8030b5e0, 0x8030b5cc`, which are the header's own `Poll`, `GetDeviceCount`, `GetGamepadData` in declaration order - and retail calls `GetDeviceCount` **first** and `Poll` second. `fn_80306BB0` also reads a byte at +0x29 of the gamepad data, so `CControllerGamepadData` is at least 0x2a. |
| `CModel::Touch` (DOL, not a module) | **Landed, 2026-09-26 (lane `g4`) - `Matching`, 100.00%, 76 bytes, `flip_test.sh` PASS** - `Touch__6CModelCFi`, 0x803112DC, 0x4C: three calls and a two-register prologue, `fn_80310F38(this)`, `fn_803115F8(this, part)`, `fn_802BBDB8(*(void**)(this+0x28))`. It is the function `CModelTouchParts.cpp` (itself `Matching`) has been calling since it landed, which is why it was invisible on the link gap list until now: nothing *referenced* it until `CGunEffectTouch.cpp` and `CGunEffectTouchAll.cpp` were compiled. One header change was needed - `CModel` had **no member at +0x28** and the header was not self-contained (it uses `uint` and never included `types.h`). The +0x28 pointer is opaque: the callee reads a byte flag at +0x40 of it and nothing in the tree names the type. Closes `_ZNK6CModel5TouchEi` for the port, which is the +1 lane `f4` was measuring. |
| `CGameState::CGameState(CInputStream&, int)` (DOL, not a module) | **Attempted 2026-09-26 (lane `j1`) - the recorded reason it was never attempted is wrong, and measuring that is the lane's real output.** The 1,668-byte constructor was left alone because it has "seventeen unwritten callees"; **a `Matching` unit needs only relocations to its callees**, and `dtk dol split` writes a *filled* `build/G2ME01/obj/<unit>.o` for every unit and links that one for `NonMatching` units, so a callee whose range a `NonMatching` unit claims **is** defined. Measured: all 42 named callees of `fn_80144140` resolve (37 from ranges nobody claims, 4 from `NonMatching` units, 3 from `Matching` ones), and a probe unit calling one of each linked cleanly with all three defined in `main.elf`. **`configure.py`'s note on `CInputGeneratorUpdate.cpp` states the same false premise** ("nothing in the DOL link defines it and a Matching unit calling it would not link") and should not be repeated. The function is **unattempted, not blocked**; what it needs is twenty unnamed member types. `CGameState` is **0x2F0, measured with mwcceppc, and agrees with `operator new(0x2F0)`**; `CHintOptions` was 0x16 and is **0x18**, which moves `CPersistentOptions` from 0xDA onto the 0xDC retail constructs. Byte split: 417 instructions, **0x580 = 1,408 inline against 0x104 = 260 of call instructions**, nothing blocked on a callee. One leaf landed: `fn_80180738` (36 bytes, the `CHintOptions` constructor) at 100%, `Matching`, `flip_test.sh` PASS - the only callee of the 1,668 bytes that calls nothing, so the only one that is net -1 on the port's link. Next, in order: `fn_80144924`+`fn_8014495C`+`fn_80142A10` (164 contiguous bytes, the +0x110/+0x144 constructors), then `CWorldState::CWorldState` = `fn_8015C34C` (276 bytes, the **+0x3C** member, and its only two callees are *named* retail functions), then the constructor. Full map, blockers and reproduction commands in `docs/research/cgamestate_layout.md`. |
| `CWorldState::CWorldState` = `fn_8015C34C` (DOL, not a module) | **Attempted 2026-09-26 (lane `k1`) - written, `NonMatching` at 89.13%, 272 of 276 bytes, and the whole 0x4B0 member map is landed.** `src/MetroidPrime/CWorldStateCtor.cpp` claims `.text 0x8015C34C-0x8015C460`; `include/MetroidPrime/CWorldState.hpp` now names **all 25 members the constructor touches**, every offset measured with mwcceppc's own flags against the instruction that fixes it, and `CHECK_SIZEOF(CWorldState, 0x4b0)` passes. `flip_test.sh` says FAIL and reverts, as it must: retail has an **unused `mr r3,r31`** between its `+0x4A4` and `+0x4A8` stores that no spelling of the source produces, and the register allocator then gives the bitfield block r3/r4 where retail uses r4/r5. Tried for that `mr` and measured not to produce it: an empty inline member function and an empty `static` free function at that point, a member function carrying the whole tail, a static member function taking `CWorldState*`, a member function with a dummy argument, the flags written through a local pointer, `volatile` lvalues for the two stores, and all six permutations of the three tail statements. Two smaller differences come from the same place: retail's first `lfs` is its **third** instruction where mwcceppc always puts it seventh, and moving the read in the source does not move it. **Three techniques here are worth keeping.** (1) *mwcceppc deletes the construction of a class member this translation unit never reads* - measured for `rstl::string`, for a three-word struct with a user-provided constructor with and without a non-trivial destructor, from a mem-init list, and from `x = rstl::string()`; a **scalar** member written from the body always survives. That is why a constructor which must write an `rstl::string`'s three words has to call a member function to do it, which is what `rstl::basic_string::SetEmpty()` was added for. (2) *mwcceppc re-reads a non-`const` global after every store it cannot prove does not alias it*, so a constant used for five members is re-read five times where retail reads it twice; reading it into a **non-`const` local** reproduces retail's two reads, and a **`const` local is worse than nothing** - it gets an FPR *pair*, a `xscmpeqdp` and a 16-byte-larger frame. (3) *placement `new` costs a null check*: `new (&m) CRandom16(99)` emits `addic.` plus a `beq` over the call, so a member with no default constructor is built by calling retail's constructor by name. Point 3's declarations **must be `__MWERKS__`-only**: the host mangles the members to `_ZN9CRandom16C1Ej`, and the C-linkage retail spellings were measured at **+2 undefined** on `link_check.sh`. The second named callee, `__ct__12CTransform4fFRC12CTransform4f` (0x802C9054, 52 bytes), is **already at 100% - but as an inline-assembly transcription**, which this project does not count; a plain C++ mem-init list gives 192 bytes (12 `lfs`/`stfs` pairs) and a `double`-view loop gives the right 52-byte shape with a different FPR rotation, so it was not promoted. `fn_80144924`+`fn_8014495C`+`fn_80142A10` was **not attempted** - out of budget. |
| `CResFactory::Build` and `vtable for CResFactory` (DOL, not a module) | **Landed, 2026-09-26 (lane `m3`) - the third and last frame-0 vtable.** `CResFactory::Build` (retail `fn_802FA960`, 0x802FA960, 0xC0 = 192 bytes) is a `Matching` unit, `src/Kyoto/CResFactoryBuild.cpp`, **100.00%, 1 of 1, `flip_test.sh` PASS**, and the port's `link_check.sh` no longer asks for `vtable for CResFactory` at all: **330 -> 332 undefined, gross +3 (`fn_802FAAE4`, `fn_802FA1BC`, `fn_802FA7D4`) against gross -1, net +2, 0 duplicates.** Reaching it took the class's whole interior, which is in `docs/research/paks.md`; three things there are worth repeating here. (1) *A constructor that nothing else calls is the cheapest map of a class there is.* `CResFactory::CResFactory` (`fn_802FB154`) is twenty instructions that touch every member except one, and it is what fixed the last 0x44 bytes of the object - including **`CFactoryMgr` is 0x28, not 0x38**, whose four unnamed `uint`s were the first half of an `rstl::list` the manager was swallowing. (2) *`AsyncIdle` is worth reading for what it does not touch*: its four words are `+0xA0`, `+0xB0` (the first list's count) and the second list's `x4_start`/`x8_end`, and its timed half is a 64-bit divisor at `0x80411050` that is **not a member of anything**. (3) *A `Matching` unit's object must be exactly the size of its claimed range.* A `Build` that was 24 bytes short still linked and still failed only the hash - and `.text` **shrank by 0x18 and everything after it moved**, which is a much more alarming symptom than the per-function diff suggests; bisect `main.elf` against a saved baseline rather than the REL hashes. Two MWCC facts cost the most time and are recorded in full in `paks.md`: **MWCC emits a class's vtable in the unit defining its *first* virtual, not its key function** (so with the destructor merely declared, `Build`'s unit emits none at all), and **a `CFactoryFnReturn` returned by value must be a temporary, not a named local** - a named one is built in the frame, copied into the return slot and then destroyed, which is 0x50 bytes of `__dt__16CFactoryFnReturnFv` retail does not have. `~CResFactory` is written and byte-exact and still not landed: its object also carries `__vt__8IFactory` and a weak `__dt__8IFactoryFv`, and **retail's all-zero `__vt__8IFactory` and retail's base-vptr store cannot both be produced** from this header - measured both ways, in `paks.md`. |
| `CGameState`'s five remaining constructor callees (DOL, not modules) | **Four landed, 2026-09-26 (lane `cal3`), +4 matched and +4 linked.** `fn_80142CF8` (`CGameStateSysOptsPutTo.cpp`, 0x80142CF8, 0x80), `fn_80142DD4` (`CGameStateSlotDefaults.cpp`, 0x80142DD4, 0x94), `fn_801440C0` (`CGameStatePlayerLoop.cpp`, 0x801440C0, 0x80) and `fn_80009898` (`SGameStateMemcardReset.cpp`, 0x80009898, 0x34) are all **`Matching` at 100.00%** with `flip_test.sh` PASS, `unit_fit.sh` clean and `check_decl_order.py` ok. Three findings worth carrying: **(a) a retail `.sdata` global's address needs a NON-`const` declaration** - `const` puts the object in the read-only small-data area and mwcceppc emits `lis`+`addi` with `R_PPC_ADDR16_HA`/`LO` where retail has a single `R_PPC_EMB_SDA21` (100% -> 88.44%), which is the mirror image of the missing-`const` reload finding in `CGameStateMemcardCtor.cpp` and the same root cause: the C++ type decides which small-data section the object lands in. **(b) An array element's address comes out strength-reduced the other way round if the array base goes into its own local first** - `&self->x144.x04_blk[idx]` emits `addi r31,r4,328 ; add r31,r30,r31` (`self + (idx*16 + 0x148)`), and two statements (`SGameStateBlock* blocks = self->x144.x04_blk; block = blocks + idx;`) emit `slwi r0,r4,4 ; add r31,r30,r0 ; addi r31,r31,328` (`(self + idx*16) + 0x148`), which is retail's (83.05% -> 100.00%, same object size, so nothing but the allocator differs). **(c) A loop's two comma operands are ordered, and `player += 2, ++i` is the spelling retail has** (99.31% -> 100%). | `fn_800098CC`, the other half of `fn_80009898` (DOL, not a module) | **Written, `NonMatching` at 99.55%, 2026-09-26 (lane `cal3`)** - 356 bytes, and the only thing stopping the pair being one `Matching` unit. The body is a **76-iteration byte append that mwcceppc has unrolled by eight** (9 iterations of 8, then a bottom-tested remainder of 4; nothing in the source says 72 or 9) plus a **22-instruction loop whose body is empty** - the unroller's `addi r4,r4,8` and a `bdnz` to itself, with no stores anywhere. The empty body is **measured, not inferred**: putting a store in it stops the unroll entirely and the unit falls to 81-88%. Three spelling facts, each measured: the destination must be a named `unsigned char*` (77.16% -> 99.19% - written as `self->x54_buf[count]`, mwcceppc folds `+0x54` into the index and uses `self` as the `stbx` base where retail computes the base once before the loop), `+0x50` must be written through `self` every iteration rather than kept in a local, and `lbl_80417D93` must be non-`const`. The last **7 instructions** are an r4/r5 swap inside that empty loop and did not move for ~30 spellings (`for`/`while`, `++i`/`i++`/`i += 1`, increment in the third clause or the body, `int`/`u32`/`long` for either variable, bound hoisted or inline, declarations in either order or at function scope, the loop and the destination pointer each in a nested scope, a `volatile` read, an extra unused local). **One real finding inside the failure**: rewriting the loop as `while` with the increment in the body puts the *bound* in `r6`, as retail has it, where `for` puts it in `r5` - worth 3 of the 7. The pair was then **split at the symbol boundary** so `fn_80009898` is `Matching` and this is not; that is the `ScriptRiftPortal` prefix/matching/tail arrangement. |
| `CStateManager` and `CAnimData` layouts (DOL, not modules) | **Landed, 2026-09-26 (lane `v3`), +18 matched, `linked` held at 1811, 0 units newly linked, no regression on any function.** Not a function written: **one character in a pad, twice.** `CStateManager.hpp` had `pad2_2[0x34]` where retail has `0x2C`, so every member from `x1684` up was 8 high, `m_isDarkWorld` sat at 0x2954 instead of 0x294C and `sizeof(CStateManager)` was 0x2958 instead of 0x2950; retail's own `SetIsDarkWorld` reads `lbz 10572(r3)` against our 10580, which is the whole of a 99.79% function. Fixing the pad took 17 functions to 100% - 12 in `CStateManager`, plus `CPlayerGun::RenderBeamParticles` and `CPlayerState::GetRenderSuit`, which read `CStateManager` members from other translation units. `CAnimData.hpp` had the same disease with a note that said so: it recorded mwcceppc's `x178_particleDB` at 0x188 and treated the figure as retail's, so `x120_unk` is `0x48` and not `0x58`; retail's `CActor::SetModelData` does `addi r3,r3,376` where we emitted 392. All 25 documented `CStateManager` offsets now agree with the compiler and `sizeof` is 0x2950. **Why it was free:** of the 19 functions in `CStateManager` that touch the moved range, **none was at 100%**, so nothing could fall. **The method is in the new section "A header comment that recorded *mwcceppc's own output*" - the scan is `tools/offset_shift.py`, the probe is `tools/probe_offsets.cpp`, and the discriminator is a *uniform operand delta*, which a codegen difference never produces.** After both fixes the same scan finds no third instance in `main/`. Everything else this lane looked at is register allocation or scheduling and is recorded as not-moved. |
| `fn_8000934C` = `rstl::rc_ptr<CPlayerState>::ReleaseData` (DOL, not a module) | **Written and **correct, and BLOCKED on `dtk`'s link-order cycle - 2026-09-26 (lane `cal3`).** 0x8000934C, 0x50 = 80 bytes, `rstl::rc_ptr<CPlayerState>::ReleaseData()` on the 8-byte `{ptr, refcount}` pair the constructor builds four of. The source is in `src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` and is **deliberately not registered** in `configure.py`, because the range cannot be claimed. 0x8000934C is inside `MetroidPrime/main.cpp`'s `.text 0x800053B8..0x80009880`, and **a unit may not claim two discontiguous ranges in one section** (the `CModelDataModelSlots.cpp` failure), so main.cpp cannot keep the two pieces either side of a 0x50-byte hole. Splitting it and re-adding the range does not help either: the real cycle is four hops - `main.cpp` calls `fn_801449C8` (`CGameStateCtor.cpp`), which calls `fn_8000934C` at 0x80144C40, which is this unit, which calls `__dt__12CPlayerStateFv` at 0x8000939C, still inside main.cpp - and `dtk` reports it as `Cyclic dependency encountered while resolving link order: MetroidPrime/main.cpp -> MetroidPrime/Player/CPlayerStateRefRelease.cpp`. **The minimum carve-out that would work, measured off `symbols.txt`: `0x80008F40..0x8000934C` (11 functions, 0x40C bytes) and `0x8000939C..0x80009880` (13 functions, 0x4E4 bytes) must both leave main.cpp, as two new units, before an 80-byte win can be claimed** - and the tail reaches back into main.cpp through `ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv` at 0x80008F40, which is why moving only `__dt__12CPlayerStateFv` is not enough. Not the port's blocker; `fn_80009898` is. |
| `CResFactory::CResFactory()` = `fn_802FB154` (DOL, not a module) | **Body written, 93.86%, 2026-09-26 (lane `pool`), `NonMatching`, and it *fits*.** 0x802FB154, 0xA8 = 168 bytes, `src/Kyoto/CResFactoryCtor.cpp`; `tools/unit_fit.sh` reports 168 claimed / 168 ours / 168 retail and no extra functions, so it is one instruction placement from a `flip_test`. **This file used to be the port-only home of `fn_803096C4` and called *that* `CResFactory::CResFactory()` - a mis-attribution three sessions had recorded as settled, and the fix is a move, not a rewrite: `fn_803096C4` is the constructor of the four bytes at `CGameGlobalObjects`+0x00 (a member that holds nothing but a vptr and whose body is a one-shot `CARDInit`) and is now `src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp`.** Three measurements here are worth more than the 168 bytes. (1) **mwcceppc decides small-data addressing from the declared _size_ of a global**, so a retail `.data` operand comes out as `lwz r0,0(r13)` / `R_PPC_EMB_SDA21` unless the symbol is declared as a *sized array* (`extern "C" char lbl_803B19B8[0x20];` gets retail's `lis`+`addi`). This is the same fact as the `const` rule in the other direction, and it applies to every constructor that stores a vtable. (2) **A constructor's dead `mr r3,r31` is the return-value copy and it is load-bearing**: retail's epilogue has no `mr r3,r31` and MWCC puts the copy in the *middle* of the body, so "the epilogue has none, therefore the function returns void" is the wrong inference - declared `void` this is 91.36% with r6/r5/r4/r3 where retail has r7/r6/r5/r4, and declared `CResFactory*` with `return self;` it is 93.86% with retail's registers. (3) **The two vtable stores must be data operands, not a derived class**: deriving them makes the object emit `__vt__8IFactory` (0x20 bytes at 0x803B19B8), `__vt__11CResFactory` (0x20 at 0x803BAF08) and a weak `__dt__8IFactoryFv` (0x48 at 0x802FB0E0, which is retail's `fn_802FB0E0`), none of whose ranges is claimed - which is the same wall `paks.md` records for `~CResFactory`. Left: eight instructions, all one list-schedule priority (`li r6,0` and `addi r0,r31,212` want to be hoisted above the two volatile byte loads and are not); eleven orderings measured, this is the best. Port: **excluded from `files.cmake`, net +4** (326 -> 330), so the port's `CResFactory::CResFactory()` is the default one in `CResFactoryPortVirtuals.cpp` - which is still a net improvement, because the old body called `CARDInit` on a `CResFactory`. |
| `CSimplePool::CSimplePool(IFactory&)` = `fn_80301008` (DOL, not a module) | **Body written, 94.32%, 2026-09-26 (lane `pool`), `NonMatching`, and it *fits*.** 0x80301008, 0x150 = 336 bytes, `src/Kyoto/CSimplePoolCtor.cpp`; `unit_fit.sh` reports 336/336/336 and no extra functions. The second of the two constructors on the port's frame-0 critical path, and the nearest unwritten body on it. Two structural findings, both worth stealing. (1) **The class is not modelled the way `Kyoto/CSimplePool.hpp` models it, and the header is left alone**: the 8-byte object `operator new(8)` returns has a vtable *and* a `CSimplePool*`, and it is what `+0x1C` points at, while the header calls `+0x1C` an `rstl::rc_ptr< CVParamTransfer >` and `CVParamTransfer` is `{ rstl::rc_ptr< IVParamObj > }` - 8 bytes and no vtable of its own. The body carries a two-word overlay instead, so this is **not** a header change and has no blast radius; a lane that identifies the 8-byte object can fix the member's type properly. (2) **`stb r6,12(r1)` is the `.sbss` byte going into a second uninitialised frame slot out of the same register**, so the value is a named local and the global is read once - the `CPersistentOptionsCtor` spelling again, extended from two `volatile bool`s to one 8-byte `volatile u32` read as two bytes four apart plus a byte store through it. Also measured: **`tmpEntry.x0_ptr` must be stored *before* the second `operator new` call**, or the pointer lives in r30 across it and the function spends `stw r30,40(r1)` / `lwz r30,40(r1)` on a register retail has no room for (13 fewer differing instructions with the store first, 0 with it last - the `uint*` alias for the member's `rc_ptr` is worth 8 on its own). Left: five instructions, one list-schedule priority; twenty-two orderings measured. Port: **excluded from `files.cmake`, net +8** (326 -> 334) and closing nothing, because `CSimplePool.hpp` declares the constructor inline and `main.cpp` already emits it. The port's actual `CSimplePool` gap is `vtable for CSimplePool`, which needs `~CSimplePool` (the key function) and the ten virtuals. |
| `fn_8000934C` = `rstl::rc_ptr<CPlayerState>::ReleaseData` (DOL, not a module) | **Written and **correct, and BLOCKED on `dtk`'s link-order cycle - 2026-09-26 (lane `cal3`).** 0x8000934C, 0x50 = 80 bytes, `rstl::rc_ptr<CPlayerState>::ReleaseData()` on the 8-byte `{ptr, refcount}` pair the constructor builds four of. The source is in `src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` and is **deliberately not registered** in `configure.py`, because the range cannot be claimed. 0x8000934C is inside `MetroidPrime/main.cpp`'s `.text 0x800053B8..0x80009880`, and **a unit may not claim two discontiguous ranges in one section** (the `CModelDataModelSlots.cpp` failure), so main.cpp cannot keep the two pieces either side of a 0x50-byte hole. **SUPERSEDED 2026-09-26 (lane `v4`), and the correction is worth more than the row: there is no link-order cycle, and the carve-out works.** The four-hop cycle above does not exist. Measured, by doing it: adding the split on its own fails *earlier and differently* - `Split 3:0x8000934C..3:0x8000939C overlaps with previous split`, dtk refusing the claim outright because main.cpp already has it, so link order is never reached. Doing the **full** carve-out the row itself describes - `main.cpp` cut into `0x800053B8..0x80008F40` + `mainMid.cpp` at `0x80008F40..0x8000934C` + `mainTail.cpp` at `0x8000939C..0x80009880` with `.ctors` and `.sbss` moved to `mainTail` - plus this unit `Matching` at `0x8000934C..0x8000939C` **builds and links cleanly**: `dtk dol split` succeeds, the DOL sha1 stays `6ef9b491...`, all 86 RELs are byte-identical, and `fn_8000934C` reports **100.00% in a `Matching` unit**, `flip_test` PASS. The reason it is still not registered is the price, which no earlier note had measured: **the two carve-out units have no source, so objdiff pairs nothing in them and `matched` goes 3195 -> 3187 against `linked` 1811 -> 1812 - eight functions for one**, four of the eight having been at 100.00% inside main.cpp (`__dt__12CPlayerStateFv`, `__dt__Q212CPlayerState16SPersistentStateFv`, `__dt__Q24rstl81vector<Q312CPlayerState16SPersistentState10SScanState,...>Fv`, `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv`). That is the same failure `HANDOFF.md` records for the `CGameGlobalObjects` constructor experiment (`__dt__24CGameArchitectureSupportFv` 95.27% -> 0.00%), eight times the size. And the port does not want it either: `tools/link_check.sh` **326 -> 327**, listing this file opens `__dt__12CPlayerStateFv` and closes nothing, 0 compile errors and 0 duplicate definitions both ways. So the re-split is available to any lane that wants it and is declined on the numbers, not on an impossibility; the minimum carve-out is still `0x80008F40..0x8000934C` (11 functions, 0x40C) **and** `0x8000939C..0x80009880` (13 functions, 0x4E4), both leaving main.cpp, which is 24 functions of main.cpp re-homed for an 80-byte win. `fn_80009898`, the row's "not the port's blocker" note, has since landed (`Matching`); the remaining blocker on that path is `CGameGlobalObjects`'s constructor, which is a stub. |
**Superseded, 2026-09-25:** an earlier version of this table concluded that no module had been
decompiled and that the route was gated on the DOL hierarchy. Both halves were wrong in an
important way. `ScriptRiftPortal`, `Metaree` and `AIMannedTurret` now link our own C++ and hash to
what `config.yml` records, using the split described under "The recipe" - a module can be
*partly* decompiled and still correct, which is what makes the route viable before the actor
hierarchy exists. The hierarchy still gates the *behavioural* functions (see that section), but
accessors, predicates, loaders and setup can be taken now.
Current module status:
| module | our code in the link | notes |
| `AIMannedTurret` | 3 functions (`fn_1_0`, `fn_1_8`, `fn_1_10`, all `extern "C"`) | unit `Matching`, sha1 `949b8c21…` verified; the first module whose unit flips - see "Declare in reverse" |
| `ScriptRiftPortal` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) | first with a three-way split; sha1 `a0fa6c69…` verified against config.yml |
| `Metaree` | 23 named functions exact (18 ours + 5 setup), of 59 total; the rest unclaimed | first creature-family module; ranges unclaimed rather than named |
| `MetareeSwarm` | **10 functions: the module head `.text 0x0..0xD8` (5 ours) + 5 setup**, of 61 total; the other 51 unclaimed | landed 2026-09-29, module 43, one id after `Metaree`. `fn_43_0`, `fn_43_3C`, `RELExit`, `RELMain`, `fn_43_A8`, all 100.00%; sha1 `e9b5a7bd…` unchanged and all 86 held. `fn_43_D8` (0xD8, 0x3D8) is the module's entity loader and stays retail - it is class code and needs the CActor/CPatterned hierarchy. See "`CMetareeSwarmRel` is the module head, and `>> 7` is a 25-bit rotate" below |
| `IngPuddle` | **10 functions: the module head `.text 0x0..0xA8` (5 ours) + 5 setup**, of 68 total; the other 57 unclaimed | landed 2026-09-29, module 32. `fn_32_0`, `fn_32_8`, `RELExit`, `RELMain`, `fn_32_78`, all 100.00%; sha1 `312b87ac…` unchanged and all 86 held. `fn_32_A8` (0xA8, 0x1E4) is the module's `SLdrIngPuddle` entity loader and stays retail - it is class code and needs the CActor/CPhysicsActor hierarchy. See "`CIngPuddleRel` is a module head, and a vtable call needs a class" below |
| `IngSnatchingSwarm` | **10 functions: the module head `.text 0x0..0xA8` (5 ours) + 5 setup**, of 101 total; the other 91 unclaimed | landed 2026-09-29, module 33, sixth of the 27 and **IngPuddle's head instruction for instruction** - same 0xA8 bytes, same two vtable accessors at 0x38/0x3C, same four-byte loader slot at `+0x0`; only the two `bl` targets and `addi r3,r3,0x1F4` differ. `fn_33_0`, `fn_33_8`, `RELExit`, `RELMain`, `fn_33_78`, all 100.00%; sha1 `c8483963…` unchanged and all 86 held, `cmp` clean against `orig`. `fn_33_A8` (0xA8, 0x5D4) is the module's entity loader and stays retail - class code needing the CActor hierarchy (`TypesMatch__18CIngSnatchingSwarmCFi` gives parent `CActor`). **Not added to `files.cmake`**, for the same reason as the two above. See "`CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out" below |
| `DarkTrooper` | **21 functions: the module head `.text 0x0..0x12C` (16 ours) + 5 setup**, of 172 total; the other 151 unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-darktrooper`), module 12, one of the 27 and the **second head claimed from 0x0 to carry a full vtable call as well as the loader trio** - `Tryclops` is the first, at 0x178. Its thirteen-function block is **`CPillBugRel`'s in a different order**: an 8-byte member-address accessor at 0x0, PillBug's 0x10-byte float store moved to 0x08, one `li r3,0; blr` predicate in the run where PillBug has four (3 against 6 over the block), and one function PillBug has not - `fn_12_38`, the `+0x34C` flag bit, whose encoding transfers verbatim from `AtomicBetaAccessors.cpp`'s `fn_5_48` (`88 03 03 4C / 54 03 EF FE / 4E 80 00 20` in both). `fn_12_0`, `fn_12_8`, `fn_12_18`, `fn_12_20`, `fn_12_28`, `fn_12_38`, `fn_12_44`, `fn_12_50`, `fn_12_58`, `fn_12_60`, `fn_12_68`, `fn_12_70`, `fn_12_8C`, `RELExit`, `RELMain`, `fn_12_FC`, all 100.00%; sha1 `f216a5cd…` unchanged and all 86 held, `cmp` clean against `orig`; `audit_rel_claim.py` 0 problems (`16/16 functions`), 0 of 172 text symbols dropped by `-strip_partial`; `flip_test.sh` PASS; `unit_fit.sh` `300 claimed / 300 ours / 300 retail, fits`; `check_decl_order.py` ok. **172 is the module's complete text symbol count on the `audit_rel_claim.py` convention** - 16 ours + 151 unclaimed + 5 setup - which on this module coincides with the `total_functions` of its named plus `auto_*` units, `auto_00_0000012C_text` (150) and `auto_fn_12_6E58_text` (1) included; read the per-unit numbers in `build/report.json` rather than adding up unit names. The import is the plain DOL symbol `fn_80218DF4`, so no `symbols.txt` rename; the loader slot is `lbl_12_bss_0` at `.bss:0x0`. `fn_12_12C` (0x12C, 0x614) is the module's entity loader and stays retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the same reason as the heads above. See "`CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order" below |
| `DestructibleBarrier` | **9 functions: the module head `.text 0x0..0xA0` (4 ours) + 5 setup**, of 117 total; the other 108 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-destructiblebarrier`), module 13, one of the 27, and **the second head that is a sibling's instruction for instruction with nothing left to discover** (BacteriaSwarm was the first): 40 instructions against 40 in the same order, 7 differing lines, all of them a symbol name. `fn_13_0`, `RELExit`, `RELMain`, `fn_13_70`, all 100.00%; module sha1 `e1744b2c…` unchanged and all 86 held, `cmp` clean against `orig`; `audit_rel_claim.py` 0 problems (`4/4 functions`), 0 of 117 text symbols dropped by `-strip_partial`; `flip_test.sh` PASS; `unit_fit.sh` `160 claimed / 160 ours / 160 retail, fits`; `check_decl_order.py` ok. The import is the plain DOL symbol `fn_8022EBFC`, so no `symbols.txt` rename; the loader slot is `lbl_13_bss_0` at `.bss:0x0`. `fn_13_A0` (0xA0, 0x8A0) is the module's entity loader and stays retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the heads above. **117 is this module's complete text symbol count** on the `audit_rel_claim.py` convention: 4 ours + 108 unclaimed + 5 setup. |
| `PillBug` | **22 functions: the module head `.text 0x0..0x130` (17 ours) + 5 setup**, of 82 total; the other 59 unclaimed | landed 2026-09-29, module 48, one of the 27 and its head carries the loader generator's thirteen-accessor block, which `grep -l lbl_8041AAB8 src/` shows is already reproduced in the DOL by 24 `*Accessors.cpp` units. `fn_48_0`..`fn_48_74` (thirteen), `fn_48_90` (the CAi vtable call at slot 0x38), `RELExit`, `RELMain`, `fn_48_100`, all 100.00%; sha1 `261c9127…` unchanged and all 86 held, `cmp` clean against `orig`. `fn_48_130` (0x130, 0x5A4) is the module's entity loader and stays retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the same reason as the heads above. See "`CPillBugRel` is a module head, and the accessor block is already in the DOL" below |
| `Blogg` | **8 functions: the module head `.text 0x94..0x108` (3 ours) + 5 setup**, of 217 total; the other 209 unclaimed | landed 2026-09-29, module 7. `RELExit`, `RELMain`, `fn_7_D8`, all 100.00%; sha1 `2def4cc1…` unchanged and all 86 held, `cmp` clean against `orig`. `fn_7_0` (a `CDamageVulnerability`-owning destructor) below the claim and `fn_7_108` (the entity loader) above it stay retail - class code. **Not added to `files.cmake`**, for the same reason as the heads above |
| `PlantScarabSwarm` | **10 functions: the module head `.text 0x0..0xD8` (5 ours) + 5 setup**, of 71 total; the other 61 unclaimed | landed 2026-09-29, module 49, third of the 27 and **the same head as `MetareeSwarm` instruction for instruction** - same 0xB8-byte record, same `+0x184` array, same `+0x17C` count, same `+0xB2` flag byte, the two accessors at the *same two words of the same 50-word vtable*, and only the two `bl` targets differ. `fn_49_0`, `fn_49_3C`, `RELExit`, `RELMain`, `fn_49_A8`, all 100.00%; sha1 `67240808…` unchanged and all 86 held, `cmp` clean against `orig`. `fn_49_D8` (0xD8, 0x6A0) is the module's entity loader and stays retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the two above. See "`CPlantScarabSwarmRel` is `CMetareeSwarmRel` with another module number" below |
| `AtomicAlpha` | **23 functions: the module head `.text 0x0..0x13C` (18 ours) + 5 setup**, of 70 total; the other 47 unclaimed | landed 2026-09-29, module 2, and the **first head larger than the loader trio** - the fourteen-accessor block comes first, so the claim starts at 0x0. `fn_2_0`, `fn_2_8`, `fn_2_10`, `fn_2_20`, `fn_2_28`, `fn_2_30`, `fn_2_38`, `fn_2_48`, `fn_2_54`, `fn_2_60`, `fn_2_68`, `fn_2_70`, `fn_2_78`, `fn_2_80`, `fn_2_9C`, `RELExit`, `RELMain`, `fn_2_10C`, all 100.00%; sha1 `ade8972e…` unchanged and all 86 held, `cmp` clean against `orig`. Twelve of the fourteen accessors are the bodies `AtomicBetaAccessors.cpp` carries; the two that differ are AtomicAlpha's leading pair. `fn_2_13C` (0x13C, 0x420) is the module's entity loader and stays retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the three above. See "`CAtomicAlphaRel` is a module head, and twelve of its fourteen accessors are shared" below. **The denominator is 70, not the 65 an earlier draft of this row said**: 18 ours + 47 unclaimed + 5 setup = 70, on the same convention as the three rows above, which also leave out the module's `auto_fn_2_2578_text` unit - a 71st function, the `.ctors`/`.dtors` pointers. So `audit_rel_claim.py`'s "71 text symbols" and this row's 70 are both right about different things; read the per-unit `total_functions` in `build/report.json` rather than adding up unit names |
| `BacteriaSwarm` | **9 functions: the module head `.text 0x0..0xA0` (4 ours) + 5 setup**, of 114 total; the other 105 unclaimed | landed 2026-09-29, module 6, **the shortest of the eight landed heads that start at 0x0** - 0xA0 = 160 bytes against IngPuddle's and IngSnatchingSwarm's 0xA8 = 168, FishCloud's 0xAC = 172, MetareeSwarm's and PlantScarabSwarm's 0xD8 = 216, SnakeWeedSwarm's 0xDC = 220, AtomicAlpha's 0x13C = 316 - because the loader registration is this module's first function and there is no accessor block in front of it. `MysteryFlyer` is shorter still (0x74 = 116 bytes) but starts at 0xFC, not 0x0. `fn_6_0`, `RELExit`, `RELMain`, `fn_6_70`, all 100.00%; module sha1 `11859125…` unchanged and all 86 held, `cmp` clean against `orig`; `audit_rel_claim.py` 0 problems (`4/4 functions`), 0 of 114 text symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `160 claimed / 160 ours / 160 retail, fits`, `check_decl_order.py` ok. **`fn_6_0` is IngPuddle's `fn_32_8` and the same thirteen-virtual stand-in class reproduces it byte for byte** - measured, not assumed: aligning `fn_6_0` on `fn_32_8` (dropping IngPuddle's leading 8-byte `fn_32_0`) leaves 40 instructions against 40 in which exactly two differ in encoding, both `bl`, to each module's own setter import. A third `bl` differs only in the symbol dtk prints (`bl fn_6_70` / `bl fn_32_78`, both encoding `48000015`). So the vtable trick needed no new discovery. **The one thing that did not carry over from IngSnatchingSwarm is its import spelling**: this module's setter import is the plain DOL symbol `fn_8022A5AC` (0x8022A5AC, 8 bytes, immediately after `LoadBacteriaSwarm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022A580), confirmed in the module's own `build/G2ME01/BacteriaSwarm/BacteriaSwarm.preplf` import table, so **no `symbols.txt` rename and no DOL change were needed at all** - IngSnatchingSwarm's long `SetLoader_...` mangled form is the exception, not the rule. The loader slot is at `.bss:0x10`, not `+0x0` as IngPuddle's and IngSnatchingSwarm's are, and `fn_6_0` sits at vtable offset **0x3C** rather than 0x38 (its target is 0x38, so the same stand-in class still lands the call correctly). `fn_6_A0` (0xA0, 0x74C) is the module's entity loader and stays retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the seven above. See "`CBacteriaSwarmRel` is a module head, and the shortest head from 0x0 is four functions" below. **114 is this module's complete function count** - 4 ours + 105 unclaimed + 5 setup, and there is no further unit: unlike AtomicAlpha and IngSnatchingSwarm, BacteriaSwarm has no `auto_fn_6_7300_text` (`.ctors`/`.dtors` pointers) unit, so nothing here is excluded by that convention. `fn_6_7300` is a 76-byte function *inside* `auto_00_000000A0_text`, not a unit of its own |
| `MysteryFlyer` | **23 functions: the module head `.text 0x0..0x170` (18 ours) + 5 setup**, of 83 total; the other 59 unclaimed | landed 2026-09-29 as the loader trio `0xFC..0x170` (3 ours), then extended the same day to 0x0 once `fn_45_10` was solved: it is `fn_45_2BBC(out, GetBoundingBox())`, the `optional_object<CAABox>` ctor being out of line in retail. All 18 at 100.00%; sha1 `2770bc03…` unchanged and all 86 held, `cmp` clean against `orig`. The file declares a local `CPhysicsActor` stand-in rather than including the header, because `CMaterialList.hpp` adds 0x28 bytes of `.data` and breaks the hash. The rest (59 functions, `fn_45_170` first) is left to dtk. **Not added to `files.cmake`**. See "`CMysteryFlyerRel` is a module head, and `fn_45_10` is a hidden-return `optional_object`" below |
| `FishCloud` | **9 functions: the module head `.text 0x0..0xAC` (4 ours) + 5 setup**, of 106 total; the other 97 text functions unclaimed | landed 2026-09-29 (lane 1), module 20, and the **cheapest of the eight heads** - four functions, no accessor block, no locally spelled struct. `fn_20_0`, `RELExit`, `RELMain`, `fn_20_70`, all 100.00%; sha1 `79ae4b2e…` unchanged and all 86 held, `cmp` clean against `orig`. `fn_20_0` is CActor's `GetHealthInfo` and is `fn_71_0` **byte for byte** - same 0x2C at `.text:0x0`, all eleven instructions - stored at 0x3C of **both** the module's vtables (`lbl_20_data_8` and `lbl_20_data_84`, 0x7C bytes / 29 virtuals each) with `HealthInfo__6CActorFv` at 0x38. `fn_20_70` fills an 8-byte record of two `FScriptLoader`s, the only one of these heads with **no member-function pointer**: no `__ptmf_scall` reading three words, and nothing copied out of `.data`. `fn_20_AC` (0xAC, 0x240) is the `LoadFishCloudModifier` entity loader and stays retail. **Measured, so that a later attempt does not plan from a guess: it is not a small function.** It makes **nine** distinct calls - `__ct__`/`__dt__20SLdrEditorPropertiesFv`, `__nw__FUlPCcPCc`, `ReadFloat__12CInputStreamFv` (twice), `ReadBytes__12CInputStreamFPvUl`, `LoadTypedefSLdrEditorProperties…`, `LdrToEntityInfo…`, `AllocateUniqueId__13CStateManagerFv`, and `fn_20_6AAC` (0x6AAC, 0x120), which is the `CFishCloud` constructor and itself calls `__ct__16CActorParametersFv`, `__ct__10CModelDataFv` (through `fn_20_6BCC`) and `Translate__12CTransform4fFRC9CVector3f`. Its neighbour `fn_20_340` (0x340, 0x838), the `LoadFishCloud` half, makes nineteen distinct calls. Both need the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the same reason as the five above. See "`CFishCloudRel` is a module head, and the header already had the record" below. **The denominator is 106 and the two counters agree here**: 4 + 97 + 5 is exactly the 106 `audit_rel_claim.py` prints, because this module has no separate `.ctors`/`.dtors` unit the way `AtomicAlpha` had `auto_fn_2_2578_text` - its `ModuleConstructors`/`ModuleDestructors` (0x716C/0x7120) are inside the claimed `REL_Setup` range 0x7014..0x71B8, and `.ctors`/`.dtors` are empty section labels - so read the per-unit `total_functions` in `build/report.json` rather than adding up unit names |
| `Tryclops` | **24 functions: the module head `.text 0x0..0x178` (19 ours) + 5 setup** (was 0x4C..0x178, 16 ours, until `fn_81_10` was written the MysteryFlyer way the same day; the blocker text in this row is superseded), of 114 total; the other 93 text functions unclaimed | landed 2026-09-29 (lane 1), module 81, and the **first of the nine landed heads whose claim does not start at 0x0**. One unit cannot claim two discontiguous ranges, so the thirteen-accessor block takes the three functions in front of it too, and that is where the next step is blocked: `fn_81_10` (0x10, 0x3C) is MysteryFlyer's `fn_45_10` instruction for instruction - a member function with a hidden return pointer in r3 returning `rstl::optional_object<CAABox>`, whose converting ctor sets `m_valid` in the mem-init where retail copies the box and then sets the flag, and whose template instantiation leaves a trailing pool. `fn_81_0` and `fn_81_8` (8 bytes each, vtable entries of the module's second vtable at 0x458 and 0x424) are the `fn_2_0` / `fn_2_68` shape and follow from it for free, so the step is worth 19 functions rather than 16. `fn_81_4C`, `fn_81_5C`, `fn_81_64`, `fn_81_6C`, `fn_81_74`, `fn_81_7C`, `fn_81_8C`, `fn_81_98`, `fn_81_A4`, `fn_81_AC`, `fn_81_B4`, `fn_81_BC`, `fn_81_D8`, `RELExit`, `RELMain`, `fn_81_148`, all 100.00%; module sha1 `535aee66…` unchanged and all 86 held, `cmp` clean against `orig`. **The accessor block is AtomicAlpha's, and that is measured rather than assumed**: both `.text 0x4C..0xD8` ranges are 0x8C = 140 bytes and 35 instructions with an identical instruction multiset, and the `diff` moves two lines and adds none - Tryclops runs three `li r3,0; blr` predicates immediately after the byte read where AtomicAlpha runs two, its third sitting later beside the `li r3,0x1` - so no spelling had to be discovered. `fn_81_D8` is vtable entry 0x3C of the 82-word (0x148-byte) table at `.data:0x378` and calls slot 0x38, `HealthInfo__3CAiFv`, so **the same thirteen-virtual stand-in class as IngPuddle, AtomicAlpha and BacteriaSwarm reproduces it byte for byte**. The import is the plain DOL symbol `fn_80218D58`, out of the module's own `Tryclops.preplf`, so **no `symbols.txt` rename and no DOL change**; the loader slot is at **`.bss:0x30`**, not `+0x0`. `fn_81_178` (0x178, 0x30C) is the module's entity loader and the 89 functions from there to `fn_81_5028` are its methods; all stay retail - class code needing the CActor/CPatterned hierarchy. `audit_rel_claim.py` 0 problems (`16/16 functions`), 0 of 114 text symbols dropped by `-strip_partial`, `flip_test.sh` PASS, `unit_fit.sh` `300 claimed / 300 ours / 300 retail, fits`, `check_decl_order.py` ok. **Not added to `files.cmake`**, for the same reason as the eight above. See "`CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict" below. **114 is this module's complete function count**, on the same convention as the rows above that leave out the `auto_fn_81_5028_text` unit (the `.ctors`/`.dtors` pointers): 16 ours + 3 below the claim + 89 + 1 + 5 setup = 114, which is exactly what `audit_rel_claim.py` prints. **And this row is the one place where a REL unit's objdiff 100% was measured to be worth nothing on its own** - see the section below. |
| `EmperorIngStage3` | **19 functions: the module head `.text 0x0..0xF8` (14 ours) + 5 setup**, of 270 total; the other 251 text functions unclaimed | landed 2026-09-29 (lane 2), module 18. `fn_18_0`, `fn_18_8`, `fn_18_44`, `fn_18_4C`, `fn_18_54`, `fn_18_5C`, `fn_18_64`, `fn_18_6C`, `fn_18_7C`, `fn_18_88`, `fn_18_90`, `fn_18_98`, `fn_18_B4`, `fn_18_E0`, all 100.00%; module sha1 `775b095d…` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/EmperorIngStage3.rel`, all 86 holding, `main.dol` `6ef9b491…`, `matched` 9210 -> 9224, `linked` 4276 -> 4290. `audit_rel_claim.py` 0 problems (`14/14 functions`), 270 preplf text symbols, 270 in the plf, 0 dropped by `-strip_partial`; `flip_test.sh` PASS (`PASS -> kept as Matching`), `unit_fit.sh` `.text claimed 248 ours 248 retail 248, fits` and `no extra functions`. **Its accessor block is not byte for byte Krocuss's or MysteryFlyer's**, and that is measured, not assumed: no `lbl_8041AAB8` float store at +0x448 and no `lbl_8041B758` float accessor (so `kInvalidUniqueId` is the only DOL global its relocations name), **four** `li r3,0` predicates in a row where Krocuss has four and MysteryFlyer two and Tryclops three, `fn_18_90` always-true where the family usually puts always-false, and `fn_18_E0`, which the family has no counterpart for. `fn_18_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_18_DAEC` as its callee; `fn_18_B4` is a vtable dispatch reproduced by the same thirteen-virtual stand-in class. **RELMain (0xC330) and RELExit (0xC30C) are in the unclaimed remainder, not next to this head** - above `fn_18_C290` and its 200-odd neighbours - so they stay retail: one unit cannot claim two discontiguous ranges. Everything from `fn_18_F8` (0xF8, 0x11C) up is the module's own entity code. **Added to `files.cmake`**, unlike the other landed heads: this head defines no `RELMain`/`RELExit`, twelve of its fourteen functions read raw offsets and DOL globals and nothing else, and `fn_18_8` is behind the `#ifdef __MWERKS__` guard `KrocussAccessors.cpp` uses, so the port's undefined count stays at 259 (measured). See `EmperorIngStage3`'s accessor block is a fifth shape below. |
| `GeomBlobV2` | **15 functions: the entry-point block `.text 0x23E8..0x2490` (4 ours) + the accessor block `.text 0x2544..0x255C` and `0x256C..0x2584` (6 ours) + 5 setup**, of 130 total; the other 115 text functions unclaimed | landed 2026-09-29 (lane 2), module 25, and **the first landed head that is neither at 0x0 nor the thirteen-accessor family**. Both facts are measured, not assumed: `fn_25_0` (0x0, 0x1FC) is a real bone-blend loop over 0x50-byte records calling `close_enough__FRC11CQuaternionRC11CQuaternionf`, and none of Krocuss's / MysteryFlyer's / AtomicAlpha's accessor set is present - no `kInvalidUniqueId` store, no `+0x44f` byte, no `+0x34c` flag, no `lbl_8041AAB8` / `lbl_8041B758` floats. Its eight accessors are two pointer getters at `+0x15C`, float accessors at `+0x190` / `+0x198`, a bare-`blr` empty virtual and a byte clear at `+0x18`, and they name **no DOL global at all**, so the object relocates against nothing outside itself - which is why the two accessor units are in `files.cmake` and `CGeomBlobV2Rel.cpp` is not. `fn_25_2460`, `RELMain` (36 bytes) and `RELExit` (40 bytes), `fn_25_23E8`, all 100.00%; module sha1 `6aac53ef…` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/GeomBlobV2.rel`, all 86 holding. `matched` 9226 -> 9236, `linked` 4511 -> 4521, the module's own count 5 -> **15 of 130** (`audit_rel_claim.py`: 130 preplf text symbols, 130 in the plf, 0 dropped by `-strip_partial`, 0 problem claims). `unit_fit.sh` on all three: `claimed / ours / retail` equal, **no extra functions**. `flip_test.sh` PASS on all three, `check_decl_order.py` ok. `fn_25_48A8` / `fn_25_48CC` (the second loader's teardown and registration, through the other unnamed DOL setter `fn_802274FC`) and `fn_25_2490` (the entity loader) are in the unclaimed remainder and called by name. **See "`CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit" below** - the accessor block needed two units, not one, for a reason no other head has. |
| `IngSpaceJumpGuardian` | **23 functions: the module head `.text 0x0..0x170` (18 ours) + 5 setup**, of 148 total; the other 125 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-ingspacejumpguardian`), module 34, one of the 27. `fn_34_0`, `fn_34_8`, `fn_34_10`, `fn_34_1C`, `fn_34_58`, `fn_34_68`, `fn_34_70`, `fn_34_78`, `fn_34_80`, `fn_34_88`, `fn_34_98`, `fn_34_A4`, `fn_34_AC`, `fn_34_B4`, `fn_34_D0`, `RELExit`, `RELMain`, `fn_34_140`, all 100.00%; module sha1 `96e5208fc681177378fcaf1ed15abe20d436073a` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/IngSpaceJumpGuardian.rel`, all 86 holding, `main.dol` still `6ef9b491…`, `matched` 9236 -> **9254**, `linked` 4521 -> **4539**, the module's own count 5 -> **23 of 148** (`audit_rel_claim.py`: 148 preplf text symbols, 148 in the plf, 0 dropped by `-strip_partial`, 0 problem claims; `18/18 functions` in the claim). `unit_fit.sh` `.text claimed 368 ours 368 retail 368, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_docs_claims.py` clean. **The block is the family in a different order, and that is measured rather than assumed**: it opens with `addi r3,r3,0x8d0` and `li r3,1`, and **`fn_34_10` is a module-local `.rodata` constant** (`.rodata:0x0`, `.float 60`) where the family puts the `GetBoundingBox` wrapper - the `lbl_8041B758` accessor Tryclops carries at 0x98 is not here at all. So `fn_34_10` is the only body with no precedent, and the spelling is the one `CScriptRubiksPuzzle.cpp` already uses for its own `lbl_4_rodata_0`: a `float` return of an `extern "C" const float`, with the split claiming `.text` only so dtk's `.rodata` object keeps defining it. `fn_34_1C` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_34_6814` as its callee; `fn_34_D0` is a vtable dispatch reproduced by the same thirteen-virtual stand-in class. **No dead-strip hazard here, unlike `CGeomBlobV2`**: the module's `ldscript.lcf` puts all fifteen of `fn_34_0`..`fn_34_D0` in FORCEACTIVE **and** `.data:0x3E4` (0x148 bytes, two leading words and one per virtual) stores them, so nothing needs a `force_active:` entry. The import is the plain DOL symbol `fn_8021DC2C` (0x8021DC2C, 8 bytes, immediately after `LoadIngSpaceJumpGuardian__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8021DC00, `stw r3, gLoader_IngSpaceJumpGuardian@sda21(r0)`), so **no `symbols.txt` rename and no DOL change**; the loader slot is `lbl_34_bss_0` at `.bss:0x0`. `fn_34_170` (0x170, 0x330) is the module's entity loader and the 125 functions above it are its methods; all stay retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the same reason as the other heads. **148 is the module's complete text symbol count** on the `audit_rel_claim.py` convention: 18 ours + 125 unclaimed + 5 setup, which is exactly the `total_functions` of its named plus `auto_*` units in `build/report.json` - `auto_00_00000170_text` (124) and the one-function `auto_fn_34_6850_text` unit (the `.ctors`/`.dtors` pointers) included. See "`CIngSpaceJumpGuardianRel` is a module head, and one accessor is a module-local `.rodata` constant" below. |
| `MediumIng` | **20 functions: the module head `.text 0x0..0x150` (15 ours) + 5 setup**, of 184 total; the other 164 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-mediuming`), module 41, one of the 27. `fn_41_0`, `fn_41_8`, `fn_41_44`, `fn_41_4C`, `fn_41_54`, `fn_41_5C`, `fn_41_64`, `fn_41_74`, `fn_41_80`, `fn_41_8C`, `fn_41_94`, `fn_41_B0`, `RELExit`, `RELMain`, `fn_41_120`, all 100.00%; module sha1 `4ff29124bdf74564c3d33e6ae5948071479d9e61` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/MediumIng.rel`, all 86 holding, `main.dol` still `6ef9b491…`, `matched` 9255 -> **9270**, `linked` 4539 -> **4554**, the module's own count 5 -> **20 of 184** (`audit_rel_claim.py`: 184 preplf text symbols, 184 in the plf, 0 dropped by `-strip_partial`, 0 problem claims; `15/15 functions` in the claim). `unit_fit.sh` `.text claimed 336 ours 336 retail 336, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_decl_order.py` ok; `check_docs_claims.py` clean. **The block is the family in a *different order*, and only a diff of the bytes establishes that** - the `fn_<id>_<off>` dtk labels say nothing about which function is which, so where the `GetBoundingBox` wrapper sits has to be read off the disassembly: over the 0x150 claimed, this module shares **no two-accessor prefix** with `CMysteryFlyerRel.cpp`'s 0x170 (it opens `addi r3,r3,0x7c0` then the wrapper at 0x8, MysteryFlyer `li r3,1` then `addi r3,r3,0x818`), runs **three** `li r3,0` predicates in a row to MysteryFlyer's two, and has **no** `lbl_8041AAB8` store at +0x448 and **no** `li r3,1` at all - so it covers one member fewer than the rest of the family. `fn_41_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_41_A708` (0xA708, 0x3C) as its callee; `fn_41_B0` is a vtable dispatch reproduced by the same thirteen-virtual stand-in class. **No dead-strip hazard**: `ldscript.lcf` puts all twelve of `fn_41_0`..`fn_41_B0` in FORCEACTIVE and `.data:0x5C8` stores every one, so no `force_active:` entry is needed. The import is the plain DOL symbol `fn_80218A6C`, so no `symbols.txt` rename; the loader slot is `lbl_41_bss_10` at `.bss:0x10`, not `.bss:0x0`. `fn_41_150` (0x150, 0x868) is the module's entity loader and stays retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **184 is the module's complete text symbol count**: 15 ours + 164 unclaimed + 5 setup, which is exactly the `total_functions` of its named plus `auto_*` units in `build/report.json` - `auto_00_00000150_text` (163) and the one-function `auto_fn_41_A90C_text` unit included |
| `Metroid` | **23 functions: the module head `.text 0x0..0x17C` (18 ours) + 5 setup**, of 167 total; the other 144 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-metroid`), module 40, one of the 27. `fn_40_0`, `fn_40_8`, `fn_40_10`, `fn_40_4C`, `fn_40_5C`, `fn_40_64`, `fn_40_6C`, `fn_40_74`, `fn_40_7C`, `fn_40_8C`, `fn_40_98`, `fn_40_A4`, `fn_40_AC`, `fn_40_B4`, `fn_40_BC`, `RELExit`, `RELMain`, `fn_40_12C`, all 100.00%; module sha1 `c46c7d6e7b24bef74ae279a0f97cbaddfe1ad97f` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/Metroid.rel`, all 86 holding, `main.dol` still `6ef9b491…`, `matched` 9270 -> **9288**, `linked` 4554 -> **4572**, the module's own count 5 -> **23 of 167** (`audit_rel_claim.py`: 167 preplf text symbols, 167 in the plf, 0 dropped by `-strip_partial`, 0 problem claims; `18/18 functions` in the claim). `unit_fit.sh` `.text claimed 380 ours 380 retail 380, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_decl_order.py` ok; `check_docs_claims.py` clean. **The record is 0x10 bytes where almost every other head in this family stores four, and that is the one thing here that had to be worked out rather than copied**: the setter `fn_80218B68` stores the *address* of a record the DOL reads twice - `src/MetroidPrime/ScriptLoader/MetroidAlpha.cpp` calls word 0 as the loader, and `OnDockTouch__13CMetroidAlphaFR13CStateManager` at 0x80218B10 does `addi r12, r5, 0x4; bl __ptmf_scall` on it - so words 4..15 are a CodeWarrier pointer-to-member-function, and the registration copies them out of `.data:0x358` (`0 / 0xFFFFFFFF / fn_40_FB4`) rather than building them. `fn_40_12C` is therefore `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` with one member-function pointer instead of two, at 0x50 bytes against its 0x6C. The accessor block is the family close to MysteryFlyer's but not identical: the two leading accessors are swapped, there are **three** `li r3,0` predicates to MysteryFlyer's two, and there is **no three-float copy** (`fn_40_B4` is an eight-byte `li r3,0`). **No dead-strip hazard**: `ldscript.lcf` puts all fifteen of `fn_40_0`..`fn_40_BC` in FORCEACTIVE and `.data:0x364` stores every one, so no `force_active:` entry is needed. The import is the plain DOL symbol `fn_80218B68`, so no `symbols.txt` rename; the loader slot is `lbl_40_bss_10` at `.bss:0x10`, not `.bss:0x0`. `fn_40_17C` (0x17C, 0x79C) is the module's entity loader and the 149 functions above it are its methods; all stay retail - class code needing the CMetroidAlpha/CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **167 is the module's complete text symbol count**: 18 ours + 144 unclaimed + 5 setup, which is exactly the `total_functions` of its named plus `auto_*` units in `build/report.json` - `auto_00_0000017C_text` (110), `auto_00_00008A6C_text` (32) and the two one-function `auto_fn_40_*_text` units included |
| `Rezbit` | **22 functions: the module head `.text 0x0..0x168` (17 ours) + 5 setup**, of 169 total; the other 147 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-rezbit`), module 53, one of the 27. `fn_53_0`, `fn_53_8`, `fn_53_10`, `fn_53_4C`, `fn_53_5C`, `fn_53_64`, `fn_53_6C`, `fn_53_74`, `fn_53_84`, `fn_53_90`, `fn_53_9C`, `fn_53_A4`, `fn_53_AC`, `fn_53_C8`, `RELExit`, `RELMain`, `fn_53_138`, all 100.00%; module sha1 unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/Rezbit.rel`, all 86 holding, `main.dol` still `6ef9b491…`, `matched` 9289 -> **9306**, `linked` 4572 -> **4589**, the module's own count 5 -> **22 of 169** (`audit_rel_claim.py`: 169 preplf text symbols, 169 in the plf, 0 dropped by `-strip_partial`, 0 problem claims; `17/17 functions` in the claim). `unit_fit.sh` `.text claimed 360 ours 360 retail 360, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_decl_order.py` ok; `check_raw_offsets.py` ok; `check_docs_claims.py` clean; a per-function diff of `build/report.json` before and after shows **0 functions worse and 0 better outside Rezbit**. **The record is four bytes, and the `.bss` dump is the cheap check that says so before any C++ is written**: `lbl_53_bss_0` is `size:0x4` at `.bss:0x0`, against `CMetroidRel.cpp`'s `.bss:0x10`; the only reader of the slot is `LoadRezbit` in the `Matching` unit `src/MetroidPrime/ScriptLoader/Rezbit.cpp`, so there is no second reader and no pmf, and the registration is the family's 0x30-byte `stwu` shape. The import is the plain DOL symbol `fn_80227AF8`, so no `symbols.txt` rename. **No dead-strip hazard**: `ldscript.lcf` puts all fifteen of `fn_53_0`..`fn_53_C8` in FORCEACTIVE, so no `force_active:` entry is needed. The accessor block is MysteryFlyer's with two measured differences: the leading pair is `addi r3,r3,0xac0` then `li r3,1` (MysteryFlyer `li r3,1` then `+0x818`), and the block runs **two** `li r3,0` predicates to MysteryFlyer's three, which is why the three-float copy is at 0xAC and the claim ends at 0x168 - seventeen functions where MysteryFlyer has eighteen, with nothing missing. `fn_53_10` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_53_88A4` as its callee; `fn_53_C8` is a vtable dispatch reproduced by the same thirteen-virtual stand-in class. `fn_53_168` (0x168, 0x330) is the module's entity loader and stays retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **169 is the module's complete text symbol count**: 17 ours + 146 unclaimed + 5 setup plus the one-function `auto_fn_53_88E0_text` unit |
| `SandBoss` | **24 functions: the module head `.text 0x0..0x178` (19 ours) + 5 setup**, of 322 total; the other 298 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-sandboss`), module 55, one of the 27. `fn_55_0`, `fn_55_8`, `fn_55_10`, `fn_55_4C`, `fn_55_5C`, `fn_55_64`, `fn_55_6C`, `fn_55_74`, `fn_55_7C`, `fn_55_8C`, `fn_55_98`, `fn_55_A4`, `fn_55_AC`, `fn_55_B4`, `fn_55_BC`, `fn_55_D8`, `RELExit`, `RELMain`, `fn_55_148`, all 100.00%; module sha1 `0a28cddbe1a04c8c4ca2bf6a6750d0467b9e805e` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/SandBoss.rel`, all 86 holding, `main.dol` still `6ef9b491…`, `matched` 9306 -> **9325**, `linked` 4589 -> **4608**, the module's own count 5 -> **24 of 322** (`audit_rel_claim.py`: 322 preplf text symbols, 322 in the plf, 0 dropped by `-strip_partial`, 0 problem claims; `19/19 functions` in the claim). `unit_fit.sh` `.text claimed 376 ours 376 retail 376, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_decl_order.py` ok; `check_raw_offsets.py` ok; `check_docs_claims.py` clean; a per-function diff of `build/report.json` before and after shows **0 functions worse and 0 better outside SandBoss**. **The record is four bytes and the `.bss` dump is the cheap check that says so before any C++ is written**: `lbl_55_bss_0` (`size:0x4`, `.bss:0x0`), **`lbl_55_bss_4` (`size:0x4`, `.bss:0x4`)** and `lbl_55_bss_8` (`size:0xC`, `.bss:0x8`) are the module's three `.bss` objects, and it is the **middle** one that `fn_55_148` stores through, not `.bss:0x0` as in MysteryFlyer; the only reader of the slot is `LoadSandBoss` in the `Matching` unit `src/MetroidPrime/ScriptLoader/SandBoss.cpp`, so there is no second reader and no pmf. The import is the plain DOL symbol `fn_802189D0`, so no `symbols.txt` rename. **No dead-strip hazard**: `ldscript.lcf` puts all sixteen of `fn_55_0`..`fn_55_D8` in FORCEACTIVE, so no `force_active:` entry is needed. The accessor block is the family with two measured differences: it opens with the always-true predicate twice (MysteryFlyer: once then `+0x818`) and runs **three** `li r3,0` predicates in a row to MysteryFlyer's two, which pushes `kInvalidUniqueId` from 0x74 to 0x7C and the whole tail by 8 bytes - nineteen functions where MysteryFlyer has eighteen, with nothing missing. `fn_55_10` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_55_10C78` as its callee; `fn_55_D8` is a vtable dispatch reproduced by the same thirteen-virtual stand-in class. `fn_55_178` (0x178, 0x33C) is the module's entity loader and stays retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **322 is the module's complete text symbol count**: 19 ours + 298 unclaimed + 5 setup, which is exactly the sum of its units' `total_functions` in `build/report.json` - `auto_00_00000178_text` and the one-function `auto_fn_55_11184_text` unit included |
| `SwampBossStage1` | **22 functions: the module head `.text 0x0..0x160` (17 ours) + 5 setup**, of 251 total; the other 229 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-swampbossstage1`), module 78, one of the 27. `fn_78_0`, `fn_78_8`, `fn_78_44`, `fn_78_4C`, `fn_78_54`, `fn_78_5C`, `fn_78_64`, `fn_78_74`, `fn_78_80`, `fn_78_8C`, `fn_78_94`, `fn_78_9C`, `fn_78_A4`, `fn_78_C0`, `RELExit`, `RELMain`, `fn_78_130`, all 100.00%; module sha1 `ea208a1a6484a28df1261b9418c831a0b00bfc0d` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/SwampBossStage1.rel`, all 86 holding, `main.dol` still `6ef9b491…`, `matched` 9325 -> **9342**, `linked` 4608 -> **4625**, the module's own count 5 -> **22 of 251** (`audit_rel_claim.py`: 251 preplf text symbols, 251 in the plf, 0 dropped by `-strip_partial`, 0 problem claims; `17/17 functions` in the claim). `unit_fit.sh` `.text claimed 352 ours 352 retail 352, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_decl_order.py` ok; `check_raw_offsets.py` ok; `check_docs_claims.py` clean; a per-function diff of `build/report.json` before and after shows **0 functions worse and 0 better outside SwampBossStage1**. **The record is four bytes and the `.bss` dump is the cheap check that says so before any C++ is written**: `lbl_78_bss_0` (`size:0x8`, `.bss:0x0`), `lbl_78_bss_8` (`size:0x18`, `.bss:0x8`) and `lbl_78_bss_20` (`size:0x4`, `.bss:0x20`) are the module's three `.bss` objects, and it is the **last** one that `fn_78_130` stores through - not `.bss:0x0` as in MysteryFlyer, nor the middle one as in SandBoss; the only reader of the slot is `LoadSwampBossStage1` in the `Matching` unit `src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp`, so there is no second reader and no pmf. The import is the plain DOL symbol `fn_8022EC64`, so no `symbols.txt` rename. **No dead-strip hazard**: `ldscript.lcf` puts all fourteen of `fn_78_0`..`fn_78_C0` in FORCEACTIVE and `.data:0x510` stores every one as a vtable entry, so no `force_active:` entry is needed. The accessor block is MysteryFlyer's with two members dropped and one predicate gained: it opens `li r3,1` and goes straight to the `GetBoundingBox` wrapper (no `+0x818` accessor), has no `lbl_8041AAB8` store at +0x448, and runs **three** `li r3,0` predicates to MysteryFlyer's two - 55 instructions over 14 accessors against MysteryFlyer's 59 over 15, differing by exactly one `li r3,0` gained, one `addi r3,r3,0x818` lost and the four-instruction float store lost. Seventeen functions where MysteryFlyer has eighteen, with nothing missing. `fn_78_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, with this module's own out-of-line `optional_object<CAABox>` constructor `fn_78_BCD0` (0xBCD0) as its callee; `fn_78_C0` is vtable entry 0x3C of the 0x148-byte table at `.data:0x510` calling slot 0x38, reproduced by the same thirteen-virtual stand-in class. `fn_78_160` (0x160, 0x314) is the module's entity loader and the 229 functions above it are its methods; all stay retail - class code needing the CActor/CPatterned/CAi hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **251 is the module's complete text symbol count**: 17 ours + 229 unclaimed + 5 setup, which is exactly the sum of its units' `total_functions` in `build/report.json` - `auto_00_00000160_text` (228) and the one-function `auto_fn_78_BD0C_text` unit included |
| `SwampBossStage2` | **23 functions: the module head `.text 0x0..0x170` (18 ours) + 5 setup**, of 243 total; the other 220 text functions unclaimed | landed 2026-09-29 (lane 2, goal item `progress-rel-head-swampbossstage2`), module 79, one of the 27. `fn_79_0`, `fn_79_8`, `fn_79_44`, `fn_79_54`, `fn_79_5C`, `fn_79_64`, `fn_79_6C`, `fn_79_74`, `fn_79_84`, `fn_79_90`, `fn_79_9C`, `fn_79_A4`, `fn_79_AC`, `fn_79_B4`, `fn_79_D0`, `RELExit`, `RELMain`, `fn_79_140`, all 100.00%; module sha1 `a6c4d72b549d4dfa4713fb1470b5d8afe9ecde40` unchanged against `config/G2ME01/config.yml` and `cmp`-identical to `orig/G2ME01/files/RelProd/SwampBossStage2.rel`, all 86 holding, `main.dol` still `6ef9b491...`, `matched` 9343 -> **9361**, `linked` 4643 -> **4661**, the module's own count 5 -> **23 of 243** (`audit_rel_claim.py`: 243 preplf text symbols, 243 in the plf, 0 dropped by `-strip_partial`, 0 problem claims). `unit_fit.sh` `.text claimed 368 ours 368 retail 368, fits` and `no extra functions`; `flip_test.sh` `PASS -> kept as Matching`; `check_decl_order.py` ok; `check_raw_offsets.py` ok; `check_docs_claims.py` clean; a per-function diff of `build/report.json` before and after shows **0 functions worse and 0 better outside SwampBossStage2** (the gate's own `report_diff.py` enforces the first). **The head is MysteryFlyer's, size for size** - 0x170, eighteen functions - and the accessor block is its 0x0..0xFC with exactly one instruction swapped each way: 63 instructions over 15 accessors on both sides, one `addi r3, r3, 0x818` lost and one `li r3, 0x0` gained. `fn_79_8` is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10` with this module's own out-of-line `optional_object<CAABox>` constructor `fn_79_D4E8` (0xD4E8) as its callee; `fn_79_D0` is vtable entry 0x3C of the 0x148-byte table at `.data:0x424` calling slot 0x38, reproduced by the same thirteen-virtual stand-in class. **The loader slot is `.bss:0x8`, not `.bss:0x0`** - this module's `.bss` holds four objects and `lbl_79_bss_8` is the second - so the offset is read from `auto_05_00000000_bss.s` rather than copied from a sibling; the setter is the plain DOL symbol `fn_8022EC30` (which sits *before* its loader, the mirror of SwampBossStage1's), so no `symbols.txt` rename. `fn_79_170` (0x170, 0x2F0) is the module's entity loader and the 220 functions above it are its methods; all stay retail - class code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the reason the other heads measure. **243 is the module's complete text symbol count**: 18 ours + 220 unclaimed + 5 setup, which is exactly the sum of its units' `total_functions` in `build/report.json` - `auto_00_00000170_text` (219) and the one-function `auto_fn_79_D60C_text` unit included |
| `Parasite` | **22 functions: the module head `.text 0x0..0x148` (17 ours) + 5 setup**, of 136 total | landed 2026-09-29, rescued from review (see its row above). `matched` 9362 -> 9398 and `linked` 4679 -> 4715 together with ElitePirate's 19; `gate.sh`'s per-function diff shows no function worse. |
| `ElitePirate` | **24 functions: the module head `.text 0x0..0x178` (19 ours) + 5 setup**, of 241 total | landed 2026-09-29, rescued from review (see its row above), together with Parasite's head. |
| `Splitter` | **26 functions: the head `.text 0x0..0xFC` (15 ours) + `0x81F8..0x82B0` (6 ours, RELExit/RELMain among them) + 5 setup**, of 307 total | landed 2026-09-29, rescued from review (see its row above). `matched` 9398 -> 9419 and `linked` 4715 -> 4736; `gate.sh`'s per-function diff shows no function worse. |
| `CScriptCannonBall` | 12 of 26 matched, unit still `NonMatching` | was blocked on `UnkVtable20`, which is resolved; the link now fails on `__ct__6CActorF...` instead |
| `CScriptForgottenObject` | 9 of 12 at 95.86%, unit still `NonMatching` | .text/.rodata/.data a few bytes off |
| `ForgottenObject` (the unit; see also the module table) | **not promoted, 95.86% -> 97.53% fuzzy**, and 55 bytes from retail in 19 runs. `.text` and `.data` now fit exactly and `.bss` always did; `.rodata` short 5 is harmless (mwldeppc pads). The remaining 55 bytes are pure register allocation in 3 functions - 13 in `LoadForgottenObject`, 28 in `RenderInternal`, 14 in `__ct__` - and all three are the entry-block load-hoisting and register-choice walls described above, so the unit is *not* one edit away. A second, non-source blocker also applies: the module defines `fn_24_1E4`, which nothing calls, and mwldeppc dead-strips it - see "A REL unit that defines a function nothing calls cannot be flipped". **Worth a follow-up lane only after that rig fix lands** |
| `MetroidPrime/Player/CPlayerState.cpp` (DOL unit, not a module) | **not promoted, and the blocker is one instruction.** 69/72 at 100%, 99.80% fuzzy. The flip is blocked by a single 4-byte unconditional `b` in `InitializeScanTimes` (0xe0 against retail's 0xe4), which is unreachable by source: see "MWCC rotates a loop only when it cannot count it". `unit_fit.sh` blamed 6 extra functions (532 B) and a `.sbss` shortfall; all six are harmless (five `WEAK`, one `LOCAL` that mwldeppc drops) and the tool could not see the real 4-byte *deficit*. The other two functions, `ShouldDrawGravityBoost` and `GetActiveVisor`, are single-instruction scheduling walls: our build hoists one `lwz` one or two prologue slots earlier than retail, and 20 variants each did not move it |
| `ScriptCoin` | 6 functions in 3 `Matching` units (`CScriptCoinRel` 4, `CScriptCoin` 1, `CScriptCoinTouchBounds` 1) | module hash held, +6 linked; six units because a unit may claim only one contiguous range, and the tail named after the module rather than `REL/REL_Setup.cpp` |
| `ScriptGui` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) + a 5-entry loader table | sha1 `2b58f6d3…` verified; widget bodies blocked, see below |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function, 1 accessor) + 5 setup | sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified; 48 class functions unclaimed |
| `ScriptRsfAudio` | 8 functions (loader registration, `RELMain`/`RELExit`) + 5 setup | sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified; 14 class functions unclaimed |
| `ScriptStreamedMovie` | 6 functions: a local `CModelData` constructor wrapper (`__ct__10CModelDataFv`, which forwards to the DOL's routine) + `RELMain`/`RELExit` and the 3 setup functions | sha1 `d9b45eae…` verified; the rest of the module stays retail |
| `RubiksPuzzle` | 6 functions: `SLdrRubiksPuzzleData::SLdrRubiksPuzzleData()` (state machine `0xFFFFFFFF`, rotation speed from `.rodata`) + `RELMain`/`RELExit` and the 3 setup functions | sha1 `a29343f9…` verified; the rest of the module stays retail. The lane checked the base classes exist before starting, which is why this one was writable |
| `SkyRipple` | 7 exact of 15 named + fuzzy loader/constructor | unit kept `NonMatching` on purpose - promoting it would break the module |
| `Puffer` | 9 functions (6 + 3 in two named units) | sha1 `ab46667b…` verified |
| `CPakFile` | 0 of 33 in the link - **not** a module | DOL unit, not REL: 24/33 at 100% after 2026-09-25, still `NonMatching`, `.text` 1904 bytes over its claimed range. **Re-measured 2026-09-26 (lane g1), unchanged and now with the blockers named**: the two functions the pak chain needs, `__ct__8CPakFileF...` (0x8032458C, 0xEC) and `__dt__8CPakFileFv` (0x80324494, 0xF8), are **already 100%**, and so is `AsyncIdle__8CPakFileFv`. What blocks the unit is `reserve<rstl::vector<CPakFile::SResInfo>>` at **33.84%**, which cannot be written without editing `include/rstl/rmemory_allocator.hpp` (its `allocate` is out of line and uses `rs_new`, where retail inlines `CMemory::Alloc` with a `CCallStack`), and `RebuildResourceLists` at **39.63%**, which calls an unnamed `fn_80052220` where the port calls `reserve<rstl::vector<uint>>`. Both are outside the files a pak lane may edit, so the unit is **blocked on `include/rstl/`**, not on decompilation. |
| `Kyoto/CResLoaderPakPump.cpp` | **2 functions, 100.00%, unit `Matching`** | `AreAllPaksLoaded` (0x802FCCE4) and `AsyncIdlePakLoading` (0x802FCCF4), 172 bytes, `flip_test.sh` PASS with the DOL sha1 held. Required renaming two unnamed DOL symbols and correcting `include/Kyoto/CResLoader.hpp` (`CResLoader` is 0x60, four `rstl::list<SPakLoadEntry>`); see `docs/research/paks.md` and the register-allocation section above |
| `Kyoto/CResLoaderInsert.cpp` | **2 functions, 100.00%, unit `Matching`**, lane `k4` 2026-09-26 | `fn_802FC350` (0x802FC350, 0x28) and `fn_802FC378` (0x802FC378, 0xA8), 208 bytes, `flip_test.sh` PASS. **The insert every pak load goes through**, and the long-blocking item in this table. Neither body is a transcription: `fn_802FC378` is `rstl::list< SPakLoadEntry >::do_insert_before(node*, const SPakLoadEntry&)` called through the list's public members, and its 0xA8 bytes are identical, register for register, to retail's own named `do_insert_before<list<auto_ptr<CFilePreloadData>>>` at 0x803445DC. Three findings: **(a)** `SPakLoadEntry` is `rstl::auto_ptr< CPakFile >`, and the `stb r0,0(r30)` that clears the caller's flag byte is that class's auto-relinquishing copy constructor - the insert has no such statement; **(b)** the `addic. r5,r3,8` / `beq` guard is `rstl::construct`'s placement `new` and it is correct - the fix is the *element*, not the guard (see `rstl/construct.hpp`); **(c)** `#pragma inline_max_size` must be **large** (125 works, 0 does not) or mwcceppc emits `do_insert_before` as a separate COMDAT and `fn_802FC378` becomes a 0x20-byte forwarder. `fn_802FC378` declared *before* `fn_802FC350` (reverse emission order). Deleted the port's transcriptions from `PortGlobals.cpp`, so the port now links the real 64-bit `rstl::list`. Full write-up: `docs/research/paks.md`, "The pak insert, landed" |
| `Kyoto/CResLoaderResAccessors.cpp` | **5 functions, 100.00%, unit `Matching`**, lane `k4` 2026-09-26 | `fn_802FCAE8`, `fn_802FCB40`, `fn_802FCB88`, `fn_802FCBD0`, `fn_802FCC00` over `.text 0x802FCAE8..0x802FCC44`, 348 bytes, `flip_test.sh` PASS. Each is `fn_802FCDE8(this, id)` and then, only if that returned non-null, a call on `this->x68_curRes`. **That fixes two of `CResLoader`'s four unnamed words**: `fn_802FCF98` writes `x64_ = id` and `x68_ = CPakFile::SResInfo*` on success (`stw r31,100(r30)` / `stw r3,104(r30)` at 0x802fcfd0/0x802fcfd4), and each of the five reads only `+0x68` - so the header now types them `CAssetId x64_curId` and `CPakFile::SResInfo* x68_curRes`, and `Kyoto/CPakFile.hpp` is included there (no cycle). Two more: `fn_802FCC00` is the only one that does not read `4(r4)`, which is what makes its parameter a bare `CAssetId` (`GetResourceTypeById`) rather than a `const SObjectTag&`; and `fn_802FCAE8` needs `IsCompressed() ? 1 : 0` and not `IsCompressed()`, which is 12 bytes and the difference between a 0x4C and retail's 0x58 function. `extern "C"`, not members, because **21 dtk objects call them by their dtk names**. The unit needs a `TARGET_PC` `fn_802FCDE8` or it adds a link-gap symbol instead of closing one |
| `Kyoto/CResLoaderFindPak.cpp` | **1 function, 100.00%, unit `Matching`**, lane `k4` 2026-09-26 | `fn_802FCEEC` (0x802FCEEC, 0x24 = 36 bytes, six instructions), `flip_test.sh` PASS. `fn_802FCDE8` with `tag.id` hoisted into r4 and the result passed through - no `stw r31` / `mr r31,r3`, which proves `this` is never live across the call. Its own unit only because 0x802FCC44..0x802FCEEC is `fn_802FCDE8`'s 0x104 bytes and a unit may not claim two discontiguous ranges |
| `Kyoto/CResLoaderLoadPartAsync.cpp`, `CResLoaderLoadAsync.cpp`, `CResLoaderGetResIdByName.cpp`, `CResLoaderLoadResourceSync.cpp` | **4 functions, 100.00% each, 4 units `Matching`**, lane `m2` 2026-09-26 | `fn_802FC81C` (0x802FC81C, 0x7C), `fn_802FCA68` (0x802FCA68, 0x80), `fn_802FCC44` (0x802FCC44, 0xA0) and `fn_802FC420` (0x802FC420, 0xB8) - 412 bytes, four `flip_test.sh` PASSes, DOL sha1 held. Four units, not one: the four ranges are not contiguous, and `fn_802FC898` sits between the first and the second. **Four codegen rules came out of it and all four are general.** (a) **Declare the return type MWCC can see is dead** - `fn_802FC420` returns `void`; as `void*` it emits a trailing `li r3,0` and is eight bytes long. This is the same polarity rule as `fn_802FCAE8`'s `IsCompressed() ? 1 : 0`. (b) **A named local decides the evaluation order of an expression's callees** - `AsyncSeekRead(buf, (res->GetSize() + 31) & ~31, kSO_Set, res->GetOffset())` inlines both accessors in the *wrong* order and puts the `+31`/`clrrwi` in the wrong place (87.03%); `const uint size = res->GetSize();` first gives retail's exact order and 100.00%. (c) **A temporary passed straight into a call is not the same as a named local** - `CMemory::Alloc(n, h, s, t, CCallStack(-1, lbl_803AFAA0))` gives retail's `addi r3,r1,8` / `bl <ctor>` / `mr r7,r3`; naming the `CCallStack` gives `addi r7,r1,8` and 91.70%, because retail's 12-byte ctor leaves r3 alone and the reference *is* r3. (d) **`CCallStack`'s third parameter is private, and its *default* is retail's symbol** - `kUnknownType__10CCallStack` at 0x803AEAB8, which the characterisation in `docs/research/paks.md` had at the wrong address (0x803AE558 is `lbl_803AE558`). The two-argument call is the only way to reach it. Full write-up and the four corrections to the previous lane's table: `docs/research/paks.md` |
| `Kyoto/CResLoaderLoadResourceSyncCompressed.cpp` / `CResLoaderLoadNewResourceSync.cpp` | **2 functions at 99.10% and 98.04%, both `NonMatching` on purpose, ranges claimed so retail's bytes stay in the link**, lane `m2` 2026-09-26 | `fn_802FC4D8` (0x164) and `fn_802FC63C` (0x1E0) - the compressed loaders. **Every instruction is right; the whole of the residual is which register MWCC hands the compressed arm's four temporaries** - retail uses r7/r6 and r30/r29 where this build uses r6/r7 and r29/r30 (20 instructions differ), and r28/r29/r30 against r27/r28/r29 in the other (16). **About forty body shapes did not move it**, so this is a register-allocator preference, not a source-shape problem. What *was* settled and is worth having: the four-byte decompressed-size prefix is read through `CInputStream`'s private `x8_ptr` and the cursor advanced, identically in both functions; **the compressed length is `GetSize() - GetReadPosition()`, not arithmetic on `SResInfo`** - the `+4`/`+8` loads the earlier characterisation read as `SResInfo` words are `CInputStream::x4_buffer` and `x8_ptr`; the teardown calls `~CInputStream(1)`, the *deleting* destructor, at **vtable slot +8** (confirmed against `__dt__15CMemoryInStreamFv`'s own `extsh. r0,r31 ; ble` before `CMemory::Free`); and `fn_802FC63C`'s `CMemoryInStream` takes `EOwnerShip` = `callerBuf != nullptr`. Two build-level traps: **a `friend` declaration inside a class is the *first* declaration of the function if nothing precedes it**, so it gets C++ linkage and then conflicts with the `extern "C"` elsewhere - the pair has to be repeated at namespace scope above the class (mwcceppc rejects `friend extern "C"`); and a `Matching` unit's `new (lbl, nullptr)` does not compile on the host, which has no three-argument `operator new`. Write-up: `docs/research/paks.md` |
| `fn_802FC898` (DOL unit, not written) | **Blocked, and the blocker is four unnamed functions rather than one**, lane `m2` 2026-09-26 | 0x1D0 = 464 bytes, the grouped-resource path. It needs `fn_802FB994` (0x802FB994, **0x88**), `fn_803434E8` (0x803434E8, **0x54**), `fn_8034353C` (0x8034353C, **0x9C**, which materialises a `CCallStack` string at **0x803B0340**) and `fn_803433D4` (0x803433D4, **0xA0**) - all four unclaimed, none referenced by anything but this function. `fn_803434E8` is a dependency-group range test reading `+8`/`+0xC`/`+0x10` of its node, and `src/Kyoto/CDependencyGroup.cpp` claims a *different* range (0x803208F0..0x80320FD8), so the class is modelled and the method is not. **And `fn_802FB994` contradicts the header**: it reads a byte at `node + 0x1E` and passes `node + 8` to the range test, so the fourth list's element is **0x18 bytes, not 8** - `rstl::list< SPakLoadEntry > x0_aramList` in `include/Kyoto/CResLoader.hpp:141` is the wrong element type. The four lists are still 0x18 apart and the 0x70 size is unaffected |
| `WallCrawler` | 18 functions | verified; no `LoadWallCrawler` or Think to attach to yet |
| `CMainFlowOnMessage` (DOL unit, not a module) | **`OnMessage` 100.00%, unit `Matching`; 4 units, 5 functions, 372 bytes** | Retail `fn_8001DF54`, `0x8001DF54`, `0xB4` = 180 bytes - the function `vtable for CMainFlow`'s slot 2 points at, and the last named hole `docs/research/boot_probe.md` found in the frame loop. Three blockers, all closed, all `flip_test` PASS: **`GetParm()`/`GetParm() const`** (`fn_80048CEC`/`fn_80048CE4`, 8 B each, unnamed until `symbols.txt` was renamed) as their own unit with `inline_max_size(0)` on the declarations; **`~CFrameMsgParm` and `~CTimerMsgParm`** (92 B each) in their own units, each carrying its class's vtable, which is what lets the destructor be the key function and so be called out of line; and **the parm classes promoted out of `main.cpp`'s anonymous namespace** into `include/MetroidPrime/CArchitectureMessageParm.hpp`, so their vtable symbols are nameable at all. Two measurements worth keeping: **the local is a copy of the message's parm, not a fresh one from its int** - written as an int the unit is 86.33% and mwcceppc hoists the `lwz` and reuses r3; written as `CFrameMsgParm parm(*static_cast<const CFrameMsgParm*>(msg.GetParm()))` it is the copy constructor expanded in place and 100.00%, the only one of five `try_batch` spellings at zero differing instructions. And **`kMR_Normal` must be one `return` after the `switch`, not one per arm**, or mwcceppc emits `li r3,0` twice (84.00%). The port's link gap moved 202 -> 203: `OnMessage` closed, `AdvanceGameState` and `SetGameState` (1,012 bytes, now named) opened |
| `CFrameMsgParmDtor` / `CTimerMsgParmDtor` (DOL units) | **2 x 92 bytes at 100.00%, both `Matching`** | Retail `fn_800487B8` and `fn_80048834`. Retail's four parm destructors are all 0x5C and differ only in which vtable they store. **The vtable is 0xC, not 0x10**: MWCC's is two zero header words plus **one** slot for the destructor, so the trailing zero at `0x803B1B6C` is the next symbol's and claiming 0x10 makes dtk refuse the split. **The base's destructor must stay inline and empty, not pure**: mwcceppc expands it and all that survives is the base-vptr store, and because the expansion needs no call nothing clobbers r4, the deleting flag stays in r4 and `this` alone gets r31 - one saved register, which is the shape. Make it pure and you get `li r4,0 ; bl __dt__24IArchitectureMessageParmFv` where retail has the store, 0x60 bytes, and 96.33% |
| `MetroidPrime/Player/CGameStateSlotsCtor.cpp` (DOL unit) | **2 functions, 164 bytes, 100.00%, unit `Matching`**, lane `m1` 2026-09-26 | `fn_80144924` (0x80144924, 0x38) and `fn_8014495C` (0x8014495C, 0x6C) over `.text 0x80144924..0x801449C8`, `flip_test.sh` PASS, `compare_unit.sh` "sections identical". The `(count, element)` fill constructor of `CGameState`'s two `{count; block[3]}` members at +0x110 and +0x144. **Three spellings had to be exact and none is the obvious one**, all found with a variant sweep: the counter is an `int` (`uint` emits `cmplw` against retail's signed `cmpw`); the element pointer is a **separate variable stepped in the increment clause**, `for (int i = 0; i < n; i++, p++)`, because `&elems[i]` puts `addi r31,r31,16` *before* `addi r30,r30,1` and retail has them the other way round; and `fn_80144924` must **return `this`**, which is a constructor's implicit `return this` and is the only way to get retail's `mr r3,r31 ; lwz r31,12(r1)` epilogue. Declared **descending** (`fn_8014495C` first). The two member types it needed are new: `SGameStateBlock` (16 bytes) and `SGameStateSlots` (0x34) in `include/MetroidPrime/Player/CGameStateBlocks.hpp` |
| `MetroidPrime/Player/CGameStateBlockCopy.cpp` (DOL unit) | **1 function, 32 bytes, 100.00%, unit `Matching`**, lane `m1` 2026-09-26 | `fn_80142A10` (0x80142A10, 0x20), `flip_test.sh` PASS. Eight instructions: a 16-byte frame and a tail call to `fn_80004D5C`, which is the 16-byte block's own copy (`if (this) fn_80004AA0(this, src)`). **Its own unit only because 0x80142A10 is 0x848 bytes from the 164 contiguous bytes above**, and a `configure.py` unit may claim only one range - `docs/research/cgamestate_layout.md` §5 originally listed all three as one 164-byte range, and that was wrong |
| `MetroidPrime/Player/CGameStateStreamCtor.cpp` (DOL unit) | **1 function, `fn_80144140`, 24.33%, `NonMatching`** (claiming 0x80144140..0x801447C4 so objdiff measures it), lane `m1` 2026-09-26 | `CGameState::CGameState(CInputStream&, int)`, 0x684 = 1,668 bytes, the **only** writer of `gpGameState` on the boot path. Written from the prologue through the +0x204 member's constructor (0x80144140..0x801442E0) and left there. **The finding that matters is that the doc's stated blocker was the wrong kind of blocker**: it compiles, links and scores with *no callee written*, because a `Matching` unit needs only relocations - the expensive things are the merged `.rodata` object (`lbl_803A9208`, referenced **nine** times, not five, and the two `CBasics::Stringize` arguments are `lbl_803A9208 + 60` and `+ 134`), the two named `.sdata2` constants (`lbl_8041C1A8` = 359999.0, `lbl_8041C1B8` = 100.0f), and member *types*. All **42** member offsets and sizes are now in `include/MetroidPrime/Player/CGameState.hpp` and are re-measured with mwcceppc's flags by `tools/probe_gs_offsets.py`, which is a new step in `tools/gate.sh` - a header edit that moves a member now fails the gate instead of quietly costing a percentage two sessions later. Three corrections to `docs/research/cgamestate_layout.md` are recorded in place: the virtual dispatch is through **`gpSimplePool`** (0x80418EA8), not a "gpTweakGame at 0x80418EF0" - the displacement arithmetic was 0x48 out; the second loop's invariant is **`gpMemoryCard`**, not `gpGameState`; and a **seventh** blocker exists that the list of six did not have, a 22-instruction `memset` at 0x801444E0 whose length is read out of an uninitialised stack word |
| `MetroidPrime/Player/CGameStateCardOptsCtor.cpp` + `CPersistentOptionsCtor.cpp` (DOL units) | **2 functions, 180 bytes, 100.00% each, both units `Matching`**, lane `cal1` 2026-09-26 | `fn_80145950` (0x80145950, 0x5C) and `fn_80146154` (0x80146154, 0x58), the constructors of `CGameState`'s two 0x2C blocks at +0x54 and +0xDC, `flip_test.sh` PASS on both, DOL sha1 and all 86 RELs unchanged, `matched` 3187 -> 3189 and `linked` 1803 -> 1805. **The second one overturns a claim three files in this tree made**, that `fn_80146154` "cannot be written as source" because it `lbz`es two bytes of "its own outgoing parameter save area". The mechanism was wrong: with `stwu r1,-32(r1)`, LR saved at 36(r1) and r31 at 28(r1), offsets 8(r1) and 12(r1) are the frame's **own local area**, not the caller's - so what is needed is not "garbage passed in" but "two uninitialised bytes of my own frame", and that is expressible. The measurements, ranked by differing instructions with `tools/try_batch.py`: two `volatile bool` locals give 8(r1) and 9(r1), 2 instructions out (a plain uninitialised `bool` is folded away entirely into `stb r3,4(r3)`); two `volatile u32` slots read through a `volatile u8*` give 12(r1) and 8(r1), the two slots the wrong way round, also 2 out; a padded struct of two `volatile bool`s, the same; **one 8-byte `volatile` local read as two bytes four apart is 0 out** and reproduces all 22 instructions including mwcceppc's choice of r5 for the hoisted first load and r4 for the second. `volatile` is load-bearing: without it there is no stack slot at all. The trap in the experiment is that a variants file which assigns a member in both the body and a shared tail double-stores it, and the diff then blames the offsets. `fn_80145950`'s own body is `fn_80146154(this, 0)`, four zero words, and `if (gpMemoryCard) fn_80145628(this)` - the header documented only the first two thirds. |
| `MetroidPrime/Player/CGameStateBlockDtor.cpp` (DOL unit) | **1 function, 84 bytes, 100.00%, unit `Matching`**, lane `cal2` 2026-09-26 | `fn_80004A4C` over `.text 0x80004A4C..0x80004AA0`, `flip_test.sh` PASS, `unit_fit.sh` "fits / no extra functions", first try. A **deleting destructor** for the 16-byte `{u32, u32, u32, void*}` block `CGameStateBlocks.hpp` calls `SGameStateBlock`: `if (this) { CMemory::Free(x0c_data); if ((short)flag > 0) CMemory::Free(this); } return this;`. Ten instructions because `CMemory::Free` (0x802CE388) is null-safe - its own `cmplwi r31,0` at 0x802CE3A4 is why the null `x0c_data` needs no test. 22 callers in the DOL, two of them from `CGameState::CGameState()`. **The parameter is compared as `(short)`, not `int`**: retail sign-extends the halfword itself (`extsh. r0,r31 ; ble`). All 22 callers pass -1, so that free is dead for every one of them and only the emitted branch says the source has it - and the branch is corroborated by the twin, `__dt__80004B9C` (0x80004B9C, 0x50), which is this function instruction for instruction apart from its first call being `fn_80004BEC` instead of `CMemory::Free`. And because the only call is `CMemory::Free`, which takes its argument in `r3`, **the flag never has to be spilled and stays in `r4`** - the frame saves `r30` and `r31` only. Same shape as `~CIOWin` and `~CMainFlow`, whose "rest of destruction" *does* contain a call and so keeps the flag in `r31` and `this` in `r30`; the two are not interchangeable |
| `MetroidPrime/Player/CGameStateMemcardCtor.cpp` (DOL unit) | **1 function, 184 bytes, 100.00%, unit `Matching`**, lane `cal2` 2026-09-26 | `fn_80009DBC` over `.text 0x80009DBC..0x80009E74`, `flip_test.sh` PASS, `unit_fit.sh` "fits / no extra functions". The constructor of the 0xE8-byte `SGameStateMemcard` at `CGameState+0x204`. **The finding worth keeping is that the four `lbz` per loop iteration are decided by the missing `const` on the two globals it fills from, and nothing else.** Retail re-reads the same byte before each of its four byte stores because it cannot rule out that the buffer it is writing *is* that global; declared `const`, mwcceppc hoists the load out of the loop and emits one `lbz` before the `mtctr` - measured with `tools/try_batch.py`, 22 differing instructions against 0. **`const volatile` does not work either** (same hoisted load, same 22), so it is specifically the absent `const` that keeps the reload and not the reload itself. The two globals are `lbl_80417D90` and `lbl_80417D91` - `.sdata` one-byte objects, values 1 and 0 - found by resolving `lbz r0,-32752(r13)` against **`_SDA_BASE_` = 0x8041FD80** (`tools/sda.py`): 0x8041FD80 - 32752 = 0x80417D90. A `Matching` unit may reference such a global by name with `extern "C" unsigned char lbl_80417D90;`; `dtk` already defines it in `auto_09_80417D80_sdata.o`, so no unit has to claim `.sdata`. Also load-bearing: the destination pointer is **stepped**, `for (int i = 0; i < 19; i++, p += 4)`, never re-indexed, and `fn_80009DBC` must **return `this`** - its own last act is `fn_80009898(self)` and the epilogue's `mr r3,r31` comes *after* the `bl`, so the callee's result is discarded. A nested `for (j = 0; j < 4; j++) p[j] = k;` is byte-exact too (mwcceppc unrolls the inner); a loop over a byte count is not. **`fn_80009898` is called, never defined, and is not harmless**: its inner `fn_800098CC` stores 72 at `+0x50` and fills `+0x54..+0x9B` from a third byte at `lbl_80417D8D`, so the 76 this constructor writes there does not survive its own last call. `include/MetroidPrime/Player/CGameState.hpp` now records that, and its two ends - `xA0` = 0 and `xE4` = the flag - are named members where the header had said "no function in the DOL writes it" |
| `CGameGlobalObjects` integration: `CInGameTweakManagerCtor.cpp`, `CGameGlobalObjectsTailCtor.cpp`, `Factories/CCharacterFactoryBuilder.cpp` (DOL units) | **Superseded in part 2026-10-01: the eighth sync absorbed `CGameGlobalObjectsTailCtor.cpp` into upstream's `CRelFile.cpp` and replaced the `pad0`/`x150_tail` members with `CMemoryCardSys mMemoryCardSys`/`CRELFileManager mRelFileManager`. As written at the time:** **Two `Matching`, one written and `NonMatching`, and the integration measured and not landed** - lane `frame`, 2026-09-26. `fn_8016C230` (0x8016C230, 0x14, `CInGameTweakManager`'s constructor) and `fn_801F0A44` (0x801F0A44, 0x30, the +0x150 member's) are 100.00% with `flip_test.sh` PASS; the second is the `volatile u32 w[2]` uninitialised-frame-byte spelling from `CPersistentOptionsCtor.cpp`, first try. `CCharacterFactoryBuilder` (0x80031E60..0x80032230) is 8 of 10 functions at 100% (80.33%): the constructor was renamed in `symbols.txt` (`fn_80032008` -> `__ct__24CCharacterFactoryBuilderFv`) with seven siblings so objdiff pairs them, and `CGameGlobalObjects.hpp`'s 0x28-byte stand-in became the real class. It cannot flip: it emits `CDummyFactory`'s vtable into unclaimed `.data`, and `fn_80031F68` inside its range is referenced by name from another lane's unit. **Two findings.** (a) `CDummyFactory::Build` returns `CFactoryFnReturn(CFactoryFnReturn(p))` - retail builds the result in a frame temporary and copies it into the return slot the way `rstl::auto_ptr` copies (owned flag loaded, not recomputed); `return CFactoryFnReturn(p)` is 43 instructions out, a named local 20, the double construction 0. (b) **On the host, g++ asks for `CCharacterFactory::~CCharacterFactory()` although nothing calls it by name**: `-O2` speculatively devirtualises the `delete` in `TObjOwnerDerivedFromIObj<CCharacterFactory>::~` and emits a guarded direct call. So a declared-only class with a virtual destructor still costs its destructor on the port. The integration itself: `docs/research/cgameglobalobjects_ctor.md`. |
| `SGameStateBlock`'s `rstl::vector<unsigned char>` operations: `CGameStateBlockCopyCtor.cpp`, `CGameStateBlockConstruct.cpp`, `CGameStateBlockClear.cpp`, `CGameStateBlockFill.cpp`, `CGameStateBlockReserve.cpp` (DOL units) | **Three `Matching`, two `NonMatching`** - lane `frame`, 2026-09-26. `fn_80004D5C` (null-guarded construct, 0x28), `fn_80142914` (clear, 0xC) and `fn_80142BA4` (fill, 0x154) are 100.00% with `flip_test.sh` PASS; `fn_80004AA0` (copy constructor, 0xFC) is 94.05% and `fn_801465EC` (reserve, 0x108) 91.44%. They are the tree's own `rstl/vector.hpp` bodies written out over `SGameStateBlock`, and they were written because `tools/boot_probe.sh` reached them inside `new CGameState`. The fill's loop has to form the element address before the store (`p = data + count++; *p = *src`): indexing `data[count++]` is 49 instructions out. `reserve` is left where retail keeps two iterator objects on the stack. **`tools/try_batch.py` cannot find a definition that starts `extern "C"`** on the same line (its regex has no `"`), so wrap such functions in an `extern "C" { }` block. |
| `MetroidPrime/Player/CGameState` (DOL unit, `NonMatching`) - goal items `progress-cgamestate-bodiless-runs` and `progress-cgamestate-partial-16` | **72 -> 86 of 116, 2026-09-29**, rescued from the review queue: both lanes' code passed the judge and was rejected by reviewers over doc claims only. The unit stays `NonMatching` (progress item, no flip). See "`CGameState` 72 -> 86" below |
| `SGameStateMemcardFill.cpp` (fix) | **98.30% back to 99.55%, and a host segfault removed** - lane `frame`, 2026-09-26. `reinterpret_cast<SMemcardA0*>(self->xa0_unk)` was written when the header's +0xA0 was a `u8` array; when the header made it `u32 xa0_unk`, the same cast became a cast of the count *value* to a pointer, on both compilers. objdiff showed a 1.25-point drop nobody chased; the port showed `new CGameState` segfaulting storing through it. `&self->xa0_unk` restores both. **A header change can silently rewrite a `reinterpret_cast` in a unit that still compiles.** |
| `CMain::RsMain` (0x80005C6C, 0x864 = 2148 B) via splitting `main.cpp` | **Split accepted, 0 gain, 2 regressions, and NOT collected** - lane `rsmain`, 2026-09-26. The `mainTail.cpp` recipe generalised: cuts at 0x800053B8-0x80005C6C / 0x80005C6C-0x800064D0 / 0x800064D0-0x8000848C, both new boundaries function boundaries that are **not** another unit's boundary, `dtk dol split` with no link-order cycle, **38 functions moved**. `RsMain` stayed `NonMatching` at 0.26% (`unit_fit`: claimed 2148, ours 8, **short by 2140**) and `CheckReset` (0x80006BA4, 0x49C) stayed at 0.47% in `mainMid`. `matched 3957` and `linked 2534` **both identical to baseline** - the port link is unchanged because `CMainRsMain.cpp` keeps `#ifndef TARGET_PC` and the host body is `PortBoot.cpp`. **Two moved functions regressed and it is not avoidable: `__ct__24CGameArchitectureSupport` 93.10% -> 87.99% and `AddPaksAndFactories` 57.15% -> 57.04%**, because mwcceppc's `@stringBase0` moved (the placement string `??(??)..` from 0 to 0x76) and two of seven references change shape. Both cut directions give 87.99%, and single-removal bisection needs the whole set, so it is not one function's placement. **Left uncollected on purpose** - see carve-vein rule 2c. The patch is preserved at `/tmp/lane-keepers/rsmain.patch`. **The real blocker is a header job, not the split:** `CMain`+0x18..+0x48 holds two 20-byte frame-time histories that `include/MetroidPrime/CMain.hpp` does not model (they sit inside `char x10_pad[0x38]` at line 137), and `fn_800069AC` - the bounded, insertion-sorted float push `RsMain` calls **six times** - is 308 bytes and unwritten. Writing a partial body *lowers* the score, because the empty 8-byte frame already matches retail's prologue exactly. |
| `Kyoto/Particles/CVectorElement` (DOL unit) | **Landed, 2026-09-28** - `Matching` 100.00% **92 / 92**, `flip_test` PASS, `main.dol` bit-identical (`6ef9b491...`), `matched` 8640 -> 8641, `linked` 3497 -> 3589, DOL units 7976 -> 7977. The one short function was `CVEKEYF::GetValue(int, CVector3f&) const` at 99.90%, and it was **two instructions in the wrong order** - the register assignment already agreed, only the emission order of two hoisted loads differed. One 9-line wrapper fixes it; the mechanism and the three sibling TUs that want the identical change are in the hoisted-load-order section below. **Superseded in part, 2026-09-28: of those three, `CRealElement` took the same change and landed; `CIntElement` should; `CColorElement` cannot - it is a register-allocation difference, not a hoist order** (that last claim was wrong: it is still a hoist-order difference, just one the plain swap does not reach - `CColorElement` landed 2026-09-29, see its row below). Landed again three further times on later bases after `git reset`; `configure.py` was `NonMatching` and the source edit gone each time, everything else reproduced exactly. |
| `Kyoto/Particles/CRealElement` (DOL unit) | **Landed, 2026-09-28** - `Matching` 100.00% **151 / 151**, `flip_test` PASS, `main.dol` bit-identical (`6ef9b491...`), `matched` 8641 -> 8642, `linked` 3589 -> 3740, DOL units 7977 -> 7978. The one short function was `CREKEYF::GetValue(int, float&) const` (0x802F0854, 396 B) at 99.88%, and it was **two instructions in the wrong order** - the same defect and the same one-line fix as `CVectorElement` above, applied to this TU's own `*KEYF::GetValue` call site; the mechanism is in the hoisted-load-order section below. The emitter call site in the same file already matched and was left on the original helper, which is the point: retail disagrees with itself between the two callers. **This unit was diagnosed and verified twice before it landed, and both earlier runs' edits were lost to a `git reset` by the driver** - so the run had to re-measure, re-apply and re-verify from scratch; the third application reproduced the earlier numbers exactly (`bytescmp` 2 real diffs -> 0, 151/151, DOL sha1 held). When a `match` item comes back with a clean tree, treat its notes as the recipe and spend the time on re-measuring, not on re-diagnosing. |
| `Kyoto/Particles/CColorElement` (DOL unit) | **Landed, 2026-09-29** - `Matching` 100.00% **45 / 45**, `flip_test` PASS, `main.dol` bit-identical (`6ef9b491...`), all 86 RELs byte-equal, `matched` 9188 -> 9189, `linked` 4014 -> 4059, DOL units 8047 -> 8048. The one short function was `CCEKEYF::GetValue(int, CColor&) const` (0x802CF66C, 392 B) at 96.408%, **28 differing instructions of 98**, and it took **two** changes, not the one its three siblings needed. See "The last particle element needs the locals too" below. |
### Two compiler facts this tree keeps rediscovering the hard way
**mwcceppc reserves r3 for `this` in a non-static member function.** `rstl/rc_ptr_copy` sat at
97.22% for a session on the belief that the body was wrong - *"MWCC allocates the AddRef r5/r4
where retail uses r4/r3"*. The body was right. Holding `this` in r3 pushes the first temporary
after it into r5, and the **same three statements** as a `static` member function get r4/r3, byte
for byte. The ABI is unchanged (r3 = dest, r4 = src) so no call site moved. This generalises to
every `rc_ptr` instantiation, and it took one unit from `NonMatching` to `Matching` to prove.
**mwcceppc 2.7 masks every `!` applied to a `bool`-typed operand.** `CErrorOutputWindow`'s
`cntlzw` is unreachable from source: `clrlwi r0,r31,24` is always emitted first, and that is what
causes the `srwi` that follows. Nine operand spellings and three destination types were measured.
When a comparison of a bool cannot be spelled two ways, it is a compiler version, not a puzzle.
**mwcceppc 2.7 also pairs apostrophes *and* backticks inside `//` comments.** An odd count on a
line is a compile error, and a dropped `//` prefix in a block comment does the same. Two hours went
into this in one lane; it is cheap to know.
**Split the access, not the index.** `TSegIdMap<CCharLayoutNode>::~TSegIdMap` sat at 93.64% on
`id = mIndirectionMap[id.val()].first;`. Spelled `const int i = id.val();
id = mIndirectionMap.begin()[i].first;` it is 100% (2026-09-29, goal lane 1, found by sweeping
1,225 spellings with the unit's own `mwcceppc` line out of `build.ninja`). The unit is 28/28 but
still cannot flip: its `.text` is 0x1BAC against retail's 0x12B0 and in a different order, so
mwldeppc places everything after it 0x1A8 early (item `match-ccharlayoutinfo-object-layout`).
### A `.data` range triggers the link-order cycle, not just a `.text` one
The rule everywhere in this file is *one discontiguous range per unit per section*. It is stated
for `.text` and reads as a `.text` rule. It is not: claiming
`.data 0x803B0D5C..0x803B0D68` for `__vt__15CMemoryInStream` fails
`dtk dol split` with the same link-order cycle, **before anything compiles**, so the error looks
unrelated to the section you touched. And that particular one is unfixable rather than merely
awkward: a vtable is emitted only by the key-function TU, so it cannot be moved to a carved unit at
all.
### `tools/offset_shift.py` - the tool that finds a layout bug by its uniformity
A class laid out wrongly by one constant makes every touching function land at 96-99% with a
**uniform operand delta**, and a codegen difference never produces that uniformity. It found
`CStateManager::pad2_2` (0x34, retail 0x2C) and `CAnimData::x120_unk` (0x58, retail 0x48), and
**nineteen functions went to 100.00%** on those two constants. It scans the DOL; a REL-side
extension is in flight.
**A negative result is a result.** Run over `src/Kyoto/Streams/`, `src/LZO/`, `src/rstl/`,
`src/Kyoto/Audio/`, `CActor` and the Enemies units, it printed nothing - and the reason is
instructive: *no function in those areas is between 90 and 100% at all*, because they are all
either exactly right or genuinely unwritten. The tool can only see a layout bug in a class that is
nearly matching, so "nothing found" is usually a statement about the areas, not about the tool.
## Findings, per item
Dated per-item findings (compiler patterns, walls, module-head shapes) are in `docs/history/decomp-findings.md`, verbatim, one section per heading below. Search it before starting a unit; new items' notes land in `docs/goal-notes/`.
- A defect found in the rig (2026-09-25)
- A 20-84 byte function in an `auto_*` unit is a whole unit, and it closes both gaps (2026-09-25)
- Run the real linker before you trust any link-gap arithmetic (2026-09-25)
- A `Matching` unit may claim one function of a three-function triple (2026-09-26)
- Two header facts that decide whether a copy constructor can be exact (2026-09-26, lane `h4`)
- A `Matching` DOL unit may define functions outside its claimed range - but they must come *after* it (2026-09-26, lane `h4`)
- Adding two lines to a header can reschedule an unrelated function in a `NonMatching` unit (2026-09-26, lane `h4`)
- A vtable is a `Matching` unit's `.data` claim, and its layout is not the Itanium one (2026-09-26, lane `j3`)
- A switch jumptable forces the unit to own the vtable next to it (2026-09-26, lane `k2`)
- Recovering functions the upstream merge dropped, and where the wall is (2026-09-28)
- The carve vein, and what it taught about `linked` and about `PortLinkStubs`
- The `@stringBase0` pool is PER TRANSLATION UNIT, and that settles the `main.cpp` split question
- `sizeof(CMain)` is 0x98, and retail says so directly
- The state block was frozen, and one of its four lines had no value check at all
- `mainMid`'s declaration order is fixed; the unit still cannot flip, and the reason is the link
- A percentage is not a link result: `Carve8026FBFC` is 100.00% and must stay `NonMatching`
- `build.ninja` goes to the repository root, and always has
- `build/report.base.json` is untracked, and a stale one turns history into regressions
- `CInputStream` reads big-endian on a host (2026-09-27, goal item `port-pak-byteorder`)
- `CResFactory::AsyncIdle` is written, and the thing under it is a `CDvdRequest` (2026-09-27, goal item `port-asyncidle`)
- `StreamNewGameState` is defined under retail's own name (2026-09-27, goal item `port-streamnewgamestate`)
- The pak pump drains: `fn_802FD174` erased from `list + 0x48` (2026-09-27, goal item `port-pak-pump`)
- `fn_8029c7e8` is `CSfxManager::LoadTranslationTable`, and the port now keeps its token (2026-09-27, goal item `port-fillinassetids`)
- `GetResourceIdByName` walks the loader first, and an owned buffer frees from the game heap (2026-09-27, goal item `port-boot-cpakfile-sresinfo-getsize`)
- `LoadTypedefEditorProperties` reads its four properties, and its one callee came with it (2026-09-28, goal item `port-loadtypedefeditorprops`)
- Retail has one `LdrToEntityInfo`, so the port's const overload is the forwarder (2026-09-28, goal item `port-ldrtoentityinfo`)
- The empty destructor is retail's, and what it must free is four members (2026-09-28, goal item `port-modeldata-dtor`)
- `IterateSearch` is a two-instruction FP-allocation wall, and Metroid Prime 1 has it too (2026-09-28, goal item `match-cpvsvisoctree`)
- The frame loop's DMA cleanup is written, so its stop moved one callee along (2026-09-28, goal item `port-boot-cmain-rsmain-0eb92a1`)
- `CSfxManager::TranslateSFXID` reads retail's table and answers with it, 0xFFFF (2026-09-28, goal item `port-translatesfxid`)
- The frame-time pair is written, so the loop's stop moved on to `fn_80049244` (2026-09-28, goal item `port-boot-frame0-fn80049244`)
- A loader can be 100% and still not link: `LoadTimeKeyframe` is `Matching` (2026-09-28, goal item `match-cunknown90`)
- mwcceppc hoists inline-helper arguments in reverse call order (2026-09-28, goal item `match-cvectorelement`)
- The last particle element needs the locals too (2026-09-29, goal item `match-ccolorelement`)
- Retail's out-of-line copy ctor decides the translation unit, and a pair of `bool : 1` is one byte (2026-09-28, goal item `progress-cstatemanager-clightcopy`)
- `mutable` on the members is what stops a copy constructor's tail from being pipelined (2026-09-29, goal item `match-cdeferredparticleeffect`)
- A local is allocated in the scope that declares it, and a `const&` to a 2-byte member is one load (2026-09-29, goal item `progress-cstatemanager-dtor-members`)
- 2026-09-29: an unmangled symbol is a free function, and the port's undefined count is a gate
- A string literal's pool slot is set by *where its first user is declared*, and that can be moved (2026-09-29, goal item `progress-cgamestate-fn-80143e88`)
- `CGameState`'s pool is now complete, and the eleven option names were what was missing (2026-09-29)
- A by-value 4-byte class parameter is passed by pointer, and that is visible in the frame (2026-09-29, goal item `progress-cstatemanager-rest`)
- The destructor's addresses, measured, and a 16-byte-element copy that is not `vector<float>` (2026-09-29, goal item `progress-cstatemanager-dtor-body`)
- `CMetareeSwarmRel` is the module head, and `>> 7` is a 25-bit rotate (2026-09-29, goal item `progress-rel-head-metareeswarm`)
- `CIngPuddleRel` is a module head, and a vtable call needs a class (2026-09-29, goal item `progress-rel-head-ingpuddle`)
- `CPlantScarabSwarmRel` is `CMetareeSwarmRel` with another module number (2026-09-29, goal item `progress-rel-head-plantscarabswarm`)
- `CSnakeWeedSwarmRel` is a module head, and a pmf is 12 bytes (2026-09-29, goal item `progress-rel-head-snakeweedswarm`)
- `CMysteryFlyerRel` is a module head, and `fn_45_10` is a hidden-return `optional_object` (2026-09-29, goal item `progress-rel-head-mysteryflyer`)
- `CAtomicAlphaRel` is a module head, and twelve of its fourteen accessors are shared (2026-09-29, goal item `progress-rel-head-atomicalpha`)
- `CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out (2026-09-29, goal item `progress-rel-head-ingsnatchingswarm`)
- `CFishCloudRel` is a module head, and the header already had the record (2026-09-29, goal item `progress-rel-head-fishcloud`, lane 1)
- `CBacteriaSwarmRel` is a module head, and the shortest head from 0x0 is four functions (2026-09-29, goal item `progress-rel-head-bacteriaswarm`)
- `CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict (2026-09-29, goal item `progress-rel-head-tryclops`, lane 1)
- What is left (not this item)
- The module head is writable without the actor hierarchy (2026-09-29, goal item `progress-rel-head-ingblobswarm`)
- `CPillBugRel` is a module head, and the accessor block is already in the DOL (2026-09-29, goal item `progress-rel-head-pillbug`)
- `CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order (2026-09-29, goal item `progress-rel-head-darktrooper`)
- `CGameState` 72 -> 86: two reviewer-rejected lanes, landed code-only (2026-09-29)
- A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1` (2026-09-29, goal item `progress-rel-extend-shredder`, lane 2)
- `Krocuss` is the third `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-krocuss`, lane 2)
- `IngSpiderballGuardian` is the fourth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-ingspiderballguardian`, lane 2)
- `Ripper` is the fourth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-ripper`, lane 1)
- `EyeBall` is the fifth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-eyeball`, lane 2)
- `DigitalGuardian` is the sixth head past the wrapper, and the only one that inlines it (2026-09-29, goal item `progress-rel-extend-digitalguardian`, lane 1)
- `EmperorIngStage3`'s accessor block is a fifth shape, and `RELMain` is not always at the head (2026-09-29, goal item `progress-rel-head-emperoringstage3`, lane 2)
- `CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit (2026-09-29, goal item `progress-rel-head-geomblobv2`, lane 2)
- `CIngSpaceJumpGuardianRel` is a module head, and one accessor is a module-local `.rodata` constant (2026-09-29, goal item `progress-rel-head-ingspacejumpguardian`, lane 2)
- A 1-byte class passed **by value** keeps a byte temporary that retail has no trace of (2026-09-29, goal item `match-csequencehelper`, lane 1)
- `x * 0.5f` and `x / 2.f` are different instructions, and a weak copy of an unnamed retail
## Recovering functions the upstream merge dropped, and where the wall is (2026-09-28)
Nine functions came back in one wave on the merge worktree, from four causes, all of them
"upstream's version of a TU replaced ours" rather than anything structural:
1. **A body the merge deleted outright.** `CScriptPickup::fn_800B4518` and its declaration were
   gone; the retail body is three instructions of bitfield set and the retail symbol is itself
   `fn_800B4518`, so the name is not the problem - the *declaration* was. Same for
   `__sinit_CScriptPickup_cpp`: upstream had a `static float skDrawInDistance = 30.f;` (referenced
   only from a comment) where retail has `static TUniqueId sUnkPickupId = kInvalidUniqueId;`. A
   static initialiser the compiler cannot fold is worth a whole static-init function, and the
   symbol it writes is the one the retail map names.
2. **A renamed function, and a typedef cannot fix it.** Retail's setter is
   `ScriptGUI_SetPtrs__FP10GUILoaders`; upstream calls it `SetSGuiWidget_FuncPtrs` and types it on
   `SGuiWidget_FuncPtrs`. **The parameter type has to be a class actually named `GUILoaders`** -
   the Itanium/MWCC mangler mangles the underlying class, so `typedef SGuiWidget_FuncPtrs
   GUILoaders;` produces the same symbol as the typedef's target and does not help.
3. **A by-reference parameter that retail passes in memory.** `SnakeWeedAlt_8021BA94` copies the
   three words of a `CVector3f` into a caller-side temporary and passes its address, which is what
   mwcceppc does for a 12-byte aggregate taken **by value**. Changing the pmf's first parameter
   from `const CVector3f&` to `CVector3f` took the function from 56.9% to 100%.
4. **A generated struct with the wrong member count.** See `docs/HANDOFF.md` step 17(b'): five
   map-icon ids the generator put in `SLdrTweakPlayerRes_AutoMapperIcons` made it 0x50 too wide
   and cost five functions across two units at once. **When retail's offsets are all off by one
   constant, count the members against retail's constructor, not against the loader** - the
   constructor's store count is the ground truth and the generator's id list is not.
Two things did *not* work, and both are walls rather than puzzles:
- **A retail symbol map can be internally inconsistent, and that is a zero-sum rename.**
  `TypesMatch` has `TCastToPtr<22CScriptPointOfInterest>__FP7CEntity` next to
  `TypesMatch__10CUnknown90CFi` for what is plainly one class. Renaming `CUnknown90` to
  `CScriptPointOfInterest` wins the `__FP7CEntity` overload and loses the `__FR7CEntity` overload
  and `TypesMatch`: net zero, twice.
- **Past ~99% the residue is register allocation, not meaning.** `CEntity::AcceptScriptMsg` at
  99.78% differs only in which halfword is loaded into `r7` first and in a 4-byte stack-slot
  offset; both sides store `m_originator` from `src+2` and `m_id` from `src+4`. `CActor::
  OnScanStateChange` at 99.79% differs only in whether one `TUniqueId` temporary gets one stack
  slot or two. `CGameOptions::InitSoundMode` at 87.7% differs only in whether `li r0,1` sits
  before or after the `cmpwi`; a named local of the enum type does not move it. Reading these as
  "a wrong expression" wastes a session - check whether the *stores* agree before rewriting.
**A build fact worth knowing before you spend a session on a DOL hash.** `build/G2ME01/obj/`
holds `dtk dol split`'s output - the *retail* objects, one per configured unit - and `main.elf`
links those, not `build/G2ME01/src/*.o`, for every unit the ninja generator did not mark
otherwise (876 of 1416 inputs at the time of writing). objdiff compares our compiled
`src/` object against the split one, so for those units **the `main.dol` sha1 cannot move no
matter what the source says**: a green sha is not evidence that an edit was harmless, and an edit
is not a gate failure either. Check `grep -c '^build build/G2ME01/obj/' build.ninja` and read
the unit's entry in the `main.elf` input list before deciding whether a change is DOL-visible.
### Regaining whole units after the merge: four causes, none in the unit's own code
These took `CAi` and `ScriptLoaderRel` back to `Matching` (DOL sha1 held, `flip_test` PASS), after
their functions had matched again for a while and their flips still failed.
1. **Weak inline copies grow `.text`.** Upstream's headers define `CHealthInfo`'s and
   `CDamageVulnerability`'s copy constructors, `SMoverData`'s constructor and four `CAi` virtuals
   inline. Retail has them out of line, at their own addresses, so with the headers as they were our
   object emitted weak copies: `.text` was 0x9f8 against retail's 0x708.
   - The fix is `#define MP_RETAIL_OUT_OF_LINE_COPIES` at the top of `CAi.cpp`. The headers declare
     those members out of line under that macro, when not building for `TARGET_PC`.
2. **Pooled constants that nothing references still take space.** `CCharAnimTime`'s inline
   factories take `const&` arguments, so each call pools a float in `.sdata`, and mwldeppc keeps the
   pooled words even when nothing references them (0x34 against 0x10).
   - `CCHARANIMTIME_LOCAL_CONSTANTS` switches them to locals.
   - **It has to be opt-in.** Making it global broke four functions in `CCharAnimTime.cpp`, which
     needs the direct form, and broke the DOL.
3. **The merge can drop a split section silently.** `CAi`'s `.sdata2` range and `kCAiSplashDenom`'s
   size were missing from `splits.txt`/`symbols.txt`, so `.sdata2` came out 0x20 too large.
   - Diff the unit's `splits.txt` entry against master before debugging code.
4. **A definition outside the split is a duplicate once the unit links.** Upstream's
   `ScriptLoaderRel.cpp` defined `SetTweaks_FuncPtrs`, which retail has at 0x802187E4, outside that
   unit's range. The auto-split asm defines that symbol too, so the flip failed with mwldeppc
   `multiply-defined`.
   - The fix: the port-only `ModulePublish.cpp` owns it again. The loader globals also went back to
     8-byte slots, matching `symbols.txt`'s `size:0x8`.
**Per-function losses came from shared headers and stubs, not from rewritten bodies.** Triage
`build/gate-diff.log` (what `gate.sh` writes against master's report) by looking at the header
and the mangled name before the body. Each of these was cheaper to fix than the unit it hit:
- **A dropped header shape can move many units.** Upstream's `CModelFlags(ETrans, float)` passes
  `rgba` straight to `CColor`. Master routed it through `AlphaOf`, which forces retail's second
  `lfs`, and restoring that took ForgottenObject's `RenderInternal` from 88% to 95%.
  `CPlane` lost its trivially-constructible trait (`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`); putting
  it back took the `CFrustumPlanes` ctor from 70% to 99.6% and `CCubeRenderer`'s dtor to 100%.
  Diff the report as a whole after any header edit, because gains and losses land in other units.
- **Qualifiers and bitfield types are proven by the asm.** Upstream declared `RenderInternal`
  `const`, but retail's mangling has no `C`, so objdiff paired nothing (0%). `TrackedShot`'s
  `bool m_b : 1` makes MWCC normalise to 0/1 (`neg`/`or`) before the `rlwimi`, and retail stores
  the raw bit, so they are `uchar`.
- **Upstream stubs replace real bodies.** `Tweaks.cpp`'s `REL_CreateTweakGlobals` and `main.cpp`'s
  `AddPaksAndFactories` are `{}` upstream, while master had 68% and 59% bodies. The first went back
  under `#ifndef TARGET_PC`. The second is still a loop item, because the port builds master's
  `mainMid.cpp` and keeps the body; only the objdiff unit lost it.
- **A "compiler implicit" claim needs `symbols.txt`.** Retail defines `__dt__14SLdrCannonBallFv`
  in the ScriptCannonBall REL, so it belongs in `CScriptCannonBall.cpp`, not in the port-only
  `SLdrStructMembers.cpp`.
**A wider upstream unit is not a regression.** `CFactoryMgr` (22 functions, 19 matching) and
`CPatterned` (103) cover what master carved as small `Matching` units. The carved functions still
match inside them, but the unit can only flip once all of it matches. Count lost *functions* by
address (`lostcmp`), not lost units.
## The carve vein, and what it taught about `linked` and about `PortLinkStubs`
**Seeding carves (2026-10-02).** `python3 tools/goal_seed.py --only carve` plans carves so a lane
only writes code: it reads `build/G2ME01/main.elf`, masks what the linker fills in (`bl` targets,
`lis` immediates, r13/r2 displacements), and pairs each unsourced `fn_` function with an
already-matched function of the same shape. An item is one run of up to four adjacent functions in
one `auto_*` unit, twins or at most 64 bytes, with the range, each twin's name and source file and
the directory of the nearest claimed range below. Runs stop at a real-named function, since that
needs its own mangling and a `.cpp`. A default kind, seeded last; the trial result is in `HANDOFF.md`.
Carving one retail function out of a dtk `auto_*` range as its own `Matching` unit is the
highest-yield thing in this tree: **11 units / 14 functions** in one batch, and 62 units / 188
functions in another, all at 100.00%, all verified by `flip_test`. Three rules came out of it
that are not obvious.
**1. `linked` counts *functions*, so a contiguous run is one unit.** The four METROTRK stubs at
`0x80003840..0x80003858` are a single 0x18-byte claim worth **four** matched functions. Do not
split them for tidiness: each split is another `splits.txt` range, another `Object`, another
`files.cmake` line and another chance for the range clash below.
**2. mwcceppc keeps a comparison's source operand order, and that order decides register
assignment.** `IsAllocValid` is 20 bytes either way and scores **59%** as
`ptr != (const void*)-1` and **100%** as `(const void*)-1 != ptr`. Eleven other spellings all
give the 59%. This is the same family as the `rc_ptr` r3 finding (see below): the register a
temporary lands in is a function of *how the expression is written*, not of what it computes. When
a function is the right length and the right arithmetic and still mismatches in one or two
registers, re-order the operands before you re-think the body.
**3. A carve that `PortLinkStubs.cpp` also defines is a duplicate the moment it is listed.** Four
of the eleven needed a hand deletion from the stub file. `link_gap.py` counts what is *missing*
and structurally cannot see a symbol that is defined twice, and the boot probe cannot see it
either - it links with the reach stubs, so the duplicate never appears there. `tools/gate.sh`
has a `port link dups` step for exactly this and it is not optional.
**And a carve can create a link-order cycle with a *neighbouring pre-existing* `Matching` unit.**
Carving `0x80302BAC..0x80302BBC` out of `auto_03_803029D8_text` - which starts exactly where
`CFrustumPlanes.cpp`'s `.text` ends - fails `dtk dol split` with
`Cyclic dependency ... CFrustumPlanes.cpp -> auto_03_803029D8_text` **before anything compiles**.
Proximity to another carve is fine (0x80335A14, 0x80335A5C and 0x80335AB0 are 12 and 24 bytes
apart and all three link); proximity to an existing *unit boundary* is not. When a carve fails
with a cycle and the range looks innocuous, check what range precedes it.
**Source order inside a carved unit is descending by address.** mwcceppc emits functions in
reverse source order, so an ascending file is a permuted `.text` - which is 100.00% per function
and still breaks the DOL.
### The mixed-compiler mechanism exists, and no unit in this tree wants it
`Object(..., mw_version=..., cflags=...)` resolves as **per-object overrides** -
`tools/project.py:65` carries `mw_version` in `Object`'s options, line 665 turns it into the
compiler path (`compilers / "$mw_version"`), and line 1025 collects `used_compiler_versions` as a
**set**. So a project can build different units with different mwcceppc versions and **no change to
`project.py` is required**. `dtk`, ninja and objdiff all accept it. For GC 3.0 the only flag
difference is `-enc SJIS` where 2.x wants `-multibyte`, so:
```python
cflags_gc30 = [("-enc SJIS" if f == "-multibyte" else f) for f in cflags_retro]
Object(NonMatching, "some/Unit.cpp", mw_version="GC/3.0a3", cflags=cflags_gc30),
```
**Measured on the one unit it was built for, and it is worse there.** `CErrorOutputWindowCtor` goes
78.56% (2.7) -> 55.44% (3.0a3 `-O4,p`) -> 13.11% (`-O1`). Across all twenty versions on the real
body: every 2.x emits 46 instructions, every 3.0a* emits 34, **retail is 45** - none byte-exact.
3.0a* fixes the `cntlzw` and then coalesces retail's four `lbz`/`rlwimi`/`stb` pairs (at `-O4,p`) or
drops a `li r3,1` the retail code CSEs (at `-O1`). Both are redundant-load/store elimination and no
flag exposes them.
Nothing else wants it either: `CFrustumPlanes::__ct__` 17 -> 293 differing instructions,
`CVector3f::Cross` 13/16 both with 3.0a3 *losing* one, and `CGX.cpp` **will not compile** under
3.0a3 (`illegal reference type 'void &'`, `single_ptr.hpp:35`).
**So: a mechanism, not a policy.** Recorded because the capability is unlocked and currently unused,
and because the next person to suspect "wrong compiler version" should find this rather than repeat
it. `tools/probe_cerror_versions.py` and `tools/probe_cntlzw_versions.py` are the two probes; the
second one ranks compilers on a synthetic function, the first settles them on the real body, **and
they disagree** - which is the point.
### 2c. A split that moves many functions costs fidelity, and the cost is not avoidable by choosing a different cut
Two splits are now measured, and the difference between them is the lesson.
| split | functions moved | cost |
| --- | --- | --- |
| `mainTail.cpp` -> `CMainShutdownSubsystems.cpp` | **1** | **none.** `CMain::ShutdownSubsystems` went 1.47% -> **`Matching` 100.00%** |
| `main.cpp` -> `CMainRsMain.cpp` + `mainMid.cpp` | **38** | `__ct__24CGameArchitectureSupport` **93.10% -> 87.99%**, `AddPaksAndFactories` 57.15% -> 57.04% |
The mechanism is **mwcceppc's `@stringBase0`**. It is a per-object symbol holding the base of the
literal pool, and the placement string `??(??)..` moved from 0 to 0x76 when 38 functions changed
units; two of seven references to it then change shape. **Both cut directions give 87.99%**, and
bisecting by removing one function at a time does not converge, because the pool base depends on the
set rather than on any one member. So this is not a placement mistake to be fixed by moving a
boundary - it is a property of how much moved.
**The rule: budget the split by how many functions it moves, and prefer the narrowest cut that
reaches the function you want.** A one-function split is free here. A 38-function split silently
degrades everything it carries, and `NonMatching` is exactly why the gate does not notice - which is
the same trade this file has refused before.
**And a split is not progress on its own.** The `main.cpp` split was accepted, gate-green, hash-stable,
and moved `matched` and `linked` by **zero**, because `CMain::RsMain` stayed at 0.26% and
`CMain::CheckReset` at 0.47%. A carve is only worth making when the function inside it can actually
be matched, so **check what is blocking the function before you split its unit** - for `RsMain` that
is a `CMain.hpp` layout job plus 308 unwritten bytes, and neither is affected by where the boundary
sits.
## The `@stringBase0` pool is PER TRANSLATION UNIT, and that settles the `main.cpp` split question
**The hypothesis that a split of `main.cpp` could be free is disproven, and the reason generalises.**
The MWCC constant pool (`@stringBase0` and the `@n` literals in it) is emitted **per translation unit**,
in that unit's own emission order. So the question "does `StreamNewGameState` staying in the head keep
`"??"` at offset 0" has the answer **no**, because the string the constructor needs is not in the same
TU as the head's first string user.
Measured on the three-way split (`main` -> `main` + `CMainFillInAssetIDs` + `mainMid`):
| function | retail | before | after |
| --- | --- | --- | --- |
| `__ct__24CGameArchitectureSupport` | 0x80007EC4 | 93.09910% | **87.98649%** (-5.113) |
| `AddPaksAndFactories` | 0x80007168 | 57.14876% | 57.03513% (-0.114) |
28033 functions unchanged, 0 lost, 0 gained. The tail's first string user in emission order is
`AddPaksAndFactories` (13 pak literals), not `"??"`, so `"??"` lands at 0x76.
**No three-way split can avoid this.** With the carve at 0x80006B38 the ranges are forced to
{head, carve, tail} and the constructor at 0x80007EC4 is always in the third. **This is a structural
property of the layout, not a choice to be tuned** - which is why it is worth writing down rather than
re-attempting.
**And the trade was still worth taking**, because the cost lands on a function that counts 0. The
constructor is 1-of-11 in its unit, so it contributes nothing to `matched` or `linked`, and the
regression is a *linkage-context* artifact: identical C++ source, different pool layout, identical
behaviour. Against that, `CMain::FillInAssetIDs` became an isolated **`Matching` 100.00% 1/1** unit and
`linked` rose 2554 -> 2555.
**The rule this gives: judge a split's cost by what it does to the COUNTS, not to the percentages.**
Percentages on functions that contribute 0 are not a currency. `CMain::AsyncIdle` was declined for
exactly the mirror image of this reason - 1-of-11, contributing 0 to both, for no Matching unit.
**Two operational notes from the same lane.** `flip_test.sh` rewrites the literal one-line
`Object(NonMatching, "<unit>"` form; a wrapped `Object(\n  NonMatching,\n  "<unit>"` is silently never
flipped, so entries must stay on one line - `extra_cflags=[...]` after the unit name is fine. And
`inline_max_size` is settable per unit
(`extra_cflags=['-pragma "inline_max_size(125)"']`), which is how the new units hold their pools.
## `sizeof(CMain)` is 0x98, and retail says so directly
`include/MetroidPrime/CMain.hpp` declared 0x94. It is **0x98**, and the proof is not an inference from
a store instruction - it is retail's own symbol:
```
config/G2ME01/symbols.txt:18933  sMainSpace = .bss:0x803C5A20; // type:object size:0x98 scope:global
config/G2ME01/symbols.txt:18934  lbl_803C5AB8 = .bss:0x803C5AB8; // type:object size:0xC
```
`0x803C5AB8 - 0x803C5A20 = 0x98`, so the size is bounded on both sides by retail, and the next object
is only 12 bytes later. `CMain` is that object: `InvokeCMain` at 0x80008818 is
`lis r9,0x803C ; addic. r31,r9,0x5A20`. All 20 probed words now agree with retail, and
`sizeof(SFrameTimeHistory) == 0x14`.
**The missing 4 bytes are at +0x94**, stored by `stw r8,148(r3)` at 0x800089A0 and by `RsMain`'s
`stw r0,148(r31)` at 0x80005E30.
**A premise in the brief was wrong, and the correction matters: the `li r3,356` in `RsMain` is
`CGameArchitectureSupport`'s size (0x164), not `CMain`'s.** So the +0x94 member is a
`CGameArchitectureSupport*`, not a `CMain*`. Note the naming trap this creates: **`0x164` is that
class's size, and it must not become this member's name** - the member is at +0x94. (The first
version of this header called it `x164_`, which by this file's own convention reads as offset 0x164;
corrected to `x94_cGameArchitectureSupport` at collection.)
**What the two 20-byte windows are.** `+0x18` and `+0x2C`, and `+0x10..+0x18` is a `double`
(`stfd f2,16(r3)`, 0.8041A3F0 = 0.0):
| offset | member | evidence |
| --- | --- | --- |
| `+0x18` | `int count` | ctor `stw r8,24(r3)` at 0x800088C4; `fn_800069AC` does `lwz r0,0(r3) ; cmpwi r0,4` |
| `+0x1C..+0x28` | `float values[4]` | `stfs f0,4(r5)` with `r5 = r3 + count*4`; `v[4]` is left uninitialised by the ctor, which is why `RsMain` pushes 4 seeds |
| `+0x2C` | the same struct, second instance | ctor `stw r8,44(r3)` at 0x800088C8 |
| `+0x40`, `+0x44` | each history's **sum** (Superseded 2026-09-28: it is the **mean** — `fn_80008B60`'s tail is `fmuls f1,f3,f0` after `fdivs f0,f2,f0`, with `f2 = 1.0f` and `f0 = count`; the constant read as `200.0` is mwcc's int-to-double bias 2^52+2^31) | `fn_80006954` returns `fn_80008B60(h->v, h->count)`, an unrolled `fadds` accumulator, stored at 0x80006120 / 0x8000623C |
**Three claims in `boot_path.md` row 10 were wrong and are corrected in place.** It **does not sort** -
there is no `fcmpo`/`fcmpu` in its 308 bytes, only a shift and an 8x unrolled accumulation. The two
floats are **sums, not a running minimum** (superseded 2026-09-28: they are **means** —
`sum * (1.0f / count)`; see the `+0x40`, `+0x44` row above and the `fn_80008B60` note at the end of
this file). And **`CMain::DrawDebugMetrics` is 0x6C bytes and reads
neither** - it toggles a global and calls `CMemory::GetMetrics`; the consumer is `fn_800597D8`. Row 10
was also not an unclaimed gap.
**The strongest single consistency check**, and the reason to believe the model: the sample is
`float(tick delta) * mData[0x10] / 0.016666668`, and `RsMain`'s seeds of 0.3f/0.2f match the
`+0x40`/`+0x44` seeds exactly. `x10_unk` (+0x10) is named for its type only - its absolute unit is
**not** derivable, because its factor is written once to 0.0f by `__sinit_CStopwatch_cpp` at
0x8028BCF0. Flagged as a guess rather than dressed up as a name.
### `fn_800069AC` - 304 of 308 bytes, and why the last 4 are not reachable from here
Written in `src/MetroidPrime/Carve800069AC.c`. NonMatching, **not claimed**, and the reason is
measured: 76 instructions / 304 B against retail's 77 / 308, 62 differing, **the first 15
instructions byte-identical**. Retail materialises the destination pointer with one extra
`addi r8,r8,4`, and **all 62 differences follow from that single `+4` placement** (`lfsx` versus
`add`+displacement). 40 spellings and 6 flag sets were tried; 304 B is the plateau.
**A claim is impossible from this lane**: 0x800069AC sits **inside** `MetroidPrime/main.cpp`'s claim
and cutting that claim in two is another lane's file. After the `6a846c2` three-way split the head is
0x800053B8..0x80006B38, so the function is in the head - claiming it means a two-way split of the
head, which is its own decision with its own pool consequences.
**And landing it buys nothing yet, which is worth saying plainly:** `linked` would not rise, and
nothing in the port can call it - `TARGET_PC` compiles a host `RsMain` that returns immediately, and an
unclaimed carve defines no DOL symbol.
## The state block was frozen, and one of its four lines had no value check at all
Found by a lane while checking something else, and every part of it is the same failure class.
**The writer keyed on hardcoded values.** The ad-hoc script that maintains
`docs/HANDOFF.md`'s state block searched for the literal prefix `"linked     2551"` and
replaced it with the current number. **After one successful run the line read `linked     2554`,
no longer matched its own key, and every later run was a no-op for that line - forever, with
no error.** The same held for the other three, so the block froze at whatever each line
happened to hold when its key last matched. `linked` drifted to 2554 while `report.json` said
2555, and nothing said so.
**Its replacement string truncated the line mid-sentence.** The `REL units` template ended at
`"...This line used to add a"`, and because the key matched it kept overwriting the line with
that fragment. The committed state block carried a dangling sentence; it is gone from the last
60 commits of history, so the prose was rewritten rather than recovered. **A presence test
cannot see a sentence that stops in the middle** - so the fix checks the *shape* of each line,
not only that it is there.
**The checker tested the value of three of the four lines.** `matched`, `DOL units` and
`REL units` were value-checked; **`linked` was only ever checked for appearing exactly once.**
That is the same defect the once-only test had two revisions earlier, in the same file, and the
code said so: *"a once-only test on two of the four lines is a check that covers half the thing
it is named after, which lends its reputation to the half it does not cover."* The value check
had the same shape and nobody extended it.
**All three are now fixed, and the fix is in the repo rather than in `/tmp`:**
`tools/sync_state_block.py` keys on the stable **prefix** so the rewrite is idempotent forever,
rewrites only the numbers and carries the prose across verbatim, **fails loudly** if a line is
missing or has no `functions` to anchor on, and warns on a line that ends mid-sentence.
`check_docs_claims.py` now checks `linked`'s **value**. Both were tested against injected drift:
`sync_state_block.py --check` exits 1 and `check_docs_claims.py` exits 1 on a wrong number, and
0 when correct.
**The general lesson, which is a new entry for `docs/PROCESS_LESSONS.md`: a rewrite keyed on the
value it is about to replace is a rewrite that stops working the moment it succeeds.** Key on
something stable - a prefix, an id, a path - and then make the tool's own drift check a gate step,
because a writer that silently stops is indistinguishable from a writer that has nothing to do.
## `mainMid`'s declaration order is fixed; the unit still cannot flip, and the reason is the link
A pure block move - **234 code lines before, 234 after, 0 changed** - putting the file in
descending-by-retail-address order. The lever: `CArchitectureQueue::Push` (0x80007A80) above
`CGameArchitectureSupport::Update` (0x80007A14). Five definitions outside this claim were moved
to the top sorted by their own addresses, read from `symbols.txt` rather than guessed.
**Measured: 2 of the 15 functions our object emits that retail names were misplaced before, 0 of
15 after** - the adjacent `Push`/`Update` transposition at positions 7-8 was displacing all 12
above it. `check_decl_order.py --unit` reports `ok`.
**And the unit did not move: 54.52%, 9/21, identical before and after.** The reason is worth
recording because it is *not* ordering:
- **`flip_test` fails at the link, not on bytes.** `CResFactory::GetResourceIdByName` is the
  36-byte forwarder at **0x80006B80 - the first byte of this claim** - and nothing else defines
  it, so that one function must be written in source.
- The two `CFactoryMgr::RegisterFactory*` undefineds come from `AddPaksAndFactories`'s **36
  registrations at 0x80007504-0x80007864**, which call symbols `symbols.txt` leaves unnamed. So
  that 1936-byte function **cannot be closed inside this file at all.**
- `unit_fit`: `.text` claimed 6412, ours 6272 - short by 140 - with 24 extra functions / 2352
  bytes.
- Three of retail's unnamed functions at 0x80007AA0/0x80007AC8/0x80007B38 are now emitted
  **byte-identical** (modulo two `bl` relocations) as `push_back` / `do_insert_before` /
  `create_node`, 40/112/136 B, in retail's exact slots. **Pairing them needs three renames in
  `config/G2ME01/symbols.txt`**, e.g.
  `push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage = .text:0x80007AA0; // type:function size:0x28`.
  **Those renames are not done** - a DOL-wide symbols edit is its own change.
**So this is a write-the-functions problem, not a reorder problem, and the next slice should be
treated as one:** 3,500+ bytes of unwritten bodies, one required forwarder, one `symbols.txt`
rename set, and 5 definitions that have to move to their own units.
## A percentage is not a link result: `Carve8026FBFC` is 100.00% and must stay `NonMatching`
`BeginScene` (retail 0x8026FBFC, 0x180 = 384 B) is **byte-exact in `.text` and objdiff 100.00%**,
and it still cannot be `Matching`. Isolated by applying one manifest edit at a time:
| edit | DOL sha1 | build.sha1 |
| --- | --- | --- |
| splits only | `6ef9b491` | 87/87 |
| `Carve80272958.c` Matching | `6ef9b491` | 87/87 |
| `Carve80270848.cpp` NonMatching | `6ef9b491` | 87/87 |
| **`Carve8026FBFC.cpp` Matching** | **`09afd3be` (+32 B)** | **44/87** |
**The mechanism:** that unit's object has **no `.sdata2` section**, yet mwldeppc attributes **20 bytes
at 0x8041E250** to it (`main.elf.MAP`, `@407..@411`). Those 20 bytes collide with `CStopwatch.o`'s 8,
`.sdata2` grows 0x54C0 -> 0x54E0, and **43 REL hashes break**.
**It is not a source problem** - the relocations already use the named `lbl_8041DFBC` - so there is
nothing to fix in the C++. This is the same class as the `SetViewPointMatrix` wall: **MWCC's constant
pool is placed by the linker, and a unit's `.sdata2` contribution is not a property of its source
alone.** The rule is the one this repo keeps re-learning, in its sharpest form: **objdiff percentage is
a signal; `Matching` with `flip_test` PASS and 87/87 sha1s is the result.**
## `build.ninja` goes to the repository root, and always has
`tools/project.py:1546` opens `"build.ninja"` as a **relative path, hardcoded to cwd** - not to
`--build-dir`. A lane's fresh worktree therefore looks broken: `ninja -C build` reports
`loading 'build.ninja': No such file or directory`, and a `build.ninja` sits at the repo root.
**The root cause is the first `configure.py` in a fresh worktree, not the change under test.** It runs
before `build/G2ME01/config.json` exists, so `build_config` is `None` and ninja is written with **7
edges instead of 1,788**. One `ninja` fixes it. **Reproduced at HEAD with zero edits.**
This cost a full collection once: the symptom was described correctly, the cause was not found in the
time available, and "I could not isolate it" was then treated as "it is unlandable" and the work was
reverted - when the change had been fine and the harness had simply never been run this way. **Before
blaming a change for a build failure in a fresh worktree, run the harness twice.**
## `build/report.base.json` is untracked, and a stale one turns history into regressions
The gate's per-function diff compares against `build/report.base.json`, which **is not in git**. Ours
still read 3973/2550 and so reported `AddPaksAndFactories` and `__ct__24CGameArchitectureSupport` as
**GONE** - they had moved to `mainMid` in the three-way split three commits earlier. A stale baseline
makes every legitimate earlier change look like a regression, **which is its own way of making a gate
meaningless**: the honest response to a gate that suddenly fails is to ask whether the gate's *input*
is current before concluding the *change* is wrong.
`./tools/gate.sh --baseline` records it, and **refuses to run on a dirty tree** - correctly, since the
baseline must come from a verified commit. Rebase it onto the last commit that passed every gate, then
hold the new change to *that*.
## `CInputStream` reads big-endian on a host (2026-09-27, goal item `port-pak-byteorder`)
**The byte-order wall on the pak chain is fixed at the reader, and only there.** Retail's
`ReadInt32` is the CPU's own `lwz` on a big-endian PowerPC, so the value it returns is the
big-endian word in the buffer with no conversion to see; a little-endian host's identical load
returns the four bytes reversed, and `CPakFile::InitialHeaderLoad` read `0x05000300` against its
`version != 0x30005` test and returned **without advancing `x2c_asyncLoadPhase`**.
**What changed, three files:**
- `include/Kyoto/Streams/CInputStream.hpp` - two host-only helpers, `cinput_stream_read_be32` and
  `cinput_stream_read_be16`, under `#ifdef TARGET_PC`, applied by `ReadInt32` and `ReadUint16` in
  an `#ifdef TARGET_PC` / `#else` around the *return only*, so mwcceppc pre-processes the function
  to byte-identical text. `ReadInt16`/`ReadInt8`/`ReadBool`/`ReadFloat` are reached through those
  two, and so is every `Get<T>()` and every `rstl` stream constructor.
- `src/Kyoto/CResLoaderLoadResourceSyncCompressed.cpp` and
  `src/Kyoto/CResLoaderLoadNewResourceSync.cpp` - the four-byte decompressed-size prefix, which
  those two friends read straight off `x8_ptr` because `Get(4)` is in another translation unit,
  goes through the same helper. **This is the one read `ReadInt32` does not perform**, and it is
  why the fix is not a swap inside `CPakFile`: the same stream also supplies the name-list length,
  `x4c_resTableCount`, every 20-byte resource-table entry and `CStringExtras::ReadString`'s string
  lengths, so a pak-local swap would fix the version word and leave all of them reversed.
Two host-only diagnostics that asserted the old cause were corrected with it:
`src/MetroidPrime/mainMid.cpp`'s `[pak] pump:` messages and the wall note in
`src/Kyoto/CResLoaderAddPakFileAsync.cpp`.
**Measured, not recalled.**
- Host runtime, the real `CInputStream` over `00 03 00 05`: `version = 0x00030005`,
  a count field of `8` read as `8`, `GetReadPosition() = 12`, a 16-bit `AB CD` read `0xABCD`, and
  `3F 80 00 00` read as `1.000000`. `BE_TEST PASS`.
- `./tools/decomp_build.sh`: `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465)`,
  unmoved. DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`.
- `tools/report_diff.py`: `matched 3980 -> 3980  linked 2557 -> 2557  no regression`, and the three
  touched units' fuzzy percentages are identical before and after - `CInputStream` 100.0,
  `CResLoaderLoadNewResourceSync` 98.041664, `CResLoaderLoadResourceSyncCompressed` 97.24719.
- `./tools/goal_check.sh build/goal/item.json`: **`goal_check: PASS port-pak-byteorder`**, all nine
  checks `ok`, exit 0. Re-run 2026-09-27 17:06 on the tree as it stands: `GATE PASS 5d41809+10
  changed`, `8 path(s) changed under src/ or include/`, `port undefined 322 -> 322`, and
  `probe:` **652** files, `0 failed, 0 errors; link: LINKED (322 undefined, 0 duplicates)`. (An earlier
  run of the same diff read `GATE PASS 8c0783d+3 changed`; the HEAD moved, the numbers did not.)
**What is NOT measured, and do not read this as more than it is:** the in-game claim. The queue
reason asked whether the seven admitted paks now reach `kAP_Loaded`; `tools/boot_probe.sh` was run
and **died in the windowing layer before the game's start-up** - `[error] [aurora::window] Error
initializing SDL: x11 not available`, exit 134, no `[pak]` line at all - so the pak chain's
behaviour at boot is still unmeasured. The measurement above is a function-level one. The next run
that gets a display should print the `[pak] pump:` counts, and they are the number that settles it.
**The judge for a `port` item is partly vacuous, and this item is the proof.** `goal_check.sh`
tests `grep -qF "$TARGET" undef_by_obj.txt`, but `CInputStream::ReadInt32` is an in-class inline -
it is never in the port's undefined set, before or after - so that line passed on a clean tree and
would have passed with the fix absent. The item's `ok` came from `gate.sh`, the counts and the
probe, all of which would also pass on a clean tree. **A `port` item whose target is an inline
function can only be failed by the per-kind check that cannot see it**; the honest verdict for
this one is the runtime test and the report diff above, not the PASS line.
**Three follow-ups this item did not take, all measured:**
- `CBitStreamReader::ReadBits` (`src/Kyoto/Streams/CBitStreamReader.cpp:46`) does
  `x0_stream.Get(&x4_bitWord, len)` and then shifts `x4_bitWord` as if it were big-endian. `Get`
  is a raw copy, so on a host the word is still reversed - the same defect, one level up, and it
  is the next byte-order hole after this one.
- `COutputStream`'s writer side is still host-endian: `CBasics::SwapBytes` is identity under
  `#if 0`, and **retail's is identity too** - proved by `CGameStateBlockFill`,
  `CGameStateSysOptsPutTo` and `CGameStateSlotDefaults`, all three `Matching` with that identity
  inlined. Enabling it under `TARGET_PC` would restore retail's stream-order semantics on the host
  and fix `CCubeMoviePlayer`'s THP headers with it; nothing round-trips a `COutputStream` into a
  `CInputStream` on the boot path today, so nothing here is broken by leaving it.
- `CStringTable`'s `uint* entry = reinterpret_cast< uint* >(x10_strings)` (`src/Kyoto/Text/CStringTable.cpp:96`)
  walks the string table's raw words without a swap.
**And a process finding: this worktree was `git reset --hard` twice out from under the lane.** The
change above passed `goal_check` at 15:52:54 with `8c0783d+3 changed`, and `tools/run_goal.sh`
started a new item at 15:55:50 and reset the tree at 15:55:51, discarding it; the driver then
judged the same item against a clean tree and advanced the queue. The edits were re-applied, reset
again before 16:01:55, and re-applied a third time. **A driver that resets the worktree while a
lane is still in flight destroys the work it is about to judge**, and the vacuous `port` check
above is what let the item read as passed anyway. If a `port` item's target is an inline function,
the driver should not treat `PASS` as evidence that the fix exists.
**Re-measured independently, 2026-09-27, on the re-applied tree (goal lane, same item).** `stat`
before and after the build shows no source byte moved during it, so every number below is
attributable to exactly this tree. The reader, tested against the real
`src/Kyoto/Streams/CInputStream.cpp` and the real header: compiled with `-DTARGET_PC` it prints
`version = 0x00030005`, a count of `8`, `0xABCD`, `1.000000` and `BE_TEST PASS`; the **same test
compiled without `TARGET_PC` fails all five checks with `version = 0x05000300`** - the control that
proves the test can fail and that the `#ifdef` is what it is measuring. Then the gates, each run
again rather than quoted: `./tools/decomp_build.sh` `All: 8.52% fuzzy, 7.54% matched, 5.32% linked
(3980 / 28465)`, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`,
`tools/report_diff.py` `matched 3980 -> 3980  linked 2557 -> 2557  no regression`, `tools/gate.sh`
`GATE PASS`, `tools/probe_sources.sh` 0 failed / 0 errors with the link at 322 undefined and 0
duplicates, `tools/check_symbol_names.py` 0 missing, `tools/check_docs_claims.py` ok, and
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS port-pak-byteorder`**. Still not
measured, and not this item's to measure: whether the seven admitted paks reach `kAP_Loaded`, which
needs a display for `tools/boot_probe.sh` and is `port-pak-warmup`'s number.
**Re-applied and re-judged 2026-09-27 17:04-17:07 (goal lane, same item, tree reset again in
between).** `git status --porcelain` was empty when this run started, so the fix had been reset a
fourth time; it was restored from the salvaged diff of the lane that wrote it and judged as above:
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS port-pak-byteorder`**, nine `ok`
and exit 0, in 51 s - `GATE PASS 5d41809+10 changed`, `counts: matched 3980 -> 3980   linked 2557 ->
2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465)`, `8 path(s) changed under
src/ or include/`, `verify port-pak-byteorder.sh: BE_TEST PASS`, `port undefined 322 -> 322`. DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`, `check_docs_claims.py`
ok. The control still fails: the same test compiled **without** `-DTARGET_PC` prints
`version = 0x05000300` and `BE_TEST FAIL (5)`. Two things were deliberately left out of the restored
diff: `src/MetroidPrime/PortReachStubs.cpp`, which `tools/boot_probe.sh` *appends* to itself
(`fn_802C1658`, `lbl_80418AE4` - both in `Carve8026FB80.cpp.o`'s undefined set) and which is not part
of the fix, and nothing else. Still unmeasured, still not this item's to measure: the boot.
## `CResFactory::AsyncIdle` is written, and the thing under it is a `CDvdRequest` (2026-09-27, goal item `port-asyncidle`)
**The port's link had asked for `_ZN11CResFactory9AsyncIdleEjb` ever since the written
`CMain::AsyncIdle` started calling `gpResourceFactory->AsyncIdle(time, flag)` (boot-path step 21e,
`src/MetroidPrime/main.cpp:307`), and `docs/HANDOFF.md` was still saying it needed a member model
past +0x9C that nothing in the tree had.** It does not: lane `m3` measured the whole `CResFactory`
interior (`docs/research/paks.md`, "The `CResFactory` interior, measured") and all four words this
function reads - `+0xA0`, `+0xB0`, `+0xCC`, `+0xD0` - are named members. What was actually missing
was the disassembly read end to end. Three things in it were not obvious and all three are measured:
- **The divisor is not this class's.** Retail's `r31` at 0x802FA3F8 is `0x80411050`, which
  `config/G2ME01/symbols.txt` names `mData__10CStopwatch` (`.bss`, `size:0x18`), and the two words
  loaded from it are `x8_timerFreqO1M` - `CStopwatch::CSWData`'s `s64` at +0x08, ticks per
  microsecond, written by retail's own `CStopwatch::CSWData::Initialize` as `stw r3,8(r31)` /
  `stw r4,12(r31)` at 0x8028C1C0-0x8028C1C4. That is `__div2i`'s divisor with `r5` the high word
  and `r6` the low, which is why the load is two `lwz`s, and it is what fixes `time`'s unit:
  `CMain::AsyncIdle` passes 500, 5000 and 1000000, so the elapsed count has to be microseconds.
  The public route to the same word is the new
  `CStopwatch::GetGlobalTimerFreqO1M()` (`include/Kyoto/Basics/CStopwatch.hpp`), an inline static
  accessor - no unit mwcceppc compiles emits anything it did not emit before.
- **The element's type is `CDvdRequest`, and the slot is `IsComplete`.** `lwz r3,20(r25)` with the
  node in `r25` is `x8_item+0x0C`, and both this function and retail's enqueue (`fn_802FAF1C`) call
  through the pointer held there. The offsets only close if MWCC's vptr points at the **vtable
  symbol's base** rather than past its two header words - which `CResFactory`'s own constructor
  states by storing `0x803BAF08`, the `__vt__` symbol itself - and then `vptr+0x10` is
  `CDvdRequest::IsComplete` and `vptr+0x18` is `CDvdRequest::GetMediaType`. The header's own slot
  comments (`// 10`, `// 18`) say the same thing, `src/MetroidPrime/mainMid.cpp:434` already relies
  on it, and `CDvdFile::AsyncSeekRead` returns `CDvdRequest*`
  (`include/Kyoto/CDvdFile.hpp:53`) which is what `fn_802FC898` - the call whose result
  `fn_802FA140` stores at `item+0x0C` - is built on.
- **The trade is one-for-one, not a win.** Retail's erase is out of line (`fn_802FB2E4`, 0x8C
  bytes: unlink, `fn_802FA070(item, -1)`, `CMemory::Free(node)`, `--x14_count`), and that symbol
  was **not** in the port's undefined set, so calling it adds one. Defining `AsyncIdle` removes
  exactly one. The counts bear it out: **322 undefined before, 322 after**, with
  `_ZN11CResFactory9AsyncIdleEjb` gone and `fn_802FB2E4` in its place - and `fn_802FA070` is not
  referenced by the new object, so the item destructor is not a second new hole.
**What changed:**
- `src/Kyoto/CResFactoryAsyncIdle.cpp` - new, port-only, listed in `files.cmake`. The body is
  retail's two halves in retail's order: the `xc8_active` sweep (advance the iterator *before* the
  possible erase, which frees the node) and the timed `x9c_loading` pump loop with its `stop` byte,
  `time - elapsed` budget and `flag` override. Its header carries the annotated disassembly.
- `include/Kyoto/Basics/CStopwatch.hpp` - `GetGlobalTimerFreqO1M()`, the accessor above.
- `src/Kyoto/CResFactoryPortVirtuals.cpp` - **the port's empty `CResFactory::CResFactory()` now
  initialises both lists.** Retail's `fn_802FB154` writes each list's four pointers to its own
  `xc_empty_prev` and its count to 0; an empty body left all six words of both `SLoadList` members
  indeterminate, which was harmless only while nothing read them. Walking an indeterminate
  `x4_start` is a segfault rather than a wrong answer, so this is a prerequisite of the function,
  not a convenience. `x0_allocator` is left alone - retail stores nothing there either.
- `src/MetroidPrime/PortReachStubs.cpp` - **`reachstub_137` deleted.** The stub and a real
  definition of the same symbol collide the moment `MP_BOOT_STUBS=ON`, which is what
  `tools/boot_probe.sh` passes, and `gate.sh`'s duplicate step cannot see it. This is the rule
  `tools/check_files_cmake.py` states for `CAudioStateWinCtor.cpp`: "Delete that alias."
- `docs/research/port_link_gap_list.md` - regenerated with `tools/link_gap.py --write-list`:
  `_ZN11CResFactory9AsyncIdleEjb` out, `fn_802FB2E4` in, 319 MISSING both before and after.
  The tool's own check is the reason the file is touched at all - a listed symbol that is no longer
  missing fails the gate until its entry is deleted.
- The probe's file count moved **652 -> 653** with the new source, so every current-state quote of
  it in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` was bumped by
  `tools/check_docs_claims.py`'s rule. Three *historical* incident quotes of the same count were
  **not** rewritten - the figure was right when written - they were re-spelled as `` `652` files ``
  so the checker reads them as a figure of the past rather than a current claim.
**Measured, not recalled.**
- `./tools/decomp_build.sh`: `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465
  functions)`; DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against
  `config.yml`. Adding a header accessor changed no unit's bytes.
- `tools/report_diff.py` over the judge's baseline: `matched 3980 -> 3980   linked 2557 -> 2557`.
- `tools/link_check.sh`: `compile errors 0`, `unique undefined symbols 322`,
  `duplicate definitions 0`, `unchanged from baseline (322 undefined, 0 duplicates)` -
  and `CResFactory::AsyncIdle(unsigned int, bool)` is no longer in `link_undefined.txt` while
  `fn_802FB2E4` is, at line 309.
- `python3 tools/link_gap.py`: `319 MISSING symbol(s), all accounted for`.
- `python3 tools/check_symbol_names.py`: `checked 322 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py`: `647 sources`, `0 on-disk sources are in no manifest`.
- `python3 tools/check_decl_order.py`: `ok: 841 unit(s) checked`.
## `StreamNewGameState` is defined under retail's own name (2026-09-27, goal item `port-streamnewgamestate`)
The target `StreamNewGameState__5CMainFR12CInputStreami` was never missing a *function* - it was
missing a *name*. `main.cpp` has carried `CMain::StreamNewGameState`'s body since the scaffold, but
the host compiles it to `_ZN5CMain18StreamNewGameStateER12CInputStreami`, and the one caller in the
port, `CMainFlowDtor.cpp:315`, reaches it through the `extern "C"` declaration at line 208 spelled
with mwcceppc's mangled name - which no host compiler will ever emit for a member. So the symbol
sat on the undefined list while its body sat in the tree. `CMainFlowDtor.cpp`'s own header comment
(point 4) says why that call site is untouchable: retail passes a **null** `CInputStream&` and
never writes r5, and no C++ spelling of "an uninitialised int" is free.
**What landed.** `src/MetroidPrime/PortStreamNewGameState.cpp`, port-only (`configure.py` does not
declare it, so no `splits.txt` range and no DOL byte moves), holding retail's 532-byte body block
by block from `powerpc-eabi-objdump` of `0x800053B8..0x800055CC`. Its header is the annotated
disassembly: every retail address, what it does, and which line here answers it. Plus
`CMain::GetGameGlobalObjects()` in `include/MetroidPrime/CMain.hpp` - retail reads that pointer as
`lwz r3,84(r28)` and the member is private, and **the offset cannot be spelled instead**, because
`CMain`+0x54 in a 32-bit GameCube object is not `CMain`+0x54 when every pointer is eight bytes
wide; the accessor is `inline` with no caller in any `configure.py` unit, so mwcceppc emits nothing.
**The three helpers, each measured rather than guessed.** `SGameStateSlots` copy, release and
assign are `fn_80004C90`/`fn_80004CD4`, `__dt__80004B9C`/`fn_80004BEC`/`fn_80004C4C` and
`fn_80142944` in retail, and all three reduce to the two primitives the port already defines:
`fn_80004C4C` is `li r4,-1; b fn_80004A4C`, `fn_80004CD4` is a `count`-iteration loop of
`fn_80004D3C` -> `fn_80004D5C` -> `fn_80004AA0`, and `fn_80142944` is `fn_80004BEC(dst)` then a
range copy-construct then `dst->x00_count = src->x00_count` - i.e. `ReleaseSlots` followed by
`CopySlots`. `fn_801427DC` is `addi r3,r3,376; b fn_80142800`, and `fn_80142800` is
`SGameStateBlock`'s `operator=` (`fn_80142914` then free-or-reserve-and-copy), so the file does
free-then-copy instead of reuse: the same bytes in the block, a fresh allocation rather than a
reused one.
**The two blocks that are not reproduced, named in the file's header rather than dropped.**
- The `SGameStateCardOpts` copy at `CGameState+0x54` and its carry-over: `fn_80005108` is
  `fn_800052A0(dst, src)` + `fn_80005158(dst+0x18, src+0x18)` + one word and
  `__dt__PersistentOptions_800050A4` destroys sub-objects at `+0x00` and `+0x18`, so the member
  owns two heap things the header's `u8 x00[0x1C]` does not model - a plain struct copy here would
  be a shallow copy of both and a double free. **The one word that local is actually read for
  survives**: retail loads the save-slot index at 0x800053E8, and that is `gpGameState->x54.x28`,
  read before the release.
- `fn_80142FEC(new)` (0x80142FEC, 0x80), only when the flag is set, unnamed and unwritten.
**Why the rest costs exactly one new symbol.** `tools/goal_check.sh` fails a `port` item whose
unique undefined count rises; resolving the target frees exactly one, and the whole budget is spent
on `fn_80144140`, retail's `CGameState` stream constructor, which the port does not define
(`CGameStateStreamCtor.cpp` is in `check_files_cmake.py`'s `EXCLUDED` list: twenty-one symbols to
close none). Everything else is defined already or written here from `fn_80004AA0`/`fn_80004A4C`.
**A NEW finding, recorded here rather than fixed: retail's `fn_80144140` takes a
`CBitStreamReader&`, not the `CInputStream&` that `CGameState.hpp:94` and
`CGameStateStreamCtor.cpp:343` both declare.** The bytes settle it - `StreamNewGameState` passes
`&r1+8`, the `CBitStreamReader` built at 0x80005498, and `fn_80144140` then calls
`ReadBits__16CBitStreamReaderFUi` on that pointer at 0x80144314 and passes it to
`__ct__12CPlayerStateFiR16CBitStreamReader` at 0x80144420. `CBitStreamReader` stores `x0_stream` at
`+0x00` and has no vtable (`__ct__16CBitStreamReaderFR12CInputStream` is four stores), so it is not
a `CInputStream` and cannot be passed as one. The new file declares it `extern "C"` with the type
retail's bytes show; the two existing declarations are untouched because both are in units this
item may not move.
`reachstub_264` was deleted from `src/MetroidPrime/PortReachStubs.cpp` in the same change: the stub
and a real definition of the same symbol collide the moment `MP_BOOT_STUBS=ON`, which is what
`tools/boot_probe.sh` passes and what `gate.sh`'s duplicate step cannot see. The file's own
"Breakdown" line claimed 317 stubs; the bodies say **298** (244 `_Z...`, 3 `REL_Load*`, 51
unmangled), so it was already stale and is now written from a count rather than from memory.
**Measured, not recalled.**
- `./tools/decomp_build.sh`: `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465
  functions)`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs against `config.yml`.
- `tools/gate.sh build/goal/judge/report.base.json`: `matched 3980 -> 3980   linked 2557 -> 2557`,
  `no regression`, every step ok - including `docs claims` after the probe count moved.
- `tools/link_check.sh`: `compile errors 0`, `unique undefined symbols 322`,
  `duplicate definitions 0`, `unchanged from baseline (322 undefined, 0 duplicates)`, and
  `StreamNewGameState__5CMainFR12CInputStreami` is no longer in `link_undefined.txt` while
  `fn_80144140` is, at line 272.
- `python3 tools/link_gap.py --rebuild`: `319 MISSING symbol(s), all accounted for`. The list was
  regenerated with `--write-list`: the target out, `fn_80144140` in, both in the unmangled group,
  so `docs/research/port_link_gap.md`'s 173/75/71 table is unchanged.
- `./tools/probe_sources.sh`: `654` files, `0 failed, 0 errors; link: LINKED (322 undefined, 0
  duplicates)`. The count moved 653 -> 654 with the new source, so every *current-state* quote of
  it in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` was bumped under
  `check_docs_claims.py`'s rule; one *historical* session-end quote was **not** rewritten - it was
  re-spelled as `` `653` files ``, the convention `RUNNING_THE_DECOMP.md` records for `` `652` files ``.
- `python3 tools/check_symbol_names.py`: `checked 322 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py`: `648 sources`, `0 on-disk sources are in no manifest`,
  `every configured DOL object is either in files.cmake or excluded with a reason`.
- `python3 tools/check_decl_order.py`: `ok: 841 unit(s) checked, 18 permuted, all 18 accounted for`.
## The pak pump drains: `fn_802FD174` erased from `list + 0x48` (2026-09-27, goal item `port-pak-pump`)
One line: **the port's own copy of `fn_802FD174` was handed `&x48_pakLoadingList` and cast it to
`CResLoader*`, so it erased from `self->x48_pakLoadingList` - `list + 0x48`, 0x90 past the start
of a 0x70 object.** `x48`'s count never moved, `AreAllPaksLoaded()` never became true, and the
`while (!AreAllPaksLoaded())` loop in `AddPaksAndFactories` block 7 kept re-moving the same
already-loaded entry. The fix is four lines inside the existing `#ifdef TARGET_PC` block of
`src/Kyoto/CResLoaderPakPump.cpp`: cast to `rstl::list< SPakLoadEntry >*` and erase from that.
Retail is unaffected - `mwcceppc` does not define `TARGET_PC`, so the matching build never sees
this block and its call still binds to retail's own `fn_802FD174`.
**Measured, not recalled.** The acceptance test is `tools/goal_verify/port-pak-pump.sh`, which
builds `tools/boot_probe.sh` and boots against the disc for up to 120 s:
- pre-fix: `verify: all 7 paks loaded but the boot never left the pump (no "Initializing
  renderer..." after it) - PAK_PUMP FAIL`, with `[pak] pump: 1000 iterations and x18+x30 is still
  1835. The list is not draining.` - 7 of 7 `phase -> kAP_Loaded` lines present, so `CPakFile`'s
  warmup chain was **not** the wall;
- post-fix: `PAK_PUMP PASS: 7/7 paks loaded, the pump drained, the boot reached the renderer`,
  and the run.log carries `Initializing renderer...` then `boot: step 21c returned -
  CCubeRenderer's constructor completed, 8 pool tokens` before the known later fault in
  `CEnvFxManager::Initialize` (past this check).
- `./tools/goal_check.sh` (the driver's invocation, `MetroidPrime2Port/tools/goal_check.sh` from
  inside the worktree): **`goal_check: PASS port-pak-pump`**, nine `ok`, exit 0 - `GATE PASS`,
  `counts: matched 3980 -> 3980   linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32%
  linked (3980 / 28465 functions)`, `1 path(s) changed under src/ or include/`, `port undefined
  322 -> 322`, `probe:` `654` files, `0 failed, 0 errors; link: LINKED (322 undefined, 0 duplicates)`.
- DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and 86/86 RELs (in `gate.sh`), plus
  `check_decl_order.py` `841 unit(s) checked, 18 permuted, all 18 accounted for`,
  `check_symbol_names.py` `0 declared names are missing`, `check_docs_claims.py` ok.
**Process note worth keeping: running `tools/goal_check.sh` by hand from the worktree fails the
item even when it is a pass.** It resolves `tools/goal_verify/` relative to its own location, so
the worktree copy - which has no `port-pak-pump.sh` - prints `verify script
tools/goal_verify/port-pak-pump.sh is missing`; the driver runs the *main repo's* script with
`cwd` set to the worktree (`run_goal.sh:378`), which finds it. The agent may not copy the script
across: it is under `tools/`, and touching that path fails the item outright.
`docs/HANDOFF.md` was corrected in place for the two stale passages this item measured: the
"seven paks reaching `kAP_Loaded` is still open" line and the "`CPakFile::Warmup` /
`CRealDvdRequest::IsComplete` gate the pak chain" line, both marked superseded.
## `fn_8029c7e8` is `CSfxManager::LoadTranslationTable`, and the port now keeps its token (2026-09-27, goal item `port-fillinassetids`)
**The symbol the port's link asks for, `CSimplePool::fn_8029c7e8(SObjectTag const&)`, is audio's
translation-table loader under the name this tree gave the address, and `./tools/dis.sh 0x8029C7E8
0x150` says so directly rather than by inference.** `r3` is used **only** as the object of one
virtual call (`lwz r12,0(r29)` / `lwz r12,12(r12)` / `bctrl`, with `r4=r29, r5=r30` - `GetObj(tag)`
at vtable slot 0xC); `r4` is tested as a **null pointer** and returns 0 when it is; `lbl_80419884`
(`.sbss`, `r13-25852`, resolved with `tools/sda.py`) is deleted through `fn_80255C00` - which frees
the buffer at `+12` and then the object, a deleting destructor - and zeroed; a fresh `CToken` is
stored into `lbl_8041988C` (`r13-25844`, 8 bytes: `x0_has` then `x4_item`), that token is `Lock()`ed,
and the function returns 1. That is `CSfxManager::LoadTranslationTable(CSimplePool* pool, const
SObjectTag* tag)` statement for statement - `../MetroidPrimePort/src/Kyoto/Audio/CSfxManager.cpp:703`,
called the same way from that tree's `main.cpp:558` with `gpSimplePool` and
`gpResourceFactory->GetResourceIdByName("sound_lookup")`. The surrounding object agrees too: a 0x4C
function in front (MP1's `TranslateSFXID` is 0x4C) and the same four between it and `CSfxManager::
PitchBend` in both trees - MP1's names for them are the `auto_ptr<CToken>` destructor, `GetRank`,
`IsHandleValid`, `IsPlaying` - and MP1's Japanese and PAL builds give the function `size:0x150`,
which is this address's size here.
**What was written: one port-only body in `src/Kyoto/CSimplePoolPort.cpp`** - `GetObj(tag)` into a
`CToken` that is *kept* and `Lock()`ed, held in a file-local `rstl::auto_ptr` named as retail's
`CSfxManager::mTranslationTableTok`. Keeping it is the whole of the work: dropping the token runs
`CObjectReference::RemoveReference` -> `CSimplePool::ObjectUnreferenced` on the spot and
`FillInAssetIDs` is a no-op. Retail's parsed `rstl::vector< short >` is **not** represented, stated
rather than faked - its only reader, `CSfxManager::TranslateSFXID`, is still undefined in the port,
so there is no table to drop and none was invented. Retail returns `true`; the declaration stays
`void` because `include/Kyoto/CSimplePool.hpp` is included by `Kyoto/CSimplePoolCtor.cpp`, a
`Matching` unit, and a `bool` there buys nothing the caller reads. `reachstub_150` came out of
`src/MetroidPrime/PortReachStubs.cpp` in the same change - the deletion `tools/boot_probe.sh`'s own
duplicate-definition branch prescribes - and the file's breakdown was recounted with its own grep:
**297 stubs** (243 Itanium, 3 `REL_Load*`, 51 unmangled), 298 before. `docs/research/
port_link_gap_list.md` was regenerated with `tools/link_gap.py --rebuild --write-list` (319 -> 318
entries, `other game methods` 173 -> 172) and `port_link_gap.md`'s table row moved with it, because
a listed symbol that is no longer missing fails the gate as stale.
**Measured, not recalled**: `./tools/probe_sources.sh` `654` files, `0 failed, 0 errors; link: LINKED
(321 undefined, 0 duplicates)`; `./tools/link_check.sh` `compile errors 0`, `unique undefined symbols
321`, `duplicate definitions 0`, with the target absent from the list it was in at the branch head
(`build/goal/judge/undef.base.count` = 322 -> 321); `./tools/gate.sh
build/goal/judge/report.base.json` `GATE PASS 1f2701c+5 changed`, `matched 3980 -> 3980   linked
2557 -> 2557`, `port link gap ok`, `docs claims ok`, `reach stubs not in a real build ok`;
`./tools/decomp_build.sh` `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`;
`sha1sum build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`python3 tools/check_symbol_names.py` `checked 322 units; 0 declared names are missing from their
object`; `python3 tools/check_docs_claims.py` `docs claims agree with the tree`.
**What this does not do.** `CMain::FillInAssetIDs` is still off the host boot ladder - `PortBoot.cpp`
runs steps 12 and 17-20 and stops - so nothing calls the new body yet, and the `sound_lookup` table
it would load needs `Strings.pak`. Both are recorded rather than papered over: the entry answers
"the pool knows the tag and cannot build it", which is the answer `PortPoolStandIns.cpp`'s
`sound_lookup_ATBL` row was written to give.
**One derived input is knowingly left stale.** `docs/research/boot_path_undefined.txt` and
`docs/research/boot_path_reachable.tsv` still name this symbol - both say "Generated by
tools/link_reach.py - do not hand-edit", and that tool was run against a fresh
`build-port-link/build.log` and its output measured (`the linker asked for: 321`,
`referenced by a REACHABLE object: 319`, `referenced only by UNREACHABLE objects: 2`, all three
files rewritten) **but not kept**: the same run also refreshes entries that went stale in earlier
landings - symbols since defined (`AllocateRenderer`, the `CARAMManager` and `CCallStack`
families) and reference moves (`main.cpp` -> `mainMid.cpp`), 59 insertions and 61 deletions across
the three files, none of them this item's. So they belong to their own change. The consequence is
recorded where it bites: `src/MetroidPrime/PortReachStubs.cpp`'s RETIRED comment says re-running
`tools/gen_link_stubs.py --reachable` puts `reachstub_150` back until those two files are
regenerated.
## `GetResourceIdByName` walks the loader first, and an owned buffer frees from the game heap (2026-09-27, goal item `port-boot-cpakfile-sresinfo-getsize`)
Two `src/` fixes moved the boot off `CPakFile::SResInfo::GetSize` (`src/Kyoto/CPakFile.cpp:94`,
the branch head's stop, reproduced twice on the clean tree) and into retail's frame loop.
`src/Kyoto/CResFactoryPortVirtuals.cpp` now does retail's body first - `fn_802FCC44`, the loader's
two-list walk - and keeps the stand-in registry as the fallback: the registry answered
`"DUMB_SnowForces"` with `kStandInIdBase + 10` = `0xF000000A`, an id no pak row carries, so
`fn_802FCDE8` returned null, `x68_curRes` stayed null and `fn_802FC63C:86` dereferenced it.
`src/Kyoto/Streams/CInputStream.cpp` frees an owned buffer with `CMemory::Free` under `TARGET_PC`
- both producers (`CResLoaderLoadNewResourceSync.cpp:93`, `DolphinCLZOInputStream.cpp:9`) allocate
with `CMemory::Alloc`, and on a host the `delete[]` spelling reaches libstdc++'s `free` and aborts
in `munmap_chunk()` at the end of `CEnvFxManager::Initialize` - while mwcceppc still compiles
`delete[]`, so `tools/flip_test.sh Kyoto/Streams/CInputStream.cpp` reports
`PASS  -> kept as Matching`. Measured: `./tools/goal_verify/boot-progress.sh` ->
`BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs` (both runs reach the frame loop
and stop at its first declared stop, `fn_801F05D0`, retail 0x801F05D0), and
`./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS` (gate ok, `matched 3980 ->
3980   linked 2557 -> 2557`, `port undefined 321 -> 321`, probe `654` files 0 failed).
## `LoadTypedefEditorProperties` reads its four properties, and its one callee came with it (2026-09-28, goal item `port-loadtypedefeditorprops`)
**New, port-only: `src/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties_Load.cpp`** - listed
in `files.cmake`, absent from `configure.py`, because retail `0x8023EF3C` (size `0x140`) is in an
unclaimed `.text` range: the nearest split blocks are `MetroidPrime/ScriptLoader/RubiksPuzzle.cpp`
ending `0x802399F4` and `MetroidPrime/ScriptLoader.cpp` starting `0x80242894`, so no unit owns
these bytes and there is nothing for `flip_test.sh` to flip. A carve would be four files in one
change and is a different job; this item is `kind: port`.
The body has the shape every generated `LoadTypedef*` has - `u16` property count, then a word id
and a `u16` size per property, then a switch - and every arm was checked against
`tools/dis.sh 0x8023EF3C 0x140` rather than assumed: `0x494E414D` `name` constructs a temporary
`rstl::string` from the stream, assigns it over `name` and destroys it (`0x8023EFDC`, which is
what `sldrThis.name = rstl::string(input);` emits), `0x5846524D` `transform` calls the helper at
`0x8023F00C`, `0x41435456` `active` reads one byte, normalises it to 0/1 with `neg`/`or`/`srwi 31`
and stores it at `+0x34` (`0x8023F014`, which is what `ReadBool()` - `ReadUint8() != 0` - does),
`0x5D298A43` `unknown_0x5d298a43` reads one word into `+0x38` (`0x8023F038`), and the default arm
is `ReadBytes(nullptr, propertySize)` (`0x8023F050`). Retail's struct is 60 bytes - `name` 0x00,
`transform` 0x10, `active` 0x34, `unknown` 0x38 - which is the header's own declaration order.
**`LoadTypedefSLdrTransform` had to be written with it.** The `transform` arm's callee is retail's
`fn_8023F8CC` (`0x8023F8CC`, `0x9C`): three `CVector3f` read from the stream into offsets 0x00,
0x0C, 0x18 - position, rotation, scale. It is unnamed in `symbols.txt` because nothing else in the
DOL calls it; scanning `.text` for `bl 0x8023F8CC` returns exactly one hit, `0x8023F00C`. Nothing
in the port referenced it either, so no stub list carried it: leaving the arm as a call to an
undefined symbol would have closed one gap and opened another (`321 -> 321`) and would have broken
`tools/boot_probe.sh`'s link, which has no reach stub to answer it. Both functions are in the one
new file.
`reachstub_194` left `src/MetroidPrime/PortReachStubs.cpp` in the same change - the deletion
`tools/boot_probe.sh`'s duplicate-definition branch prescribes - and the file's breakdown was
recounted with its own grep: **296 stubs** (242 Itanium, 3 `REL_Load*`, 51 unmangled), 297 before.
**One host conversion, spelled out rather than inherited.** Retail's helper calls
`__ct__9CVector3fFR12CInputStream` three times (`0x8023F8EC`, `0x8023F910`, `0x8023F934`), and the
port's own copy of that constructor is `in.Get(this, sizeof(CVector3f))`
(`src/Kyoto/Math/CVector3f.cpp:24`, a `Matching` unit whose body may not change) over
`CInputStream::Get`'s plain `memcpy` (`src/Kyoto/Streams/CInputStream.cpp:51`, no `TARGET_PC`
arm), so on a little-endian host the twelve bytes come back in stream order and the three floats
are not retail's value. The arm therefore reads them with `ReadFloat()` - `ReadInt32` and
`cinput_stream_read_be32` under `TARGET_PC` - which is how `CColor::CColor(CInputStream&)`
(`src/Kyoto/Graphics/DolphinCColor.cpp:6`) reads its four floats, and why the three reads are a
file-local helper: three `ReadFloat()`s in one argument list are unsequenced.
`CScriptPickup.cpp:289/292/361` uses `CVector3f(input)` and inherits that pre-existing gap in
shared code; it is deliberately not touched here.
**Measured, not recalled**: `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-loadtypedefeditorprops`, nine `ok`, exit 0 - `GATE PASS 2f37441+7 changed`, `matched 3980 ->
3980   linked 2557 -> 2557   (+0 functions at 100%, 0 units newly linked)`, `All: 8.52% fuzzy, 7.54%
matched, 5.32% linked (3980 / 28465 functions)`, `2 path(s) changed under src/ or include/`,
`LoadTypedefEditorProperties(...) was undefined at the branch head and is not now`,
`port undefined 321 -> 320`, `probe: `655` files, 0 failed, 0 errors; link: LINKED (320 undefined,
0 duplicates)`; `./tools/link_check.sh` `compile errors 0`, `unique undefined symbols 320`,
`duplicate definitions 0`; `python3 tools/link_gap.py --list` `16 c++ runtime / linker, 34
libc/libm, 134 aurora source, 0 aurora header only, 317 MISSING` with `--write-list` `wrote 317
entries in 3 groups` (318 -> 317, `REL module loaders` 71 -> 70, so `port_link_gap.md`'s table row
moved with it); `python3 tools/check_files_cmake.py` `649 sources`; `python3
tools/check_symbol_names.py` `checked 322 units; 0 declared names are missing from their object`;
`python3 tools/check_docs_claims.py` `docs claims agree with the tree`; `sha1sum
build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
**Adding a source moved the probe's file count 654 -> 655**, so every *current-state* quote of it
in `docs/HANDOFF.md` and this file was bumped under `check_docs_claims.py`'s rule, and the
historical session-end quotes were re-spelled as `` `654` files ``, the convention this file
already records for `` `653` files `` and `` `652` files ``.
**Still listing the symbol, not regenerated here:** `docs/research/boot_path_undefined.txt:202`
and `docs/research/boot_path_reachable.tsv:196` are `tools/link_reach.py`'s output and are the
generator input for `tools/gen_link_stubs.py`; re-running the generator over them puts
`reachstub_194` back, which is what the retirement comment in `PortReachStubs.cpp` says.
## Retail has one `LdrToEntityInfo`, so the port's const overload is the forwarder (2026-09-28, goal item `port-ldrtoentityinfo`)
**Which of the pair is the forwarder - the question the item was queued on - is a count, not a
judgement.** `config/G2ME01/symbols.txt` has exactly one `LdrToEntityInfo`,
`LdrToEntityInfo__FR11CEntityInfoRC20SLdrEditorProperties` at `0x80239BD4` (`0x38`), and
`objdump -d build/G2ME01/main.elf` has **91 `bl 80239bd4`** and no second function anywhere in
`.text`. Those 91 include every loader whose `info` parameter is `const CEntityInfo&` and which
binds to the *const* overload in this tree - `LoadPickup` (call at `0x800B3FA4`), `LoadHUDMemo`,
`LoadSequenceTimer`, `LoadStreamedAudio`, `LoadAreaProperties`. So retail's non-const is the
body; the const overload is the port's own (it comes from `include/MetroidPrime/CEntityInfo.hpp`,
which declares only the const one, which is why `build/goal/judge/undef.base.txt:159-160` carries
both) and it is a forwarder over a `const_cast` - the cast those four retail call sites are
already making, and which costs no instruction.
**The body is three bits, not a conversion.** `tools/dis.sh 0x80239BD4 0x38`: `props.active`
(the bool at `SLdrEditorProperties +0x34`) into the flags byte's bit 7,
`props.unknown_0x5d298a43` (`+0x38`) bit 0 into bit 6, and its bit 1 into bit 5 - three
`lbz`/`stb` read-modify-writes in source order, which is how mwcceppc emits three independent
field stores (the rule `real_loaders.md` item 5 used for `LoadAreaProperties`' stores). Bits
7/6/5 are `active`/`scriptingBlocked`/`unk` in declaration order: the constructor at `0x800484D4`
proves the layout rather than the header does - `editorId` is written with `stw r0,20(r29)`, the
flags byte is `lbz r0,24(r29)` (+0x18), and that constructor's own `bool active` argument goes to
bit 7 with the same `rlwimi ...,7,24,24`. There is no area id, no connection list and no call in
the 0x38 bytes: the name overstates the work, and anything that reads only the name will go
looking for a conversion that is not there.
**New, port-only: `src/MetroidPrime/LdrToEntityInfo.cpp`**, both overloads, listed in
`files.cmake` and absent from `configure.py` because `0x80239BD4` sits in the same unclaimed
`.text` range as the previous item's `0x8023EF3C` (nearest splits `RubiksPuzzle.cpp` ending
`0x802399F4`, `ScriptLoader.cpp` starting `0x80242894`) - no unit owns the bytes, so there is
nothing for `flip_test.sh` to flip and a carve (four files in one change) is a different job.
`CEntityInfo.hpp` gained a `friend` for the non-const - retail names no setter for those bits,
only the two constructors and the destructor - and `struct SLdrEditorProperties;` moved above the
class so the friend declaration can see it; the const overload needs no access, only
`const_cast`. `reachstub_187` and `reachstub_188` came out of `src/MetroidPrime/PortReachStubs.cpp`
in the same change, and its header breakdown was recounted with its own grep: **294 stubs**
(240 Itanium, 3 `REL_Load*`, 51 unmangled), 296 before.
**Measured, not recalled**: `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-ldrtoentityinfo`, nine `ok`, exit 0 - `GATE PASS 4d89321+8 changed`, `matched 3980 -> 3980
linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`,
`3 path(s) changed under src/ or include/`, `LdrToEntityInfo(...) was undefined at the branch head
and is not now`, `port undefined 320 -> 318`, `probe: `656` files, 0 failed, 0 errors; link: LINKED
(318 undefined, 0 duplicates)`; `./tools/link_check.sh` `compile errors 0`, `unique undefined
symbols 318`, `duplicate definitions 0`; `powerpc-eabi-nm` on the new object lists both
`_Z15LdrToEntityInfoR11CEntityInfoRK20SLdrEditorProperties` and
`_Z15LdrToEntityInfoRK11CEntityInfoRK20SLdrEditorProperties` as `T`; `python3 tools/link_gap.py`
`measured over 650 object(s)`, `16 c++ runtime / 34 libc / 134 aurora source / 0 header only /
315 MISSING`, `ok: 315 MISSING symbol(s), all accounted for`, `--write-list` `wrote 315 entries in
3 groups` (317 -> 315, `other game methods` 172 -> 170, so `port_link_gap.md`'s table row moved
with it); `python3 tools/check_files_cmake.py` `650 sources`; `python3 tools/check_symbol_names.py`
`checked 322 units; 0 declared names are missing from their object`; `python3
tools/check_docs_claims.py` `docs claims agree with the tree`; `sha1sum build/G2ME01/main.dol`
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
**The boot probe linked with both stubs out, and asked for the body instead.**
`./tools/boot_probe.sh` -> `relink status 0`, `linked 93119976 bytes`, `the log names 0
unresolved symbols` (the `fn_80270xxx` `[auto-stub]` lines are unnamed retail functions that
are in `link_gap.py`'s MISSING list before and after this item), **no** `multiple definition of` anywhere - the one
failure the boot probe can see that `gate.sh`'s duplicate step cannot, because that step links
without `MP_BOOT_STUBS=ON` - and **zero** `[reach-stub ... LdrToEntityInfo ...]` lines in the run
log, so the nine objects resolve to the new file rather than to a stub that logs and returns. The
boot itself went where the branch's last measured boot went: `boot: step 21 - the frame loop`,
`frame: 1`, `frame loop stopped: fn_801F05D0(lbl_80418EC8) (retail 0x801F05D0, 0xF8) is not
written`. This item does not move that wall - it is a script-loader helper, not one of the frame
loop's callees.
**Adding a source moved the probe's file count 655 -> 656**, so the six *current-state* quotes of
it in `docs/HANDOFF.md` and this file were bumped under `check_docs_claims.py`'s rule, and the
two historical session-end quotes were re-spelled as `` `655` files ``, the convention this file
already records for `` `654` files ``.
**Still listing the two symbols, deliberately not regenerated here.** `tools/link_reach.py` was
run and measured - `the linker asked for: 318`, `referenced by a REACHABLE object: 316`,
`referenced only by UNREACHABLE objects: 2` - but it rewrites `docs/research/boot_path_undefined.txt`,
`boot_path_reachable.tsv` and `boot_path_stubbable.tsv` with a 54/66-line diff that also drops
symbols this item never touched (`AllocateRenderer`, `CARAMManager::*`, `CGraphics::*`,
`CCallStack::*`, all closed by earlier items - i.e. those two files were already stale before
this one), so the writes were reverted rather than folded into an item about `LdrToEntityInfo`.
They still list both symbols (`boot_path_undefined.txt:195-196`), so re-running
`tools/gen_link_stubs.py --reachable` here puts `reachstub_187` and `reachstub_188` back, which is
what the retirement comment in `PortReachStubs.cpp` says.
## The empty destructor is retail's, and what it must free is four members (2026-09-28, goal item `port-modeldata-dtor`)
**The item's `reason` named the trap - "a destructor which does nothing is a plausible lie - record
what the real one must free" - and the record is the argument that the empty body is right.**
Retail `__dt__10CModelDataFv`, `0x800E6810`, `0xF0` = 240 bytes (the next symbol is `fn_800E6900`,
so 0xF0 is retail's own size), and `tools/dis.sh 0x800E6810 0xF0` spends every instruction on a
member: four guarded blocks in reverse declaration order, then the deleting tail.
| retail | member | what the call frees |
| --- | --- | --- |
| `lbz r0,72(r30)` … `li r4,0; bl __dt__6CTokenFv` @`0x800E6830` | `x3c_infraModel` (+0x3C) | `optional_object` tests `m_valid`, destroys `TLockedToken<CModel>` → `CToken::~CToken()`, the unlock + `RemoveRef` on the `CObjectReference` |
| `lbz r0,56(r30)` … `bl __dt__6CTokenFv` @`0x800E6864` | `x2c_xrayModel` (+0x2C) | same |
| `lbz r0,40(r30)` … `bl __dt__6CTokenFv` @`0x800E6890` | `x1c_normalModel` (+0x1C) | same |
| `lbz r0,12(r30)`; `lwz r3,16(r30)`; `li r4,1`; `bl fn_8002C340` @`0x800E68BC` | `xc_animData` (+0x0C) | `auto_ptr` tests `x0_has` and `delete`s → **`fn_8002C340` is `CAnimData::~CAnimData`**, identified member by member from its own call sites (the ladder at the bottom of `include/MetroidPrime/CAnimData.hpp`) |
| `extsh. r0,r31`; `ble`; `bl Free__7CMemoryFPCv` @`0x800E68D4` | `this`, deleting flag | `operator delete` → `CMemory::Free` (`CMemory.hpp:46`) |
No flag is written back after any of the four calls, and `x0_scale`, `x14_flags` and
`x18_ambientColor` have trivial destructors - so there is no statement in those 240 bytes that a
body would have produced. An empty body over these members *is* that code, which is why
`src/MetroidPrime/CModelDataDtor.cpp` is `{}` and not a stand-in. The lie to avoid was the other
one: `CModelData.hpp` only forward-declares `CAnimData`, and `delete` on an incomplete type still
compiles while dropping the destructor call, so the file includes `MetroidPrime/CAnimData.hpp` to
make retail's `bl` at `0x800E68D0` come out of the port.
**What that costs, measured rather than assumed.** The include means the port's undefined list
gains `CAnimData::~CAnimData()` - nothing in the tree defines it; `CAnimData.hpp:43` is the only
declaration - in exchange for losing `CModelData::~CModelData()`. `./tools/link_check.sh` ->
`compile errors 0`, `unique undefined symbols 318`, `duplicate definitions 0`, and the undefined
list diff against the branch head is **exactly those two lines**, so the count holds:
`port undefined 318 -> 318`. `--strict` -> `STRICT PASS - ... 318 undefined against a baseline of
322 (no growth)`. The dependency is retail's own, so it is queued rather than papered over: a
`NEW: port-animdata-dtor | port | CAnimData::~CModelData()` line is in
`build/goal/notes/port-modeldata-dtor.md`, with the ladder of what its 0x2F8 bytes must free.
**One reach stub had to come out with it, and one had to go in - both for the same tool.**
`reachstub_92` aliased `_ZN10CModelDataD1Ev`; `PortReachStubs.cpp` is linked only under
`-DMP_BOOT_STUBS=ON`, which only `tools/boot_probe.sh` passes, so the real definition and the stub
would collide there while `gate.sh`'s `port link dups` step - which links without the option -
reports `duplicate definitions 0` either way. Retired with a `RETIRED 2026-09-28` comment.
**And `./tools/boot_probe.sh` then failed, which is the finding worth the lines.** The first link
named 5 unresolved symbols; the script's own pass stubs the ones that are legal C identifiers
(four `fn_`/`lbl_` names) and prints `not declarable as C identifiers (left for a human):
CAnimData::~CAnimData()` for the fifth - ld prints the *demangled* name and a C++ destructor is not
an identifier - so the relink could not fix it: `relink status 1`, `undefined reference to
'CAnimData::~CAnimData()'` from `rstl/auto_ptr.hpp:21`, `BUILD FAILED`. **A link that does not
finish is the one failure mode where the probe reports no symbol at all**, so it had to be closed
rather than noted: `reachstub_318` is hand-added for `_ZN9CAnimDataD1Ev`, with why in its own
comment. Re-run after that - `link named 4 unresolved symbol(s), 4 not stubbed`, `relink status 0`,
`linked 93182792 bytes`, `the log names 0 unresolved symbols`, **zero** `multiple definition of`
lines (the retirement, measured), then `boot: step 21 - the frame loop`, `frame: 1`,
`frame loop stopped: fn_801F05D0(lbl_80418EC8) (retail 0x801F05D0, 0xF8) is not written - frame
1`: the same wall as the branch's last measured boot, so this item did not move it. The header's
own recount went with it - measured with its own grep, **294 stubs** - 240 Itanium, 3
`REL_Load*`, 51 unmangled: 293 after the retirement, 294 once the callee's stub went in, the same
total with a different Itanium symbol in it.
**Measured, not recalled**: `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-modeldata-dtor`, nine `ok`, exit 0 - `GATE PASS 1fa2358+6 changed`, `matched 3980 -> 3980
linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`,
`2 path(s) changed under src/ or include/`, `CModelData::~CModelData() was undefined at the branch
head and is not now`, `port undefined 318 -> 318`, `probe: `657` files, 0 failed, 0 errors; link:
LINKED (318 undefined, 0 duplicates)`; `python3 tools/link_gap.py --write-list` `wrote 315 entries
in 3 groups` with a two-line diff (`- _ZN10CModelDataD1Ev`, `+ _ZN9CAnimDataD1Ev`) and the recheck
`ok: 315 MISSING symbol(s), all accounted for` - both symbols are `other game methods`, 170 in and
170 out, so `port_link_gap.md`'s group table did not move; `check_files_cmake.py` `651 sources`,
`0 on-disk sources are in no manifest at all (dead)`; `check_symbol_names.py` `checked 322 units; 0
declared names are missing`; `check_decl_order.py` `ok: 841 unit(s) checked`;
`check_raw_offsets.py` `ok: 108 raw-offset site(s)`; `sha1sum build/G2ME01/main.dol`
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
**Adding a source moved the probe's file count 656 -> 657**, so the six *current-state* quotes of
it in `docs/HANDOFF.md` and this file were bumped under `check_docs_claims.py`'s rule (it failed
first, on its own `stale:` line, naming `656` against the probe's `657`), and the two historical
session-end quotes were re-spelled as `` `656` files ``, the convention this file already records for
`` `655` files `` and `` `654` files ``.
**No `configure.py` claim, and the research files were not regenerated.** There is no unit for
this function: no split covers `0x800E6810`, so claiming it would be a carve of a range that
belongs to nobody, and the DOL build never sees this file. `docs/research/boot_path_undefined.txt`
(line 100) and `boot_path_reachable.tsv` (line 94) still list the symbol for the same reason the
`LdrToEntityInfo` pass left its two: they are `tools/link_reach.py`'s output, and regenerating
them rewrites three research files with a diff that also drops symbols this item never touched.
Re-running `tools/gen_link_stubs.py --reachable` over them therefore puts `reachstub_92` back,
which is what its retirement comment says.
## `IterateSearch` is a two-instruction FP-allocation wall, and Metroid Prime 1 has it too (2026-09-28, goal item `match-cpvsvisoctree`)
**Not promoted; the unit stays `NonMatching` at 99.921875 % (2 functions, 1 matched), and this is
the second run on it.** `IterateSearch` (492 B, `99.9187 %`) is instruction-for-instruction retail
except at unit offset `0x78` / `0x80`: retail emits `fadds f1,f1,f8` / `stfs f1,16(r1)`, we emit
`fadds f0,f1,f8` / `stfs f0,16(r1)`. `GetNumChildren` is 100 %. The first run proved the bound by
flipping the unit to `Matching` and byte-diffing the DOL: **two differing bytes, both that register
field, nothing else in any section.**
**What this run added (the first run's ~40 source variants, 20 compilers and 26 flag sets were not
repeated):** 26 new source variants, ~45 new flag configurations, a version x flag-set matrix done
with each compiler's own flags, 3 line-number perturbations and 1 build-wrapper experiment. Every
one of them scores 2 differing instructions or worse. Two negatives worth keeping: **dead locals do
not perturb MWCC's allocator** (five different unused locals, all still 2 - they are dropped before
the deciding pass), and **line numbers do not matter either**, so `#line`/whitespace nudges are not
a lever. Anything that restates the `center` computation while keeping the 32-byte frame scores far
worse (24-130), which closes the one search the first note left open.
**The version matrix now pairs each compiler with the flags this tree actually uses for it**, which
the first run did not: `configure.py` builds the Dolphin libs with `GC/1.2.5n` and the RELs with
`GC/1.3.2`, so "1.0-1.2.5 fail on flags, and no unit uses them" was wrong on its second half. With
`cflags_dolphin` those five compile - and produce **157 instructions**, a different code-generation
generation, not a near miss. `1.3 1.3.2 1.3.2r 2.0 2.0p1 2.5 2.6 2.7` x `cflags_retro` all give
exactly 2; `cflags_dolphin` gives 24 and `cflags_rel` 118, so `cflags_retro` is the best of the
three and the tie-break has not moved across eight compiler versions.
**Independent confirmation, and the reason this is filed as a wall rather than unfinished work:**
PrimeDecomp's Metroid Prime 1 tree hit the same place - PR `PrimeDecomp/prime#383`, *"Implement
octree search with two floating-point register differences remaining"*, reports
`IterateSearch 1.14 % -> 99.92 %` and `.sdata2 0.00 % -> 66.67 %` on GM8E01_00/01 and GM8P01_00,
their `configure.py` still has `Object(NonMatching, "Kyoto/PVS/CPVSVisOctree.cpp")`, and their
`CPVSVisOctree.cpp`/`CPVSVisOctree.hpp` are byte-identical to ours (fetched 2026-09-28). Two
projects, two games, the same source, the same two floating-point registers.
Full measurements, the per-variant table and the exact commands are in
`build/goal/notes/match-cpvsvisoctree.md`. `NEW:` lines: none - nothing outside this item was
found broken.
## The frame loop's DMA cleanup is written, so its stop moved one callee along (2026-09-28, goal item `port-boot-cmain-rsmain-0eb92a1`)
**What was written:** retail's `fn_8030172C` (0x8030172C, 0x20) - the call `CMain::RsMain`'s frame
loop makes at 0x800060A8, and the DOL's only caller of it - plus the `fn_8030174C` (0x74) it wraps.
`./tools/dis.sh 0x8030172C 0x20` is `stwu r1,-16; mflr r0; stw r0,20(r1); bl 8030174c; lwz/mtlr/
addi/blr`: no argument of its own, so both are `extern "C"` with no parameters.
`./tools/dis.sh 0x8030174C 0x74` is the walk of `lbl_804175B8` (0x18-byte .bss = one
`rstl::list`, `sActiveDMAs` in the port) - for each node, `r3 = *(node+8)` is the request and
`lbz r0,36(r3)` its `+0x24` byte; if it is set, `CMemory::Free(r3)` frees the request and
`fn_8030215C(&list, node)` relinks, `x14_count--`, frees the node and hands back the next. That
second call is `rstl::list::do_erase` (include/rstl/list.hpp:284, read against retail's own
instructions), so the port writes it as `delete *it; it = sActiveDMAs->erase(it);` - free before
unlink, which is also what `IsDMACompleted` and `WaitForDMACompletion` in the same file do.
Both bodies went into `src/Kyoto/CARAMManagerPort.cpp`, a port-only file, and
`src/MetroidPrime/PortBoot.cpp`'s stop became `fn_8030172C();`. **There is no carve:** retail
0x8030172C..0x8030184C is still unclaimed in `config/G2ME01/splits.txt`, so nothing was added to
`configure.py` or `splits.txt`, and `files.cmake` already listed the file.
**The one host line, and why it is not a deviation from retail:** the pass calls `ARQPoll()` first.
On the cube the `+0x24` byte is written by the ARQ interrupt, which needs nobody's help; on the
host Aurora does the copy at post time and *defers* that callback to `ARQPoll`
(`CARAMManagerPort.cpp`'s own header), so a pass that did not poll would sweep a list whose
completion bytes are never written and could never free anything. `IsDMACompleted`, `CancelDMA`
and `WaitForDMACompletion` in that file each poll first for the same reason, and `ARQInit` has run
by then: `fn_80301CC4`, which `rs_new`s the list the pass starts by testing, is called after it in
`PortInitializeSubsystems`. `CARAMManager::WaitForAllDMAsToComplete` - retail's `fn_8030184C` -
now *calls* the pass instead of carrying a second copy of the walk, which is the relation its own
comment already described: one poll and one sweep per iteration, exactly the sequence it had.
**Measured:** `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-boot-cmain-rsmain-0eb92a1`, `GATE PASS 0eb92a1+2 changed`, `matched 3980 -> 3980 linked 2557
-> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`, `port undefined
318 -> 318`, `probe: 658 translation units, 0 failed, 0 errors; link: LINKED (318 undefined, 0 duplicates)` (that run's own count; the source of truth is `./tools/probe_sources.sh`),
`verify boot-progress.sh: BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs`. The
undefined-symbol list is line-for-line the baseline's - the two new bodies add definitions, and the
one call they introduce is satisfied by the other one. The boot now stops at the loop's next
declared stop, `fn_80006954` (0x58, called at 0x80006114 and 0x80006234), at
`src/MetroidPrime/PortBoot.cpp:410` where the head stopped at `:398`; DOL sha1 unchanged at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `check_symbol_names.py` 0 missing.
**Next wall:** `fn_80006954(&total, &x18_frameTimeHistory)`, retail 0x80006954, 0x58, declared
twice (0x80006114 and 0x80006234). Its body is measured in `docs/research/boot_path.md` row 10 -
a valid flag at `total+4`, and `fn_80008B60(h->v, h->count)`'s unrolled `fadds` sum over the
history at `CMain`+0x18/+0x2C stored at `total+0` - and while `fn_80006954` itself has no body in
this tree, `fn_80008B60` (0x80008B60, 0xC8) does not either. **Superseded 2026-09-28: that
accumulator's return is the **mean**, `sum * (1.0f / count)`, not the bare sum** - both are written
now, in `src/MetroidPrime/PortFrameTimeHistory.c`.
## `CSfxManager::TranslateSFXID` reads retail's table and answers with it, 0xFFFF (2026-09-28, goal item `port-translatesfxid`)
**The symbol is `fn_8029C79C`, 0x4C bytes, and it was found by its size and its neighbour rather
than by name.** `./tools/dis.sh 0x8029C79C 0x4C` is a load of `-25852(r13)`, a null test, a
`lwz 0x4(r4)` count, a `cmpw` against the zero-extended id, a `lwz 0xC(r4)` buffer, a
`lhax` halfword load and a `clrlwi 16` - and `fn_8029c7e8`, the 0x150-byte function the previous
item established is `CSfxManager::LoadTranslationTable`, starts at exactly 0x8029C79C + 0x4C.
MP1's `TranslateSFXID` is also 0x4C, and `../MetroidPrimePort/src/Kyoto/Audio/
CSfxManager.cpp:716` is statement for statement the body written below.
**The table is retail's own, at retail's own address, and the pointer is retail's field.** `-25852`
is `lbl_80419884` in `.sbss` (`python3 tools/sda.py -25852` -> `0x80419884 lbl_80419884 (in .sbss,
+0x0)`), and the function reads it as `count = *(int*)(p+4)`, `items = *(short**)(p+12)` -
**this tree's `rstl::vector` layout** (`x4_count` at +4, `xc_items` at +12,
`include/rstl/vector.hpp:18-21`), so the declaration is `rstl::vector< short >*` and not a
shape-compatible guess. `fn_8029C7E8` drops it before every load (`li r4,1; bl fn_80255C00` at
`+0x40`, `stw r0,-25852(r13)` at `+0x4C`), and that statement is now in the port's
`fn_8029c7e8` too, so the pointer the reader tests has the lifecycle retail gives it.
**What changed for the boot is the failure mode, and it is the dangerous kind.** The reach stub
returned 0 in `r3`, and 0 is a *valid* runtime sound id, so every sound the game asked for by
per-area id would have become a real-looking wrong sound. The body returns
`kInternalInvalidSfxId` (0xFFFF), which is **retail's own answer for a missing table** - the
first two statements of the body - so the port now answers the same thing retail does on a disc
where `LoadTranslationTable` was never reached.
**No table is built, and that is stated rather than faked.** The bytes are the
`sound_lookup_ATBL` resource in `Strings.pak`, and `Strings.pak` is **not on the ISO** -
`docs/HANDOFF.md` records the measurement (20 `.pak`s, none named that, so retail's own
`CDvdFile::FileExists` probe at 0x800071A8 fails as well). The pool keeps its token over a null
object (`src/Kyoto/CSimplePoolPort.cpp`), and `fn_8029AB80`, the 0x68-byte `ATBL` factory
(`li r3,0x10` / `__nw__FUlPCcPCc` / a `rstl::vector< short >` off the stream), is still
`return CFactoryFnReturn()` in `src/Kyoto/CFactoryFunctionsPort.cpp` because there is no stream
to hand it. **Writing a mapping here would be fabricating the game's sound table, and a
plausible-looking fabricated id is the failure mode `PortPoolStandIns.cpp` calls the most
dangerous possible wrong answer**, so the vector stays null.
**Files.** `include/Kyoto/Audio/CSfxManagerPort.hpp` is new and port-only - the one accessor the
loader needs, so the loader in `src/Kyoto/CSimplePoolPort.cpp` and the reader in
`src/MetroidPrime/PortAudio.cpp` reach one object. It is a separate header because
`include/Kyoto/Audio/CSfxManager.hpp` is included by `Kyoto/CSimplePoolCtor.cpp`, a `Matching`
unit, and adding a member there would be a change to a matching object. `src/MetroidPrime/
PortAudio.cpp` gained the `TranslateSFXID` body, `port::sfx::ClearTranslationTable()` and the
`kInternalInvalidSfxId` definition (0xFFFF, derived in `src/MetroidPrime/PortGlobals.cpp`'s
comment on `kMedPriority` from `.sdata2` 0x8041E2E6); it was put there rather than in a file of
its own because the file is already the port's audio bodies and `files.cmake` already lists it, so
**no manifest moved**. `src/Kyoto/CSimplePoolPort.cpp` gained one call and the paragraph that said
the reader was still undefined. `reachstub_148` came out of `src/MetroidPrime/PortReachStubs.cpp`
- the deletion `tools/boot_probe.sh`'s own duplicate-definition branch prescribes - and the file's
breakdown was recounted with its own grep: **293 stubs** (239 Itanium, 3 `REL_Load*`, 51
unmangled), 294 before. `docs/research/port_link_gap_list.md` was regenerated with
`tools/link_gap.py --rebuild --write-list` (314 entries in 3 groups, `other game methods`
**170 -> 169**) and `port_link_gap.md`'s table row moved with it.
**Measured, not recalled:** `./tools/probe_sources.sh` -> `probe: 658 translation units, 0 failed,
0 errors;
link: LINKED (317 undefined, 0 duplicates)` (that run's own count; the source of truth is
`./tools/probe_sources.sh`); `./tools/link_check.sh` -> `compile errors 0`,
`unique undefined symbols 317`, `duplicate definitions 0`, and the target is absent from
`build-port-link/link_undefined.txt` where the judge's recorded base had it - that base was
**318** (`build/goal/judge/undef.base.count`, the driver's, not the agent's) and the tree now
measures **317**, with no symbol added to the gap; `./tools/decomp_build.sh` -> `All: 8.52%
fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)`, unchanged; `sha1sum
build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `python3
tools/check_symbol_names.py` -> `checked 322 units; 0 declared names are missing from their
object`; `check_decl_order.py` -> `841 unit(s) checked, 18 permuted, all 18 accounted for`;
`check_files_cmake.py` -> every configured DOL object is either listed or excluded;
`check_boot_stubs.py` ok; `python3 tools/check_docs_claims.py` -> `docs claims agree with the
tree`. **`docs/HANDOFF.md`'s state block moved with it**: it said `port link 322 undefined`, which
was already stale (the tree measured 318 before this change), and it now says **317** and says
that `docs/research/port_link_baseline.txt` is the recorded *floor* at 322 rather than a mirror -
`link_check.sh --strict` fails only on growth, so a tree below 322 is a win. That file is the
judge's and is not touched.
**What this does not do.** Nothing calls the new body yet: `CActor::ProcessSoundEvent` is reached
only once an actor is spawned, and `CMain::FillInAssetIDs` - the one caller of the loader - is
still off the host boot ladder (`PortBoot.cpp` runs steps 12 and 17-20 and stops). The boot path
is therefore unchanged and no `verify` script was needed for this item. What would make the table
real is the loader's missing half, not this function: `AddPaksAndFactories` (boot step 13) and
`AsyncIdlePakLoading` filling `CResLoader`'s lists, plus a `Strings.pak` on the disc, plus
`fn_8029AB80` written to build the vector off the resource stream.
## The frame-time pair is written, so the loop's stop moved on to `fn_80049244` (2026-09-28, goal item `port-boot-frame0-fn80049244`)
The head at `23075fb` stopped on the first of two `PORT_FRAME_STOP`s, at
`src/MetroidPrime/PortBoot.cpp:410`: `fn_80006954(&total, &x18_frameTimeHistory)`. Both are written
now, in **`src/MetroidPrime/PortFrameTimeHistory.c`** - a `.c` file in `files.cmake`, port-only,
claiming nothing in the DOL - and both stops are replaced by retail's call on retail's line.
**Why `files.cmake` and not a carve.** Neither address is in an unclaimed gap, and they are not in
the *same* claim: `config/G2ME01/splits.txt` gives 0x80006954 to `MetroidPrime/main.cpp`
(`.text` 0x800053B8-0x80006B38) and 0x80008B60 to `MetroidPrime/mainTail.cpp`
(`.text` 0x80008680-0x80009880), which the objects confirm -
`build/binutils/powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/mainTail.o | grep 80008B60` ->
`000004e0 T fn_80008B60`, and `main.o` carries it as `U`. A carve here is therefore **two** claim
cuts, each in another lane's file (`mainsplit` for the first, mainTail's own cut for the second),
so this is the `PortGlobals.cpp`/`PortModuleManager.cpp` shape instead: host link only, and delete
it when a cut lands (as `PortModuleManager.cpp` was, 2026-10-01, when upstream's `CRelFile.cpp` took its place). `src/` **or** `include/` is satisfied by `PortBoot.cpp` and by the new file.
**Measured against retail, per function, not as a percentage** (flags of `tools/probe_cc.sh` plus
`-lang=c`, the language the `.c` rule gets):
    fn_80008B60  0x80008B60  0xC8  retail 50 insn / 200 B   ours 50 / 200
                 7 lines differ, every one a relocation: 5 branch displacements and the two
                 constant loads `lfd f1,-32664(r2)` / `lfs f2,-32740(r2)`
    fn_80006954  0x80006954  0x58  retail 22 insn /  88 B   ours 22 /  88
                 4 lines differ: the `bl` displacement (a relocation) and `stfs/li/stb` against
                 retail's `li/stb/stfs` - the flag store after the value store, which is why
                 `out->value = ...` is written before `out->valid = 1`
**`fn_80008B60` is a mean and the constant is not `200.0`.** The tail reads
`xoris r3,r4,32768 ; lis r0,17200 ; lfd f0,8(r1) ; lfd f1,-32664(r2) ; fsubs f0,f0,f1 ;
fdivs f0,f2,f0 ; fmuls f1,f3,f0`, and `0x8041A428` is `43300000 80000000` =
**2^52 + 2^31 = 4503601774854144.0** (`objdump -s -j .sdata2 build/G2ME01/main.elf`), not an
int-to-double encoding of a small integer: the pair is mwcc's integer-to-double bias, and the
`fsubs` cancels it and leaves exactly `count`. So the body is `sum * (1.0f / count)` - 50
instructions in 200 bytes, byte-exact bar relocations. **Written as
`sum * (1.0f / ((float)count - 200.0f))` it is 58 instructions in 232 bytes**, because mwcceppc
then emits *two* subtractions of 200 (one against the double, one against the float). **So what this
supersedes is the "sum" reading, not a "200.0" one:** the passages that call `CMain`+0x40/+0x44 each
history's *sum* are `:3237` (the layout table row), `:3241` ("sums, not a running minimum"), `:4117`,
`docs/research/boot_path.md:140` and `docs/HANDOFF.md:2679`/`:2699` - each annotated in place where
it stands. `_SDA2_BASE_` is 0x804223C0 (`tools/sda.py`), confirmed twice: `-32740` ->
0x8041A3DC = `3F800000` = 1.0f, and in `fn_800597D8` `-31336` -> 0x8041A958 = `3F4CCCCD` = 0.8f.
The sum is a **pointer walk**; the same sum as an index loop is 58 instructions in 232 bytes.
**The store, and the one place this is not retail's:** `CMain::RsMain`'s 8-byte local is
`{ 0.0f, 0 }`-initialised here. `fn_80006954` returns early on `count == 0` without writing +0,
so retail stores an uninitialised word to `CMain`+0x40; that path is unreachable from the loop
(`fn_800069AC` at 0x80006108 runs first and raises the count, and `CMain` is placement-new'd into
`mainTail.cpp`'s `static uchar sMainSpace[]`, so the count starts at 0 and is >= 1 by then), and
reading an uninitialised local is undefined behaviour a host compiler may act on.
**Measured, not recalled, on the final tree:**
    $ ./tools/goal_check.sh build/goal/item.json
    goal_check: PASS port-boot-frame0-fn80049244
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 3980 -> 3980   linked 2557 -> 2557
      ok    check_symbol_names.py
      ok    All:  8.52% fuzzy, 7.54% matched, 5.32% linked (3980 / 28465 functions)
      ok    2 path(s) changed under src/ or include/
      ok    verify boot-progress.sh: BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs
      ok    port undefined 317 -> 317
      ok    probe: probe: (then 659 source) files, 0 failed, 0 errors; link: LINKED (317 undefined, 0 duplicates)
The judge passes by the documented declared-stop rule (`boot_progress.py`: the head stopped on a
`PORT_FRAME_STOP` and the candidate's stack goes through that rewritten line into a deeper frame).
The new stop is the first frame's draw, SIGSEGV, `fn_80049244` at `Carve80049244.cpp:143`,
called from `CMain::RsMain` at `PortBoot.cpp:459` (the marker line is spelled out in
`build/goal/notes/port-boot-frame0-fn80049244.md`, not here: `boot-progress.sh` fails a diff that
writes one). **The port probe's file count moved 658 -> 659** with the new
`.c`, so every "N files" claim in these docs moved with it; the four historical transcripts keep
their own figure, reworded so they no longer read as the current count.
**The `fn_80049244` fault is not this item and is not fixed.** Characterised in
`build/goal/notes/port-boot-frame0-fn80049244.md`: the draw list holds four IOWins and the two
whose constructors are not written (`CConsoleOutputWindow`, `CAudioStateWin`) carry garbage
vtable pointers, so the walk dispatches through them. Adding the two `configure.py` units to
`files.cmake` was measured and **rejected**: it takes the port's undefined count 317 -> 327, and
it would not fix the fault anyway, because both bodies store a **retail PowerPC vtable address**
(`lbl_803B37F0`, `lbl_803B3950`) as the object's vptr, which no host process can call.
## A loader can be 100% and still not link: `LoadTimeKeyframe` is `Matching` (2026-09-28, goal item `match-cunknown90`)
**`MetroidPrime/ScriptObjects/CUnknown90.cpp` is `Matching` at 100%** (320/320 `.text`,
8/8 `.rodata`), confirmed by `tools/flip_test.sh MetroidPrime/ScriptObjects/CUnknown90.cpp`:
DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs cmp-equal, and the `All:` line
went 8098 -> 8099 / 28465 functions. Two independent defects were standing behind the 99.75%
the previous run reported, and the first one is the one to generalise.
**1. The register wall was one redundant conversion, and the "twenty spellings" were the wrong
axis.** mwcceppc gives the `operator new` result the register the hoisted `0x44335aff` case
constant occupied (r29, dead by then); retail's is r28, the property count's. One extra
`u16`-typed node on the `input.ReadUint16()` read moves it: `static_cast< u16 >`, `+x`,
`x & 0xffff` and `u16(u16(x))` all reproduce retail byte for byte. What does *not*, measured:
the same node typed `u32`/`uint`/`long`; the same node on `propertySize` or `propertyId`; a
cast on the loop bound rather than the read; 192 combinations of the other spellings; and 26
flag sets (`-O4,s`, `-O3`, three `-inline` settings, `-fp_contract off`, `cats on`,
`common off`, two `inline_max_size` values, `peephole`/`schedule`/`unroll`/`extbug` off).
So the lever is *one same-typed node on this one read*, and no flag is involved. The
mechanism generalises - `LoadRelay` picks the FourCC register exactly as retail does, so
"the `new` result takes the hoisted constant's register" is MW's rule and retail's
`LoadTimeKeyframe` is the one that breaks it.
**2. At 100% bytes the unit still did not link, and this is the part worth carrying.** The
object referenced `__ct__16SLdrTimeKeyframeFv` / `__dt__16SLdrTimeKeyframeFv`; retail
references `__ct__20SLdrEditorPropertiesFv` / `__dt__20SLdrEditorPropertiesFv`. Both are `bl`
sites, and until the link resolves them both are the placeholder word `48 00 00 01`, so
**objdiff reported 100.00% and `tools/unit_fit.sh` reported "no extra functions" while the
link failed** with two `undefined:` lines. `include/MetroidPrime/ScriptLoader/SLdrTimeKeyframe.hpp`
now declares the pair under `#ifdef TARGET_PC`, the same treatment `SLdrRelay` already had,
and `SLdrStructMembers.cpp` still defines them for the port.
That is `docs/PROCESS_LESSONS.md`'s "three green checks agreeing on a broken change" in a new
costume, and the only gate that caught it was the one the project already insisted on. The
general form: **for a unit that contains a call, the relocation target is part of its
correctness and no byte comparison can see it.** Any `SLdr*` aggregate a `Matching` loader
instantiates on the stack needs the same check, and `LoadTypedefSLdrRelay` /
`LoadTypedefSLdrAreaAttributes` are the next candidates. What remains from `unit_fit.sh` is
the 84-byte weak COMDAT `__dt__16SLdrTimeKeyframeFv` the aggregate's scope exit now emits; the
flip confirms mwldeppc drops it.
## mwcceppc hoists inline-helper arguments in reverse call order (2026-09-28, goal item `match-cvectorelement`)
`Kyoto/Particles/CVectorElement` was **91 / 92** functions with one at 99.90%:
`CVEKEYF::GetValue(int, CVector3f&) const`, 476 bytes. The diff was **two instructions swapped and
nothing else** - no register-allocation difference, no operand difference:
```
retail  lwz r4,0x14(r30)   ; mLoopStart      ours (before): lwz r6,0x10(r30)  ; mLoopEnd
        lwz r6,0x10(r30)   ; mLoopEnd                 lwz r4,0x14(r30)  ; mLoopStart
```
Both callers inline the same
`static inline int GetKeyframeIndex(int frame, bool loop, int loopStart, int loopEnd)`, and
mwcceppc **emits the hoisted member loads in the reverse of the order the arguments are written at
the call**. So swapping the last two *written* arguments swaps the two loads and changes nothing
else - the same values still reach the same parameters, because the parameter *names* follow their
positions, not their spelling order at the call.
**Retail is not consistent between the two paths in the same TU.** Measured off retail's `.text`,
the asymmetry holds in all four particle element TUs:
| unit | `*KEYF::GetValue` hoists | `*KeyframeEmitter::GetValue` hoists |
|---|---|---|
| `CIntElement` | `0x14` (start), `0x10` (end) | `0x10`, `0x14` |
| `CRealElement` | `0x14`, `0x10` | `0x10`, `0x14` |
| `CColorElement` | `0x14`, `0x10` | `0x10`, `0x14` |
| `CVectorElement` | `0x14`, `0x10` | `0x10`, `0x14` |
The KEYF path wants the range **end-first**, so no single argument order serves both callers and
each TU needs its own spelling at one call site. The fix is a 9-line wrapper that takes the range
end-first and forwards to the original helper, used only by the KEYF path:
```cpp
static inline int GetKeyframeIndexEndFirst(int frame, bool loop, int loopEnd, int loopStart) {
  return GetKeyframeIndex(frame, loop, loopStart, loopEnd);
}
```
**`CRealElement.cpp` took the identical one-line change and is `Matching` 151 / 151** (goal item
`match-crealelement`, 2026-09-28, `flip_test` PASS) - same wrapper, same call-site transposition,
nothing else, measured the same way: `bytescmp` on `GetValue__7CREKEYFCFiRf` went from 2 real
differing instructions to 0, and retail's own `.text` has the same asymmetry
(`0x802f08b4 lwz r4,20(r30)` start-first in KEYF against `0x802f0bc8 lwz r4,16(r3)` end-first in
the emitter). **`CIntElement.cpp` wants exactly the same change and is still queued.
`CColorElement.cpp` did not take it**, and the reason is measured, not guessed: its
`CCEKEYF::GetValue` (0x802cf66c, 392 B) is 96.408% with **28 differing instructions of 98**, and
only two of those are the hoist pair. The other 26 are a **one-slot register shift across the
whole inlined `GetKeyframeIndex` expansion** - retail puts the index in `r6`
(`and r6,r3,r0` / `cmpw r6,r5` / `subf r6,r4,r6` / `divw r0,r6,r3`) where ours has `r5` - plus the
`CColor::Lerp` argument block built in the opposite order (ours `slwi r4,r5,2` then `add r4,r6,r4`
then `slwi r0,r0,2`; retail `slwi r0,r0,2` then `slwi r5,r5,2` then `add r5,r6,r0`). **Retail calls
`CColor::Lerp` out of line here too** (`bl 48050f05`, and ours carries the matching undefined
`Lerp__6CColorFRC6CColorRC6CColorf`), so this is *not* an inlining difference. Note also that this
TU hoists the pair into `r4`/`r5` rather than `r4`/`r6` as `CRealElement` does, which is why the
wrapper does not reach it: the register assignment itself has to move first.
**Superseded 2026-09-29 - it landed anyway, and the fix is in the next section.**
### Ruled out here (each changed the register allocation or made it worse)
- spelling the index computation inline instead of calling the helper - the loads stop being hoisted
  at all, 25 differing instructions;
- hoisting `mLoopStart` / `mLoopEnd` into `const int` locals **read from the members directly**,
  in either definition order (25) - ruled out for `CVectorElement` and `CRealElement`, and it
  still fails here: it is the *accessor* call that makes mwcceppc allocate the index last.
  With the accessors it is the fix, and the section below has the measurement;
- reordering the helper body so `loopStart` is mentioned first;
- splitting `GetKeyframeTime(...)` into its own statement;
- turning `bool lerp` into the `if` it stands for - that **loses** the `clrlwi.` / `li r3,1` pair the
  bool materialises, so the bool form is required.
### Three things to stop re-reading
1. **`tools/compare_unit.sh` prints `.text: DIFFERS` on this unit and that is not a verdict.** Its
   compare is raw: ours is `.text 0x3a58` against a retail-derived `0x3694`, and the 11 extra
   functions are the `__dt__` weak COMDAT copies and the `rstl::vector` instantiations that
   `unit_fit.sh` already explains (964 bytes). Only `flip_test.sh` decides, and the flip passed.
2. **The state block's two moving numbers move by different amounts and that is correct.**
   `matched 8640 -> 8641` is **+1** (objdiff's *matched* count; the one function that was
   fuzzy-only became matched), `linked 3497 -> 3589` is **+92** (the whole unit's function count,
   because it is now `Matching`), and `DOL units 7976 -> 7977` is **+1**. Run
   `python3 tools/check_docs_claims.py` after the flip: it prints the exact replacement string for
   each stale figure, so there is no guessing which number belongs in which line.
3. **A landed flip reads as no change until the report is regenerated.** `flip_test.sh` edits
   `configure.py` and relinks, but the `build/report.json` it leaves behind was produced by the
   *pre-flip* build: `complete` stayed `False` for the unit, `complete_units` did not move, and
   `report_diff.py` printed `0 units newly linked` - for a change that had just linked 151
   functions. Re-run `./tools/decomp_build.sh` before quoting any `complete` / `complete_units`
   figure. The reverse trap is in the same family: the config key is `hash:`, not `sha1:`, so a
   REL check written against `sha1` matches nothing and passes having verified nothing.
## The last particle element needs the locals too (2026-09-29, goal item `match-ccolorelement`)
`Kyoto/Particles/CColorElement` was **44 / 45** functions at 100% with `CCEKEYF::GetValue`
(0x802CF66C, 392 B) at **96.408%** - 28 differing instructions of 98. It landed on **two small
changes and nothing else**; `flip_test` PASS, `main.dol` bit-identical, all 86 RELs byte-equal,
45 / 45 at 100.00%, `matched` 9188 -> 9189, `linked` 4014 -> 4059.
**The register shift is not a separate problem from the hoist order - it is the hoist order
again, and the fix is to read both members through accessors into `const int` locals, in
source order.** The sibling TUs read `mLoopStart`/`mLoopEnd` directly and only wanted the call
arguments transposed. Here, reading them directly gives `r4 = start`, `r6 = end`, `r5 = index`;
retail wants `r4 = start`, `r5 = end`, `r6 = index`. Measured, the seven spellings tried at that
one call site:
| spelling of the range at the `*KEYF` call site | differing instrs |
| --- | --- |
| `mLoopStart, mLoopEnd` (baseline) | 28 |
| `GetLoopStart(), GetLoopEnd()` (the sibling fix) | 27 |
| `GetKeyframeIndexEndFirst(..., mLoopEnd, mLoopStart)` (the 9-line wrapper) | 27 |
| helper signature transposed to `(loopEnd, loopStart)` | 29 |
| `const int ls = mLoopStart; const int le = mLoopEnd;` | 25 |
| `const int le = GetLoopEnd(); const int ls = GetLoopStart();` | 18 |
| **`const int ls = GetLoopStart(); const int le = GetLoopEnd();`** | **10** |
Read the shape: with the locals, mwcceppc allocates the index **last**, so it lands in the
highest free register (`r6`) and the two hoisted loads take `r4`/`r5` in **source order**. Both
halves of retail's requirement - start hoisted first *and* end in `r5` - come out of the one
spelling, which is why none of the transpositions reach it. The accessors are the same two the
other three element headers already carry (`CRealElement.hpp`, `CVectorElement.hpp`,
`CIntElement.hpp`), so this is the house spelling, not a new invention. **`const int` matters:
the same two locals declared plain `int` score 26, and using the accessors *inline* at the call
instead of binding them scores 27.**
**The remaining five instructions are the `CColor::Lerp` argument block, and one more local
fixes it.** Retail scales `idx + 1` and adds the base **before** it scales `idx`:
```
retail  addi r0,r6,1 ; lwz r4,44(r30) ; slwi r0,r0,2 ; add r5,r4,r0 ; slwi r0,r6,2 ; add r4,r4,r0
ours    addi r0,r6,1 ; lwz r5,44(r30) ; slwi r4,r6,2 ; add r4,r5,r4 ; slwi r0,r0,2 ; add r5,r5,r0
```
i.e. retail evaluates the **second** argument's address before the first's. Binding only the
second one to a reference flips it, and the two instructions that move are the only ones that do:
```cpp
const CColor& b = mKeys[idx + 1];
valOut = CColor::Lerp(mKeys[idx], b, t);        // 5 differing instructions, all relocations
```
The same source line written straight as `CColor::Lerp(mKeys[idx], mKeys[idx + 1], t)` is what
`CCEKeyframeEmitter::GetValue` in this same TU uses, **and that function is already 100%** -
retail's two `GetValue`s disagree with each other here too, in the same way it disagrees between
the `*KEYF` and `*KeyframeEmitter` paths of the hoist pair. Do not rewrite the emitter's line to
match; it is already correct.
**After both changes `tools/bytescmp.py` reports 5 differing instructions of 98, and all five
are relocation fields** (three `lfs`/`lfd` against `R_PPC_EMB_SDA21` float constants and the
`bl` against `Lerp__6CColorFRC6CColorRC6CColorf`), which objdiff ignores - objdiff reports the
function at 100%.
## Retail's out-of-line copy ctor decides the translation unit, and a pair of `bool : 1` is one byte (2026-09-28, goal item `progress-cstatemanager-clightcopy`)
`__ct__6CLightFRC6CLight` (0x80038C9C, 0xA4 bytes) is retail's out-of-line `CLight` copy
constructor, and it is in **`CStateManager.o`** - not in `CLight.o`, which is where every other
`CLight` member lives and where the obvious place to write it is. `CStateManager.cpp` calls it from
`fn_80038C5C`, which returns a `{u16, CLight}` aggregate by value and copy-constructs the light
into the return slot. So it was written in `src/MetroidPrime/CStateManager.cpp` and it is
**100.00%, 41 instructions byte-identical to retail's**. That also closed the port's
`_ZN6CLightC1ERKS_`: `main/MetroidPrime/CStateManager.cpp` is in `files.cmake`, so a function
written for the DOL's sake handed the port a symbol for free. **Port undefined 314 -> 313**
(`CLight.cpp` was never the answer, and the flip's remaining undefined list is now three symbols
down from the four the baseline tree fails on).
**The function is 0xA4 bytes, not the 0x4D the item guessed, and the interesting part is one
byte.** Retail copies 0x00..0x4C: six `lfs/stfs` pairs for the two vectors, `lwz/stw` for
`CColor`'s packed word and for `mType`, eight more float pairs, `lwz/stw` for the two ids, two
float pairs, and then **one `lbz/stb` at 0x4C**. A memberwise initialiser list of the two trailing
`mutable bool : 1` members does *not* produce that: mwcceppc read-modify-writes each bit in turn
(`lbz; lbz; rlwimi; stb; lbz; lbz; rlwimi; stb`) and the function sits at **85.24%**, 188 bytes,
8 instructions too many. **The two flags are one byte, so they are one object.**
| spelling of the two flags | result |
| --- | --- |
| two `bool : 1` members in the initialiser list (baseline) | 85.24% |
| the same two, assigned in the body instead | 85.24% |
| the two declarators in one declaration, `bool a : 1, b : 1;` | 85.24% |
| declared radius before intensity (reversed) | 85.24% |
| `*this = other` | 24.05% |
| one nested `SDirtyFlags` member, `mDirty(other.mDirty)` | **100.00%** |
The nested struct keeps every observable fact: still one byte at 0x4C, still `mIntensityDirty` at
bit 7 and `mRadiusDirty` at bit 6, and `SetSpotCutoff` - which read-modify-writes the two bits
separately in retail and in ours - stays at 100%, as does `main/Kyoto/Graphics/CLight` at 19/19.
`mutable` on the struct member is what keeps `GetIntensity() const` and `GetRadius() const` able to
clear the flags: mutability propagates into a mutable member's subobjects. The rejected alternative
was a `reinterpret_cast<SDirtyFlags*>(reinterpret_cast<char*>(this) + 0x4c)` in the body, which also
reaches 100% and which `check_raw_offsets.py` rightly refuses - a modelled member is exactly what a
raw offset is not for.
**The generalisable rule, and it is about the object rather than the copy.** mwcceppc copies adjacent
`bool : 1` bitfields one bit at a time but moves a one-byte aggregate whole, so a class whose last
member is a *pair* of bit flags models them as a one-byte struct if anything ever copies the class
out of line. The same rule in reverse: `SetSpotCutoff` proves the bits are still separate
read-modify-writes, so do not "fix" them into one field - the struct is right *and* the bitfields
are right, and both are in the tree at once.
`main/MetroidPrime/CStateManager` is **69 -> 70 / 239** and stays `NonMatching`; `matched 8681 ->
8682`, `linked 3740 -> 3740`, DOL sha1 and all 86 RELs unchanged, `report_diff.py` reports
`+1 functions at 100%` and no regression anywhere.
## `mutable` on the members is what stops a copy constructor's tail from being pipelined (2026-09-29, goal item `match-cdeferredparticleeffect`)
`Kyoto/Particles/CDeferredParticleEffect` was 17 of 18 at 100.00%, the one holdout being
`__ct__21CDependencyGroupTokenFRC21CDependencyGroupToken` (0x8033ECCC, 0x58 = 88 bytes) at
**90.68%**. It landed on **two words in a header** and nothing else; the unit is `Matching`,
`flip_test.sh` PASS, `main.dol` bit-identical, all 86 RELs byte-equal, 18 / 18 at 100.00%,
`matched` 9325 -> 9326, `linked` 4608 -> 4626, `complete_units` 699 -> 700.
**The whole difference is the register allocator, and the diff says so.** Retail loads the word
into `r0` and stores it before loading the byte; ours loads the byte first, into `r0`, and keeps
the word in `r4` until the store:
```
retail  lwz r0,24(r31) ; mr r3,r30 ; stw r0,24(r30) ; lbz r0,28(r31) ; stb r0,28(r30)
ours    lwz r4,24(r31) ; mr r3,r30 ; lbz r0,28(r31) ; stw r4,24(r30) ; stb r0,28(r30)
```
Five instructions, two temporaries, one scheduling decision: `-O4,p`'s pipeliner sees two
independent load/store pairs and software-pipelines them, which costs the second load a live
range and so forces it into a second register. **`mutable` on both trailing members stops it**,
because a mutable subobject is reachable through a `const` path, so mwcceppc can no longer
prove that the store to `this` cannot disturb the load from `other` and the two pairs stay in
source order. It changes no layout, no mangled name, no observable behaviour - the class has no
`const` member function that writes either member - and the copy constructor's five instructions
then come out byte-identical to retail's.
**The negative half matters as much as the lever, because ten shapes of the class do not move
it.** Measured with a standalone `mwcceppc` probe at the unit's own flags: `bool : 1` vs a plain
`bool` vs `uchar` vs `u8 : 8` vs `bool : 2` vs three `bool : 1` in one byte, `uint` vs `int` vs
`u32` for the word, a one-byte named struct for the flag (the `bool : 1` rule above), a
`#pragma pack(1)` five-byte nested struct, a third trailing byte, and the pair as a private base
class - **every one emits the same interleaved five instructions**, so none of them is worth
trying again. Neither is the optimisation level, and the reason is the point: **`-O4,p` and
everything below it each move one half of the diff and not the other.**
| knob | tail order | `mr r3,r30` |
|---|---|---|
| `-O4,p` (the unit's flags) / `#pragma scheduling on` | pipelined | middle - **retail's** |
| `-O3,p`, `-O3`, `-O2,p`, `-O2`, `-O1,p`, `#pragma scheduling off` | source order | **last** - not retail's |
| `#pragma optimization_level 1/2/3`, `#pragma global_optimizer off` | pipelined | middle |
| `+ mutable` on both trailing members | source order | **middle** - retail's, 100.00% |
So the pipeliner and the `mr` hoisting are driven by the same scheduler and cannot be separated
with a flag; `mutable` is the only lever found that separates them, because it removes the
licence to pipeline rather than the licence to schedule. Start here for any remaining
"4-byte word then 1-byte byte" copy-constructor tail at 90-99%: the discriminator is whether
retail's `lbz` sits **after** the `stw`. When it does, the copy is not a five-byte block copy
(a packed five-byte struct pipelines too), and `mutable` on the two members is the fix.
## A local is allocated in the scope that declares it, and a `const&` to a 2-byte member is one load (2026-09-29, goal item `progress-cstatemanager-dtor-members`)
`main/MetroidPrime/CStateManager` is **70 -> 73 / 239** and stays `NonMatching`; `matched 8817 ->
8820`, `linked 3875 -> 3875`, DOL sha1 `6ef9b491...` and all 86 RELs unchanged, `report_diff.py`
reports `+3 functions at 100%` and no regression anywhere. **Every figure in this section was
measured in this run**; each is one ~1.5 s incremental `./tools/decomp_build.sh
main/MetroidPrime/CStateManager` away from being re-measurable, and the disassembly quoted is
`build/binutils/powerpc-eabi-objdump -dr --section=.text` on the two objects named beside it.
### Read the right object, or a change that moved the score 15 points looks like a null result
`build/G2ME01/obj/<unit>.o` is the **retail** base objdiff compares against - 129108 bytes for
this unit. **Ours** is `build/G2ME01/src/<unit>.o`, 26752 bytes. Disassembling `obj/` while asking
what *we* emit answers the retail question instead: stashing the change and diffing the two
`obj/` objects gave a byte-identical pair (same sha1) across an edit that took
`fn_8003BF84` from 85.50% to 100.00%, which reads as "the compiler ignored the edit" and is enough
to burn a session. `grep '<unit>' build.ninja` settles it in one command. (`build.ninja:696` is the
compile edge, `:21798` the objdiff base, `:25776` the link input.)
| function | before | after | the difference |
| --- | --- | --- | --- |
| `fn_8003C3A8` | 78.70% | **100.00%** | `return TIdListResult(a, b)` directly, not a named `const TIdListResult` that is then copied out of |
| `fn_8003BF84` | 85.50% | **100.00%** | `GraveyardBucket fresh;` declared **inside each `if` body**, not at function scope |
| `TouchPlayerActor` | 85.48% | **100.00%** | `const TUniqueId& head = m_playerActorHead;` used for **both** the test and the call |
**Rule 1 - a local lives in the scope that declares it, and MWCC's frame was the whole
percentage.** `fn_8003BF84`'s two objects already agreed one-for-one over 44 instructions; only
where the zero was spilled differed. One local at function scope is one object in one slot,
reused by both branches:
```
ours before  stwu r1,-160(r1)   stw r0,0x8(r1)   addi r4,r1,0x8   (and the same slot again)
ours after   stwu r1,-288(r1)   stw r0,0x8c(r1)  ... stw r0,0x8(r1)  (one each)
retail       stwu r1,-288(r1)   stw r0,140(r1)   ... stw r0,8(r1)    (one each)
```
0xa0 -> 0x120 is the whole 85.50% -> 100.00%. The comment in the tree had asserted the opposite
(that retail shared one slot); it was wrong and is corrected in place.
**Rule 2 - a `const&` to a 2-byte member is the same load, and re-reading it is not.**
Retail's compare and its argument are one `lhz r4,9298(r3)` (0x2452) / `cmplw r4,r0` / `beq` /
`sth r4,0x8(r1)`. Ours emitted a second `lhz r0,9298(r31)` for the call's argument. Binding the
member by reference once measures 100.00%; the second read measures 85.48%.
**Rule 3 - a copy out of a copy hoists all its words before it stores any.** `fn_8003C3A8` as a
named local is four loads then four stores (`lwz r3.. ; lwz r4.. ; lwz r5.. ; lwz r0.. ; stw.. ;
stw.. ; stw.. ; stw..`); retail and the direct return both interleave (`lwz r0,0x10(r1) ;
stw r0,0(r29) ; ...`). Swapping the two constructor arguments is *also* 78.70% - measured by an
earlier run of this item and not re-measured here - which is how you know the score is about the
copy and not the argument order, so the source order stays as it is.
### The item's premise, re-measured: the 12 destructor members are real and are not a slice
`python3 .tmp/opencode/dtor.py obj` (written by an earlier run of this pair; `obj` = retail,
`src` = ours, which is the right way round) lists, in order, every release site. It re-runs to the
same numbers here: **retail 407 lines, 37 sites; ours 111 lines, 11 sites.**
```
0x2904 0x24E4 0x1E98 0x16F4 0x16D8 0x16C8 0x16B8 0x169C 0x1694 0x168C 0x1684 0x167C
0x1658 0x1650 0x163C 0x1620 0x1608 0x08D4 0x08C0 0x0808 ...
```
**`dtor.py` undercounts, and its blind spot is where the item's twelfth member is.** It only pairs
an `addi`/`addic` with a following `bl`, so a release site that goes through a *vtable* is
invisible to it - there is at least one in this destructor, 0x1604. Read the raw disassembly
before believing any count of release sites:
```
c928:  addic.  r0,r28,5636          # 0x1604
c92c:  beq     c950
c930:  lwz     r3,0x1604(r28)
c934:  cmplwi  r3,0
c938:  beq     c950
c93c:  lwz     r12,0(r3)            # vtable
c940:  li      r4,1                 # deleting flag
c944:  lwz     r12,8(r12)           # slot 1 = the deleting destructor
c948:  mtctr   r12
c94c:  bctrl
```
That confirms the item's 0x1604 and pins its shape: **retail's `m_world` is an owning pointer to a
polymorphic object, not the raw `CWorld*` our header declares** (`CStateManager.hpp:318`) - it is
null-checked and then destroyed with the deleting flag, which a raw pointer member never is.
The item's offsets are otherwise all in that list and none in ours, so the item is **right about
the count**. Two further corrections, both measured here against the retail object:
- **`0x1694` is a thirteenth.** Retail releases four 8-byte slots (0x167C/0x1684/0x168C/0x1694)
  where our header has **three** `rc_ptr`s (`CStateManager.hpp:332-334`) and then
  `CWorldLayerState* m_currentWorldLayerState` at 0x1694. So the header is *missing* a member
  here, not only mistyping the ones it has - and because the release order is descending, whatever
  fills 0x1694 has to be declared after the three that are there. The four retail callees are
  `fn_80009008` / `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` / `fn_800095E4` /
  `fn_80009224`; **only the second is an `rc_ptr`**, and all four use the **double-`addic.`
  no-flag pattern** (the address is tested twice, no `li r4` before the `bl`), which is
  `rstl::ncrc_ptr`'s destructor. Three distinct out-of-line symbols, so three distinct `T`s - the
  same conclusion an earlier run reached from the ctor's four
  `ncrc_ptr<CScriptMailbox|CMapWorldInfo|CPlayerState|CWorldTransManager>` parameters. The
  header's "Four rc_ptrs" comment (`CStateManager.hpp:329`) is wrong: it declares three.
- **`0x1650`/`0x1658` are the first two words of `pad2_2`, not of `m_scriptIdMap`.** `TIdList` is
  `rstl::map<TEditorId, TUniqueId>` and `rstl::map` is 0x14 bytes (`CHECK_SIZEOF(unk_map,0x14)`,
  `include/rstl/map.hpp:34`), so `m_scriptIdMap` is 0x163C..0x1650 and `pad2_2[0x2C]` is
  0x1650..0x167C - exactly where our three `rc_ptr`s start, consistent with our own destructor
  releasing 0x163C and then 0x167C/0x1684/0x168C with nothing between. The `CToken` releases are
  real: `__dt__6CTokenFv` with `li r4,0` at `c900` for 0x1650.
**Why this run did not land any of it.** Seven of the twelve (0x24E4, 0x1E98, 0x16F4, 0x16C8,
0x16B8, 0x1658, 0x8D4) are released by an unnamed retail function through a `li r4,-1` deleting
call, so each slot needs an **invented class** whose destructor is a one-line wrapper around a
symbol we neither have nor can name - the "plausible stand-in" the goal prompt forbids. And it
buys **zero** matched functions: `__dt__13CStateManagerFv` measures **23.37%** on its own
hand-written body, so repairing the tail of a function whose body is wrong cannot move
`report.json`. The offsets only pay off in the same change as the body.
### Three walls, each measured here rather than asserted
- **Five functions sit at 96.06% / 64 B, and all five are one `lwzu`.** Four are named
  `fn_800379xx` - `fn_80037904`, `fn_80037944`, `fn_80037984`, `fn_800379C4` - and the fifth is
  `fn_80037A04`, which is the same shape and the same score. `report.json` lists all five and
  `objdump -t` on **our** object shows five consecutive 0x40-byte symbols; all five bodies are the
  same `if (list.size() == 20) return false; list.push_back(id); return true;` over a different
  `IdList13xxx()`. Retail keeps `r6 = r3+0x10000` and re-displaces every access
  (`lwz r0,16088(r6)` / `sth r5,16092(r4)` / `lwz r4,16088(r6)` / `stw r0,16088(r6)`); ours folds
  the address in once (`lwzu r0,16088(r6)`) and uses `0x0`/`0x4` after. The one spelling that
  *changes* the addressing - no local at all, calling `IdList13ED8().size()` and
  `.push_back(id)` separately - reaches retail's split form and measures **61.69%** (re-measured
  here), because the compiler then re-derives the address per use, reloads the count and puts the
  split base in r3 not r6. The addressing form and the CSE of the count are two independent asks
  and no spelling tried so far gives both.
- **The two `SendScriptMsg` (99.52 / 99.58) differ only in two dead stores.** The frame
  (`stwu r1,-48(r1)`), the call and the live object all agree; the spill block before the argument
  object is ten instructions in both, and the first two are transposed:
  ```
  retail  sth r7,0x8(r1)   sth r8,0xc(r1)   sth r5,0x10 ... (8 more, byte-identical)
  ours    sth r8,0x8(r1)   sth r7,0xc(r1)   sth r5,0x10 ... (8 more, byte-identical)
  ```
  Retail puts the 4th parameter `other` in the first dead slot where we put `dest`.
  **`DeleteObjectRequest` is the control and it is 100.00%**: it makes the same kind of dead stores
  and matches, which is what says these two are unreachable rather than a missing object.
- **`DeferStateTransition` (98.18%), `fn_80037784` (96.67%) and `AllocateUniqueId` (83.31%) are
  on the `@stringBase0` wall,** and the objects say why exactly. The retail object has **no
  `.rodata` section at all** and 54 sites across 14 functions referencing `lbl_803A64F0`; ours has
  a **0x19-byte `.rodata`** holding only `"Object list full!"` and **3** sites, in exactly those
  three functions, referencing the linker-synthesised `@stringBase0`. The `__FILE__` string's
  position is a property of the **merged** rodata of the whole link, so no per-unit change reaches
  it. `AllocateUniqueId` has one reachable defect on top of the wall: retail calls
  `__vc__Q24rstl38bit_vector<Q24rstl17rmemory_allocator>Fi` (= `rstl::bit_vector<rstl::rmemory_
  allocator>::operator[](int)`) twice, we call `fn_80041518(queryOutput&, MapWorldInfoAreas&,
  ushort)` twice - and the two call sites are instruction-for-instruction identical
  (`addi r3,r1,<hidden return> ; addi r4,r29,2240 ; mr r5,r31 ; bl`), so it is a **symbol rename**,
  not a missing function. The string offset survives the rename, so it scores nothing alone.
`python3 tools/check_decl_order.py --unit main/MetroidPrime/CStateManager` still reports 80+
violations. That is pre-existing and irrelevant here - the unit is `NonMatching` and the rule only
bites on a flip.
## 2026-09-29: an unmangled symbol is a free function, and the port's undefined count is a gate
**`fn_8003B21C` (0x8003B21C, 32 bytes) 0.00% -> 100.00%**; `main/MetroidPrime/CStateManager`
**73 -> 74 / 239**, `matched 8820 -> 8821`, DOL sha1 `6ef9b491...` and all 86 RELs unchanged,
port undefined 313, **`goal_check.sh` PASS**. The item asked for the four 8-byte slots at
0x167C-0x1694 to be retyped from `rc_ptr` to `ncrc_ptr`; that is still unlanded (see below) and
it moves no offset either way, so this run took the reachable function instead.
### The lesson: `nm` tells you member or free function, and objdiff will not tell you
Retail's body is seven instructions with no frame and no calls:
```
lis r4,31 ; li r0,0 ; addi r4,r4,-31616 ; stw r4,0x24dc(r3)
stw r0,0x15f8(r3) ; stw r0,0x15fc(r3) ; stw r0,0x1600(r3) ; blr
```
Written as a member (`mgr->mCurrentRenderPlayerIndex = 2000000; mCurrentRenderPlayer = nullptr;
m_playerState = nullptr; m_cameraManager = nullptr;` - all four members already exist at those
offsets, `mCurrentRenderPlayerIndex` is the one at 0x24dc) it compiles to **byte-identical
instructions** and objdiff still reports **0.00%**, because the symbol comes out as
`fn_8003B21C__13CStateManagerFv` while retail's is unmangled `fn_8003B21C`. objdiff pairs by
name, so a perfect body scores zero. **`nm -n build/G2ME01/obj/.../<unit>.o | awk '$2=="T"'`
and look for a name with no `__`: that is the free-function list.** This unit already uses the
form - `fn_8003AD74`, `fn_800388EC`, `fn_80039B1C` at the top of `CStateManager.cpp` are all
`extern "C"` for this reason.
### The lesson: adding a forwarder can fail the item, and the gate says so
`fn_80043180` / `fn_800434CC` / `fn_80043688` (0xCF80/0xD2CC/0xD488) and `fn_800391B4` (0x2FB4)
are bare one-`bl` forwarders and **all four measured 100.00%** as written. Adding them took
`matched` to 8825 - and `tools/probe_sources.sh` reported
```
link_check: STRICT FAIL - regression gate: 317 undefined against a baseline of 314 (GREW)
NEW  fn_800391E4   NEW  fn_800431A0   NEW  fn_800434EC   NEW  fn_800436A8
```
because each forwards to a callee the unit does not define. `probe_sources.sh` gates the port's
undefined count against `docs/research/port_link_baseline.txt` and a `progress` item's judge
runs the full gate, so four matched functions were worth **less than none**. Reverted; the
finding is recorded in the source as a comment at the point of use. **A forwarder is a function
whose callee you must also write** - a decomp item that adds calls has to add definitions, or
the count goes the wrong way.
## A string literal's pool slot is set by *where its first user is declared*, and that can be moved (2026-09-29, goal item `progress-cgamestate-fn-80143e88`)
`fn_80143E88` (0x80143E88, 0x238) reaches 100.00% in `src/MetroidPrime/Player/CGameState.cpp`;
the unit goes 70 -> 71 / 116 and stays `NonMatching`. Three things had to be right, and only the
first is in the previous notes for this item.
### 1. A file-scope `static` array's literals are emitted where it is *declared*, not where it is used
`CGameState.cpp` had `static rstl::pair< const char*, uint > sGameModeLayers[]` at the **top** of
the file. That put its three literals at pool `+0x07`, `+0x11`, `+0x19` - so retail's
`"InitialWorld"` at `+0x07` had nowhere to go, and the function could not pass no matter how it
was written. **Moving the table down the file, to just before `ConfigureGameModeLayers` (its only
user), moves its literals with it**: `"Samus01"`/`"Coins"` land at `+0x40`/`+0x48` and
`fn_80143E88` becomes retail's first user of a string literal, exactly as retail has it.
Retail confirms the target, not just the guess. `__sinit_CGameState_cpp` (0x80146874) builds the
table at runtime and its three `addi` immediates are `+42`, `+440`, `+448` against
`lbl_803A9208` - `+42` is the shared `"Deathmatch"`, and the other two are the last two literals
in a 0x1C8-byte pool. That is only reachable if `fn_80143E88` (which the descending declaration
order puts above `ConfigureGameModeLayers`) is the unit's first literal user.
Measured, after the move, in `build/G2ME01/main.elf`: `"InitialWorld"` 0x803A920F, `"FrontEnd"`
0x803A921C, `"Results"` 0x803A9225, `"Coin"` 0x803A922D, `"Deathmatch"` 0x803A9232, `"%s%s%d"`
0x803A923D - retail's `+0x07/+0x14/+0x1D/+0x25/+0x2A/+0x35` byte for byte. **The general rule:
a `static` with an initializer emits its literals at its declaration site, so on a unit where a
literal's offset is part of an instruction, the declaration's position in the file is part of
the match.**
### 2. `lbl_803A91C8` is real retail data below the unit's own pool, so the 64-byte copy takes a name
The local's sixteen `lwz`/`stw` pairs read `lis r4,0x803B ; addi r9,r4,-28216` = 0x803A91C8, the
0x40 zero bytes immediately *below* `lbl_803A9208`. `config/G2ME01/splits.txt` claims this unit's
`.rodata` from 0x803A9208, so that object is retail's and has to be referenced, not created:
`extern "C" const char lbl_803A91C8[];` plus
`SGameStateName name = *reinterpret_cast< const SGameStateName* >(lbl_803A91C8);` gives the
sixteen pairs. `memcpy` is one instruction; a plain `char[0x40]` local does not get them.
### 3. Register numbering: the three `const char* const` are declared *before* the member reads
At 99.61% the whole function was byte-identical except that `r4` and `r8` were swapped - retail
holds the pool base in `r4` and `mGameMode` in `r8`, and the source above holds them the other
way round. Moving the `kResults`/`kCoin`/`kDeathmatch` block **above** the two member reads puts
the pool base in `r4`. This is the same fact as point 1 seen from the allocator's side: the
value whose address is computed first gets the low register.
### The things the earlier notes for this item got right, re-confirmed
- `+0x1F4` (`fn_800068F4`'s argument) is the matching build's `mAudioGroups`, not
  `PreviousGameResults()`. One `#ifdef TARGET_PC`-guarded `AudioGroups()` and one `#else` twin in
  `CGameState.hpp`, both returning `void*` - `fn_800068F4` is retail code this port has no type
  for, and `reinterpret_cast` from a vector reference is rejected by mwcceppc ("illegal type
  cast"), so the accessor has to return the pointer itself.
- `'DTHM'` / `'COIN'`: `addis r3,r8,0xBBAC ; cmplwi r3,0x484D` is a full-word compare against
  `0x4454484D`, because `0xBBAC == -0x4454` in a 16-bit immediate.
- The pool order *within* the function: `Results` `+0x1D`, `Coin` `+0x25`, `Deathmatch` `+0x2A`
  named as `const char* const` in that order, with `"%s%s%d"` created last by the first `sprintf`
  at `+0x35`.
- Reading `mGameMode` and `mPlayerCount` before the copy fixes the frame. **Correcting point 2 of
  the previous run's notes: hoisting all three - `mShowResults` included - is also wrong**, and
  costs 99.61% -> nothing. Retail reads `mShowResults` at 0x80144050, after the sixteen stores.
### Gate
```
./tools/gate.sh build/goal/judge/report.base.json            ->  GATE PASS  9948823+3 changed
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  8821 -> 8822   linked 3875 -> 3875   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CGameState :: fn_80143E88__Fv
per-function sweep over both reports: 0 worse, 0 disappeared, 0 new
sha1sum build/G2ME01/main.dol  ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  ->  0 missing names
python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState  ->  ok
./tools/probe_sources.sh  ->  a 741-file sweep at the time (726 since the 2026-09-29 upstream sync), 0 failed; LINKED (313 undefined, 0 duplicates)
tools/unit_fit.sh MetroidPrime/Player/CGameState.cpp  ->  98 extra functions (unchanged)
```
The unit is still not flippable, for the reason the earlier notes recorded: it emits 98 functions
the retail object does not define. **This change added none of them** - calling
`gpResourceFactory->GetResourceIdByName` instantiated no new weak copy, so the 98 is the same
98 the previous run measured.
## `CGameState`'s pool is now complete, and the eleven option names were what was missing (2026-09-29)
`progress-cgamestate-diagnostic-strings`. The item asked for the two diagnostic strings
`CGameState::CGameState(CBitStreamReader&)` builds and throws away, which our source skipped with a
comment. They are at `lbl_803A9208 + 60` (74 bytes) and `+134` (79 bytes), and adding them as plain
literals put both at exactly the right pool offsets on the first build - **the pool's order is the
file's function-declaration order reversed, with static-data literals last**, so no nudging was
needed:
```
"Cannot find World Asset(%x) to load save data.  Skipping save game info.\n"
"Save game did not contain World Asset(%x).  Creating default world save info.\n"
```
The second one is gated on `mWorldStates.size()` changing across `StateForWorld` (0x8014470C:
`lwz r25,12(r30)` before the call, `cmpw` after), which is what the old comment's "the original
also constructs an unused diagnostic string" had elided. The ctor's own fuzzy went **82.24% ->
84.14%**.
**That alone moved no function to 100%, so the item as written could not pass its own judge**
(`progress` requires the unit's `matched_functions` to rise strictly). It does not need the pool
filled to +426 to be *written* - it needs the pool filled *past* the two messages, and the eleven
option names are the only thing that lives there. So `fn_80145C98` came with them, ported from
`src/MetroidPrime/Player/CPersistentOptionsInit.cpp` (which is still on disk, unbuilt) into
upstream's TU:
- **Its names are literals here, not `lbl_803A9208 + K`.** The carve could not use literals
  because a `Matching` unit may not own `.rodata`; this unit does (the split claims
  `0x803A9208..0x803A93D0`), and a literal is what makes `lis/addi/addi K` come out.
- **Declaration position is load-bearing twice over** - once for `check_decl_order.py` and once for
  the pool. Retail's offset order puts `fn_80145C98` (0x80145C98) between
  `CGameStateEnvVarManager::FindEnvironmentVariable` (0x80145E24) and `::AddVariable` (0x801442CC),
  so the block sits between those two definitions. Declared after `CPersistentOptions::PutTo` it is
  still exactly right for the pool but puts the unit's tail 7 slots out of retail order, and the
  gate's decl-order step is the only thing that reports that.
- `fn_80145ACC` is a declaration only. It is a relocation; the DOL link uses the retail-filled
  object for a `NonMatching` unit, so the missing callee costs nothing and the port does not build
  this file.
`fn_80145C98` is at **100%**, the unit 71 -> 72 of 116, `.rodata` 74 -> 454 bytes and byte-identical
to retail over its whole 456 (`cmp` of the two `objdump -s` dumps differs only in the two trailing
alignment NULs). **`__sinit_CGameState_cpp` is the one that still wants the pool**: it is 63.35%,
and its `addi r9,r10,42` / `+440` / `+448` are already right - what differs is that retail
materialises `'DTHM'`/`'SNGL'`/`'COIN'` as `lis`+`addi` pairs inside `__sinit` (68 bytes ours,
80 retail) while we emit three `lwz` relocations into `.sdata`. That is the next thing on this
unit, and it is a *constant-pool* question, not a source-order one.
```
./tools/gate.sh build/goal/judge/report.base.json            ->  GATE PASS  6e3b568+2 changed
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  8822 -> 8823   linked 3875 -> 3875   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CGameState :: fn_80145C98
  no regression
sha1sum build/G2ME01/main.dol  ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  ->  484 units, 0 missing names
python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState  ->  ok
./tools/probe_sources.sh  ->  a 741-file sweep at the time (726 since the 2026-09-29 upstream sync), 0 failed; LINKED (313 undefined, 0 duplicates)
docs/HANDOFF.md state block updated in the same commit (8822 -> 8823, DOL 8022 -> 8023)
```
The unit is still not flippable and still emits **98 functions the retail object does not define**
(10896 bytes) - unchanged by this edit, since `fn_80145C98` calls nothing that instantiates a new
weak copy.
## A by-value 4-byte class parameter is passed by pointer, and that is visible in the frame (2026-09-29, goal item `progress-cstatemanager-rest`)
`CStateManager` 74 -> 78 / 239, global `matched` 8823 -> 8827, `tools/gate.sh` PASS against
`build/goal/judge/report.base.json`, DOL sha1 and all 86 RELs unchanged, port undefined 313 -> 314
against the 314 in `docs/research/port_link_baseline.txt` (no growth). The unit stays
`NonMatching`; nothing was flipped and `flip_test` was not run.
| function | retail | before | after | how |
|---|---|---|---|---|
| `CStateManager::AreaUnloaded(TAreaId)` | 0x800419C8, 4B | 0.00% | 100.00% | a **rename** of `fn_800419C8`, no new bytes |
| `CStateManager::RayCollideWorld(start, end, filter, damagee)` | 0x800422D4, 220B | 0.00% | 100.00% | the non-const 4-arg overload |
| `CStateManager::AreaLoaded(TAreaId)` | 0x80041A60, 60B | 0.00% | 100.00% | needs `fn_800B89FC` |
| `CStateManager::UpdateActorInSortedLists(CActor*)` | 0x80041B08, 308B | 0.00% | 100.00% | needs `fn_80041CCC` |
Sections 1, 2 and 3 of the two earlier attempts at this item reproduced exactly, including the
`dir *= (1.f / len)` frame-size argument (99.16% with `*`, 100.00% with `*=`) and the
`fn_800419C8` rename. What is new is below.
### 1. mwcceppc passes a by-value 4-byte class type as a pointer, and you can see the copy
`AreaLoaded` declares no local, no frame slot and no argument shuffling: `mr r31,r3 ; mr r5,r31 ;
lwz r3,0x167c(r3) ; bl fn_800B89FC`. Written with `fn_800B89FC(CMapWorldInfo*, TAreaId, CStateManager*)`
it compiled to `stwu r1,-0x20` and carried three instructions that retail does not have -
`lwz r0,0x0(r4) ; addi r4,r1,0x8 ; stw r0,0x8(r1)` - a copy of the area id onto the callee's own
frame before forwarding the address. **Changing the parameter to `const TAreaId&` reproduced retail
instruction for instruction** (`stwu r1,-0x10`). A 4-byte struct by value is not passed in r4; the
caller passes a pointer to a temporary, and the callee may or may not copy it. When a function is a
few instructions short and the extra ones are a load/store pair, look at the parameter list before
the arithmetic.
The same effect, opposite direction, in `UpdateActorInSortedLists`: retail keeps the validity byte
live in **r4** across the whole tail (`cmplwi r4,0` twice, no second `lbz`). Reading it back off
the frame as `bounds.valid` makes mwcceppc reload it; assigning it to a plain local first does not.
A local of the same type costs nothing and changes the register allocation.
### 2. `cmplwi` vs `cmpwi` is a signedness choice, and it is the whole last 2.7%
With the control flow correct the function sat at **97.27%**, and every remaining difference was
`cmplwi r4,0` against `clrlwi. r0,r4,24` - the same test in two encodings. The chain:
| spelling | emitted |
|---|---|
| `const bool valid = bounds.valid;` | `clrlwi. r0,r4,24` (normalise to a bool, then test) |
| `const uchar valid` / `const int valid` | `cmpwi r4,0` (signed) |
| `const uint valid` with the member left `uchar` | `cmplwi r4,0` - **retail's encoding** |
`lbz` already zero-extends, so the member stays a byte and only the local is widened. The four
combinations were all measured; `uint` local over a `uchar` member is the one that matches.
**`cmpwi` and `cmplwi` are the same comparison with different sign extension, and objdiff scores
them as different instructions.**
### 3. `CEntity`'s bitfield order is not what the header reads like - and the probe settles it
Retail's second test is `lbz r0,32(r31) ; rlwinm. r0,r0,25,31,31` on `CEntity+0x20`. The header
declares `m_active:1, m_notInArea:1, m_castFlags:4, m_scriptingBlocked:1, m_entityUnknown:1`, so
`IsScriptingBlocked()` is bit 6 and should be exactly that instruction. It is not: mwcceppc emits
`rlwinm. r0,r0,31,31,31` for it, and `rlwinm. r0,r0,25,31,31` for `GetActive()`. **The two are
swapped relative to the declaration order.** `fn_8003BE54` in this same unit, 100.00%, uses
`rlwinm r6,r0,25,31,31` after `ent->GetActive()` - so the header's *field order* is right and
mwcceppc's layout is what retail emits; the accessor that produces retail's bytes here is
`GetActive()`, not `IsScriptingBlocked()`.
The one-line probe that settles it, for any bitfield question, is to compile each accessor into its
own function and read the rotate immediate:
```c
extern "C" int p_blocked(CEntity* e) { return e->IsScriptingBlocked() ? 0x11 : 0x22; }
```
```
p_blocked:  rlwinm. r0,r0,31,31,31      <- bit 0
p_active:   rlwinm. r0,r0,25,31,31      <- bit 6
```
Distinct return values per accessor, or the compiler folds two of them into one test. Do not
reason about a bitfield's position from the declaration order; measure it.
### 4. Block layout is chosen by the polarity of the `if`, and inverting it is worth 2.7%
The same 97.27% build had the right instructions in the wrong order: retail's `beq` skips over
**Move** and falls through to **Remove**, ours skipped Remove. Written `if (active && valid) Move;
else Remove;` mwcceppc emits Move as the fall-through. Written `if (!active || !valid) Remove; else
Move;` - the same condition, negated - it emits exactly retail's layout, and the function goes to
**100.00%**. Three other rearrangements of the same logic measured 89.16%, 89.22% and 97.27%; only
the negation matched. **When a function is one branch layout away from 100%, negate the `if` before
you try anything else** - the fall-through is the compiler's to choose and the source's `if`
polarity is what it chooses from.
### 5. `UpdateActorInSortedLists` is net zero on the port's undefined count, and the reason generalises
The earlier attempts recorded it as "net 0, therefore still open" and left it unwritten. It is
written now, and the cancellation is worth stating as a rule: **defining a DOL function removes its
symbol from the port's undefined list, so the cost of a new callee is measured against that.**
`UpdateActorInSortedLists` went in (-1) while `fn_80041CCC` went in (+1), and `AreaLoaded` spent
the last slot the tree had. Had either been attempted in the other order the pair would not have
fitted at all. `fn_80041CCC` is declared but not written - 0x194 bytes needing `TCastToPtr<CPhysicsActor>`
(not in the port) plus vtable slot 9 of `CPhysicsActor`.
`SFoundBounds` is `{CVector3f min; CVector3f max; uchar valid;}` with a `Box()` accessor that
reinterprets the first 24 bytes as a `CAABox`. A `CAABox` member does not work: it has no default
constructor, and every spelling that gave it one emitted a real
`bl CAABox::CAABox(CVector3f, CVector3f)` that retail does not have. A `uchar raw[0x19]` buffer with
the copy spelled as a `for` loop was worse still - the loop did not inline and became 25 `lbz`/`stb`
pairs against retail's six `lwz`/`stw`. **The box has to be a real `CAABox` lvalue for the compiler
to emit a word-wise copy, and the flag has to be a separate byte beside it.**
### 6. Still open
- `SetCurrentAreaId` (0x80041728, 168B) - unchanged from section 3 of the first attempt: its four
  callees live in three TUs `files.cmake` does not list, and it duplicates `stub_59`.
- `SetActorAreaId` (0x800383E4, 296B) - `fn_801E4F0C` (+1) and `stub_58` must go together. There is
  no longer a spare slot.
- `UpdateObjectInLists` (0x80042434, 332B) and `PrepareAreaUnload` (0x800419CC, 148B) - +3 each.
- `AddDrawableActor` / `AddDrawableActorPlane` - the `mutable mAddedToken` finding from the previous
  attempt stands and is **not** applied here; it moves 45 functions in five unrelated units.
## The destructor's addresses, measured, and a 16-byte-element copy that is not `vector<float>` (2026-09-29, goal item `progress-cstatemanager-dtor-body`)
`main/MetroidPrime/CStateManager` 78 -> **79 / 239**, `All:` 8827 -> 8828, DOL sha1 and all 86 RELs
unchanged, **no function anywhere worse**. The destructor itself is still unmoved and still 23.37%;
this section is the measurement that decides how it gets written, plus one landed function and
three walls that are now characterised rather than suspected.
### 1. `__dt__13CStateManagerFv` is at 0x8004269C, and the previous note's range was in object space
The earlier notes quote the body as `0xC4B8-0xC7D4`. That is **right in `objdump -dr` output and
wrong as a DOL address**, which is the `left`-is-retail trap for the third time in this unit: the
object's `.text` base is 0, so every offset in that dump is an object offset. The DOL addresses are
object + 0x80036200 (the unit's `splits.txt` start):
| | object offset | DOL address | size |
| --- | --- | --- | --- |
| whole function | `0xC49C` | **0x8004269C** | 1420 bytes, ends 0x80042C28 |
| hand-written body | `0xC4B8` | **0x800426B8** | through `0xCA00` / **0x80042C00** |
| epilogue | `0xCA00` | 0x80042C00 | `extsh. r0,r29 ; ble ; bl fn_80045DC8` |
Calibrate with `build/binutils/powerpc-eabi-objdump -h <obj> | awk '/.text/'` against
`splits.txt`, never by reading the offset column. The 407 instructions, the `0xD0` frame and the
22 member releases in the earlier note are confirmed; so is the tail: it is a
`~CStateManager(int deletingFlag)` that calls `fn_80045DC8` (the unnamed base *deleting*
destructor at 0x80045DC8) only when the flag is non-zero, and our declaration has no parameter for
it. The 22 release sites and the 37 distinct `bl` targets are tabulated in the goal notes.
### 2. A pointer-bounded copy loop is not an indexed one: 94.38% -> the last 2 instructions
`fn_800391E4` (0x800391E4, 96 bytes) is a copy-assign over a counted array of **16-byte** elements,
and `fn_800391B4` (0x800391B4, 48 bytes) is its `return this` forwarder - the fourth of the
forwarders the 2026-09-28 note had to leave out because the callee raised the port's undefined
count. With the callee written, **the forwarder is 100.00%** and the undefined count is unchanged
(the pair cancels: the forwarder asks for `fn_800391E4`, the callee defines it).
The spelling rule, and it generalises to every float-array copy in the tree:
- An **indexed** `for (int i = 0; i < n; ++i) a[i] = b[i];` over a 16-byte element makes mwcceppc
  unroll **4x with a remainder**: `srwi. r0,r4,2 ; mtctr r0 ; <16 lfs/stfs pairs> ; bdnz ;
  andi. r4,r4,3 ; beqlr ; <4 lfs/stfs pairs> ; bdnz`. That is 101 instructions against retail's 24.
- A **pointer-bounded** `while (src != end) { *dst = *src; ++dst; ++src; }` emits retail's loop
  exactly: four `lfs`/`stfs` pairs, `addi r5,r5,16 ; addi r6,r6,16` straddling the last store, and
  `cmplw r5,r7 ; bne`. **`fn_8003ABF0`, 0x7A0 bytes away, is the indexed form of the same copy** -
  the two are the same operation written two ways, and that is why guessing from one of them fails.
- The residue at 94.38% is entirely the prologue: retail builds the end pointer from the *raw*
  source pointer (`add r7,r4,r0 ; addi r7,r7,4`, so `end == (char*)other + count*16 + 4`) and
  allocates the two data pointers as r6 then r5; mwcceppc strength-reduces `other->m_items` into
  r5 first and puts `end` in r0. Fourteen further spellings (declaration order, `const` on the end
  pointer, `for` vs `while` vs `do`-`while`, a `(char*)other + 4 + count*16` end expression, a
  memberwise copy, a counting-down loop, a combined-increment `for`) all produce the same two
  bytes. The `while (src < end)` spelling is not equivalent: it compiles to 0%.
### 3. Three walls in this unit, each with the exact bytes
- **The five `fn_800379xx` / `fn_80037A04` at 96.06% are one `lwzu`.** Retail
  `lwz r0,16088(r6)` against our `lwzu r0,16088(r6)`: mwcceppc folds the address of the
  `reserved_vector` (at container + 0x13EE8) into r6 and reuses it, where retail keeps r6 at
  `container + 0x10000` and reaches the member with a 0x3EE8 displacement. Same addresses, same
  16 instructions, one addressing mode. Twelve more spellings measured here (repeat the accessor
  call, pointer-to-list, a named count, `capacity()`, `20 == size()`, a `size() != 20` inverted
  block, a `const TIdList&` for the test and the accessor for the push, a named `TUniqueId` value,
  a cached container pointer) leave it at 96.06%, 61.69% or 70.44% - the two lower numbers are the
  shapes that lose the early return. **This is worth five functions if it is ever reachable.**
- **The two `SendScriptMsg` at 99.58 / 99.52% are a dead spill, not a spelling.** Both build a
  `CScriptMsg` at `r1+24` / `r1+20` *and* spill three or four `TUniqueId` values into a dead
  parameter save area at `r1+8..r1+20`; every instruction matches except the two `sth` operands in
  that dead area, where retail and mwcceppc disagree about which of `r7`/`r8` is `other` and which
  is the id. Nine spellings of the `CScriptMsg` construction (a named `TUniqueId id`, a named local,
  a `const` local, a cached `CEntity*`, member-by-member assignment, swapped `m_unk`/`m_originator`,
  swapped `m_id`) moved it to 99.42% at
  best. `CScriptMsg`'s own layout is `{TUniqueId @0, @2, @4, int @8, int @12}` - 16 bytes, which the
  five stores at +0/+2/+4/+8/+12 fix.- **`fn_80037784` and `DeferStateTransition` are one string address.** Both call
  `rs_new CSaveGameScreen`, and retail's `operator new` placement args are `lbl_803A64F0 + 147`
  where ours is our own `stringBase0`; everything else, including the `li r4,0` / `li r5,0` that
  the dead setup feeds, is identical. The unit claims no `.rodata` section, so the string pool slot
  is set by the linker and there is nothing in this unit to move. This is the `CMemory::Alloc` /
  `CCallStack` wall the handoff already records, seen from the other end: it is not a source
  problem, it is a section-claim problem.
### 4. What is left in the unit, for the next run
`__ct__13CStateManager` (7.45%, 4536 bytes) and `__dt__13CStateManagerFv` (23.37%, 1420) are the
two large ones and both need the destructor body's member list and its two unwritten callees
(`fn_800417D0`, 168 bytes; `fn_800412EC`, 556 bytes). `ApplyLocalDamage` (40.99%, 1360 bytes) and
`AddDrawableActor` / `AddDrawableActorPlane` (52.10 / 65.00%) are the next-largest uncharacterised
ones; the two `AddDrawableActor` bodies are instruction-for-instruction identical and differ only in
load-chain scheduling, and the `mutable mAddedToken` finding recorded on 2026-09-29 moves 45
functions in five unrelated units, so it is not a cheap fix. Gate: `tools/gate.sh` and
`tools/goal_check.sh` both pass; `report_diff` reports `+1 functions at 100%, no regression`.
## `CMetareeSwarmRel` is the module head, and `>> 7` is a 25-bit rotate (2026-09-29, goal item `progress-rel-head-metareeswarm`)
MetareeSwarm is module 43 and the first of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28 to get any class code. The head is now the whole `.text 0x0..0xD8` - **five** functions,
all 100.00%, `CMetareeSwarmRel.cpp` - and the module's sha1 against `config/G2ME01/config.yml` is
**unchanged** (`e9b5a7bd0c482e1bfecfd104e9d805bf579df554`), with all 86 holding and
`main.dol` still `6ef9b491...`. `matched` 8828 -> 8833, `linked` 3875 -> 3880, the module's own
count 5 -> 10 of 61. `tools/audit_rel_claim.py MetareeSwarm` reports 0 problem claims and 0 of 61
text symbols dropped by `-strip_partial`; `tools/check_decl_order.py --unit
MetareeSwarm/MetroidPrime/ScriptObjects/CMetareeSwarmRel` is ok and `tools/flip_test.sh` on the unit
reports `PASS -> kept as Matching`.
Four things came out of it that are not obvious from the recipe.
### 1. `fn_43_0` is an ordinary flag test, and an earlier reading of it was wrong (superseded)
**This corrects the record left by the 2026-09-29 run of the same item**, which claimed `.text
0x3C..0xD8`, called `fn_43_0` "retail dead code" and said the bit it tests "is always 0". That was
wrong, and it is corrected here and in `src/MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp` rather
than left in the tree.
`fn_43_0`'s tail is
```
mulli r4,r4,0xb8 ; lwz r3,0x184(r3) ; addi r0,r4,0xb2 ; lbzx r0,3,r0
rlwinm. r0,r0,25,31,31 ; beq .L34 ; li r5,1
```
`54 00 cf ff` is `rlwinm. r0,r0,25,31,31`, and dtk's own asm spells it `extrwi. r0,r0,1,24`. Reading
the dtk name literally - "bit 24 of the register" - and concluding that a `lbzx` zero-extended byte
has nothing there, so the `beq` is always taken and the function always returns 0, is what produced
the dead-code claim. **The measurement that settles it is in-tree and is a `Matching` unit**:
`fn_42_36C` (`CScriptMetaree.cpp:78`) is spelled `(*(uchar*)(self + 0x34c) >> 3) & 1` and its
retail object is
```
00000048 <fn_42_36C>:
  48:  88 03 03 4c   lbz     r0,844(r3)
  4c:  54 03 ef fe   rlwinm  r3,r0,29,31,31
  50:  4e 80 00 20   blr
```
so `mwcceppc` emits **`SH = 32 - shift`, `MB = ME = 31`** for `(byte >> n) & 1`, and `SH = 25` is
`n = 7`: bit 7 of the byte, mask `0x80`. The same word is the `CMain` frame loop's back-edge at
0x8000645C, which `docs/research/boot_path.md:188` already documents as testing `0x80`,
`finished`. So `fn_43_0` is a plain index-guarded flags accessor and costs nothing to write.
**General form, and the reason the mistake was available: dtk's `extrwi`/`ext` names are a
decoding of the *register* bit position, and they are only as trustworthy as the `rlwinm` operands
behind them. Do not reason about a bit out of the mnemonic - reason about the encoding, and look for
a `Matching` sibling in the tree that already spells the same shape.** Three statements in the
earlier attempt were false and are superseded: "bit 24 of the loaded byte is always 0", "the
function returns 0 for every input", and "reproducing it is a bit-field question, not a codegen
one". The claim now starts at 0x0 and the function is `Matching`.
### 2. The registration stores the address of a slot, not a loader
`fn_43_A8` is 0x30 bytes and reads as
```
lis r4, fn_43_D8@ha ; lis r3, lbl_43_bss_20@ha ; addi r0,r4,fn_43_D8@l
stwu r0, lbl_43_bss_20@l(r3)     ; r3 is still &lbl_43_bss_20
bl fn_8022D5A8                   ; so the argument is that address
```
The DOL callee settles it: `fn_8022D5A8` (0x8022D5A8, 8 bytes) is `stw r3, gLoader_MetareeSwarm;
blr`, and `LoadMetareeSwarm` (0x8022D57C) reads the result as
`lwz r6, gLoader_MetareeSwarm; lwz r12, 0(r6); mtctr r12; bctrl`. So the DOL holds a **pointer to a
loader slot**, and the module owns the slot - `lbl_43_bss_20`, `.bss:0x20`, `size:0x4
data:4byte`. `CScriptPlayerProxy.cpp` already had this shape; what is new is that it is
*measurable from the DOL's own two instructions* rather than assumed from the 8-byte `.sbss` size,
and it is why the module's slot is 4 bytes while the DOL's is 8 (`struct SLoaderSlot` in
`src/MetroidPrime/ScriptLoader/MetareeSwarm.cpp`, whose second word is untouched by anything
retail does).
### 3. Three floats 0x10 apart have to be built, not indexed
`fn_43_3C` (0x3C, 0x28 = 10 instructions) copies three floats out of a 0xB8-byte record at
`self+0x184`, at `+0x0C`, `+0x1C` and `+0x2C`. Retail loads **all three before it stores any**:
```
mulli r0,r5,0xb8 ; lwz r4,0x184(r4) ; add r4,r4,r0
lfs f2,0x2c(r4) ; lfs f1,0x1c(r4) ; lfs f0,0xc(r4)
stfs f0,0(r3) ; stfs f1,4(r3) ; stfs f2,8(r3) ; blr
```
Written as three indexed stores it is the **same ten instructions, interleaved** - load, store,
load, store, all through `f0` - and scores **58.30%**. Written as
`*out = CVector3f(values[0x0C/4], values[0x1C/4], values[0x2C/4])` it is 100.00%, because
`CVector3f`'s three-argument constructor is what hoists the three arguments into `f0`/`f1`/`f2`
before the copy. This is the `mwcceppc hoists inline-helper arguments in reverse call order` note
above, and the general form is worth keeping: **when retail loads N values into N registers and
then stores them, the source constructed an N-wide value, and indexing will not reproduce it even
though the instruction count already agrees.**
The 0x10 stride is also a measurement, not an assumption: it rules out a 12-byte `CVector3f` at
+0x0C, so the record has three separate floats there rather than a vector.
### 4. `*(records + index * 0xB8 + 0xB2)` indexes, `records[index * 0xB8 + 0xB2]` does not
`fn_43_0` addresses its flag byte differently from `fn_43_3C` addressing its floats, and the
difference is the C++, not the compiler's mood. Retail:
```
mulli r4,r4,0xb8 ; lwz r3,0x184(r3) ; addi r0,r4,0xb2 ; lbzx r0,3,r0
```
- base in `r3`, the whole byte offset in `r0`, indexed load. **That is the subscript form**
  `records[index * 0xB8 + 0xB2]`, or its spelled-out equivalent - it is 100.00% only when the
  offset is added through a pointer dereference,
  `*(reinterpret_cast<const uchar*>(records + index * 0xB8 + 0xB2))` (measured: the other two
  spellings, `records[offset]` with `const int offset = index * 0xB8 + 0xB2` and
  `records[index * 0xB8 + 0xB2]`, both fold the constant into the displacement and give
  `mulli r0,r4,0xb8 ; add r3,r3,r0 ; lbz r0,0xb2(r3)`, 4 instructions different).
- `fn_43_3C`'s `add r4,r4,r0` + `lfs ...,0x2c(r4)` is the *pointer-arithmetic* form, and the two
  forms appear in the same module, 0x3C apart, over the same array.
**General form: when retail uses `lbzx`/`lwzx` with a base and an offset that is one register
holding the *whole* offset, the source formed the address with a pointer dereference rather than
forming an element pointer. A subscript and a dereference are the same expression to a reader and
not the same expression to `mwcceppc`.** The same split is already visible inside this tree:
`CFlyerSwarm.cpp:11` (the `fn_43_3C` shape) computes `boids + index * 0xB8` as a pointer.
### What is left in the module
51 functions, of which `fn_43_D8` (0xD8, 0x3D8 = 984 bytes) is the entity loader the registration
installs - behavioural class code, and it needs the CActor/CPatterned hierarchy this tree does
not model, exactly as the item expected. It is the same blocker the other 26 modules of 2026-09-28
have. The head is not on the boot path, so nothing here moves the boot.
### The port side, and why the file is not in `files.cmake`
Listing `CMetareeSwarmRel.cpp` in `files.cmake` and registering `mp_relmain_metareeswarm` in
`platform/compiled_modules.cpp` compiles, links and scores - and makes the probe's regression gate
fail: the host body references `fn_43_D8` and `fn_8022D5A8`, neither of which the port can link,
and the undefined count went **314 -> 316** (measured with `tools/link_check.sh --strict`;
`link_check` named both). `fn_43_D8` is 984 bytes of class code and cannot be stubbed honestly.
So the file is left out of the port build, which is a documented state rather than an omission:
`tools/check_files_cmake.py` counts it under "further units are out because they define a module
entry point (RELMain/RELExit), which collides in a flat link". The port keeps reading
`MetareeSwarm.rel` off the disc through `platform/rel.cpp`, which is the correct arrangement for a
module whose code is not in `mp_game` - and **is** the reason `CScriptPlayerProxy.cpp`'s
`fn_62_188` sits in the link-gap list rather than defined. Measured at this commit: the probe
reports `LINKED (314 undefined, 0 duplicates)`, equal to the baseline, with the file absent.
## `CIngPuddleRel` is a module head, and a vtable call needs a class (2026-09-29, goal item `progress-rel-head-ingpuddle`)
IngPuddle is module 32 and the second of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28 to get class code. The head is now the whole `.text 0x0..0xA8` - **five** functions,
all 100.00%, `src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`312b87acb1dea81e5c03fccd6b87e68366f17a6f`), with all
86 holding and `main.dol` still `6ef9b491...`. `matched` 8833 -> 8838, `linked` 3880 -> 3885, the
module's own count 5 -> 10 of 68. `tools/audit_rel_claim.py IngPuddle` reports 0 problem claims
and 0 of 68 text symbols dropped by `-strip_partial`; `tools/check_decl_order.py --unit
IngPuddle/MetroidPrime/ScriptObjects/CIngPuddleRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 168 ours 168 retail 168 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.
The five, from `config/G2ME01/rels/IngPuddle/symbols.txt`:
```
0x00  fn_32_0   0x08   addi r3,r3,0x460 / blr
0x08  fn_32_8   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl
0x34  RELExit   0x24   li r3,0 / bl fn_80229EE0
0x58  RELMain   0x20   bl fn_32_78
0x78  fn_32_78  0x30   lbl_32_bss_0 = fn_32_A8 ; fn_80229EE0(&lbl_32_bss_0)
```
`RELMain`, `RELExit` and `fn_32_78` are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp`
arrangement, unchanged: the registration hands the setter the *address* of a four-byte `.bss` slot
(`fn_80229EE0` is the DOL's `stw r3, 0x80419598; blr`, and `LoadIngPuddle` at 0x80229EB4 reads it as
`lwz r6,slot; lwz r12,0(r6); mtctr r12; bctrl`), and the `.bss` symbol stays `extern` under MWCC
because dtk's `auto_05_00000000_bss.s` is the definition. The file is **not** in `files.cmake`, for
the reason measured on `CMetareeSwarmRel` two lanes earlier: a host body would reference `fn_32_A8`
and `fn_80229EE0`, and `tools/link_check.sh --strict` fails on a growing undefined count.
### The one instruction that mattered: `fn_32_8` has to be a member call
`fn_32_0` and `fn_32_8` are not free functions. dtk lists both in the module's FORCEACTIVE block,
and `build/G2ME01/IngPuddle/asm/auto_04_00000000_data.s` shows `.data:0xD0` - CIngPuddle's vtable -
storing `fn_32_0` at 0x38 and `fn_32_8` at 0x3C. `fn_32_0` is `return this + 0x460`; `fn_32_8` calls
whichever function is in vtable slot 0x38, which for CIngPuddle is `fn_32_0` itself.
The obvious spelling - load the vtable pointer and call through it by hand:
```cpp
void* const* vt = *reinterpret_cast<void* const* const*>(self);
(*reinterpret_cast<void (*)(void*)>(vt[0x38 / sizeof(void*)]))();
```
compiles, links, and scores **99.09%**: `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and one
wrong register in a twelve-instruction function. (`mwcceppc` also refuses the
`reinterpret_cast` outright - "function call does not match prototype" - so the load has to go
through a union or a stand-in class anyway.)
`mwcceppc` only reaches for r12 on its own virtual-dispatch path, so the call has to be written as
a member call. A class gives the slot number for free: MWCC lays a class's virtuals out the way
retail's vtable is laid out - **two leading words** (offset-to-top, then the RTTI pointer, which a
REL leaves null; `build/G2ME01/IngPuddle/asm/auto_04_00000000_data.s` shows `.data:0xD0` and
`.data:0xD4` both holding `0x00000000` before the first virtual) and then one word per virtual. So
**thirteen** virtuals put the last one at 0x38, and `self->Slot12()` gives all twelve instructions
byte for byte. Measured directly rather than reasoned from the pattern: a fifteen-virtual class
compiled to `lwz r12,0x40(r12)`, so the count is `(0x38 - 8) / 4 + 1`.
The stand-in class is named `CIngPuddleVTable`, declares its slots by position, and defines none of
them: no header in this tree models a CActor virtual, and the only object carrying this vtable is
the module's own retail bytes. The neighbouring function `fn_32_0` needs no class at all -
`return static_cast<const char*>(self) + 0x460` is retail's two instructions.
### What is left
`fn_32_A8` (0xA8, 0x1E4) is the module's `SLdrIngPuddle` entity loader, and the 57 functions from
there to `fn_32_3190` are CIngPuddle's methods (`TypesMatch.cpp` gives CIngPuddle the parent
`CPhysicsActor`). None is claimed; dtk fills `0xA8..0x31F4` from retail. That is class code and it
needs the CActor/CPhysicsActor hierarchy, which is the same blocker the item's `reason` names.
## `CPlantScarabSwarmRel` is `CMetareeSwarmRel` with another module number (2026-09-29, goal item `progress-rel-head-plantscarabswarm`)
PlantScarabSwarm is module 49 and the third of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0xD8` - **five** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`67240808f42d66482dfaa11daa09a9994cd63e92`, `cmp`-equal
to `orig/G2ME01/files/RelProd/PlantScarabSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8838 -> 8843, `linked` 3885 -> 3890, the module's own count 5 -> 10 of 71.
`tools/audit_rel_claim.py PlantScarabSwarm` reports 0 problem claims and 0 of 71 text symbols
dropped by `-strip_partial`; `tools/check_decl_order.py --unit
PlantScarabSwarm/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 216 ours 216 retail 216 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.
The five, from `config/G2ME01/rels/PlantScarabSwarm/symbols.txt`:
```
0x00  fn_49_0   0x3C   index-guarded flag test on record[index], index * 0xB8
0x3C  fn_49_3C  0x28   record[index].x0C/x1C/x2C -> *out, index * 0xB8
0x64  RELExit   0x24   li r3,0 / bl fn_8022FFF8
0x88  RELMain   0x20   bl fn_49_A8
0xA8  fn_49_A8  0x30   lbl_49_bss_20 = fn_49_D8 ; fn_8022FFF8(&lbl_49_bss_20)
```
### The finding: this head is module 43's head, instruction for instruction
Diffing `build/G2ME01/PlantScarabSwarm/asm/auto_00_00000000_text.s` (dtk's view of retail) against
`build/G2ME01/MetareeSwarm/asm/MetroidPrime/ScriptObjects/CMetareeSwarmRel.s` (dtk's view of *our*
object, which is 100.00% of retail) over `0x0..0xD8` - all 54 instructions - gives **7 differing
lines, and only 2 of them differ in bytes**: the two `bl` encodings (`48 00 32 2D` and
`48 00 31 DD` against `48 00 28 A9` and `48 00 28 59`). The other five differ only in a reloc name
(`fn_43_A8`, `fn_43_D8` twice, `lbl_43_bss_20` twice). That is a fact about the two modules, not
a coincidence of a template: both are 0xB8-byte records with a float triple at +0x0C/+0x1C/+0x2C
and a flag byte at +0xB2, counted at +0x17C and indexed at +0x184, and dtk puts **both** head
accessors in the module's FORCEACTIVE block and **both** vtables are 50 words with the two
accessors at words 38 and 39 - `.data:0x98` and `.data:0x9C`, in `auto_04_00000000_data.s` for each
module. So the two classes really are built from the same template in retail.
**The practical consequence is that none of module 43's three hard-won spellings had to be
rediscovered.** They transfer, and each is worth keeping for the next module of this family:
- the three floats have to be **built**, not indexed - `*out = CVector3f(v[3], v[7], v[11])` is
  100.00%, the three assignments are 58.30% (measured on `fn_43_3C`, 2026-09-29);
- `index > -1`, not `index >= 0` - the same test, but only the `<= -1` spelling gives retail's
  `cmpwi r4,-1 / ble`;
- the flag byte has to be read through a **pointer dereference**, not a subscript, or the constant
  folds into the load's displacement and retail's `addi r0,r4,0xb2 / lbzx r0,3,r0` becomes
  `add r3,r3,r0 / lbz r0,0xb2(r3)`.
A lane about to write a module head should **diff the candidate's `.text 0x0..0x1FF` against a head
that already landed before it writes any C++.** If the bytes match, the body is known and the item
is minutes; if they do not, the diff says which part is different, which is exactly the thing that
was expensive to find the first time.
`RELMain`, `RELExit` and `fn_49_A8` are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp` /
`CIngPuddleRel.cpp` arrangement, unchanged: the registration hands the setter the *address* of a
four-byte `.bss` slot (`fn_8022FFF8` is the DOL's `stw r3, gLoader_PlantScarabSwarm; blr`, and
`LoadPlantScarabSwarm` at 0x8022FFCC reads it as `lwz r6,slot; lwz r12,0(r6); mtctr r12; bctrl` -
`src/MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp` already documents that half), and the `.bss`
symbol stays `extern` under MWCC because dtk's `auto_05_00000000_bss.s` is the definition. The file
is **not** in `files.cmake`, for the reason measured on `CMetareeSwarm` three lanes earlier: a host
body would reference `fn_49_D8` and `fn_8022FFF8`, and `tools/link_check.sh --strict` fails on a
growing undefined count. `tools/check_files_cmake.py` counts it as a module entry (6 units, up from
5) rather than as an omission, which is the right bin.
### What is left
`fn_49_D8` (0xD8, 0x6A0) is the module's entity loader, and the 61 functions from there to
`fn_49_323C` are CPlantScarabSwarm's methods. None is claimed; dtk fills `0xD8..0x32A0` from retail.
That is class code and it needs the CActor/CPatterned hierarchy, which is the same blocker the
item's `reason` names. Note the module is 71 functions, not 66: `auto_00_00000000_text` reports 66
and the five `REL_Setup` functions are the other five, so a "27 unclaimed" style count taken from
the `auto_*` unit alone is short by the setup tail.
## `CSnakeWeedSwarmRel` is a module head, and a pmf is 12 bytes (2026-09-29, goal item `progress-rel-head-snakeweedswarm`)
SnakeWeedSwarm is module 71 and the fourth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0xDC` - **four** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`f59a2a74718b31bff0e76d9716e39091e1e11bba`, `cmp`-equal
to `orig/G2ME01/files/RelProd/SnakeWeedSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8843 -> 8847, `linked` 3890 -> 3894, the module's own count 5 -> 9 of 74.
`tools/audit_rel_claim.py SnakeWeedSwarm` reports 0 problem claims and 0 of 74 text symbols dropped
by `-strip_partial`; `tools/check_decl_order.py --unit
SnakeWeedSwarm/MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 220 ours 220 retail 220 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.
**The head is not the other three modules' head, and the "diff the first 0x200 bytes against a head
that already landed" shortcut says so in one command.** The four, from
`config/G2ME01/rels/SnakeWeedSwarm/symbols.txt`:
```
0x00  fn_71_0   0x2C   the CActor `GetHealthInfo` slot, calling vtable slot 0x38
0x2C  RELExit   0x24   li r3,0 / bl SetLoader_SnakeWeedSwarm
0x50  RELMain   0x20   bl fn_71_70
0x70  fn_71_70   0x6C   lbl_71_bss_40 = {fn_71_DC, lbl_71_data_18, lbl_71_data_24}
```
`RELMain`, `RELExit` and the registration are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp`
arrangement, unchanged, including the `extern`-under-MWCC `.bss` slot. Two things are new.
### 1. A CodeWarrior pointer-to-member-function is 12 bytes, so the record is 0x1C and not 8
`fn_71_70` writes **seven** words. `SetLoader_SnakeWeedSwarm` is the DOL's 0x8021BB08
(`stw r3, gLoader_SnakeWeed; blr`), and its two readers are in
`build/G2ME01/asm/MetroidPrime/ScriptLoaderRel.s`: `LoadSnakeWeedSwarm` (0x8021BADC) reads
`lwz r6, gLoader_SnakeWeed; lwz r12, 0(r6); mtctr r12; bctrl` - the loader at **+0x00** - and
`SnakeWeedAlt_8021BA94` (0x8021BA94) reads `lwz r7, gLoader_SnakeWeed; addi r12, r7, 0x4;
bl __ptmf_scall`, i.e. a pointer-to-member-function at **+0x04**. And `__ptmf_scall`
(`build/G2ME01/asm/Runtime/ptmf.s:0x80345454`) reads **three** words out of r12 - the `this`
adjustment, a vtable offset, and the address - which is why `__ptmf_null` is 0xC bytes. So the
record is `FScriptLoader` + 12 + 12 = **0x1C**, the size of `lbl_71_bss_40` in
`build/G2ME01/SnakeWeedSwarm/asm/auto_05_00000000_bss.s`, and the members land at +0x00, +0x04 and
+0x10 exactly as the stores in `fn_71_70` do.
`include/MetroidPrime/ScriptLoaderRel.hpp` models this struct as **eight** bytes (a loader and one
member-function pointer, which is a host-sized guess). The module fills seven words and the third
has no reader in the DOL, so the type is spelled locally in `CSnakeWeedSwarmRel.cpp` and the header
is left alone - fixing it is a port-side model change, not this item's business. **The general
rule: `__ptmf_scall` is three words, so any `*FuncPtrs` struct in this tree that holds a
pointer-to-member-function is 12 bytes wider than a host C++ member pointer, and a struct that
looks too small in a header is a header bug rather than a codegen puzzle.**
### 2. The two member-function pointers are copied out of `.data`, not assigned
The two 12-byte objects at `.data:0x18` and `.data:0x24` (`auto_04_00000000_data.s`, both
`0 / 0xFFFFFFFF / fn_71_1AF8` and `0 / 0xFFFFFFFF / fn_71_1B3C`) are non-virtual member-function
pointers: the vtable offset is -1, so `__ptmf_scall` skips the vtable lookup and calls the address
directly. `fn_71_70` loads all six words into r9/r8/r7 and r5/r4/r0, stores the loader through
`stwu` so r3 walks the record, stores the six words back, and only then calls the setter - so the
source is three field assignments with the two right-hand sides read out of `.data`:
```cpp
lbl_71_bss_40.swarm = fn_71_DC;
lbl_71_bss_40.damage = lbl_71_data_18;
lbl_71_bss_40.alt = lbl_71_data_24;
SetLoader_SnakeWeedSwarm(&lbl_71_bss_40);
```
That spelling is what produces the schedule, including the `stwu` in the middle of the loads, and it
is 100.00% on the first try. **Building the two pmfs in place** - `&CEntity::SomeMethod` written out
as a `{0, -1, fn}` aggregate - would put three `li`s and a `stw` per member in the function and
lose the `.data` objects; the copy is not an accident of the compiler. Both `.data` symbols stay
`extern` and unclaimed for the same reason the `.bss` slot does: dtk's data object is the
definition, and a second one under MWCC is what broke mwldeppc on `ScriptPlayerProxy`.
### 3. `fn_71_0` is a vtable entry, and the slot is CActor's `HealthInfo`
`lbl_71_data_30` (`.data:0x30`, 0x7C bytes) is CSnakeWeedSwarm's vtable and stores `fn_71_0` at
offset 0x3C. That table is **CActor's** (`__vt__6CActor`, in
`build/G2ME01/asm/MetroidPrime/CActor.s`: 29 entries after two zero words) with one slot replaced -
the 14th virtual, `GetHealthInfo__6CActorCFv`, is `fn_71_0` here, and the 13th, the one `fn_71_0`
dispatches on, is `HealthInfo__6CActorFv`. So the call is retail's own shape: the DOL's
`GetHealthInfo__6CActorCFv` at 0x8000B900 is these same eleven instructions, `lwz r12,0(r3) /
lwz r12,0x38(r12) / mtctr r12 / bctrl`, which is the `CIngPuddleRel` measurement from the day before
(hand-loading the vtable gives `lwz r3,0(r3)` and 99.09%). The stand-in class
`CSnakeWeedSwarmVTable` declares all 29 virtuals by position, so the called one is at
`(0x38 - 8) / 4 + 1` = the 13th, and none of them is defined: the only object carrying this vtable
is the module's own retail bytes.
**The cheap way to name a vtable slot is to find the base class's table in the DOL and diff the
two.** `lbl_71_data_30` and `__vt__6CActor` are the same 29 entries with one substitution, which
turns "call vtable offset 0x38" into "`GetHealthInfo`, returning `HealthInfo()`" without reading a
single instruction of the module's class code. `TypesMatch.cpp` already says CSnakeWeedSwarm's
parent is CActor.
### What is left
`fn_71_DC` (0xDC, 0x544) is the module's entity loader, and the 65 functions from there to
`fn_71_3CF4` are CSnakeWeedSwarm's methods. None is claimed; dtk fills `0xDC..0x3D44` from retail.
That is class code and it needs the CActor/CPatterned hierarchy, which is the blocker the item's
`reason` names. The module is 74 functions: `auto_00_00000000_text` reported 69 before the carve
and reports 65 after it, and the five `REL_Setup` functions are the other five.
## `CMysteryFlyerRel` is a module head, and `fn_45_10` is a hidden-return `optional_object` (2026-09-29, goal item `progress-rel-head-mysteryflyer`)
**Solved, later on 2026-09-29 - the rest of this section is the reasoning that led there, and its
conclusion that `fn_45_10` needs `optional_object`'s converting ctor is superseded.** The ctor is
**out of line in retail**: `fn_45_2BBC` *is* `optional_object<CAABox>(const CAABox&)` (six word
copies, then the flag), so `fn_45_10` never instantiates the template. It is one call:
```cpp
void fn_45_10(void* out, const CPhysicsActor* self) { fn_45_2BBC(out, self->GetBoundingBox()); }
```
with `fn_45_2BBC` declared `extern "C" void fn_45_2BBC(void* out, const CAABox& box)`. Taking the
box by value instead copies the temporary a second time (frame 0x40, extra `lfs`/`stfs`); the const
reference gives the retail 0x30 frame byte for byte. Two traps on the way to the sha1:
**including `MetroidPrime/CPhysicsActor.hpp` breaks the module hash with every function at 100%**,
because it pulls in `Collision/CMaterialList.hpp`, whose file-scope `static` material constants
add 0x28 bytes of `.data`; `CMysteryFlyerRel.cpp` declares a one-method local `class CPhysicsActor`
instead, and includes `MetroidPrime/TGameTypes.hpp` for `TUniqueId`. The head is now
`.text 0x0..0x170`, 18/18 at 100.00%, `matched` 9102 -> 9117, sha1 unchanged. **The same wrapper
sits near 0x0 in about twenty other modules** (`fn_81_10` Tryclops, `fn_40_10` Metroid,
`fn_55_10` SandBoss, ...), so this spelling is the lever for extending their heads to 0x0.
MysteryFlyer is module 45 and the fifth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is **the loader trio only**, `.text 0xFC..0x170` - three functions, all
100.00%, `src/MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`2770bc0304d6e5511c151e34a89c5e0d57af1bc5`, `cmp`-equal
to `orig/G2ME01/files/RelProd/MysteryFlyer.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8865 -> 8868, `linked` 3912 -> 3915, the module's own count 5 -> 8 of 83.
`tools/audit_rel_claim.py MysteryFlyer` reports 0 problem claims, `tools/flip_test.sh` on the unit
passes and keeps it `Matching`, and `tools/unit_fit.sh` says
`.text claimed 116 / ours 116 / retail 116, fits` with no extra functions.
### Why the claim starts at 0xFC and not at 0x0
The four heads before this one all claimed from `0x0`, because the accessors the REL loader
generator emits come *before* the trio and are cheap. Here they do not come first, or rather
they come first but they are **not all cheap**, and one contiguous claim cannot skip any of them.
From `config/G2ME01/rels/MysteryFlyer/symbols.txt`, `.text 0x0..0xFC` is fifteen functions:
`fn_45_0` (0x0) and `fn_45_8` (0x8) are `li r3,1` and `addi r3,r3,0x818`; **`fn_45_10` (0x10, 0x3C)
is not a member accessor at all**; then ten functions that are the same fourteen-accessor block
`CAtomicAlphaRel.cpp` already reproduces (0x4C..0xFC, the float store, the two predicates, the
unique-id reset, the `>> 3 & 1` flag, the constant float, `+0x754`, two predicates and the
three-float copy); and `fn_45_D0` (0xD0, 0x2C), a vtable-0x38 dispatch that `CAtomicAlphaRel.cpp`
already writes as a member call against a thirteen-virtual stand-in class.
So **fourteen of the fifteen are known-good bodies today and only `fn_45_10` is new work** - but
because the claim must be one contiguous range, `fn_45_10` is the whole of the difference between
this head's three functions and eighteen. Landing the trio now and leaving the range below it to
dtk is the correct outcome for this item, not a smaller version of it.
### `fn_45_10` is a hidden-return `optional_object<CAABox>`, and here is the measurement
The disassembly (`build/G2ME01/MysteryFlyer/asm/auto_00_00000000_text.s:0x10`) is 0x3C bytes:
```
stwu r1,-0x30(r1) / mflr r0 / stw r0,0x34(r1) / stw r31,0x2c(r1) / mr r31,r3
addi r3, r1, 8
bl GetBoundingBox__13CPhysicsActorCFv
mr r3, r31
addi r4, r1, 8
bl fn_45_2BBC
```
**The caller never sets r4 before the `bl`, which settles the calling convention.** The DOL
callee at 0x800EA054 (`build/G2ME01/asm/MetroidPrime/CPhysicsActor.s:371`) reads its actor out of
**r4** throughout (`lfs f3, 0x258(r4)` and eleven more) and leaves r3 alone until it calls
`__ct__6CAABoxFRC9CVector3fRC9CVector3f`, which is a constructor and so takes its `this` in r3.
So `GetBoundingBox` receives **r3 = the destination box, r4 = `this`** - and since `fn_45_10` enters
with `this` in r3 and copies it to r31 without ever writing r4, **r4 must already hold `this`,
which means `fn_45_10` itself is a member function with a hidden return pointer in r3.** The
`mr r3, r31` after the call is that return pointer coming back, and `fn_45_2BBC` is therefore
writing the return value, not a member.
`fn_45_2BBC` (0x2BBC, 0x3C) is the rest of the reading: it copies **six** words, `r4+0x00` through
`r4+0x14`, to `r3+0x00` through `r3+0x14` and then `stb 1, 0x18(r3)`. Six words is a `CAABox` (a
`CVector3f` pair) and a byte at +0x18 is a validity flag, so the return type is
**`rstl::optional_object<CAABox>`, 0x1C bytes** - and `include/rstl/optional_object.hpp` is the type
that models it.
**So `fn_45_10` is one line of C++** - `return rstl::optional_object<CAABox>(GetBoundingBox());`
in a `CPhysicsActor`-derived class - and it is still the blocker, for a reason worth writing down
because it is not a spelling problem. `rstl::optional_object`'s converting constructor is
`optional_object(const T& item) : m_valid(true) { rstl::construct<T>(m_data, item); }`, so the
**flag store is the mem-init and the copy is the body** - the opposite order from retail, which
copies six words and *then* sets the flag. And instantiating `optional_object<CAABox>` in a unit
whose other eighteen functions are the loader trio pulls `rstl::construct`/`rstl::destroy` out of
line into a **trailing pool**, which is the emission-order wall "An emission-order wall: out-of-line
template instantiations" above measures on `CStaticAudioPlayer`. Three things would have to be
true at once - the mem-init order inverted, no pool, and the 0x30 frame - and this lane did not
spend the builds to find out. **`NEW:` in the notes records it.**
The frame is the one part that is settled: a single 0x18-byte local at `r1+8`, plus MWCC's 8-byte
doubleword at `r1+0`, the saved `r31` and the saved `LR`, is exactly the `stwu r1,-0x30(r1)` retail
emits, so the stack is not the problem - the type's initialisation order is.
### What is left
`fn_45_170` (0x170, 0x30C) is the module's entity loader, and the 74 functions from `fn_45_0` at
0x0 to `fn_45_2BF8` are CMysteryFlyer's members. None is claimed; dtk fills `0x0..0xFC` and
`0x170..0x2BF8` from retail, which is what `auto_00_00000000_text` (15 functions) and
`auto_00_00000170_text` (59) in `build/report.json` are. The module is 83 functions: 3 ours, 5
`REL_Setup`, 74 unclaimed, and the 83rd is `auto_fn_45_2BF8_text`, the `.ctors`/`.dtors` pointers -
the same convention the `AtomicAlpha` row above uses.
## `CAtomicAlphaRel` is a module head, and twelve of its fourteen accessors are shared (2026-09-29, goal item `progress-rel-head-atomicalpha`)
AtomicAlpha is module 2 and the fifth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0x13C` - **eighteen** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`ade8972eb74648c3c5c99caa022ff457ff2b27fd`,
`cmp`-equal to `orig/G2ME01/files/RelProd/AtomicAlpha.rel`), with all 86 holding and `main.dol`
still `6ef9b491...`. `matched` 8847 -> 8865, `linked` 3894 -> 3912. `tools/audit_rel_claim.py
AtomicAlpha` reports 0 problem claims (`18/18 functions` inside the claim, so the unit really did
write every byte it claims) and 0 of 71 text symbols dropped by `-strip_partial`; `tools/unit_fit.sh`
reports `.text claimed 316 ours 316 retail 316 fits` with no extra functions; `tools/flip_test.sh`
on the unit reports `PASS -> kept as Matching`.
The eighteen, from `config/G2ME01/rels/AtomicAlpha/symbols.txt`:
```
0x00  fn_2_0   0x08   addi r3,r3,0x8c8 / blr
0x08  fn_2_8   0x08   addi r3,r3,0x7d8 / blr
0x10  fn_2_10  0x10   lbl_8041AAB8 -> *((float*)(self + 0x448))
0x20  fn_2_20  0x08   lbz r3, 0x44f(r3)
0x28  fn_2_28  0x08   li r3,0
0x30  fn_2_30  0x08   li r3,0
0x38  fn_2_38  0x10   *self = kInvalidUniqueId
0x48  fn_2_48  0x0C   byte at +0x34c, bit 3
0x54  fn_2_54  0x0C   lbl_8041B758
0x60  fn_2_60  0x08   addi r3,r3,0x754
0x68  fn_2_68  0x08   li r3,1
0x70  fn_2_70  0x08   li r3,0
0x78  fn_2_78  0x08   li r3,0
0x80  fn_2_80  0x1C   three floats from self+0x54 -> *out
0x9C  fn_2_9C  0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
0xC8  RELExit  0x24   li r3,0 / bl SetLoader_AtomicAlpha
0xEC  RELMain  0x20   bl fn_2_10C
0x10C fn_2_10C 0x30   lbl_2_bss_0 = fn_2_13C ; SetLoader_AtomicAlpha(&lbl_2_bss_0)
```
### The finding, stated correctly this time: twelve of the fourteen, not fourteen
**This is the first of the module heads that is larger than the loader trio**, and that changes
what the claim looks like. The other four claim five, four, five and five functions; here the
*fourteen-accessor block that the REL loader generator emits at the head of a scripted-actor module
comes first*, from 0x0 to 0x9C, and only then the vtable dispatch and the trio. So the claim
reaches from 0x0 rather than from `fn_2_9C`, and it is **18 functions** - the largest single step.
Twelve of those fourteen accessors were already written:
`src/MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp` claims AtomicBeta's `.text 0x0..0x9C` as
a `Matching` unit at 100.00%, with the same three relocations - `lbl_8041AAB8`, `kInvalidUniqueId`
and `lbl_8041B758`, all three in the DOL, which is exactly why one body serves every module. The
same `store default float at +0x448` / `read byte at +0x44f` / `li r3,0` predicates /
`reset unique id` / `bit 3 of +0x34C` / `constant float` / `+0x754` / `li r3,1` / two more
predicates / `copy three floats from +0x54` sequence, in the same order.
**But the two blocks are not byte for byte identical, and an earlier draft of this section said they
were.** AtomicAlpha's two *leading* accessors are **extra**: `fn_2_0` returns `self + 0x8C8` and
`fn_2_8` returns `self + 0x7D8`, where AtomicBeta opens with the float store at 0x0. In exchange
AtomicAlpha carries two fewer `li r3,0; blr` predicates after the byte read (its 0x28/0x30 are
Alpha's, AtomicBeta's 0x18 and 0x20 have no counterpart here). The count that survives checking
against the bytes is **twelve of fourteen**, and the honest generalisation is *"read
`AtomicBetaAccessors.cpp` and diff the candidate's `.text 0x0..0x1FF` against a landed head before
writing any C++, and expect 18 functions here"* - not *"copy the file"*.
Even so **no spelling had to be discovered**, which is the transferable part. A lane that diffs
against a head that already landed - the advice in the `CPlantScarabSwarmRel` section above - finds
this in one command; a lane that diffs against `AtomicBeta` finds twelve of the eighteen written and
the other six as three-line bodies.
Two of the fourteen still read oddly in dtk's rendering, and both are dtk's, not the source's:
- `fn_2_48` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
  `extrwi r3, r0, 1, 28`, which reads as bit 28 of a byte and would therefore be a function that is
  always false. The word's opcode is 21, not 31, so it is `rlwinm r3, r0, 29, 31, 31`: a rotate
  left by 32-3, masked to one bit - **bit 3**, the same bit `AtomicBetaAccessors.cpp`'s `& 8`
  already spells, and the same encoding as `fn_42_36C`'s measured `>> 3`.
- `fn_2_80` loads and stores **interleaved** (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 /
  lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores over a `const float*`. Do
  not "improve" it into a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in
  spelling reverses the loads and the score falls. This is the *opposite* of `fn_49_3C` in the same
  family, where retail does build the three floats first, and the difference is in the bytes.
### `fn_2_9C` is a vtable entry, and the same member-call trick applies
`fn_2_0`, `fn_2_8` and `fn_2_9C` are not free functions. dtk lists all three in the module's
FORCEACTIVE block and `build/G2ME01/AtomicAlpha/asm/auto_04_00000000_data.s` shows `.data:0xC4` -
AtomicAlpha's own 82-word (0x148-byte) vtable - storing `fn_2_9C` at 0x3C, so nothing here is a
dead-stripping hazard. `fn_2_9C` reads vtable slot 0x38, which `.data:0xC4` names
`HealthInfo__3CAiFv`, so it is written as a member call against a thirteen-virtual stand-in class
for the reason measured in the `CIngPuddleRel` section above: the hand-loaded spelling gives
`lwz r3,0(r3)` where retail has `lwz r12,0(r3)`. `Slot12` is declared `virtual float` because that
is what the vtable entry is; the call discards the result, so it does not affect the bytes, and
naming it anything else would misdescribe the vtable.
`RELMain`, `RELExit` and `fn_2_10C` are the `CScriptPlayerProxy.cpp` arrangement, unchanged. The one
difference from the four modules before it is the **name of the setter**: AtomicAlpha's is
`SetLoader_AtomicAlpha`, a real C++ function already at 100.00% in
`src/MetroidPrime/ScriptLoaderRel.cpp` (that unit is 42/42), so it is declared here in C++ rather
than as an `fn_80xxxxxx` DOL label, and `mwcceppc` mangles it to
`SetLoader_AtomicAlpha__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity` - the name
`config/G2ME01/symbols.txt` gives the DOL's 0x8021BB9C. The modules whose setter is still an
unnamed `fn_80xxxxxx` had to invent that name for the same two-instruction `stw r3, slot; blr`; a
module whose setter is already decompiled should **not**, because an invented name is a different
symbol and the call stops resolving to the function that actually exists. Check per module with
`grep SetLoader_ config/G2ME01/symbols.txt`. The `.bss` symbol stays `extern` under MWCC because
dtk's `auto_05_00000000_bss.s` is the definition, and the file is **not** in `files.cmake` for the
reason measured on `CMetareeSwarm`: a host body would reference `fn_2_13C`, which the port cannot
link, and `tools/link_check.sh --strict` fails on a growing undefined count.
### What is left
`fn_2_13C` (0x13C, 0x420) is the module's entity loader, and the 47 functions from there to
`fn_2_2578` are AtomicAlpha's methods (`auto_04_00000000_data.s` names them: `CPatterned`'s
`PreThink`, `CActor`'s `SetActive`, `CAi`'s `HealthInfo`, `CPhysicsActor`'s `GetWeight`, and so
on, so `TypesMatch.cpp`'s parent chain is what the tree is missing). None is claimed; dtk fills
`0x13C..0x267C` from retail. That is class code and it needs the CActor/CPatterned hierarchy,
which is the blocker the item's `reason` names.
**Counting a module's functions: read the per-unit `total_functions`, do not add up unit names.**
Before the claim, `build/report.json` had `auto_00_00000000_text` 65 + `auto_fn_2_2578_text` 1 +
`REL_Setup` 5 = 71. After it has `CAtomicAlphaRel` 18 + `auto_00_0000013C_text` 47 + 1 + 5 = 71.
The unit *names* changed and the total did not - the same attribution effect "Why the matched total
can go *down* when module work lands" describes. So the module is **71 functions**, and the 70 in
the status table above is 71 less the `auto_fn_2_2578_text` unit, on the same convention as the
three rows above it. `gate.sh` calls the split out itself, as `SPLIT ... exact count match - a
split, not a loss`, which is the right reading.
## `CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out (2026-09-29, goal item `progress-rel-head-ingsnatchingswarm`)
IngSnatchingSwarm is module 33 and the sixth of the 27 modules whose `REL_Setup` tail was claimed on
2026-09-28. The head is now the whole `.text 0x0..0xA8` - **five** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`c84839632c931841a91b29e2a30dff53bc6f2408`, `cmp`-equal
to `orig/G2ME01/files/RelProd/IngSnatchingSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8868 -> 8873, `linked` 3915 -> 3920, the module's own count 5 -> 10 of 101.
`tools/audit_rel_claim.py IngSnatchingSwarm` reports 0 problem claims and 0 of 102 text symbols
dropped by `-strip_partial`; `tools/check_decl_order.py --unit
IngSnatchingSwarm/MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel` is ok; `tools/unit_fit.sh`
reports `.text claimed 168 ours 168 retail 168 fits` with no extra functions; `tools/flip_test.sh`
on the unit reports `PASS -> kept as Matching`.
The five, from `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt`:
```
0x00  fn_33_0   0x08   addi r3,r3,0x1F4 / blr
0x08  fn_33_8   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl
0x34  RELExit   0x24   li r3,0 / bl SetLoader_IngSnatchingSwarm
0x58  RELMain   0x20   bl fn_33_78
0x78  fn_33_78  0x30   lbl_33_bss_0 = fn_33_A8 ; SetLoader_IngSnatchingSwarm(&lbl_33_bss_0)
```
### The finding: the head is module 32's head, instruction for instruction
Diffing `build/G2ME01/IngSnatchingSwarm/asm/auto_00_00000000_text.s` (dtk's view of retail) against
`build/G2ME01/IngPuddle/asm/MetroidPrime/ScriptObjects/CIngPuddleRel.s` (dtk's view of *our* object,
which is byte-identical) over the same 0xA8 bytes: **42 instructions, differing in 3** - the
`addi r3,r3,0x1F4` against IngPuddle's `addi r3,r3,0x460`, and the two `bl` displacements
(`48 00 53 51` and `48 00 53 01` against `48 00 32 B5` and `48 00 32 65`), because each module
registers its own loader. Every other line differs only in the module's own symbol names
(`fn_32_78`/`fn_33_78`, `lbl_32_bss_0`/`lbl_33_bss_0`, `fn_32_A8`/`fn_33_A8`) and is byte-identical. The two accessors are at the same two words of the same 31-word
vtable: `build/G2ME01/IngSnatchingSwarm/asm/auto_04_00000000_data.s` shows `.data:0x264` (0x7C
bytes: two zero words, then 29 virtuals) storing `fn_33_0` at 0x38 and `fn_33_8` at 0x3C, the same
0x38/0x3C as IngPuddle's `.data:0xD0`. So the member-call spelling the
`CIngPuddleRel` section measures is the spelling here, not a re-derivation: thirteen virtuals put
the last one at 0x38 and `self->Slot12()` gives retail's seven instructions byte for byte. **General
form, now twice measured: before rewriting a module head, diff its dtk `.s` against the previous
head's, and the codegen is free - only the class's own offsets and the two call targets move.**
The parent class differs and is worth recording: `TypesMatch__18CIngSnatchingSwarmCFi` at 0x8009C5F4
is `cmpwi r4,0x1e` falling through to `TypesMatch__6CActorCFi`, so `CIngSnatchingSwarm` derives from
**`CActor`**, where IngPuddle's derives from `CPhysicsActor`. That is one fewer class in the chain
the rest of the module is waiting on, and it is measured from the DOL, not from `TypesMatch.cpp`.
### The trap: an `extern "C"` import keeps its identifier, so the import has to be spelled out
IngPuddle's setter is the *unnamed* DOL symbol `fn_80229EE0`, so the C++ identifier and the import
name were the same string and the issue never arose. IngSnatchingSwarm's is already decompiled:
`src/MetroidPrime/ScriptLoaderRel.cpp:141` defines `SetLoader_IngSnatchingSwarm(FScriptLoader*)`,
and because that file is *not* `extern "C"`, `mwcceppc` mangles the `FScriptLoader*` parameter -
a function-pointer typedef - into
`SetLoader_IngSnatchingSwarm__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`, which
is the name `config/G2ME01/symbols.txt:9529` gives the DOL's 0x8021BA8C and the name the module
imports.
Writing the short name here compiles, emits, and then fails at the REL step, not earlier:
```
[2/9] LINK build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.plf
[4/9] REL  FAILED
Failed: While resolving relocations in 'build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.plf'
Caused by:
    Failed to find symbol SetLoader_IngSnatchingSwarm in any module
```
**So the rule for a module head is: look up the setter's name in `config/G2ME01/symbols.txt` and
write *that*, not the C++ name.** An invented `fn_80xxxxxx` for a setter that is already named is a
different symbol and the call stops resolving; the `AtomicAlpha` section above says the same thing
for a module whose setter is named, and this is the other half of it - the mangled form, not the
short one. The C++ identifier is not recoverable from the mangled one, so nothing else in the file
has to change.
The rest of the arrangement is `CIngPuddleRel`'s unchanged: the registration hands the setter the
*address* of a four-byte `.bss` slot (`SetLoader_IngSnatchingSwarm` is the DOL's
`stw r3, gLoader_IngSnatchingSwarm; blr` at 0x8021BA8C, and `LoadIngSnatchingSwarm` at 0x8021BA60
reads it as `lwz r6,slot; lwz r12,0(r6); mtctr r12; bctrl`), the `.bss` symbol stays `extern` under
MWCC because dtk's `auto_05_00000000_bss.s` is the definition, and the file is **not** in
`files.cmake` for the reason measured on `CMetareeSwarmRel`: a host body would reference `fn_33_A8`,
which the port cannot link, and `tools/link_check.sh --strict` fails on a growing undefined count
(measured here: 314, unchanged, because the file is absent).
### What is left
`fn_33_A8` (0xA8, 0x5D4 = 1492 bytes) is the module's entity loader, and the 91 functions from
there to `fn_33_5004` are CIngSnatchingSwarm's methods. None is claimed; dtk fills `0xA8..0x5394`
from retail. That is class code and it needs the CActor hierarchy, which is the blocker the item's
`reason` names. Counting note, on the same convention as the status table above: the module is 101
functions (5 ours + 91 unclaimed + 5 setup), which excludes the `auto_fn_33_5078_text` unit -
a 102nd, the `.ctors`/`.dtors` pointer function. Read the per-unit `total_functions` in
`build/report.json`; `gate.sh` prints the claim as a `SPLIT ... exact count match - a split, not a
loss`.
## `CFishCloudRel` is a module head, and the header already had the record (2026-09-29, goal item `progress-rel-head-fishcloud`, lane 1)
FishCloud is module 20 and the eighth module head claimed, after `MetareeSwarm`, `IngPuddle`,
`IngSnatchingSwarm`, `PlantScarabSwarm`, `AtomicAlpha`, `SnakeWeedSwarm` and `MysteryFlyer`. The
head is the whole `.text 0x0..0xAC` - **four** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CFishCloudRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged** (`79ae4b2efb35bbb627f3a76d811d5272e6643e62`,
`cmp`-equal to `orig/G2ME01/files/RelProd/FishCloud.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8873 -> 8877, `linked` 3920 -> 3924, REL 845 -> 849.
`tools/audit_rel_claim.py FishCloud` reports 0 problem claims (`4/4 functions` inside the claim) and
0 of 106 text symbols dropped by `-strip_partial`; `tools/unit_fit.sh` reports
`.text claimed 172 ours 172 retail 172 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.
The four, from `config/G2ME01/rels/FishCloud/symbols.txt`:
```
0x00  fn_20_0   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
0x2C  RELExit   0x24   li r3,0 / bl SetLoader_FishCloud
0x50  RELMain   0x20   bl fn_20_70
0x70  fn_20_70  0x3C   lbl_20_bss_0 = {fn_20_340, fn_20_AC} ; SetLoader_FishCloud(&lbl_20_bss_0)
```
### Nothing had to be discovered, and that is the finding
Every other head cost something. `MetareeSwarm` found that `>> 7` is a 25-bit rotate and that the
three floats have to be built rather than indexed. `AtomicAlpha` found that twelve of its fourteen
accessors are shared and two are not. `SnakeWeedSwarm` found that a CodeWarrior pmf is 12 bytes,
because `__ptmf_scall` reads three words of it. **FishCloud cost nothing at all**, and the reason
is worth recording precisely, because the temptation is to file it as a copy of SnakeWeedSwarm:
- **The vtable entry is not merely the same function - the bytes are identical.** `fn_20_0` and
  `CSnakeWeedSwarmRel.cpp`'s `fn_71_0` are both `.text:0x0`, `size:0x2C`, and the eleven
  instructions of one are the eleven of the other, word for word. That is CActor's
  `GetHealthInfo`, called through the same 0x38 slot. Two things make it safe for one body to serve
  both. `build/G2ME01/FishCloud/asm/auto_04_00000000_data.s` shows **two** 0x7C-byte tables,
  `lbl_20_data_8` (`.data:0x8`) and `lbl_20_data_84` (`.data:0x84`), and **both** store `fn_20_0` at
  vtable offset 0x3C with `HealthInfo__6CActorFv` at 0x38 - the same 31-word layout (two leading
  zero words, then 29 virtuals) and the same 15th/14th pair `lbl_71_data_30` has. And being in two
  tables is the opposite of a dead-stripping hazard: the link cannot drop it. The 29-virtual
  stand-in class therefore already exists, spelled out in `CSnakeWeedSwarmRel.cpp`, and is reused
  here unchanged.
- **The record is already in a header.** `include/MetroidPrime/ScriptLoaderRel.hpp` has
  `SFishCloud_FuncPtrs` {FScriptLoader fishCloud; FScriptLoader fishCloudModifier;}, and its
  `gLoader_FishCloud` is `.sbss:0x80419490` **size 0x8** - two words, matching the `.bss` object.
  SnakeWeedSwarm had to spell its record locally precisely *because* the header was one member
  short. So the `#include` is the whole of the difference, and the field names are retail's own:
  the DOL's `LoadFishCloud` (0x8021BB3C) reads `value->fishCloud` and `LoadFishCloudModifier`
  (0x8021BB10) reads `value->fishCloudModifier`, which is what fixes the assignment order and so
  the `r5`/`r4` in `fn_20_70`.
- **The setter is already decompiled.** `SetLoader_FishCloud` is the DOL's 0x8021BB68, two
  instructions, and its body lives in `src/MetroidPrime/ScriptLoaderRel.cpp` at 100.00%, so it is
  *declared* in C++ here and `mwcceppc` mangles it to
  `SetLoader_FishCloud__FP19SFishCloud_FuncPtrs` (`config/G2ME01/symbols.txt:9535`). The same trap
  as `CAtomicAlphaRel` applies in reverse: the header's own `SetSFishCloud_FuncPtrs` is a third
  name for the same idea, is not what the call site mangles to, and an `fn_80xxxxxx` alias would be
  a fourth. Only `SetLoader_FishCloud` resolves.
The one thing that is genuinely *this module's* is the record: **two plain `FScriptLoader`s, eight
bytes, no member-function pointers at all** - the first of these heads whose registration has no pmf
in it. What is *not* this module's is the shape of the head, and the useful habit is to check that
before assuming anything. Most of the heads are shaped as "accessors, then dispatch, then the trio",
and this one has no accessors at all, so `.text 0x0` is the vtable entry itself - **as it already was
in `SnakeWeedSwarm`**, whose `fn_71_0` is also 0x2C at 0x0 and byte for byte the same. So there are
two head shapes here, and `head -4 config/G2ME01/rels/<Module>/symbols.txt` - is `.text:0x0` a
`0x2C` vtable dispatch, or an `0x08`/`0x3C` accessor? - settles it in one command before any C++ is
written.
**Check that the byte comparison is not vacuous.** The obvious way to write the `diff` above is
```sh
awk '/^\.fn fn_[0-9]+_0,/,/^\.endfn/' build/G2ME01/$m/asm/auto_00_00000000_text.s \
  | grep -oE '\*/\s+[0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} ' | tr -d ' ' > .tmp/$m.txt
```
which **matches nothing at all** in dtk's listing format - the four bytes come *before* the `*/`,
not after it. Both files come out empty, `diff` reports no difference, and the check passes while
proving nothing. That is `docs/PROCESS_LESSONS.md`'s first lesson in its purest form, and it is
recorded here because a previous attempt at this item published the empty-`diff` result. The form
that actually works, and that still fails loudly if the extraction breaks:
```sh
for m in FishCloud SnakeWeedSwarm; do
  awk '/^\.fn fn_[0-9]+_0,/{f=1} f{print} /^\.endfn/{if(f)exit}' build/G2ME01/$m/asm/auto_00_00000000_text.s \
    | grep -oE '[0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} [0-9A-F]{2} \*/' | tr -d ' */' > .tmp/$m.txt
done
wc -l .tmp/FishCloud.txt          # 11 - if this is 0, the diff below is meaningless
diff .tmp/FishCloud.txt .tmp/SnakeWeedSwarm.txt
```
`RELMain`, `RELExit` and `fn_20_70` are the `CScriptPlayerProxy.cpp` arrangement, unchanged,
including the `extern` `.bss` slot under MWCC (a second definition there is what produced
mwldeppc's internal linker error on ScriptPlayerProxy) and the file's absence from `files.cmake`
(measured: the port's undefined count is 314 before and 314 after this change - `fn_20_340`,
`fn_20_AC` and `SetLoader_FishCloud` are all symbols the host cannot link, so the port keeps
reading `FishCloud.rel` off the disc through `platform/rel.cpp`).
### What is left, measured rather than summarised
`fn_20_AC` (0xAC, 0x240) is the module's `LoadFishCloudModifier` entity loader. **It is not a small
function, and an earlier draft of this section said it called `__ct__20SLdrEditorPropertiesFv` "and
nothing else", which was wrong** - it makes **nine** distinct calls:
```
__ct__20SLdrEditorPropertiesFv   __dt__20SLdrEditorPropertiesFv
__nw__FUlPCcPCc                  ReadFloat__12CInputStreamFv  (twice)
ReadBytes__12CInputStreamFPvUl   LoadTypedefSLdrEditorProperties__FR20SLdrEditorPropertiesR12CInputStream
LdrToEntityInfo__FRC11CEntityInfoRC20SLdrEditorProperties
AllocateUniqueId__13CStateManagerFv
fn_20_6AAC                        (0x6AAC, 0x120)
```
and the last of those is the one that settles it: `fn_20_6AAC` is the `CFishCloud` **constructor**,
and it calls `__ct__16CActorParametersFv`, `__ct__10CModelDataFv` (through `fn_20_6BCC`, 0x20) and
`Translate__12CTransform4fFRC9CVector3f` before storing `lbl_20_data_84` - one of the two vtables
above - into the object. So the `CModelData` and the actor are built one call below `fn_20_AC`, not
by `fn_20_340` alone. Its neighbour `fn_20_340` (0x340, 0x838) is the `LoadFishCloud` half and makes
**nineteen** distinct calls, adding `__ct__6CColorFR12CInputStream`,
`__ct__10CModelDataFRC10CStaticRes`, `__dt__23SLdrAnimationParametersFv`,
`LoadTypedefSLdrAnimationParameters…`, `LoadEditorTransform__FRC20SLdrEditorProperties`, four calls
into the module's own class code (`fn_20_B78`, `fn_20_C88`, `fn_20_DFC`, `fn_20_57D0`) and
`fn_800DFFA8`.
The 97 functions from 0xAC to 0x7014 are FishCloud's methods (`auto_04_00000000_data.s` names them:
`TypesMatch__10CFishCloudCFi`, `CActor`'s `SetActive`, `PreThink`, `HealthInfo`, `GetAimPosition`,
`FluidFXThink`, and so on, so `TypesMatch.cpp`'s parent chain is what the tree is missing). None is
claimed; dtk fills `0xAC..0x7014` from retail. That is class code and it needs the
CActor/CPatterned hierarchy, which is the blocker the item's `reason` names.
**Counting this module: the per-unit `total_functions` are `CFishCloudRel` 4 +
`auto_00_000000AC_text` 97 + `REL_Setup` 5 = 106**, and that is exactly the 106
`audit_rel_claim.py` prints, because here the two agree. The off-by-one that forced the AtomicAlpha
row above to subtract a unit **does not recur here**, and the reason is worth stating rather than
guessing: FishCloud has **no** separate `.ctors`/`.dtors` unit in the report, because its
`ModuleConstructors` (0x716C) and `ModuleDestructors` (0x7120) both sit *inside* the claimed
`REL_Setup` range 0x7014..0x71B8 and are therefore counted in that unit's five. `.ctors` and
`.dtors` are empty section labels in `splits.txt` and contribute no unit. So whether the two numbers
agree is a property of the module, not of the counting rule, and the habit to keep is the same one
the AtomicAlpha row taught: read the per-unit `total_functions` in `build/report.json` rather than
adding up unit names.
## `CBacteriaSwarmRel` is a module head, and the shortest head from 0x0 is four functions (2026-09-29, goal item `progress-rel-head-bacteriaswarm`)
BacteriaSwarm is module 6 and one of the nine of the 27 modules whose `REL_Setup` tail was claimed
on 2026-09-28 that have since gained class code. The head is `.text 0x0..0xA0` - **four**
functions, all 100.00%, `src/MetroidPrime/ScriptObjects/CBacteriaSwarmRel.cpp` - and the module's
sha1 against `config/G2ME01/config.yml` is **unchanged**
(`11859125ea9ab676bf42431cc948ec8d08c86552`, `cmp`-equal to
`orig/G2ME01/files/RelProd/BacteriaSwarm.rel`), with all 86 holding and `main.dol` still
`6ef9b491...`. `matched` 8877 -> 8881, `linked` 3924 -> 3928. `tools/audit_rel_claim.py
BacteriaSwarm` reports 0 problem claims (`4/4 functions` inside the claim, so the unit really did
write every byte it claims) and 0 of 114 text symbols dropped by `-strip_partial`;
`tools/unit_fit.sh` reports `.text claimed 160 ours 160 retail 160 fits` with no extra functions;
`tools/check_decl_order.py --unit BacteriaSwarm/MetroidPrime/ScriptObjects/CBacteriaSwarmRel` is
ok; `tools/flip_test.sh` on the unit reports `PASS -> kept as Matching`.
The four, from `config/G2ME01/rels/BacteriaSwarm/symbols.txt`:
```
0x00  fn_6_0   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
0x2C  RELExit  0x24   li r3,0 / bl fn_8022A5AC
0x50  RELMain  0x20   bl fn_6_70
0x70  fn_6_70  0x30   lbl_6_bss_10 = fn_6_A0 ; fn_8022A5AC(&lbl_6_bss_10)
```
### What makes it the shortest head from 0x0, and what that does *not* mean
Measured across all twelve heads landed so far, from each module's `splits.txt` and the per-unit
`total_functions` in `build/report.json`:
| head | `.text` claim | bytes | functions |
| --- | --- | --- | --- |
| **`BacteriaSwarm`** | **`0x0..0xA0`** | **160** | **4** |
| `IngPuddle` | `0x0..0xA8` | 168 | 5 |
| `IngSnatchingSwarm` | `0x0..0xA8` | 168 | 5 |
| `FishCloud` | `0x0..0xAC` | 172 | 4 |
| `MetareeSwarm` | `0x0..0xD8` | 216 | 5 |
| `PlantScarabSwarm` | `0x0..0xD8` | 216 | 5 |
| `Krocuss` | `0x0..0xD8` (was `0x3C..0xD8`, 156 bytes, 14) | 216 | 15 |
| `EyeBall` | `0x0..0xD8` (was `0x3C..0xD8`, 156 bytes, 14) | 216 | 15 |
| `IngSpiderballGuardian` | `0x0..0xD8` (was `0x44..0xD8`, 148 bytes, 13) | 216 | 15 |
| `Ripper` | `0x0..0xD8` (was `0x3C..0xD8`, 156 bytes, 14) | 216 | 15 |
| `SnakeWeedSwarm` | `0x0..0xDC` | 220 | 4 |
| `AtomicAlpha` | `0x0..0x13C` | 316 | 18 |
| `MysteryFlyer` | `0x0..0x170` (was `0xFC..0x170`, 116 bytes, 3) | 368 | 18 |
| `MediumIng` | `0x0..0x150` | 336 | 15 |
| `Metroid` | `0x0..0x17C` | 380 | 18 |
| `Rezbit` | `0x0..0x168` | 360 | 17 |
| `SandBoss` | `0x0..0x178` | 376 | 19 |
| `SwampBossStage1` | `0x0..0x160` | 352 | 17 |
| `SwampBossStage2` | `0x0..0x170` | 368 | 18 |
| `Shredder` | `0x0..0xC8` (was `0x3C..0xC8`, 140 bytes, 12) | 200 | 13 |
(MysteryFlyer's row was `0xFC..0x170` when this was written; it has since been extended to 0x0 - the sentence below is about the table as it stood.) **It is the shortest of the twelve that start at 0x0, and not the shortest head.** `MysteryFlyer`
claims fewer functions (three) and fewer bytes (116); it is excluded only because its claim starts
at 0xFC. The reason BacteriaSwarm is short *from 0x0* is that its loader registration is the module's first function, with no accessor block in front of it - measured, not visible: over the
range each module claims, **BacteriaSwarm's 0xA0 is 40 instructions against IngPuddle's 42**,
IngPuddle's extra two being `fn_32_0` (`addi r3,r3,0x460; blr`) at 0x0. Aligning `fn_6_0` on
`fn_32_8` leaves 40 against 40, and **exactly two instructions differ in encoding, both `bl`** - the
calls to each module's own loader-setter import. A third `bl` differs only in the symbol dtk prints,
`bl fn_6_70` against `bl fn_32_78`: both encode `48000015`, because each branches to the function
immediately after it. So `fn_6_0` is IngPuddle's `fn_32_8` renamed, and `CIngPuddleRel`'s
thirteen-virtual stand-in class reproduces it byte for byte with no new discovery -
`docs/research/raw_offsets.md` has no `CBacteriaSwarmRel` section to add, because no raw offset
appears in the file.
Two details that differ from IngPuddle and that a copy of that file would have got wrong, both
measured:
- **`fn_6_0` is vtable entry 0x3C, not 0x38.** `build/G2ME01/BacteriaSwarm/asm/auto_04_00000000_data.s`
  shows CBacteriaSwarm's vtable at `.data:0x18` (0x98 bytes = 38 words, two leading and 36
  virtuals) and `fn_6_0` at `.data:0x54`, which is offset 0x3C. Its *call target* is 0x38
  (`HealthInfo__6CActorFv`), which is what the stand-in class has to place the thirteenth virtual
  at - so the same class gives the right call, and the accessor's own position does not matter
  because the head does not call it.
- **The loader slot is `.bss:0x10`, not `+0x0`** as IngPuddle's and IngSnatchingSwarm's are, and it
  is `lbl_6_bss_10` rather than `lbl_32_bss_0`. Same shape (`.bss`, 0x4 bytes, `data:4byte`), same
  `extern`-under-MWCC treatment, different name and offset.
**And the import needed no rename, which is worth stating because IngSnatchingSwarm's did.**
`fn_8022A5AC` is the DOL's 0x8022A5AC, 8 bytes, immediately after
`LoadBacteriaSwarm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022A580 (0x2C bytes, so
it ends exactly there). It is `stw r3, gLoader_BacteriaSwarm; blr`, so it stores the *address* of
a loader slot rather than a loader, and `src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp` - a
`Matching` unit that already exists - reads that slot as
`lwz r6, gLoader_BacteriaSwarm; lwz r12, 0(r6); mtctr r12; bctrl`. That file also records why the
setter is deliberately unclaimed in the DOL: REL modules import it by its retail name. The
`CIngSnatchingSwarmRel` section above had to write the long MWCC-mangled
`SetLoader_...__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity` out in full and cost
an item's only build failure; **here the module's own import table is the plain string** -
`strings build/G2ME01/BacteriaSwarm/BacteriaSwarm.preplf | grep 8022A5` gives `fn_8022A5AC` - so the
C++ identifier is the same string, `config/G2ME01/symbols.txt` needs no entry, and the DOL is
untouched. **So the mangled form is the exception, not the rule: read the import out of the
module's own `*.preplf` before writing anything.**
Not added to `files.cmake`, for the reason measured on `CMetareeSwarmRel` and `CIngPuddleRel`: a
host body would reference `fn_6_A0` and `fn_8022A5AC`, which the port cannot link, and
`tools/link_check.sh --strict` fails on a growing undefined count (measured here: 314, unchanged).
`tools/check_files_cmake.py` counts this file under "further units are out because they define a
module entry point (RELMain/RELExit), which collides in a flat link".
### What is left, and the module's function count
`fn_6_A0` (0xA0, 0x74C = 1868 bytes) is the module's entity loader, and the 105 functions from
there up to `fn_6_7300` are CBacteriaSwarm's methods. None is claimed; dtk fills `0xA0..0x734C` from
retail, which is `BacteriaSwarm/auto_00_000000A0_text` (105 functions) in `build/report.json` -
`gate.sh` prints it as `SPLIT BacteriaSwarm/auto_00_00000000_text: 109 function(s) accounted for
across 2 new unit(s) in BacteriaSwarm (exact count match - a split, not a loss)`, which is the
re-split of the former 109-function `auto_00_00000000_text` into our 4 plus dtk's 105. That is
class code and it needs the CActor/CPatterned hierarchy, which is the blocker the item's `reason`
names.
**114 is this module's complete function count, with no unit left out**: 4 ours + 105 unclaimed +
5 setup = 114, and `grep -c "type:function" config/G2ME01/rels/BacteriaSwarm/symbols.txt` is also
114. **This module has no `auto_fn_6_7300_text` unit** - unlike AtomicAlpha and IngSnatchingSwarm,
where such a unit really does exist and is excluded by the rows' counting convention.
`fn_6_7300` is a 76-byte function *inside* `auto_00_000000A0_text`, not a unit of its own. An
earlier draft of this section claimed a 115th function in an `auto_fn_6_7300_text` unit, inherited
from those two modules' rows; it does not exist here, and the claim is corrected in place rather
than left to mislead the next reader.
## `CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict (2026-09-29, goal item `progress-rel-head-tryclops`, lane 1)
**Superseded in part, same day:** the unit now claims `.text 0x0..0x178`, nineteen functions. `fn_81_10` is not blocked; it calls the out-of-line `optional_object<CAABox>` ctor at 0x4FEC, as MysteryFlyer's `fn_45_10` does. The rest of this section (the objdiff-100%-is-not-the-verdict finding) stands.
Tryclops is module 81 and the tenth of the 27 whose `REL_Setup` tail was claimed on 2026-09-28.
This run wrote `src/MetroidPrime/ScriptObjects/CTryclopsRel.cpp` - the module head, `.text
0x4C..0x178`, sixteen functions - and the module's sha1 against `config/G2ME01/config.yml` is
unchanged at `535aee6611c3cc1d6986d99d7d33448e6df5e1f6` (`cmp`-equal to
`orig/G2ME01/files/RelProd/Tryclops.rel`), with all 86 holding and `main.dol` still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `audit_rel_claim.py Tryclops` reports 0 problem claims
(`16/16 functions` inside the claim), 0 of 114 text symbols dropped by `-strip_partial`,
`flip_test.sh` PASS, `unit_fit.sh` `300 claimed / 300 ours / 300 retail, fits`.
The sixteen, from `config/G2ME01/rels/Tryclops/symbols.txt`:
```
0x4C  fn_81_4C  0x10   lbl_8041AAB8 -> *((float*)(self + 0x448))
0x5C  fn_81_5C  0x08   the byte at +0x44F
0x64  fn_81_64  0x08   li r3,0
0x6C  fn_81_6C  0x08   li r3,0
0x74  fn_81_74  0x08   li r3,0
0x7C  fn_81_7C  0x10   *self = kInvalidUniqueId
0x8C  fn_81_8C  0x0C   the byte at +0x34C, bit 3
0x98  fn_81_98  0x0C   lbl_8041B758
0xA4  fn_81_A4  0x08   addi r3,r3,0x754
0xAC  fn_81_AC  0x08   li r3,1
0xB4  fn_81_B4  0x08   li r3,0
0xBC  fn_81_BC  0x1C   three floats from self+0x54 -> *out
0xD8  fn_81_D8  0x2C   virtual dispatch, vtable slot 0x38
0x104 RELExit   0x24   li r3,0 / bl fn_80218D58
0x128 RELMain   0x20   bl fn_81_148
0x148 fn_81_148 0x30   lbl_81_bss_30 = fn_81_178 ; fn_80218D58(&lbl_81_bss_30)
```
### The claim starts at 0x4C, and that is forced, not caution
One unit cannot claim two discontiguous ranges, so taking the block above 0xD8 takes the three
functions in front of it too. `fn_81_0` (0x0, 0x8) and `fn_81_8` (0x8, 0x8) are free - they are the
`fn_2_0` / `fn_2_68` shape, vtable entries of the module's *second* vtable at 0x458 and 0x424 - but
`fn_81_10` (0x10, 0x3C) is **instruction for instruction MysteryFlyer's `fn_45_10`**: a member
function with a hidden return pointer in r3 returning `rstl::optional_object<CAABox>`, whose
converting ctor sets `m_valid` in the mem-init where retail copies the box and then sets the flag,
and whose template instantiation leaves a trailing pool. So extending this claim from 0x4C to 0x0
is blocked on the same problem, and it is worth **19 functions rather than 16** when it opens,
because `fn_81_0` and `fn_81_8` follow for free.
### The block is AtomicAlpha's, and that is measured
`Tryclops`' `.text 0x4C..0xD8` and `AtomicAlpha`'s `.text 0x10..0x9C` are both **0x8C = 140 bytes
and 35 instructions**, with an identical instruction multiset. (46 is the count for the wider ranges
that run on through the vtable entry - `0x4C..0x104` and `0x10..0xC8` - and only for those; an
earlier version of this section quoted 46 for the accessor block, which was wrong by the 11
instructions of `fn_81_D8` / `fn_2_9C`.) The `diff` (extracted from
`build/G2ME01/{Tryclops,AtomicAlpha}/asm/auto_00_00000000_text.s`, the working form of the
extraction `CFishCloudRel` records - the four bytes come *before* the `*/`) moves two lines and adds
none: Tryclops runs three `li r3,0; blr` predicates immediately after the byte read
(`lbz r3, 0x44f(r3)`) where AtomicAlpha runs two, its third sitting later beside the `li r3,0x1`.
**Print the line counts first** - an earlier attempt's first extraction silently produced 0 lines and
the `diff` was then vacuous, which is `docs/PROCESS_LESSONS.md`'s first lesson happening again.
So nothing had to be discovered. Every body is the one `CAtomicAlphaRel.cpp` /
`AtomicBetaAccessors.cpp` carries, and the two odd ones carry over with their caveats: `fn_81_BC` is
three subscript stores because retail interleaves the loads and the stores, and `fn_81_8C`'s dtk
`extrwi r3, r0, 1, 28` is the word `rlwinm r3, r0, 29, 31, 31` (bit 3, not bit 28).
The import needed no rename: `strings build/G2ME01/Tryclops/Tryclops.preplf | grep 80218D` gives the
plain DOL symbol `fn_80218D58`, and `build/G2ME01/asm/auto_03_80218D58_text.s` is
`stw r3, gLoader_Tryclops@sda21(r0); blr`. No mangled `SetLoader_*`, no `symbols.txt` edit, no DOL
change. The loader slot is `lbl_81_bss_30` (`.bss:0x30`, `size:0x4`). `fn_81_D8` is vtable entry
0x3C of a 0x148-byte table at `.data:0x378` (82 words: two leading plus eighty virtuals) whose
slot 0x38 is `HealthInfo__3CAiFv`, so the thirteen-virtual stand-in class is the one
`CIngPuddleRel.cpp` already measures.
**Thirteen of the sixteen, not sixteen, are in the module's FORCEACTIVE list**
(`build/G2ME01/Tryclops/ldscript.lcf`): `fn_81_4C` through `fn_81_D8`, the thirteen accessors and
the vtable entry. `RELExit`, `RELMain` and `fn_81_148` are not in it - an earlier version of this
section claimed all sixteen were. That is not a dead-stripping hazard, and the evidence that none
was stripped is the module's own hash holding plus `audit_rel_claim.py` reporting **0 of 114 text
symbols dropped by `-strip_partial`** (114 in, 114 out).
**Two of this section's own numbers were wrong and are corrected above** (third run, 2026-09-29,
lane 1, after a reviewer rejected the first version of it on exactly this point): the accessor block
is **35 instructions**, not 46 — 46 counts the range extended through the vtable entry — and
**13 of the 16** functions are in FORCEACTIVE, not all 16. Both were re-measured before being
restated, and the identical-multiset claim and the two-line-move claim were re-measured with them
and hold. The reusable lesson is the one this section already preaches further down: a number in a
doc is a claim about the tree, and the cheapest way to be wrong about one is to write it from the
run before last.
### The finding: on a REL unit, objdiff's 100% is not the module's verdict
Worth a lane of anyone's time, and the sharpest instance yet of "a percentage is not a result",
because **every REL-specific check passed** while the module was two bytes wrong.
`fn_81_4C` was first written with a one-character slip:
```cpp
*reinterpret_cast< float* >(static_cast< char* *>(self) + 0x448) = lbl_8041AAB8;   // char**, not char*
```
`mwcceppc` emitted `stfs f0, 0x1120(r3)` - pointer arithmetic on a `char**` advances by
`sizeof(char*)` = **4** on this ABI, so the displacement is `0x448 * 4`. Same length, still "an
offset into an object", nothing in the C++ reads as wrong. Measured with the typo in place:
| check | with the typo | with the fix |
| --- | --- | --- |
| `build/report.json`, per unit | **100.00% fuzzy, 16/16 matched** | 100.00%, 16/16 |
| `tools/unit_fit.sh` | `300 claimed / 300 ours / 300 retail, fits`, no extra functions | same |
| `tools/audit_rel_claim.py Tryclops` | `16/16 functions`, **0 claims with a problem** | same |
| `build.sha1` | **FAILED, 86 files OK** | 87 files OK |
| `cmp -l` vs `orig/.../Tryclops.rel` | bytes 283, 284 | identical |
**Why objdiff could not see it.** dtk's split leaves **two copies of the same object**: the one
objdiff reads, `build/G2ME01/Tryclops/obj/MetroidPrime/ScriptObjects/CTryclopsRel.o`, and the one
`build.ninja`'s `link build/G2ME01/Tryclops/Tryclops.preplf` rule hands to `mwldeppc`,
`build/G2ME01/src/MetroidPrime/ScriptObjects/CTryclopsRel.o`. With the typo the first held
`d0 03 04 48` and the second `d0 03 11 20` - same compile, two objects, different bytes, so the
report was measuring bytes the module did not contain. `objcopy -O binary --only-section=.text` on
both files shows it in one command:
```sh
for o in build/G2ME01/Tryclops/obj/MetroidPrime/ScriptObjects/CTryclopsRel.o \
         build/G2ME01/src/MetroidPrime/ScriptObjects/CTryclopsRel.o; do
  build/binutils/powerpc-eabi-objcopy -O binary --only-section=.text "$o" /tmp/x.bin && xxd -l 16 -p /tmp/x.bin
done
```
Why the split copy differs was not established, only that it does and that it is the one objdiff
reads. The rule to keep is the one this project already has, now with a measurement attached: **on
a REL, the module's sha1 against `config/G2ME01/config.yml` is the only verdict**, and
`cmp -l <built> orig/G2ME01/files/RelProd/<M>.rel` is the cheap second opinion. It printed bytes 283
and 284; this module's `.text` starts at file offset `0xC4` (find it by searching the `.rel` for
`fn_81_0`'s first word `38 63 07 C4`), so those are `.text:0x56`/`.text:0x57` - the low half of the
`stfs` displacement at `.text:0x54` inside `fn_81_4C`, and nowhere else.
**Second habit, worth more than the fix.** `mwcceppc` on one file takes **0.04 s**. Compiling the
unit standalone with the exact `mwcc_sjis` command line from `build.ninja` and running
`objdump -d` on it localises a wrong body in seconds instead of after a full link, and it is
non-vacuous because the object's own bytes are what is being read. The recipe is the `mwcc_sjis`
rule's own command with `-c` added and the output redirected somewhere disposable:
```sh
"$MP_TOOLCHAIN_DIR/build/tools/wibo" build/tools/sjiswrap.exe \
  "$MP_TOOLCHAIN_DIR/build/compilers/GC/1.3.2/mwcceppc.exe" <cflags from build.ninja> -c -o /tmp/u.o src/.../U.cpp
```
Without `-c` the driver aborts with "Can't find linker 'mwldeppc' in path" - it is not a real error.
Then `objcopy -O binary --only-section=.text` and a `cmp` against the retail bytes at the claim's
offset settles it. The expected differences are only relocation fields: for this unit exactly five
bytes, at `.text:0xCA`, `0xEB`, `0x11A` (the three `bl` R_PPC_REL24 displacements) and `0xCB`,
`0x11B` (two R_PPC_ADDR16_LO), everything else identical to retail.
## What is left (not this item)
`fn_81_178` (0x178, 0x30C) is the module's entity loader and the 89 functions from there to
`fn_81_5028` are Tryclops' methods; dtk fills `0x178..0x54EC` as
`Tryclops/auto_00_00000178_text`. Behavioural class code, and it needs the CActor/CPatterned
hierarchy. The cheap step inside it is the same shape as `AtomicAlpha`: nothing below 0x4C is left
unclaimed-by-choice, so the next extension of this claim is the three functions above 0x4C, and
they are waiting on `fn_81_10`.
## The module head is writable without the actor hierarchy (2026-09-29, goal item `progress-rel-head-ingblobswarm`)
`IngBlobSwarm`'s first five functions are ours now, in one `Matching` unit:
`src/MetroidPrime/ScriptObjects/CScriptIngBlobSwarmRel.cpp` claims module `.text 0x0..0xD8`
and reproduces it byte for byte - `fn_31_0`, `fn_31_3C`, `RELExit`, `RELMain` and
`fn_31_A8` - so `matched` went 8828 -> 8833, `linked` 3875 -> 3880, the DOL sha1 held and
all 86 REL hashes matched `config/G2ME01/config.yml`. The class code behind them is still
retail in the unclaimed `auto_00_000000D8_text` range, and that is the point worth keeping:
**a swarm module's head is `.text` wiring, not behaviour, so it is decompilable before
`CActor`/`CPatterned` exist.** The arrangement is `CScriptPlayerProxy.cpp` verbatim, and
three details are not obvious:
- **The loader setter is the DOL's, and its import name is fixed.** `fn_31_A8` stores
  `fn_31_D8` into the module's own `.bss` slot and calls `fn_8022E134` - eight bytes at
  0x8022E134 in the *DOL*, `stw r3,gLoader_IngBlobSwarm@sda21(r0) ; blr`. The name is in
  the module's `.plf` import table, so `SetLoader_IngBlobSwarm` or anything else does not
  link. (Carving it is possible and is what `RsfAudioLoaderSet.cpp` did for that family,
  but nothing here needs it: the relocation already resolves against the unclaimed
  `auto_03_8022E134_text` object.)
- **The `.bss` slot must be `extern` under `__MWERKS__`.** A definition there is a second
  definition in the module link, and `CScriptRsfAudio.cpp` records the
  `ELF_linker.c Line: 5083` internal linker error that comes with it.
- **A REL entry source is not always listed in `files.cmake`.** `fn_31_D8`, the module's
  own 1,036-byte loader, has no body, so compiling this file on the host would take the
  port's undefined count 314 -> 315, which `tools/link_check.sh --strict` fails.
  `tools/check_files_cmake.py` accepts it anyway because the file is a `MODULE_ENTRY`
  source - the same reason `CSwarmBasicsREL.cpp`, `CScriptPlayerActor.cpp` and
  `CScriptPlayerTurretRel.cpp` are out - so the port keeps loading the retail module from
  the disc and the host `mp_relmain_ingblobswarm` is simply never called. Registering it
  needs `fn_31_D8`, so it is queued as a follow-up rather than done here.
Two mwcceppc facts turned up while matching the two accessors, and both are the kind that
cost a build each:
- **`*out = CVector3f(a, b, c)` is what produces retail's load-all-then-store float order.**
  `fn_31_3C` wants `lfs f2,44(r4) ; lfs f1,28(r4) ; lfs f0,12(r4) ; stfs f0,0(r3) ;
  stfs f1,4(r3) ; stfs f2,8(r3)`. Three separate `out->SetX/SetY/SetZ` statements emit
  load-store-load-store and score **58.30%**; the single `*out = CVector3f(...)` assignment
  is 100.00% with the same object size.
- **A `bool : 1` bitfield is what produces a tested bit; masking a byte does not.**
  `result = (byte & 1) != 0` gives `clrlwi r0,r0,31 ; mr r5,r0` and a one-bit result that
  retail never produces. A struct member `bool flag : 1;` read as `result = flag;` gives
  `rlwinm r0,r0,25,31,31` (78.27%) - and only writing it as `if (flag) { result = true; }`
  makes MWCC use the **recording** form `rlwinm.` + `beq` + `li r5,1` that retail has.
  This is the same `const`-local lever as `CGameOptions::ToggleControls`, one level over:
  a bitfield read used as a *value* is not the same code as a bitfield read used as a
  *condition*.
The array's element is 0xB8 and the blob's one-bit "is emitter" flag is at +0xB2 of it;
the blob count is the word at +0x17C and the array pointer the one at +0x184, both read by
the two accessors. A 0xB8 stride with a vector at +0x0C is what the second accessor pins;
the struct in the source is `char[0xB2]`, `bool : 1`, `char[0x5]`, and its **size is
load-bearing** - leaving the trailing pad at 6 bytes gives `mulli r4,r4,185` where retail
has 184, one instruction and a wrong stride.
## `CPillBugRel` is a module head, and the accessor block is already in the DOL (2026-09-29, goal item `progress-rel-head-pillbug`)
(Rescued from the review queue the same day: the lane's code passed every gate but raw-offsets, which
wanted a `docs/research/raw_offsets.md` section for the file; the section was added and nothing else changed.)
PillBug is module 48, one of the 27 modules whose `REL_Setup` tail was claimed on 2026-09-28.
The head is now the whole `.text 0x0..0x130` - **seventeen** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CPillBugRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged**
(`261c9127510cc69a1ce664e6e07dc4165e9d9aca`, `cmp`-equal to `orig/G2ME01/files/RelProd/PillBug.rel`),
with all 86 holding and `main.dol` still `6ef9b491...`. `matched` 9145 -> 9162, `linked` 3985 -> 4002,
the module's own count 5 -> **22 of 82**. `tools/audit_rel_claim.py PillBug` reports 0 problem claims
and 0 of 82 text symbols dropped by `-strip_partial`; `tools/check_decl_order.py --unit
PillBug/MetroidPrime/ScriptObjects/CPillBugRel` is ok; `tools/unit_fit.sh` reports
`.text claimed 304 ours 304 retail 304 fits` with no extra functions; `tools/flip_test.sh` on the
unit reports `PASS -> kept as Matching`.
**The measurement that made this cheap is one grep, and it should be run before writing any C++ for
a module head.** The advice above was to diff the module's first 0x200 bytes against a head that
already landed. The better check is that the *thirteen short accessors* at `0x0..0x90` are the block
the REL loader generator emits at the head of every scripted-actor module, and they **already exist
in this tree, in the DOL, at 100%** - they share the three globals the relocations name,
`lbl_8041AAB8`, `kInvalidUniqueId` and `lbl_8041B758`, all of which live in the DOL, so one body
serves every module of the family:
    grep -l lbl_8041AAB8 src/
That returns 24 files, of which the `*Accessors.cpp` units are the block;
`MetroidPrime/ScriptObjects/GlowbugAccessors.cpp` is the reference body. 
**But the blocks are not interchangeable, and that is the trap.** PillBug's is thirteen accessors plus
a vtable call; `EmperorIngStage2Tentacle`'s is fourteen accessors with **no** vtable call - it has a
`bool : 1` flag test at `+0x34c` (`rlwinm r3,r0,29,31,31`, its `fn_17_48`) where PillBug has
`fn_48_90`'s `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`. The two blocks differ by exactly
those functions - 188 bytes against 156 - **and every offset after `0x38` is shifted by 0x20.** Read
the offsets out of the module's own `symbols.txt`; never copy an offset from a sibling's file.
Two spellings transferred without being re-found:
- **`fn_48_74` is three stores, not a `CVector3f`.** Retail reuses f0 for all three
  (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`) - interleaved
  load/store pairs - which is `fn_17_80`'s shape exactly and the **opposite** of `fn_43_3C` in
  `CMetareeSwarmRel.cpp`, where the three floats had to be *built* through `CVector3f`'s
  three-argument constructor because retail loaded all three before storing any (58.30% -> 100.00%).
  **Both shapes are real and the disassembly says which: interleaved load/store pairs are a copy,
  all-loads-first is a constructor call.**
- **`fn_48_90` is a vtable call, and the slot is `CAi`'s `HealthInfo`, not `CActor`'s.**
  `.data:0x1CC` is CPillBug's 83-word table, with `fn_48_90` at offset 0x3C, one after
  `HealthInfo__3CAiFv` at 0x38 - the table is **CAi's**, `TypesMatch__8CPillBugCFi` and
  `DamageVulnerability__3CAiFv` are in it - so it needs fourteen virtuals where CSnakeWeedSwarm's
  CActor table needed 29. As at `CIngPuddleRel.cpp` a stand-in class of thirteen virtuals puts the
  *called* one at `(0x38 - 8) / 4` = index 12 and gives retail's twelve instructions byte for byte;
  none of the slots is defined, because the only object carrying the vtable is the module's own
  retail bytes.
`RELMain`, `RELExit` and `fn_48_100` are the `CScriptPlayerProxy.cpp` / `CMetareeSwarmRel.cpp`
arrangement unchanged, including the `extern`-under-MWCC `.bss` slot (`lbl_48_bss_0` is `.bss:0x0`,
`size:0x4`). The setter is `fn_80200F30`, the DOL's 0x80200F30, two instructions,
`stw r3, -0x6A10(r13); blr`; that displacement resolves through `_SDA_BASE_` to 0x80419370, which
`config/G2ME01/symbols.txt` names **`gLoader_PillBug`**, and `src/MetroidPrime/ScriptLoader/PillBug.cpp`
reads it as `(*gLoader_PillBug.value)(mgr, input, info)`. `tools/sda.py` is the supported way to make
that resolution - reading the displacement against the wrong base gives a plausible wrong answer.
**Not added to `files.cmake`**, for the same reason as `CMetareeSwarmRel.cpp` above: a host body
would reference `fn_48_130` and `fn_80200F30`, which the port cannot link.
### What is left
`fn_48_130` (0x130, 0x5A4) is the module's entity loader, and the 59 functions from there to
`fn_48_402C` are CPillBug's methods. None is claimed; dtk fills `0x130..0x402C` from retail. That is
class code and needs the CActor/CPatterned/CAi hierarchy - the blocker the item's `reason` names, and
the same one the other four heads stopped at. The module is 82 functions: 59 in
`auto_00_00000130_text`, 1 in `auto_fn_48_402C_text`, 5 in `REL_Setup`, 17 claimed here.
## `CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order (2026-09-29, goal item `progress-rel-head-darktrooper`)
DarkTrooper is module 12, one of the 27 modules whose `REL_Setup` tail was claimed on 2026-09-28.
The head is now the whole `.text 0x0..0x12C` - **sixteen** functions, all 100.00%,
`src/MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp` - and the module's sha1 against
`config/G2ME01/config.yml` is **unchanged**
(`f216a5cdbf86753fa7d3e8a49b7e0344fbb2dbca`, `cmp`-equal to `orig/G2ME01/files/RelProd/DarkTrooper.rel`),
with all 86 holding and `main.dol` still `6ef9b491...`. `matched` 9185 -> 9201, `linked` 4011 -> 4027,
REL units 1138 -> 1154, the module's own count 5 -> **21 of 172**, and
`tools/check_module_wiring.py` **63 units in 49 modules -> 64 in 50**. `tools/audit_rel_claim.py
DarkTrooper` reports 0 problem claims and 0 of 172 text symbols dropped by `-strip_partial`;
`tools/check_decl_order.py` is ok; `tools/unit_fit.sh` reports `.text claimed 300 ours 300 retail
300 fits` with no extra functions; `tools/flip_test.sh` reports `PASS -> kept as Matching`.
**The claim reaches 0x0 because the head and the accessor block happen to be adjacent, and the two
must be read as one run rather than two.** `RELExit` is at **0xB8**, not at 0x0, and thirteen
functions sit in front of it - twelve accessors and `fn_12_8C`, the vtable call, where PillBug runs
one longer (`fn_48_0`..`fn_48_74` is thirteen accessors, plus its own `fn_48_90`) - so
`CMysteryFlyerRel.cpp`'s loader-trio claim, `0xFC..0x170`, and
`CBacteriaSwarmRel.cpp`'s `0x0..0xA0` are the two shapes this module does not have: here the claim is
0x0..0x12C, the accessor block plus the trio, or nothing. One unit cannot claim two discontiguous
ranges, so the alternative would be two units and a gap between them. Worth reading the module's
`symbols.txt` for where `RELMain` actually sits before deciding a head is only a trio - in
`MetareeSwarm` it is at 0x88 and in `IngPuddle` and `IngSnatchingSwarm` at 0x58, so the trio is *not*
at the module's start in this family either.
**The block is `CPillBugRel`'s, and the diff between the two modules is small enough to state as
facts - which is what made this cheap.** `diff` of the two `auto_00_00000000_text` listings over
their accessor runs, checked non-empty, moves three things and adds none:
| | PillBug (48) | DarkTrooper (12) |
|---|---|---|
| at 0x0 | `fn_48_0`, the 0x10-byte float store at +0x448 | `fn_12_0`, 8 bytes, `addi r3,r3,0x7c0` - the address of a member at +0x7C0 |
| the `li r3,0; blr` run | four, at 0x18/0x20/0x28/0x30 (six over the block) | one, at 0x20 (three over the block) |
| the `+0x34C` flag bit | absent | `fn_12_38`, `lbz r0,0x34c(r3) / extrwi r3,r0,1,28 / blr` |
Everything else - the float store (0x08 here, 0x00 there), the byte at +0x44F, the unique-id reset,
the constant float, `&self[0x754]`, the `true`/`false`/`false` tail, the 12-byte copy and the
12-instruction vtable call - is the same body in both, so **no spelling had to be discovered** and
the run took one build. Two spellings transferred with it, both from `CPillBugRel.cpp`: the copy is
interleaved load/store (a copy, not a `CVector3f` constructor call) and the vtable call needs a
thirteen-virtual stand-in class.
**The flag bit's encoding transferred with the rest of the block, verbatim.** The source is
`AtomicBetaAccessors.cpp`'s `fn_5_48` - `(self[0x34C] & 8) != 0`, over the same offset, the same
fourth `bool : 1` of that byte - and the two objects hold the same three instructions,
`88 03 03 4C / 54 03 EF FE / 4E 80 00 20`, with `extrwi r3, r0, 1, 28` in both listings and one
copy of that body in `orig/G2ME01/files/RelProd/AtomicBeta.rel`. **An earlier draft of this section
claimed the encoding did *not* transfer, and said AtomicBeta compiled the same source to
`rlwinm r3,r0,29,31,31`; that was copied from the sibling's own comment at
`AtomicBetaAccessors.cpp:57` and is superseded.** That word is stale everywhere it appears - 18
`*Accessors.cpp` files carry the same comment and `rlwinm r3, r0, 29, 31, 31` appears nowhere in
`build/` - so the sibling comment is the thing to correct, not this module. A lane that had trusted
it would have written the wrong instruction here.
**The import needed no rename, which is worth saying because `CIngSnatchingSwarmRel`'s did.** The
setter is the plain DOL symbol `fn_80218DF4`, read out of the module's own `DarkTrooper.preplf` -
the same shape as `BacteriaSwarm`'s `fn_8022A5AC` and `Tryclops`'s `fn_80218D58`, so
`config/G2ME01/rels/DarkTrooper/symbols.txt` and the DOL are untouched. The loader slot is
`lbl_12_bss_0` at `.bss:0x0`, `size:0x4`, so the `extern`-under-MWCC arrangement applies unchanged.
`fn_12_8C` is vtable entry **0x3C** of the 106-word table at `.data:0x2BC` calling slot 0x38
(`HealthInfo__3CAiFv`), the `fn_6_0` / `fn_32_8` shape, so the same stand-in class lands it.
**Not added to `files.cmake`**, for the same reason as the other module heads: a host body would
reference `fn_12_12C` and `fn_80218DF4`, which the port cannot link, and the probe's regression gate
is a hard failure on a growing undefined count. Measured here: **259 -> 259 undefined, 0
duplicates**. `tools/check_files_cmake.py` accepts the omission because the file defines `RELMain`
and `RELExit`, the `MODULE_ENTRY` rule every head relies on.
### What is left
`fn_12_12C` (0x12C, 0x614) is the module's entity loader, and the 151 functions from there to
`fn_12_6E58` are CDarkTrooper's methods. None is claimed; dtk fills `0x12C..0x6E58` from retail. That
is class code and needs the CActor/CPatterned/CAi hierarchy - the same blocker the other heads
stop at. The module is 172 text functions: 150 in `auto_00_0000012C_text`, 1 in
`auto_fn_12_6E58_text`, 5 in `REL_Setup`, 16 claimed here.
## `CGameState` 72 -> 86: two reviewer-rejected lanes, landed code-only (2026-09-29)
Goal items `progress-cgamestate-bodiless-runs` (lane 1, run 11) and `progress-cgamestate-partial-16`
(lane 1, run 4) both passed `goal_check.sh`, and both reviewers said the code was right and rejected
the diff over its doc prose (unmeasured or stale numbers). Both were set aside after two failures. They
were re-applied on the current tip with **only their `src/` and `include/` hunks** (`partial-16`
needed `git apply --3way`), and the docs were written from fresh measurements. `main.dol` is still
`6ef9b491...`. The two lanes' notes stay in `build/goal/notes/` in the goal worktree.
Measured on the unit from `build/report.json`, in two steps (bodiless-runs first: the `no body` rows; then partial-16: the rest):
| function | before | after | spelling |
|---|---|---|---|
| `fn_801447C4` (CHintOptions copy-assign) | no body | 100.00% | `extern "C"`, declared *before* the class so the `friend` names the C-linkage entity |
| `fn_801465A8` | no body | 100.00% | `{u32, float, float}` per element, `dst` null-tested inside the loop |
| `fn_801467A0`, `fn_801467C0`, `fn_801435D4`, `fn_80142718`, `fn_80142738` | no body | 100.00% | 36-byte element forwarders and destroy loop |
| `fn_8014601C` | no body | 99.05% | two-word out-parameter over `fn_80145BDC` |
| `fn_801426E0` | no body | 97.50% | count bumped *before* the element is built |
| `fn_8014680C` | no body | 92.69% | range copy from a by-address begin/end |
| `fn_801466F4` | no body | 66.63% | the four-word stack range built by separate assignments |
| `ConfigureGameModeLayers` | 94.64% | 100.00% | `(second - type) == 0`, table value on the **left**; `a == b` and `type - second == 0` give `subf` in the wrong operand order. The rc_ptr's pointee is bound to a named `CWorldLayerState&` |
| `StateForWorld` | 80.66% | 100.00% | loop *breaks* to one end test; `it` hoisted, `end` re-read at each test |
| `SetCinematicState` | 76.45% | 100.00% | `push_back_unsafe` after `reserve`, plus a `construct_impl` overload for `pair<CAssetId, TEditorId>` in `CPersistentOptions.hpp` (no null test on placement) |
| `CEnvironmentVariable` ctor / `PutTo` | 76.43 / 70.41% | 100.00% | read back the stored members, not the parameters; the difference into a local before `GetBitCount` |
| `FindEnvironmentVariable` | 81.08% | 100.00% | `end` bound to a local declared **after** the find, then `it != end ? ... : nullptr` |
| `InitializeMemoryWorlds` | 99.57% | 100.00% | the three `InitializeWorldLayers` arguments as named locals, in r4/r5/r6 order |
| `AddVariable` | 82.88% | 87.78% | the same `end` hoist, `it == end` polarity |
| `CPersistentOptions::PutTo` | 84.67% | 94.35% | `push_back_unsafe` |
**Walls still open**, from the lanes' notes and not re-measured here: `fn_8014601C`, `fn_801426E0` and
`fn_8014680C` differ only in the epilogue's r0/r31/r30 reload order. `fn_801466F4` needs the stack
range's four stores in retail's order.
## A `files.cmake` head needs `#ifdef __MWERKS__`, and the failure is `link_gap`, not `build.sha1` (2026-09-29, goal item `progress-rel-extend-shredder`, lane 2)
`Shredder`'s claim was `.text 0x3C..0xC8`, twelve accessors, and the 60 bytes below it held
`fn_68_0` - a `GetBoundingBox` wrapper, the one function every head in this family opens with. The
claim now runs `0x0..0xC8`: 13/13 functions at 100.00%, module sha1
`a70ac4a192c4ea785af74c09179b6fd64c8ed382` unchanged, `.rel` `cmp`-identical, all 86 holding,
`main.dol` `6ef9b491...`, `matched` 9176 -> 9177, `linked` 4002 -> 4003, the module's own count
12 -> 13 of 62 (`auto_00_000000C8_text` 43 + `auto_fn_68_2888_text` 1 + `auto_00_00002940_text` 5 +
the 13 here, which is all 62 text symbols `audit_rel_claim.py` prints and 0 are dropped by
`-strip_partial`).
**The body was not the hard part; the host build was.** `fn_68_0` is instruction for instruction
`CMysteryFlyerRel.cpp`'s `fn_45_10` (module 45, landed two commits before this item), so the
spelling transferred without being re-found - 15 instructions, frame 0x30,
`GetBoundingBox` in r4 alongside the hidden return pointer, so `self` needs no move:
    void fn_68_0(void* out, const CPhysicsActor* self) {
      fn_68_284C(out, self->GetBoundingBox());   // .text 0x284C, unclaimed
    }
`fn_68_284C` is the module's own out-of-line `optional_object<CAABox>` converting constructor (six
words copied, then `stb 1,0x18(r3)`); it is **not** in this claim and is called by name. The
parameter must be `const CAABox&`: by value the frame grows to 0x40 and the unit stops matching.
`CPhysicsActor` is the one-method stand-in, not `MetroidPrime/CPhysicsActor.hpp` - that header
reaches `Collision/CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of `.data` in the
object, and the module's sha1 breaks on it with every function still at 100%.
**Every other head in this family is deliberately *absent* from `files.cmake`** - a head body would
reference the module's own functions, which the port cannot link, and the port's link gap would
grow. The accessor units are the exception, and all 20 of them **are** listed, this one among them:
they were safe because they read raw offsets and DOL globals and call nothing. `fn_68_0` breaks
that, and **the failure is not a byte diff**: it is `gate.sh`'s `link_gap` step, and a green
`build.sha1` and a green `All:` line will not show it. The fix is the `#ifdef __MWERKS__` arrangement
`CScriptWallCrawler.cpp` already uses for `RELMain`/`RELExit`, around the include, the stand-in, the
declaration and the body. The port reads `Shredder.rel` off the disc through `platform/rel.cpp` and
never calls into the module, so it costs nothing, and the MWCC branch is the retail source token for
token so the matching build cannot see it.
The check is one command, not a gate run, and it is worth doing **before** proposing any function
that calls out of a module in a file `files.cmake` already lists - which now means every one of the
20 accessor units, not just this one:
    # the host g++ line out of tools/probe_sources.sh, with the guard removed, then:
    build/binutils/powerpc-eabi-nm -u <that>.o
**Declare in reverse, and note what failed to catch a permuted claim.** `fn_68_0` is at retail
0x0, the lowest offset in the claim, so it is declared **last** in the file. Declared first, the
module's bytes came out permuted: `Shredder.rel` FAILED with `86 files OK`, and `cmp -l` showed the
`.text` running 0x8C further from the file head than retail's - a 0x8C shift. **objdiff reported
13/13 at 100.00% and `unit_fit.sh` said "fits" the whole time.** `check_decl_order.py --unit` printed
`ok` on the permuted source too, because it compares the *built* object's symbol order against
`build/report.json` and the build had not been re-run, so it was still looking at the previous
(correct) object - it is a pre-flip check for `NonMatching` units and **needs a build behind it**.
The instruments that saw it were the module sha1 and the `cmp`.
### The family, measured rather than assumed
The obvious follow-up - "every accessor module has a wrapper in front of it, so take it" - is
**half right, and the half that is wrong costs a session.** Measured across all 20 modules whose
claim is a `*Accessors.cpp`, from each `config/G2ME01/rels/<M>/splits.txt`:
| gap below the claim | modules | the function at 0x0 is |
| --- | --- | --- |
| 0 | `AtomicBeta`, `EmperorIngStage2Tentacle`, `Kralee`, `OctapedeSegment`, `WallWalker` | already claimed |
| **0x3C (60)** | **`Shredder` (now), `Ripper` (now), `EyeBall`, `Krocuss` (now)** | **`GetBoundingBox` wrapper, 0x3C bytes** |
| 0x44 (68) | `IngSpiderballGuardian` | a different 0x44-byte head |
| **0x3C (60)** | **`Shredder` (now), `Ripper`, `EyeBall` (now), `Krocuss` (now)** | **`GetBoundingBox` wrapper, 0x3C bytes** |
| 0x44 (68) | `IngSpiderballGuardian` (now) | a 0x08-byte predicate at 0x0, then the 0x3C wrapper at 0x8 |
| 0x78 (120) | `DigitalGuardian` | a different 0x78-byte head |
| 0x2F0 (752) .. 0x38C (908) | `PuddleSpore`, `Sporb`, `WispTentacle`, `SpankWeed`, `GunTurret`, `Glowbug`, `StoneToad` | **not a copy job - hundreds of bytes of module code** |
So the one-function extension is real and was three modules, and each was verified per module
rather than assumed: `Ripper` `fn_54_0`, `EyeBall` `fn_19_0` and `Krocuss` `fn_38_0` are each
`0x3C` bytes and each is `mr r31,r3 / bl GetBoundingBox__13CPhysicsActorCFv / bl fn_<mod>_<ctor>` -
the same wrapper, with the module number substituted. **Re-measure each module's
`config/G2ME01/rels/<M>/symbols.txt` before assuming the function below 0x0 is a `GetBoundingBox`
wrapper; a 0x2F0-byte gap is not the same problem as a 0x3C-byte one.** Of the three, `Shredder`,
`Ripper` and `Krocuss` have been taken; `EyeBall` has not. (`Ripper` is wired as of
the same wrapper, with the module number substituted. `EyeBall` and `Krocuss` are now claimed;
`Ripper` is the only one of the three still open, and it is the last clean instance of this shape.
`IngSpiderballGuardian` was the fourth to land and is the one that falsifies "the wrapper is always
at 0x0": there the head is 0x44 bytes and holds *two* functions, so the wrapper is at 0x8.
**Re-measure each module's `config/G2ME01/rels/<M>/symbols.txt`, and read
`build/G2ME01/<Module>/asm/auto_00_00000000_text.s` before assuming the function below 0x0 is a
`GetBoundingBox` wrapper: a 0x2F0-byte gap is not the same problem as a 0x3C-byte one, and a 0x44-byte
gap is not a 0x3C-byte gap either.** (`Ripper` is wired as of
2026-09-29: `check_module_wiring.py` reports 63 units of our own code in 49 modules and names it,
and `include/MetroidPrime/Enemies/CPatterned.hpp` exists - the "blocked, no CRipper/CPatterned"
note from an earlier run is superseded, though only for the head: `fn_54_178` is still class code.)
### What is left
`fn_68_C8` (0xC8) and the 49 functions above it are Shredder's own members and need the
CActor/CPatterned hierarchy. The module has no `REL_Setup` claim, so 13 + 49 = 62 is the complete
denominator here.
## `Krocuss` is the third `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-krocuss`, lane 2)
`Krocuss`'s claim was `.text 0x3C..0xD8`, fourteen accessors, and the 60 bytes below it held
`fn_38_0` - the `GetBoundingBox` wrapper that opens every head in this family. The claim now runs
`0x0..0xD8`: **15/15 functions at 100.00%**, module sha1
`fec35d1d4bd7e6815c37af398be8c1b0ff2864fa` unchanged against `config/G2ME01/config.yml` and equal to
`orig/G2ME01/files/RelProd/Krocuss.rel`, the `.rel` `cmp`-identical, all 86 holding, `main.dol`
`6ef9b491...`, `matched` 9177 -> 9178, `linked` 4003 -> 4004, the module's own count 14 -> **15 of
65** (`tools/audit_rel_claim.py Krocuss`: 65 text symbols in the preplf, 65 in the plf, 0 dropped by
`-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra
functions** - the object defines only what the retail unit object does.
**Nothing had to be re-derived.** `fn_38_0` is 15 instructions and is byte-identical to
`CMysteryFlyerRel.cpp`'s `fn_45_10` and to `ShredderAccessors.cpp`'s `fn_68_0`, so the body
transferred verbatim:
    void fn_38_0(void* out, const CPhysicsActor* self) {
      fn_38_22F0(out, self->GetBoundingBox());   // .text 0x22F0, unclaimed
    }
`fn_38_22F0` is this module's own out-of-line `optional_object<CAABox>` converting constructor
(`build/G2ME01/Krocuss/asm/auto_00_000000D8_text.s`: `li r0,1`, six `lwz`/`stw` pairs, `stb r0,0x18(r3)`,
`blr`) - it is at 0x22F0, in `auto_00_000000D8_text`, and stays unclaimed, so it is called by name.
`const CAABox&` is load-bearing again: by value the frame grows to 0x40 and the unit stops matching.
Retail takes the box by address in a 0x30 frame and needs no move for `self`, because
`GetBoundingBox__13CPhysicsActorCFv` takes `this` in r4 as well. The one-method `CPhysicsActor`
stand-in is not optional: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`,
whose file-scope statics put 0x28 bytes of `.data` in the object, and the module's sha1 breaks on it
with every function still at 100%.
**The `#ifdef __MWERKS__` guard is what the item actually turned on.** `KrocussAccessors.cpp` is
listed in `files.cmake` (like all 20 `*Accessors.cpp` units there), and the module *head* files are
deliberately not, because a head body makes the host port link the module's own functions. Measured
with `tools/link_check.sh` after the change: **unique undefined symbols 259, duplicate definitions 0** -
unchanged, so the guard costs the port nothing and `gate.sh`'s `link_gap` step is green. The failure
mode without it is that step and not a byte diff: `build.sha1` and the `All:` line both stay green
while the port's gap grows. The port reads `Krocuss.rel` off the disc through `platform/rel.cpp` and
never calls into the module, exactly as `CScriptWallCrawler.cpp` arranges for its `RELMain`/`RELExit`.
### What is left here
`fn_38_D8` (0xD8) and the 49 functions above it in `auto_00_000000D8_text` are Krocuss's own members
and need the CActor/CPatterned hierarchy. `Krocuss` has no `REL_Setup` claim, so 15 + 44 + 1 + 5 = 65
is the complete denominator `audit_rel_claim.py` prints.
## `IngSpiderballGuardian` is the fourth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-ingspiderballguardian`, lane 2)
`IngSpiderballGuardian`'s claim was `.text 0x44..0xD8`, thirteen accessors, and the 68 bytes below it
held **two** functions, not one. The claim now runs `0x0..0xD8`: **15/15 functions at 100.00%**,
module sha1 `2c171d03c7ee30a71249350731ad43256c96098d` unchanged against
`config/G2ME01/config.yml` and equal to `orig/G2ME01/files/RelProd/IngSpiderballGuardian.rel`, the
`.rel` `cmp`-identical, **all 86** REL sha1s re-checked and holding, `main.dol` `6ef9b491...`,
`matched` 9178 -> 9180, `linked` 4004 -> 4006, the module's own count 13 -> **15 of 87**
(`tools/audit_rel_claim.py IngSpiderballGuardian`: 87 text symbols in the preplf, 87 in the plf, 0
dropped by `-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216,
**no extra functions**. `check_symbol_names.py` 484 units / 0 missing; `check_raw_offsets.py` 128
sites in 46 files, all documented, unchanged; `probe_sources.sh` 727 source files (then) / 0 failed and the port
link **259 undefined, 0 duplicates**, equal to the baseline.
**The one thing to re-measure, and it is not the wrapper.** `Krocuss` and `Shredder` each had one
function below the accessor block and it was the wrapper itself, at 0x0. Here there are two, so the
wrapper is at **0x8** and `fn_35_0` sits in front of it. The dtk `fn_<id>_<off>` name does not say
which is which, so the head's own dtk output - `build/G2ME01/IngSpiderballGuardian/asm/auto_00_00000000_text.s`,
68 bytes, exactly the range being added - is the only place to read it:
    fn_35_0   0x0  0x08  li r3,0x1 / blr
    fn_35_8   0x8  0x3C  stwu r1,-0x30(r1); mflr r0; stw r0,0x34(r1); stw r31,0x2c(r1)
                   mr r31,r3; addi r3,r1,0x8; bl GetBoundingBox__13CPhysicsActorCFv
                   mr r3,r31; addi r4,r1,0x8; bl fn_35_4030
                   lwz r0,0x34(r1); lwz r31,0x2c(r1); mtlr r0; addi r1,r1,0x30; blr
`fn_35_8` transferred verbatim from `CMysteryFlyerRel.cpp`'s `fn_45_10`, the same 15 instructions:
    void fn_35_8(void* out, const CPhysicsActor* self) {
      fn_35_4030(out, self->GetBoundingBox());
    }
`fn_35_4030` (`build/G2ME01/IngSpiderballGuardian/asm/auto_00_000000D8_text.s`) is this module's own
out-of-line `optional_object<CAABox>` converting constructor - `li r0,1`, six `lwz`/`stw` pairs,
`stb r0,0x18(r3)`, `blr` - and it sits in `auto_00_000000D8_text`, so it stays unclaimed and is
called by name. `const CAABox&` is load-bearing again: by value the 0x30 frame grows to 0x40 and the
unit stops matching. The `CPhysicsActor` stand-in is the one-method local class, not
`MetroidPrime/CPhysicsActor.hpp`, whose `CMaterialList` statics would put 0x28 bytes of `.data` in
the object and break the module sha1 with every function at 100%.
`fn_35_0` is `bool fn_35_0(void*) { return true; }` - `li r3,1; blr`, a real body, the same
always-true predicate `fn_45_0` and `fn_38_0`'s neighbours carry, and not a stub for the wrapper.
**The `#ifdef __MWERKS__` guard is `KrocussAccessors.cpp`'s and is load-bearing here too.** The
head bodies are not listed in `files.cmake` because a head body makes the host port link the module's
own functions, which it cannot. `IngSpiderballGuardianAccessors.cpp` *is* listed, and it was safe
only because it read raw offsets and DOL globals; `fn_35_8` makes the host link `fn_35_4030` and a
host-mangled `CPhysicsActor::GetBoundingBox`. `probe_sources.sh` measured 259 undefined both before
and after, so with the guard the host build is byte-identical; without it `link_gap`, and not
`build.sha1`, is the step that would have failed.
### What is left here
`fn_35_D8` (0xD8) and the 66 functions above it in `auto_00_000000D8_text` are
IngSpiderballGuardian's own members and need the CActor/CPatterned hierarchy.
`IngSpiderballGuardian` has no `REL_Setup` claim, so 15 + 66 + 1 + 5 = 87 is the complete
denominator `audit_rel_claim.py` prints.
## `Ripper` is the fourth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-ripper`, lane 1)
`Ripper`'s claim was `.text 0x3C..0xD8`, fourteen accessors, and the 60 bytes below it held
`fn_54_0` - the `GetBoundingBox` wrapper that opens every head in this family. The claim now runs
`0x0..0xD8`: **15/15 functions at 100.00%**, module sha1
`f3ab11c967c58f4483a4264fbeb1ba4a837e8719` unchanged against `config/G2ME01/config.yml` and equal to
`orig/G2ME01/files/RelProd/Ripper.rel`, the `.rel` `cmp`-identical, `main.dol` `6ef9b491...`,
`matched` 9178 -> 9179, `linked` 4004 -> 4005, the module's own count 14 -> **15 of 56**
(`tools/audit_rel_claim.py Ripper`: 56 text symbols in the preplf, 56 in the plf, 0 dropped by
`-strip_partial`, 0 problem claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra
functions**. `check_decl_order.py`: ok. The unit was already `Matching` and stays `Matching`, so
`flip_test.sh` was not run - `AGENTS.md` names the module sha1 plus the `cmp` as the acceptance
test for a REL unit, and both hold.
**Nothing had to be re-derived, a third time.** `build/G2ME01/Ripper/asm/auto_00_00000000_text.s`
is 15 instructions and is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10` and
`KrocussAccessors.cpp`'s `fn_38_0`, so the body transferred verbatim:
    void fn_54_0(void* out, const CPhysicsActor* self) {
      fn_54_158C(out, self->GetBoundingBox());   // .text 0x158C, unclaimed
    }
`fn_54_158C` is this module's own out-of-line `optional_object<CAABox>` converting constructor - it
is the **last** function in `auto_00_000000D8_text`, and the same 20 instructions as MysteryFlyer's
`fn_45_2BBC` and Tryclops's `fn_81_4FEC` - so it stays unclaimed and is called by name.
`const CAABox&` is load-bearing again: by value the frame grows to 0x40 and the unit stops matching.
Retail takes the box by address in a 0x30 frame and needs no move for `self`, because
`GetBoundingBox__13CPhysicsActorCFv` takes `this` in r4 as well. The one-method `CPhysicsActor`
stand-in is not optional: `MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`,
whose file-scope statics put 0x28 bytes of `.data` in the object, and the module's sha1 breaks on it
with every function still at 100%.
**The `#ifdef __MWERKS__` guard is not optional in this file**, unlike in the module *head* files,
and that is the one thing about the recipe that is per-file rather than per-module.
`RipperAccessors.cpp` **is** listed in `files.cmake` - it is one of the 20 `*Accessors.cpp` units
there, and the head files are deliberately not - so a host-compiled `fn_54_0` would make the port
link `fn_54_158C` and a host-mangled `CPhysicsActor::GetBoundingBox`. The first attempt at this item
measured the failure directly: **`link_check: STRICT FAIL - regression gate: 260 undefined against
a baseline of 259 (GREW)`, `NEW fn_54_158C`**, and `build.sha1` and the `All:` line were both still
green while it happened. The Krocuss form - the whole block, stand-in and include, inside the guard -
avoids it without the host-only definition the first attempt used, and leaves the port compiling
exactly what it compiled before.
### What is left here, and what the next lane has
**41 functions are still unclaimed** (56 in the module, 15 claimed): 35 in
`auto_00_000000D8_text`, 5 in `auto_00_00001618_text` and `fn_54_158C` itself in
`auto_fn_54_15C8_text`. The first four are a contiguous 0xA0-byte block already reproduced for
MysteryFlyer, Tryclops and the rest of this family:
- `fn_54_D8` (0xD8, 0x2C) - `lwz r12,0(r3); lwz r12,0x38(r12); mtctr; bctrl`, a vtable dispatch:
  `CMysteryFlyerRel.cpp`'s `fn_45_D0` plus a thirteen-virtual stand-in class.
- `fn_54_104` (0x104, 0x24) - `li r3,0; bl SetLoader_Ripper__…P7CEntity`: `RELExit`.
- `fn_54_128` (0x128, 0x20) - `bl fn_54_148`: `RELMain`.
- `fn_54_148` (0x148, 0x30) - the loader registration, `CMysteryFlyerRel.cpp`'s `fn_45_140`.
Two traps for that step, both visible in the tree now: the import is the **long MWCC-mangled** form
`SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`, so unlike
BacteriaSwarm's and Tryclops's plain `fn_802…` setter it needs the module's `symbols.txt` renamed
the way IngSnatchingSwarm's was; and `.text 0xD8` sits *inside* the existing
`auto_00_000000D8_text` unit, so taking `0xD8..0x178` is a **sub-range carve of one existing auto
unit**, not a new head.
**The wall is `fn_54_178`** (0x178, **0x35C** = 860 bytes), the module's entity loader, and it is
where the earlier "blocked, no `CRipper`/`CPatterned`" note still bites: it opens
`stwu r1,-0x790(r1)`, saves `r23`..`r31`, and its first act is
`addi r26,r1,0x430; bl __ct__20SLdrEditorPropertiesFv`, followed by `LoadTypedefSLdrEditorProperties`
and `__dt__20SLdrEditorPropertiesFv`. That is class construction, not wiring, and it needs the
CActor/CPatterned hierarchy this tree does not model. Everything from there to the end of the module
is the same story.
### And a correction worth keeping
**The loader generator does not put this block in front of every scripted-actor module, and an
earlier version of this section claimed it did.** `MetareeSwarm` and `PlantScarabSwarm` are loader
heads with **no accessor block at all**: their `0x0..0xD8` is five unrelated functions
(`fn_43_0`, `fn_43_3C`, `RELExit`, `RELMain`, `fn_43_A8` in MetareeSwarm's case), `lbl_8041AAB8`
appears nowhere in either module's disassembly, and `0x448` - the offset `fn_54_3C` stores the
default float at - appears nowhere in either module either. So "0x9C accessor block at the head" is
a property of the `*Accessors.cpp` family, not of the loader generator in general, and a head
skipped over it (`IngPuddle`, `BacteriaSwarm`, `FishCloud`) is a different shape rather than a
contradiction.
## `EyeBall` is the fifth `*Accessors.cpp` head extended past the wrapper (2026-09-29, goal item `progress-rel-extend-eyeball`, lane 2)
`EyeBall`'s claim was `.text 0x3C..0xD8`, fourteen accessors, and the 60 bytes below it held
`fn_19_0` - the `GetBoundingBox` wrapper that opens this family of heads. The claim now runs
`0x0..0xD8`: **15/15 functions at 100.00%**, `flip_test.sh` **PASS** ("kept as Matching"),
module sha1 `96c3406ac17b2e275b05b890070c4eeac75baa72` unchanged against
`config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/EyeBall.rel`, the `.rel`
byte-identical, `main.dol` `6ef9b491...`, `matched` 9180 -> 9181, `linked` 4006 -> 4007, the
module's own count 14 -> **15 of 68** (`tools/audit_rel_claim.py EyeBall`: `15/15 functions` in
the claim, 68 text symbols in the preplf, 68 in the plf, 0 dropped by `-strip_partial`, 0 problem
claims). `unit_fit.sh`: claimed 216, ours 216, retail 216, **no extra functions** - the object
defines only what the retail unit object does. `check_symbol_names.py` 484 units / 0 missing;
`check_raw_offsets.py` 128 sites in 46 files, all documented, unchanged **as measured in this checkpoint and superseded since - the tool now prints 139 sites in 50 files** (`fn_19_0` reaches no raw
offset - it goes through `self->GetBoundingBox()` - so the existing `EyeBallAccessors.cpp` section
still lists the same two sites, 0x54 and 0x44F).
**Nothing had to be re-derived.** `fn_19_0` is 15 instructions and is byte-identical to
`CMysteryFlyerRel.cpp`'s `fn_45_10`, `ShredderAccessors.cpp`'s `fn_68_0` and
`KrocussAccessors.cpp`'s `fn_38_0`, so the body transferred verbatim:
    void fn_19_0(void* out, const CPhysicsActor* self) {
      fn_19_2590(out, self->GetBoundingBox());   // .text 0x2590, unclaimed
    }
`fn_19_2590` is this module's own out-of-line `optional_object<CAABox>` converting constructor
(`build/G2ME01/EyeBall/asm/auto_00_000000D8_text.s:2671` - `li r0,1`, six `lwz`/`stw` pairs,
`stb r0,0x18(r3)`, `blr`, 0x3C bytes) - it is in `auto_00_000000D8_text`, so it stays unclaimed and
is called by name. `const CAABox&` is load-bearing again: by value the frame grows to 0x40 and the
unit stops matching. Retail takes the box by address in a 0x30 frame and needs no move for `self`,
because `GetBoundingBox__13CPhysicsActorCFv` takes `this` in r4 as well. The one-method
`CPhysicsActor` stand-in is not optional: `MetroidPrime/CPhysicsActor.hpp` reaches
`Collision/CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of `.data` in the object -
and `powerpc-eabi-objdump -h` on the built object confirms the reason directly: **`.text` 0xD8 and
no `.data` section at all**.
**The `#ifdef __MWERKS__` guard is what the item actually turned on**, for the fourth time.
`EyeBallAccessors.cpp` is listed in `files.cmake` (like all 20 `*Accessors.cpp` units), and the
module *head* files are deliberately not, because a head body makes the host port link the module's
own functions. It was safe only because it read raw offsets and DOL globals; `fn_19_0` makes the
host link `fn_19_2590` and a host-mangled `CPhysicsActor::GetBoundingBox` -
`powerpc-eabi-nm -u` on the MWCC object lists exactly those two new undefined names. Measured with
`tools/link_check.sh` after the change: **unique undefined symbols 259, duplicate definitions 0**,
equal to the judge's recorded baseline, so with the guard the host build is byte-identical; without
it `gate.sh`'s `link_gap` step, and not `build.sha1`, is what would have failed.
### What is left here
`fn_19_D8` (0xD8) is the first of the 47 functions in `auto_00_000000D8_text` and they are EyeBall's
own members, needing the CActor/CPatterned hierarchy; `auto_fn_19_25CC_text` holds 1 and
`auto_00_000026D0_text` holds 5. `EyeBall` has no `REL_Setup` claim, so 15 + 47 + 1 + 5 = 68 is the
complete denominator `audit_rel_claim.py` prints, and it is the sum of the `total_functions` in
`build/report.json` for the four units. **`Ripper` (`fn_54_0`) is now the only module in
this family still unclaimed at its 0x0 wrapper**, and it is the last clean instance of the shape.
## `DigitalGuardian` is the sixth head past the wrapper, and the only one that inlines it (2026-09-29, goal item `progress-rel-extend-digitalguardian`, lane 1)
`DigitalGuardian`'s claim was `.text 0x78..0x10C`, thirteen accessors. The claim now runs `0x0..0x10C`:
**16/16 functions at 100.00%**, module sha1 `a3798856ec6b175272529f6a6295a29140662bcc` unchanged
against `config/G2ME01/config.yml` and equal to `orig/G2ME01/files/RelProd/DigitalGuardian.rel`, the
`.rel` `cmp`-identical, `main.dol` `6ef9b491...`, `matched` 9181 -> 9184, `linked` 4007 -> 4010, the
module's own count 13 -> **16 of 420**. `tools/audit_rel_claim.py DigitalGuardian`: 16/16 functions in
the claim, 0 problem claims, 420 text symbols in the preplf, 420 in the plf, 0 dropped by
`-strip_partial`. `unit_fit.sh`: claimed 268, ours 268, retail 268, **no extra functions**. The unit
was already `Matching` and stays `Matching`, so `flip_test.sh` was not run - `AGENTS.md` names the
module sha1 plus the `cmp` as the acceptance test for a REL unit, and both hold.
### The one thing that did not transfer: the wrapper is inlined here
`fn_14_0` and `fn_14_8` are `li r3,1; blr` each and cost nothing. `fn_14_10` did not, and **the
FN_XX_10 HINT that six of these items were queued with is wrong for this module.** Every earlier
head in this family - `fn_45_10` (MysteryFlyer), `fn_81_10` (Tryclops), `fn_35_8`
(IngSpiderballGuardian), `fn_38_0` (Krocuss), `fn_54_0` (Ripper), `fn_68_0` (Shredder) - is **0x3C
bytes** and ends in `bl <module>_ctor`, calling its own out-of-line `optional_object<CAABox>`
converting constructor, so all six are written as the free function it compiles to:
    void fn_XX_10(void* out, const CPhysicsActor* self) { fn_XX_ctor(out, self->GetBoundingBox()); }
`fn_14_10` is **0x68 bytes and has no such call.** `build/G2ME01/DigitalGuardian/asm/auto_00_00000000_text.s`
shows the conversion in the body: `li r0,1; stb r0,0x18(r31)` for the valid flag and then six
`lwz`/`stw` pairs copying the `CAABox` word by word. A `bl fn_XX_ctor` there would be four
instructions of wrong bytes, and the function would drop well below 100% while the two 8-byte
predicates beside it stayed at 100% - so objdiff on the unit would still look healthy.
The module has no out-of-line converting constructor to call, and the two functions a grep for the
flag store finds are not one. `fn_14_D774` (`.text 0xD774`, 0x48 bytes) and `fn_14_1AB50` (`0x1AB50`,
0x48) each open `lbz r0,0x17c(r4)` / `0x730(r4)` and store it to `0x18(r3)`, where a converting
constructor has to `li r0,1` - so neither can be one. They reload the flag, `cmplwi r0,0; beqlr`,
and only then run the same six `lwz`/`stw` pairs, which makes them `optional_object<CAABox>`
builders out of a `{CAABox, bool}` member: the box sits at source+0x164 and source+0x718, the flag
just past it. Both are unclaimed. What tells the two shapes apart in a module you have not looked
at is the `bl`, not the flag store.
    for m in Ripper Krocuss MysteryFlyer IngSpiderballGuardian Shredder DigitalGuardian; do
      printf '%-24s ctor-call: ' "$m"
      grep -c 'bl fn_[0-9]*_[0-9A-F]*$' "build/G2ME01/$m/asm/auto_00_00000000_text.s"
    done
Five `1`s and one `0`. The `0` is the one that needs a different spelling.
**The spelling that works is the real return type, and it is the shortest of the six:**
    rstl::optional_object<CAABox> fn_14_10(const CPhysicsActor* self) { return self->GetBoundingBox(); }
MWCC reproduces the bytes from it with nothing hand-written: `optional_object`'s converting
constructor (`include/rstl/optional_object.hpp:16`) sets `m_valid` in its mem-init list and then
placement-constructs the box, and because `CAABox` carries `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`
(`include/Kyoto/Math/CAABox.hpp`, bottom) that construction is a word-wise copy rather than a call.
The `li r0,1; stb` before the copy, rather than after it, is the mem-init running first, and the
`li r0,1` is the whole of the difference from the neighbours: they emit a `bl` to a converting
constructor and do no copy of their own, so they need the `fn_XX_ctor(out, box)` spelling and this
one does not.
The `CPhysicsActor` stand-in, the `const` return and the `GetBoundingBox` frame argument are the
same as the other five, for the same reasons: `MetroidPrime/CPhysicsActor.hpp` reaches
`Collision/CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of `.data` in the object and
break the module sha1 with every function still at 100%; `GetBoundingBox` takes `this` in r4, the
same register `self` arrives in, so no move is needed, and it returns the box through a pointer at
r1+0x8 - which is the 0x30 frame and the 0x34 saved-LR slot retail has.
The `#ifdef __MWERKS__` guard is `ShredderAccessors.cpp`'s and is load-bearing for the reason
`RipperAccessors.cpp` records: `DigitalGuardianAccessors.cpp` **is** listed in `files.cmake`, so a
host-compiled `fn_14_10` would make the port link a host-mangled `CPhysicsActor::GetBoundingBox`.
Measured here with the guard: **259 -> 259 undefined, 0 duplicates.**
### What is left here, and what the next lane has
**404 functions are still unclaimed** (420 in the module, 16 claimed), almost all of it in
`auto_00_0000010C_text` (0x10C..0xD904). The first three are the same opening this family has
everywhere, and all three already have a spelling in the tree:
- `fn_14_10C` (0x10C, 0x2C) - `lwz r12,0(r3); lwz r12,0x38(r12); mtctr; bctrl`, a vtable dispatch:
  `CMysteryFlyerRel.cpp`'s `fn_45_D0` and a thirteen-virtual stand-in class.
- `fn_14_138` (0x138, 0x24) - `li r3,0; bl fn_8021F9B0`: `RELExit`, a plain DOL import needing no rename.
- `fn_14_15C` (0x15C, 0x20) - `bl fn_14_17C`: `RELMain`, the wrapper around the registration, exactly
  `CMetareeSwarmRel.cpp`'s `void RELMain() { fn_43_A8(); }`. `fn_14_17C` (0x17C, 0x3C) is the
  registration: `lis r5,fn_14_1B8@ha / lis r3,lbl_14_bss_C@ha`, then `addi` + `stwu r5,lbl_14_bss_C@l(r3)`
  and `addi` + `stw r0,0x4(r3)` filling the two words of the slot, then `bl fn_8021F9B0` with the
  slot address still in r3 (the same arrangement as `MetareeSwarm`'s `fn_43_A8`; no symbol exists
  at 0x170, it is retail's `lwz r0,0x14(r1)` inside `fn_14_15C` itself).
The wall is the same one as the rest of this family and it is not a spelling problem: from
`fn_14_1B8` (0x1B8, **0x330** = 816 bytes) on, the module is entity class code that needs the
CActor/CPatterned hierarchy this tree does not model. The largest unclaimed functions are
`fn_14_B100` (0xF90), `fn_14_18264` (0xE7C) and `fn_14_1AD2C` (0x9CC) - 0x9CC bytes is the same
order as MysteryFlyer's `fn_45_170`, which is what blocked that head.
## `EmperorIngStage3`'s accessor block is a fifth shape, and `RELMain` is not always at the head (2026-09-29, goal item `progress-rel-head-emperoringstage3`, lane 2)
The item's brief, and several rows above it, describe a scripted-actor module head as "the accessor
block, then `RELExit`, `RELMain` and the registration". That is `MetareeSwarm`, `IngPuddle`,
`FishCloud`, `BacteriaSwarm` - **it is not a property of the REL format, and it is not even the
common case.** `EmperorIngStage3` (module 18) is the clean counterexample, and both halves of the
lesson cost the shape of the claim:
1. **The accessor block is emitted per class, and it varies.** Five shapes are now measured, and a
   copy of any one of them into another module is a guess until `diff`ed:
   | module | byte at +0x448 (`lbl_8041AAB8`) | `li r3,0` run above `kInvalidUniqueId` | always-true predicate | extra function |
   |---|---|---|---|---|
   | `Krocuss` (38) | yes | 4 (`fn_38_54/5C/64/6C`) | at 0xA4 | - |
   | `MysteryFlyer` (45) | yes | 2 (`fn_45_64/6C`) | at 0x0 **and** at 0xA4 | `fn_45_8` (`addi r3,r3,0x818`) |
   | `Tryclops` (81) | yes | 3 (`fn_81_64/6C/74`) | at 0x8 **and** at 0xAC | - |
   | `IngSpiderballGuardian` (35) | yes | 4 (`fn_35_5C/64/6C/74`) | at 0x0 **and** at 0xAC | - |
   | `EmperorIngStage3` (18) | **no** | **4** (`fn_18_4C/54/5C/64`) | at 0x0 **and** at 0x90 | `fn_18_E0` |
   So **read the relocations in the range before deciding which globals to declare.** Module 18
   names only `kInvalidUniqueId`; declaring `lbl_8041AAB8`/`lbl_8041B758` "because every other head
   does" puts two unused `.rodata` references in the object and would move the module's bytes for
   nothing. (`unit_fit.sh` reports the same thing from the other side: `no extra functions` is what
   catches it.)
2. **`RELMain`/`RELExit` can be far from the head, and then they cannot be claimed with it.**
   `EmperorIngStage3`'s are at **0xC330 and 0xC30C**, immediately above `fn_18_C290` (0xC290, 0x7C)
   and 215 of the module's 270 text symbols in - **far above the head, and not claimable with it.**
   The common case is the opposite: **most modules that have a landed head carry the trio inside
   that head, at 0x2C-0xF0** - `BacteriaSwarm`, `DestructibleBarrier`, `FishCloud` and
   `SnakeWeedSwarm` at 0x2C, `IngPuddle`/`IngSnatchingSwarm` at 0x34, `AtomicAlpha` at 0xC8,
   `WallCrawler` at 0xCC, with `MysteryFlyer` (0xFC) and `Tryclops` (0x104) the two above the band.
   `IngBlobSwarm`'s is at **0x64/0x88**, inside its 0x0..0xD8 claim - **not** at 0x138.
   **`DigitalGuardian`'s sits just past 0x10C** (0x138/0x15C/0x17C against a head that stops at
   0x10C), and its `symbols.txt` leaves the trio **unnamed**, so the grep below returns nothing
   there and the three functions have to be recognised by shape: `fn_14_138` is `RELExit`,
   `fn_14_15C` is `RELMain` and `fn_14_17C` the registration. One unit cannot claim two
   discontiguous ranges (the link-order cycle, "Four structural facts" above), so a head at 0x0 and
   a trio at 0xC330 are **two items' work at best**, and the head is worth taking first because it
   is the contiguous part. **Check `grep -n "RELMain\|RELExit" config/G2ME01/rels/<Module>/symbols.txt`
   before planning a head** - it is the difference between a 14-function item and a 3-function one,
   and it is one grep.
**`fn_18_E0` is the one function here with no in-tree precedent, and it took one spelling.**
`lwz r3,0x48c(r3)` / `lwz r0,0x37c(r3)` / `subfic r0,r0,6` / `cntlzw r0,r0` / `srwi r3,r0,5` / `blr`
is `*(int*)(*(char**)(self + 0x48c) + 0x37c) == 6`, and it is 100.00% as:
    bool fn_18_E0(const void* self) {
      const char* sub = *reinterpret_cast<const char* const*>(static_cast<const char*>(self) + 0x48C);
      return *reinterpret_cast<const int*>(sub + 0x37C) == 6;
    }
The `subfic`/`cntlzw`/`srwi 5` tail is mwcceppc's `== constant` idiom when the comparison *is* the
return value and nothing is branched on - the same shape `CMetareeSwarmRel.cpp` measures for
`index > -1`, and the reason a compare-and-branch spelling is wrong here is that it is longer, not
that it is wrong logic. The pointer-then-int spelling is also load-bearing: reading
`*reinterpret_cast<const int*>(...)` off `self + 0x48C` directly is a different instruction.
**Measured, the gates that matter for a REL unit**: `flip_test.sh` PASS
(`PASS -> kept as Matching`), module sha1 `775b095db13cb3e1ec6c5bb51a129263f7b0fa18` unchanged
against `config/G2ME01/config.yml` and `cmp`-equal to `orig/G2ME01/files/RelProd/EmperorIngStage3.rel`,
all 86 RELs `cmp`-identical, `main.dol` `6ef9b491...`, `audit_rel_claim.py` 0 problems with 270 of 270
preplf text symbols in the plf, `unit_fit.sh` `248 claimed / 248 ours / 248 retail, fits`,
`check_decl_order.py` ok, `tools/check_files_cmake.py` ok, and `tools/probe_sources.sh` **727 source files (then),
0 failures, 259 undefined, 0 duplicates** - the undefined count unchanged by the `files.cmake`
entry, which the `#ifdef __MWERKS__` guard is there to guarantee.
### What is left here
`fn_18_F8` (0xF8, 0x11C) through `fn_18_C8CC` and the `RELMain`/`RELExit` trio at 0xC30C are
unclaimed, all of it entity class code needing the CActor/CPatterned hierarchy this tree does not
model. The trio on its own is 3 functions and would need a **second** unit whose claim starts at
0xC30C, not an extension of this one - that is the natural next step for this module.
## `CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit (2026-09-29, goal item `progress-rel-head-geomblobv2`, lane 2)
Module 25's accessor block is eight trivial functions at `0x2544..0x2584`, and **six of them are
ours in two units, because the other two are not in dtk's FORCEACTIVE list.** The measurement is
the point, and it is the third time this family has produced a surprise, so the order of the facts
below is the order they were found in.
**One unit claiming all eight built, linked, and broke the module's hash.** `CGeomBlobV2Accessors.cpp`
claimed `.text 0x2544..0x2584`, all eight bodies written, and every check that is not the module's
sha1 said yes:
```
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CGeomBlobV2Accessors.cpp
   .text      claimed     64   ours     64   retail     64   fits
   no extra functions: our object defines only what the retail unit object does
```
`objdiff` reported the unit at 100%, `powerpc-eabi-nm` over our object and the retail unit object
agreed on all eight symbols and their sizes, and the link succeeded. And
`GeomBlobV2.rel` came out **16 bytes short of retail**: 33756 against 33772. Those 16 bytes are
`fn_25_255C` and `fn_25_2564`, the two float **getters**, and `build/G2ME01/GeomBlobV2/ldscript.lcf`
says why - dtk's FORCEACTIVE list for this module names `fn_25_2544`, `fn_25_254C`, `fn_25_2554`,
`fn_25_256C`, `fn_25_2574` and `fn_25_2578` and **not** those two, because nothing in the module's
own data or code references them. mwldeppc dropped them and every byte after them moved down 16.
This is the dead-stripping trap of the "Four structural facts" section above, and the **superseded
2026-09-29 note there is the whole finding**: `scope:global` in `symbols.txt` does not fix it; only
a `force_active:` list in `config/G2ME01/config.yml` does, which is what `Tweaks` carries. So the
block is split in two and the two middle functions stay retail:
```
0x23E8..0x2490   CGeomBlobV2Rel.cpp              4 functions, the entry points
0x2490..0x2544   unclaimed: fn_25_2490, the module's entity loader
0x2544..0x255C   CGeomBlobV2Accessors.cpp        3 functions, all FORCEACTIVE
0x255C..0x256C   unclaimed: fn_25_255C, fn_25_2564, the two float getters
0x256C..0x2584   CGeomBlobV2AccessorsTail.cpp   3 functions, all FORCEACTIVE
```
**Two more things the same module contradicts, both worth not re-deriving.**
1. **Its head is not at 0x0, and the accessor block is not the family.** The landed heads
   (`MetareeSwarm`, `IngPuddle`, `IngSnatchingSwarm`, `PlantScarabSwarm`, `SnakeWeedSwarm`,
   `AtomicAlpha`, `MysteryFlyer`, `FishCloud`, `Tryclops`, `DigitalGuardian`, `EmperorIngStage3`)
   all open with the loader generator's thirteen-accessor block: the `kInvalidUniqueId` store, the
   `li r3,0` predicate run, the `+0x44f` byte, the `+0x34c` flag, the `lbl_8041AAB8` /
   `lbl_8041B758` float pair, the `+0x754` address. **None of it is in module 25.** Its entry points
   are at `0x23E8..0x2490`, and `fn_25_0` (0x0, 0x1FC) is a real bone-blend loop over 0x50-byte
   records calling `close_enough__FRC11CQuaternionRC11CQuaternionf` and
   `__as__9CMatrix3fFRC9CMatrix3f`. Its eight accessors are two pointer getters at `+0x15C`, float
   accessors at `+0x190` / `+0x198`, a bare-`blr` empty virtual and a byte clear at `+0x18`.
   **Before writing a module head, read the module's own ldscript and the first function in its
   `symbols.txt`; do not assume the family.**
2. **`fn_25_2578`'s byte is at `+0x18`, and `*self = 0` gives `+0x0`.** The obvious spelling of a
   `li r0,0 ; stb r0, N(r3) ; blr` clear - `*static_cast<unsigned char*>(self) = 0;` - produces
   `stb r0, 0x0(r3)`, and the module's `.rel` came out **one byte** from retail at `0x2633`. Every
   size-based check still passed; only `cmp` against `orig` found it. The accessor files' own header
   comments now carry both measurements.
`fn_25_23E8` is the family's vtable-dispatch shape unchanged (`.data:0x10` and `.data:0xA8` both
store it at offset 0x3C, it calls slot 0x38, which is `HealthInfo__6CActorFv`), and the
thirteen-virtual stand-in class of `CIngPuddleRel.cpp` reproduces its seven instructions byte for
byte. Both loaders here are **unnamed DOL setters** - `fn_80229EAC` (0x80229EAC, `stw r3,
lbl_80419590`, resolved with `tools/sda.py` against `_SDA_BASE_` 0x8041FD80) and `fn_802274FC`
(0x802274FC, `stw r3, lbl_80419558`) - so there is no `symbols.txt` rename and no DOL change, the
same situation as `Tryclops` and `Blogg`.
**What is left here.** 115 of the module's 130 text symbols stay retail, starting at `fn_25_2490`
(0x2490, 0xB4), which allocates 0x1E0 bytes and calls `fn_25_4290` - entity class code needing the
CActor/CPatterned hierarchy. The two unclaimed float getters at `0x255C` / `0x2564` are one
`force_active:` entry away and are worth a two-line follow-up rather than a lane.
## `CIngSpaceJumpGuardianRel` is a module head, and one accessor is a module-local `.rodata` constant (2026-09-29, goal item `progress-rel-head-ingspacejumpguardian`, lane 2)
Module 34 is the eleventh of the 27 modules whose `REL_Setup` tail was claimed on 2026-09-28 to
gain class code. The head is now the whole `.text 0x0..0x170` - **eighteen** functions, all
100.00% - in `src/MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp`, and the module's sha1
`96e5208fc681177378fcaf1ed15abe20d436073a` is **unchanged** against `config/G2ME01/config.yml`
and the `.rel` is `cmp`-identical to `orig/G2ME01/files/RelProd/`, with all 86 holding and
`main.dol` still `6ef9b491...`. `matched` 9236 -> **9254**, `linked` 4521 -> **4539**, the module's
own count 5 -> **23 of 148** (`tools/audit_rel_claim.py IngSpaceJumpGuardian`: 148 preplf text
symbols, 148 in the plf, 0 dropped by `-strip_partial`, and 0 problem claims - `18/18 functions`
in the claim). `tools/unit_fit.sh` says `.text claimed 368 ours 368 retail 368, fits` with no extra
functions, `tools/flip_test.sh` passes and keeps it `Matching`, and `tools/check_docs_claims.py`
agrees with the tree.
### The one body with no precedent: `fn_34_10` reads the module's own `.rodata`
The brief's hint was that this module's head is the `GetBoundingBox` wrapper plus the family block,
and **thirteen of the fifteen accessors turned out to be bodies already in the tree** - but the
block is the family in an order none of the landed heads has, and diffing
`build/G2ME01/IngSpaceJumpGuardian/asm/auto_00_00000000_text.s` against Tryclops is what shows it:
```
fn_34_0   addi r3,r3,0x8d0      fn_81_0   addi r3,r3,0x7c4
fn_34_8   li r3,0x1             fn_81_8   li r3,0x1
fn_34_10  lis r3,lbl_34_rodata_0@ha / lfs f1,...@l(r3) / blr     <-- no counterpart
fn_34_1C  stwu r1,-0x30(r1) ... bl fn_34_6814                   fn_81_10 the same shape
fn_34_58  lbl_8041AAB8 -> +0x448                                 fn_81_4C the same
...  fn_34_98 (the +0x34C bit 3), fn_34_A4 (+0x754), fn_34_AC, fn_34_B4 (the interleaved
    three-float copy), fn_34_D0 (the vtable-0x38 call), RELExit, RELMain, fn_34_140
```
So **`fn_34_10` (0x10, 0xC) is the only function here with no precedent in the tree**, and the
difference from the family is the point: the family's thirteenth accessor is a *DOL* constant
(`lbl_8041B758`, `.sdata2` at 0x8041B758), and this module's is a **constant in its own `.rodata`**
- `build/G2ME01/IngSpaceJumpGuardian/asm/auto_03_00000000_rodata.s:0` names it
`lbl_34_rodata_0, size:0x4, .float 60` - and it sits at 0x10, where the family puts the
`GetBoundingBox` wrapper (which is here instead at 0x1C). The `lbl_8041B758` accessor is **not in
this module at all**.
The spelling is the same one either way and it was already in the tree:
`src/MetroidPrime/ScriptObjects/CScriptRubiksPuzzle.cpp:5` declares `extern "C" const float
lbl_4_rodata_0;` for its own module's `.rodata:0x0` and returns it from a constructor. **The split
claims `.text` only**, so `lbl_34_rodata_0` stays defined in dtk's `.rodata` object and the
reference is an ordinary cross-object relocation - the same situation as `lbl_8041AAB8` and
`kInvalidUniqueId` beside it. `float fn_34_10(void*) { return lbl_34_rodata_0; }` was the whole of
it, and the object's `.rodata` is empty, so nothing moves.
**The `kInvalidUniqueId` spelling is not free, and the compile error says so.** `CTryclopsRel.cpp`
and `CAtomicAlphaRel.cpp` write `extern "C" const unsigned short kInvalidUniqueId;`, but
`include/MetroidPrime/TGameTypes.hpp:17` already declares it as `const TUniqueId`, and
`CIngSpaceJumpGuardianRel.cpp` includes that header for the wrapper, so the second declaration is
`identifier 'kInvalidUniqueId' redeclared` and the build stops. `CMysteryFlyerRel.cpp` has it
right: **no second declaration at all**, and the body is
`void fn_34_88(TUniqueId* id) { *id = kInvalidUniqueId; }`, which is the same
`lis r4, kInvalidUniqueId@ha / lhz r0, ...@l(r4) / sth r0, 0x0(r3)`. `TUniqueId` is 0x2 bytes
(`CHECK_SIZEOF(TUniqueId, 0x2)` in the same header), so the store is the same word.
### Two things this module has that `CGeomBlobV2` did not, both worth not re-deriving
1. **No dead-strip hazard, and no `force_active:` entry is needed.** `CGeomBlobV2`'s accessor
   block came out 16 bytes short of retail because dtk's FORCEACTIVE list omitted two of its
   getters. This module's `build/G2ME01/IngSpaceJumpGuardian/ldscript.lcf` lists **all fifteen** of
   `fn_34_0` .. `fn_34_D0` in FORCEACTIVE, *and* `asm/auto_04_00000000_data.s:0x3E4` - a 0x148-byte
   vtable, two leading words and one per virtual - stores fifteen of them, so the link keeps them
   twice over. That is why the 18-function claim came out byte-exact on the first build, with no
   carve and no second unit.
2. **`fn_34_D0` is vtable entry 0x3C of a 82-word table, and the thirteen-virtual stand-in class
   is the same one IngPuddle, AtomicAlpha, BacteriaSwarm, Tryclops and PillBug use.** `.data:0x3E4`
   stores `fn_34_D0` at offset 0x3C and `HealthInfo__3CAiFv` at 0x38, so the call target is slot
   0x38 and `CIngSpaceJumpGuardianDispatch` with thirteen virtuals puts its thirteenth there.
   The table itself is unclaimed - it stays in dtk's `.data` object - so the class is only ever
   named, never instantiated by anything but the member call.
**What is left here.** 125 of the module's 148 text symbols stay retail, starting at `fn_34_170`
(0x170, 0x330), the module's own entity loader: a 0x810 frame whose first act is
`bl __ct__20SLdrEditorPropertiesFv` and which then makes thirty further calls (31 `bl` sites, 25
distinct callees), so it is entity
class code needing the CActor/CPatterned/CAi hierarchy. The next step is not this head - the head
is finished at 0x170, and the neighbour above it is a loader, so extending means the loader
itself, which is the wall every other landed head in this family is parked on.
## A 1-byte class passed **by value** keeps a byte temporary that retail has no trace of (2026-09-29, goal item `match-csequencehelper`, lane 1)
`Kyoto/Animation/CSequenceHelper` sat at 17/18 with only `__defctor__16CParticlePOINodeFv` (retail
`0x80299DCC`, 0x98 bytes) unmatched. The previous attempt's notes called it "one function plus three
data sections" and stopped at `#pragma inline_max_size`; the pragma is necessary and **not
sufficient**, and the twelve missing bytes are a one-word change in a shared header.
**The shape of the defect.** Retail inlines the whole 9-argument `CParticlePOINode` constructor into
the implicit default constructor, so retail's DOL defines no such symbol. mwcceppc will not inline it
under the project-wide `inline_max_size(125)`, so the TU emitted a forwarding 0x94-byte defctor plus a
`__ct__16CParticlePOINode...` the retail symbol table has no name for. Because that callee is weak and
this TU is first in link order, mwldeppc kept *this* copy and every function after it moved 0x74.
`#pragma inline_max_size(140)` in `CSequenceHelper.cpp` fixes that part - measured with
`fast_try.sh`: at 134 and below the constructor is still not inlined (16.18%), 136 and up inline it.
**The part the pragma cannot reach.** At 140 the defctor is 0xA4, not 0x98, and the surplus is exactly
three instructions: `stb r0,8(r1)`, `stb r0,12(r1)`, `lbz r4,12(r1)`. The frame is -64 rather than -48,
`CCharAnimTime` sits at `sp+16` rather than `sp+8`, and `mBone` is written from a reloaded byte instead
of from the register that already holds zero. **Those are two temporaries for a 1-byte class.** The
default argument `CSegId bone = CSegId(0)` materialises one at `sp+8`, and the by-value parameter is a
second copy of it at `sp+12`; the member initialiser reads the second back. Every other default
argument collapses - the `SObjectTag(0,0)` is written straight through as two `stw`, and only the
`EParentedMode` keeps a temporary, in retail too (`lwz r0,36(r1)` then `stw r0,64(r31)`).
**The fix, and why it is safe to make in a shared header.** `CParticleData`'s `bone` becomes
`const CSegId&`, exactly like the `const SObjectTag& tag` beside it, and the unit goes 18/18 at
100.00% with `flip_test.sh` PASSing. The generalisable rule: **when mwcceppc materialises a
temporary that retail does not have, check the parameter's size before its type** - a 1-byte class by
value is the case that survives copy propagation, where a 4- or 8-byte one does not. Taking it by
const reference is semantically identical at every call site, and it lifted two `CAnimData` functions
on the way past (`InitializeEffects...` 36.17% -> 49.23%, `__ct__9CAnimData...` 87.11% -> 87.45%) with
no function anywhere worse.
**Both of the previous run's "blockers" were one bug.** It also reported `.sdata` "over by 20" and 50
extra COMDAT functions from `unit_fit.sh`. Those are still there after the fix and the flip still
passes: mwldeppc folds every weak instantiation away, exactly as the tool's own "harmless causes first"
note says. **`unit_fit.sh` complaining about extra COMDAT weak copies is not a blocker; only
`flip_test.sh` decides**, and the extra constant-pool entries it predicted would follow the constructor
did - there simply was no second bug.
**Measured.** `All: 29.04% fuzzy, 21.15% matched, 11.16% linked (9344 / 28465 functions)`, from 9343.
`linked`, the one rule's count, rose 4643 -> **4661**: +18, not +1, because a `NonMatching` unit's
already-matched functions do not count until the unit is complete. `main.dol` still hashes to
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs are still `cmp`-equal.
## `x * 0.5f` and `x / 2.f` are different instructions, and a weak copy of an unnamed retail
function is what stops a flip (2026-09-29, goal item `match-cguipane`)
`GuiSys/CGuiPane.cpp` was a near-miss: 10 of 11 functions at 100.00%, and only
`InitializeBuffers__8CGuiPaneFv` (188 bytes) at 98.30%. Two things had to be right, and they are
unrelated, so the item needed both.
### 1. mwcceppc strength-reduces `x / 2.f` to a multiply and does **not** touch `x * 0.5f`
The whole residual was eight instructions, and every one of them was the same two bytes:
```
retail   ec 00 00 b2   fmuls f0,f0,f2      ; f0 = +-width/height, f2 = 0.5f
ours     ec 02 00 32   fmuls f0,f2,f0      ; the same multiply, the operands the other way round
```
`f2` is the `.sdata2` constant `3f000000` = 0.5f, loaded once at the top and live across all eight,
so this is not a scheduling difference - only the source order of a commutative `fmuls`. Measured
with `tools/try_edit.py` over six spellings of the eight assignments:
| spelling | `InitializeBuffers` |
|---|---|
| `-mWidth * 0.5f` (the obvious one) | 98.30% |
| `0.5f * -mWidth` (constant first) | 98.30% |
| `-(mWidth * 0.5f)` (negate after) | 74.89% |
| `0.f - mWidth * 0.5f` | 74.89% |
| `mWidth * -0.5f` (fold the sign into the constant) | 82.98% |
| **`-mWidth / 2.f`** | **100.00%** |
**`/ 2.f` is the only one of the six that matches, and the reason generalises: MWCC rewrites a
division by a power-of-two constant into a multiply by its reciprocal, and that rewrite goes through
a different path than a multiply written in the source, so it keeps the source operand order. `*`
does not canonicalise, and in this tree it comes out constant-first.** The same rule is already in
this file for comparisons ("mwcceppc keeps a comparison's source operand order", and the
`ConfigureGameModeLayers` table row); this is the arithmetic twin. `2.f` is also the tree's own
spelling for halving a float (`CCredits.cpp:585`, `CSimpleShadow.cpp:29`).
### 2. A `Matching` unit fails the link on a weak copy of a function retail's own copy of is **unnamed**
With all 11 functions at 100.00% and 100.00% matched code, `flip_test.sh` still failed, and the
DOL was 96 bytes long. `unit_fit.sh` had been reporting this all along and its "harmless causes
first" note is not always right:
```
.text      claimed   1692   ours   1812   over by 120
   +   92  __ct__Q210CGuiWidget15CGuiWidgetParmsFRCQ210CGuiWidget15CGuiWidgetParms
   +   12  GetIsActive__10CGuiWidgetCFv
   +   12  GetIsVisible__10CGuiWidgetCFv
   +    4  Initialize__10CGuiWidgetFv
```
Only the first one mattered. The other three are weak COMDAT copies of functions retail defines
strongly in `auto_03_802740A4_text.o` and in `MetroidPrime/HUD/CSamusHud.cpp`, so mwldeppc folded
them. The copy ctor had **no other owner**: the retail-derived `build/G2ME01/obj/GuiSys/CGuiPane.o`
carries it as an **undefined** `fn_80274608` (that is dtk reading the DOL, in which the name does
not exist), and the 0x5C bytes live at 0x80274608 in an unclaimed range filled by
`auto_03_802740A4_text.o`, which defines them strongly - under a *different* name. So our weak copy
was not a duplicate of anything as far as the linker was concerned, and it was placed immediately
after our claim, at 0x80278BF8, straight on top of retail's `fn_80278BF8`:
```
first moved symbol   fn_80278BF8   80278bf8 -> 80278c54   (+0x5C)
symbols that moved   14730
```
**0x5C is the copy ctor's size, and it moved every function in the DOL after 0x80278BF8.** This is
the "weak instantiations can steal a symbol retail has somewhere else" mechanism above, and the
section there says the only fixes are a wider claim or accepting the loss. There is a third, and it
is one line. The function *is* `CGuiWidgetParms::CGuiWidgetParms(const CGuiWidgetParms&)` - the
bytes are a member-wise copy of exactly its 22-byte layout (4-byte `mFrame`, two `short`s, two
4-byte words, six `bool`s, `lha`/`sth` for the shorts, `lbz`/`stb` for the bools), and
`powerpc-eabi-objdump` of our weak copy is byte-identical to retail's 0x80274608. So give retail's
copy its name, with `tools/apply_rename.py`:
```
fn_80274608 = __ct__Q210CGuiWidget15CGuiWidgetParmsFRCQ210CGuiWidget15CGuiWidgetParms
```
Now the two definitions are one symbol, the strong one wins, the weak one folds at link time (the
object is still 0x714 bytes - the 0x5C is what stops reaching the DOL), and the flip passes.
**So the fix for "our object emits a weak copy retail also has" is to check whether retail's copy
has a *name*, and if it does not, whether it should - and to rename it in
`config/G2ME01/symbols.txt` when the bytes identify it. No byte of the DOL changes; only the name the linker can match on.** `total_functions` is
unchanged at 28465 and no function anywhere scores worse (`gate.sh`'s per-function diff is the
check). Eight `auto_*` objects referenced the symbol before the rename and dtk regenerates all
of them consistently from `symbols.txt`; `build/G2ME01/obj/GuiSys/CGuiPane.o`'s undefined
`fn_80274608` becomes an undefined `__ct__Q2...`, which our object now defines.
This is a `config/` change, so it is reported as an intended change rather than a copy:
`config/G2ME01/symbols.txt` line 10935, `fn_80274608` -> `__ct__Q210CGuiWidget15CGuiWidgetParmsFRC
Q210CGuiWidget15CGuiWidgetParms`, and nothing else in that file.
**Measured.** `GuiSys/CGuiPane.cpp` is 11/11 at 100.00% fuzzy, 100.00% matched code, and
`flip_test.sh GuiSys/CGuiPane.cpp` prints `PASS -> kept as Matching`.
`All: 29.05% fuzzy, 21.16% matched, 11.19% linked (9363 / 28465 functions)`; `matched`
9362 -> **9363** and `linked` 4679 -> **4690**, i.e. +1 for the function and +11 for the flip.
`main.dol` hashes to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs are `cmp`-equal to
`orig/G2ME01/files/RelProd/`.
## `(*it).GetSourceId()` defeats a common-subexpression elimination retail does not perform, and the emission order is the rest of the flip (2026-09-29, goal item `match-cstaticinterference`, lane 2)
`MetroidPrime/Player/CStaticInterference` needed one function, then needed the same fix three more
times for instantiations. Both halves are worth not re-deriving.
**`(*it).GetSourceId()` is one character pair, and it takes `GetTotalInterference` from 94.67% to
100%.** Retail's loop body at `0x8013CAE8` loads the source id **twice** — once for each of the two
`if`s — while the natural spelling
```cpp
if (it->GetSourceId() == kInvalidUniqueId) { invalidAccum += v; }
if (it->GetSourceId() != kInvalidUniqueId) { validAccum += v; }
```
compiles to one load: mwcceppc CSEs the two identical member reads. Writing the second one
`(*it).GetSourceId()` — `const_pointer_iterator::operator*` returns `const T&` and
`operator->` returns `const T*` — makes the two reads distinct expressions to MWCC's front end, so
both survive, and the rest of the body (registers, branch displacements, the two `lfs`/`fadds`)
falls out identical to retail for free. No `volatile`, no pragma, no inline-size knob. **When a
retail function reads a field once per `if` and yours reads it once, try spelling one of the reads
through a different access path before assuming a codegen bug.**
**The flip then failed at 9/9 with every function at 100%, and the cause was the emission-order
wall** — the pool of out-of-line vector instantiations. Retail's unit interleaves them with the
source functions:
```
retail ascending : Update  GetTotalInterference  RemoveSource  erase  erase  AddSource
                    __ct__  reserve  uninitialized_copy
ours (implicit)  : Update  GetTotalInterference  RemoveSource  AddSource  __ct__  reserve
                    uninitialized_copy  erase  erase
```
The documented fix applies and is one edit per instantiation: declare a non-inline **explicit
specialization** before first use and define it where MWCC's reverse source order puts it where
retail's text shows it (`CPASDatabase`'s `insert`, `CGameHintInfo`'s `assign`). `reserve` is
defined just before the constructor, `erase(first, last)` and `erase(it)` just before
`RemoveSource`, bodies copied verbatim from `rstl/vector.hpp`. That fixed the order in one build.
**Two details the wall's write-up does not say, both measured here.** The specializations come out
**strong (`T`) where retail's copies are weak (`W`)** — `nm` shows that on all three — and the flip
still passes, because ours is the copy the linker keeps either way (`main.o` also emits
`__dt__<vector<CStaticInterferenceSource>>` weak, at `0x54`, and the linker's `main.dol` is
byte-identical). And `tools/unit_fit.sh` still reports **`+84  __dt__<vector<CStaticInterferenceSource>>`
as an extra function and `.sdata2` short by 4 after the unit matches**; that extra weak copy is
discarded by mwldeppc exactly as `unit_fit.sh`'s own note says, and the flip is the only verdict.
Neither is a reason to keep working.
**Measured.** `MetroidPrime/Player/CStaticInterference.cpp` is 9/9 at 100.00% fuzzy, 100.00%
matched code, and `flip_test.sh MetroidPrime/Player/CStaticInterference.cpp` prints
`PASS -> kept as Matching`. `All: 29.05% fuzzy, 21.16% matched, 11.21% linked (9364 / 28465
functions)`; `matched` 9363 -> **9364** and `linked` 4690 -> **4699** (+1 function, +9 for the
flip). `main.dol` hashes to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs are
`cmp`-equal to `orig/G2ME01/files/RelProd/`, `probe_sources.sh` reports 0 failures, and
`check_symbol_names.py` reports 0 missing.
## `CGameState` 86 -> 88: a loop that returns its own output is a different function (2026-09-29, goal item `progress-cgamestate-elem12`, lane 1)
Two functions, both in the `SGameStateBlock` helper run, both reached by asking what the
**return value** is rather than what the loop does.
**`fn_801429AC` (0x801429AC, 100 bytes), no body -> 100.00%.** It is the 16-byte block's element
copy, the same shape as the 12-byte block's `fn_801465A8` above. Retail's epilogue is
```
801429f4:  mr      r3,r31          <- r31 is the destination, walked forward by the loop
```
so it returns the **advanced destination**, not the `dst` it was handed. The first spelling wrote
`return dst`, and it scored **86.12% with a 112-byte function**: the original destination has to
stay live across the loop, so the compiler allocates a fourth callee-saved register (`r28` saves
`end`, `r29` holds the original `dst`) that retail does not have. `return out` is a one-word
change that takes it to 100. **A 16-byte-stride loop that returns a pointer is very likely an
`uninitialized_copy`, not a `copy`**, and `return dst` is the spelling that reads like the caller
wrote it.
**`fn_801426E0` (0x801426E0, 56 bytes), 97.50% -> 100.00%.** It appends one 36-byte element:
`elem = data + count * 36` and the count is bumped **before** the element is built. Both halves
were already right; the residue was register assignment on the multiply. Counting the index in
**words** rather than bytes is what fixes it:
```c++
u32* const words = static_cast< u32* >(self->x0c_data);   // 36 bytes == 9 u32s
const u32 n = self->x04_count;
self->x04_count = n + 1;
fn_80142718(words + n * 9, src);                          // mulli r0,r5,36 == words + n * 9
```
`n * 36` on a `uchar*` gives `mulli` onto the count's *own* register and 97.50%; `words + n * 9`
puts the product in a temporary and matches. Four spellings measured (byte offset, `u32*` words,
`++n` in the assignment, a named `void*` local); the word count is the only one at 100%. **This is
the same lesson as the `{u32, float, float}` element in `fn_801465A8`, one level down: mwcceppc
sinks a multiply into the register that holds an operand when the source spells the offset in
bytes, and a temporary when the source spells it in the unit the pointer is typed in.** Check the
element's type before believing a "register wall" in this unit.
### Walls re-measured here, so the next lane does not re-derive them
`fn_80142944` (0x80142944, 104 bytes) is `SGameStateSlots::operator=` and is **91.92% at best of
eleven spellings** - the same `beq` early-out, the same three `bl`s, and the only difference is
the range-end address: retail forms `src + count*16` and *then* adds `x04_blk`'s `+0x04`
displacement (`add r4,r31,r0` / `addi r4,r4,4`), and every spelling measured folds the `+4` into
the index first (`slwi r4,r0,4` / `addi r4,r4,4` / `add r4,r31,r4`). The word-index trick that
fixed `fn_801426E0` does **not** transfer here (91.73%): the `+4` is a struct-member displacement,
not a byte offset, and `src->x04_blk + count` and `s + count*4 + 1` both reduce the same way. Not
written - it needs `fn_80004BEC` declared, which is a separate change.
`fn_8014680C` (0x8014680C, 104 bytes) stays at **92.69%**, and `fn_801466F4` (0x801466F4, 172) at
**91.26%** of the four spellings measured (it was 66.63%). Both are the same wall: retail keeps
`begin` **dereferenced once** in `r31` and `end` **by address** in `r29`, re-loading it at the
bottom of the loop (0x80146848), while our object keeps both dereferenced and hoisted. Writing it
as `do { } while (in != *end)` does produce the re-load and costs the `b` at the top - net worse
(88.85%). `fn_8014601C` (0x8014601C) stays at **99.05% of eight spellings**: the only difference is
that retail reloads `r0` (the link register) **before** `r31`/`r30` in the epilogue, and no source
shape moves that.
**Measured.** `MetroidPrime/Player/CGameState` 86 -> **88** matched functions (of 116),
`fn_801429AC` and `fn_801426E0` both at 100.00%; the unit stays `NonMatching`.
`./tools/gate.sh build/goal/judge/report.base.json`: `matched 9363 -> 9365`, `linked 4690` unchanged,
`+2 functions at 100%, 0 units newly linked`, every other step `ok`, `GATE PASS`.
`main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs hash-match `config.yml`.
`python3 tools/check_docs_claims.py` agrees with the tree after the `docs/HANDOFF.md` state block
was moved 9363 -> 9365 and DOL 8056 -> 8058 in the same change.
## A flip sweep of the units that are 100% but not linked: 3 of 16 pass (2026-10-01)
`build/report.json` listed 17 units with every function at 100% that were still `NonMatching`
(414 functions). `tools/flip_test.sh` on the 16 DOL ones:
- **PASS, now `Matching`**: `MetroidPrime/Weapons/GunController/CGSFidget.cpp` (6 functions),
  `MetroidPrime/BodyState/CABSReaction.cpp` (8), `Collision/CCollidableSphere.cpp` (17).
  `linked` 5625 -> 5656, `matched` unchanged, DOL sha1 and all 86 RELs unchanged, full `gate.sh` ok.
- **FAIL, reverted** (13): `CSortedLists`, `CLight`, `CPathFindRegion`, `CTweakPlayer` (129
  functions), `CTweakAutoMapper` (68), `CGunMotion`, `CCollisionSurface`, `CAnimTreeDoubleChild`,
  `CPlayerEnergyDrain`, `CQuaternion`, `CCharLayoutInfo`, `CFBStreamedCompression`, `CParticleGen`.
  Several also change RELs, which means the unit's object shifts data addresses - a size
  difference in a data section, not a code difference. One fails at the `main.elf` link.
- Not tested: the REL unit `Tweaks/MetroidPrime/Tweaks/Tweaks`.
The one failure diagnosed so far, `CSortedLists`: `.text`/`.rodata`/`.data`/`.bss` are identical.
Ours emits 8 bytes of `.sdata2` (8000.0f, 0.0f) that retail has at `0x8041BA88..0x8041BA90`, which
the unit's split does not claim, and 0x28 bytes of `.sdata` (header statics and `SolidMaterial`)
whose byte pattern occurs nowhere in the retail DOL, so retail did not link them for this unit.
`SL::SSortedList::SSortedList()` is strong in retail and weak in ours. Untried fix: claim the
`.sdata2` range and stop the `.sdata` statics being emitted.
**The lesson for a lane**: "every function 100%" is where the flip work starts. A unit in this
state needs `tools/compare_unit.sh` on its data sections and its split, not more C++.
#### Second pass, same day: 3 of the 13 were missing split claims only
`CSortedLists`, `CAnimTreeDoubleChild` and `CCollisionSurface` now pass `flip_test.sh` and are
`Matching`, with no source change - the 13 above are 10. The "untried fix" for `CSortedLists` was
half right: the `.sdata2` claim was needed, the `.sdata` statics were not a problem.
The method, which took minutes per unit once found:
1. List the sections our object emits (`powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o`) against
   the unit's entry in `splits.txt`. A section we emit that the split does not claim is linked
   somewhere else, and everything after it shifts.
2. Read the **retail split object's relocations** (`objdump -r build/G2ME01/obj/<unit>.o`). They name
   the `.sdata`/`.sdata2`/`.rodata` addresses the unit really references. Claim exactly those ranges.
3. A local `.sdata` static that no code references is dead-stripped by MWLD and needs no claim; do
   not spend time suppressing it.
Claims added: `CSortedLists` `.sdata2 0x8041BA88..0x8041BA90`; `CAnimTreeDoubleChild`
`.data 0x803B97D8..0x803B9858`, `.sdata 0x80418A78..0x80418A88`, `.sdata2 0x8041E378..0x8041E388`;
`CCollisionSurface` `.rodata 0x803AD830..0x803AD840`.
**What the method cannot fix**, measured on the rest:
- **`CTweakAutoMapper` (68 functions) and `CTweakPlayer` (129) cannot link as separate objects.**
  Retail `.data 0x803B8038..0x803B830C` is one unbroken run of jump tables across the `CTweak*`
  files (Targeting `0x8038`, an unclaimed table at `0x805C`, Ball `0x818C`, AutoMapper `0x822C`,
  Player `0x824C`), and these units begin at 4 mod 8. MWCC emits `.data` with section alignment
  2**3, so the linker pads 4 bytes in front of ours. They were one translation unit in retail; only
  its head, `CTweakTargeting`, can stand alone (it is `MatchingFor` already). Linking them takes one
  unit spanning Targeting..Player, which waits on `CTweakPlayerGun` (20/33) and `CTweakBall`
  (75/84). Only four units in the tree start `.data` at 4 mod 8, all `NonMatching`.
- **`.sdata2` literal order** fails `CLight`, `CQuaternion` and `CTweakPlayer` with every function at
  100%. objdiff does not see it because each function's relocation still resolves to the right
  value. Retail `CLight`: `1/255, 1.19e-7, 1, 0, -1, 3e36, …`; ours `…, 1, 3e36, 0, …, -1`. Retail
  `CQuaternion` starts `0, 1, 2, 0.5, -1`; ours `2, 0.5, -1, 1, 0`. Literals are pooled in
  code-generation order, so retail generated something using 0 then 1 before the first function it
  emitted - an inline or stripped weak copy, or a function that sits elsewhere in the source.
  Changing `sNoRotation`'s initializer did not move them. Prime 1's `CQuaternion.cpp` has
  `IsValidQuaternion` and an out-of-line `BuildEquivalent` that ours lacks; untried.
- **`CPathFindRegion`** and **`CPlayerEnergyDrain`**: superseded later the same day - both were
  emission-order cases and are now `Matching`; see the third pass below. (The 0xC displacement in
  `CPathFindRegion` was one `rstl::vector< CVector3f >::clear()` sitting in the wrong place.)
#### Third pass: 2 more were the order of out-of-line rstl instantiations
`CPlayerEnergyDrain` and `CPathFindRegion` pass `flip_test.sh` and are `Matching`. Add a **step 0**
to the method above: diff the function order of the two objects,
```sh
build/binutils/powerpc-eabi-nm -n -S build/G2ME01/obj/<unit>.o   # retail
build/binutils/powerpc-eabi-nm -n -S build/G2ME01/src/<unit>.o   # ours
```
Retail interleaves out-of-line `rstl` instantiations with the source functions that use them; ours
pools them after the last source function. Every function is 100% in objdiff and every section size
agrees, yet the unit does not link. The fix is the one in `CStaticInterference.cpp`: an explicit
non-inline `template <>` specialization carrying the header's body, defined in the source where
retail has it. That makes ours strong (`T`) where retail's is weak (`W`), which the link does not
care about. `CPathFindRegion` needed one (`vector< CVector3f >::clear` before `FindBestPoint`);
`CPlayerEnergyDrain` needed six for `vector< CEnergyDrainSource >` (`reserve`, `lower_bound`,
`insert`, both `erase`, `clear`).
Examined with step 0 and **not** fixed - 8 all-100% units still fail the flip:
- **`CParticleGen`**: retail's object holds only `AddModifier` and the two `list< CWarp* >` helpers
  it calls, and no vtable. `__vt__12CParticleGen` is at `.data 0x803B1D9C`, just before
  `__vt__14CDummyGameArea`, with a null destructor slot, and the inline virtuals are at
  `0x800534B0..` in the `CGameArea` region. Ours emits a strong vtable (0x8C `.data`), the weak
  inline virtuals, `__dt__list<CWarp*>` and a 4-byte `.sdata2` 1.0f, because `AddModifier` is the
  key function. Two things do not work: claiming `.data 0x803B1D9C..0x803B1E28` for the unit gives
  "Cyclic dependency encountered while resolving link order" (early `.data`, late `.text`), and
  making the destructor pure with an inline body keeps the DOL but not the vtable out. Retail's
  `AddModifier` is therefore not the key function - probably an out-of-class `inline` that was
  not inlined. Untried: declaring it so and letting `CGameArea`'s unit own the vtable.
- **`CGunMotion`**: ours has 13 extra weak `CPAS*`/vector copies before `EnterFidget`, plus
  `__dt__` of `CPASAnimParmData`, `CGunController`, `CGSFidget`, `vector<CToken>` and
  `vector<int>::reserve`; retail references `kInvalidAreaId` at `.sbss 0x80419128`, unclaimed.
- **`CFBStreamedCompression`**: ours has an extra `GetAnimationDuration` (0x34 at 0x68), weak
  `__dt__` of `single_ptr<Ui>`/`auto_ptr<Ui>` and three `AfterEnd` copies; helpers that are in-unit
  in retail are weak in ours.
- **`CCharLayoutInfo`**: retail opens with weak `__dt__CCharLayoutNode`, `__dt__vector<CSegId>` and
  the `vector<CSegId>` copy constructor before `GetSegIdFromString`; ours has a different weak set
  (`GetFromParentUnrotated`, `ContainsDataFor`, ...).
- `CTweakAutoMapper`, `CTweakPlayer`, `CLight`, `CQuaternion`: the walls above, unchanged.
### The goal loop now judges a worktree the provider dropped (exit 1)
Measured over 2026-09-30..10-01 in the lane logs: 263 PASS, 94 judge FAIL, 48 agent exits. 41 of the
48 were exit 1 - the provider ending the stream ("stream ended without finish_reason", "socket
connection was closed unexpectedly") after a median of about 20 minutes - and each was reset
unjudged. `tools/run_goal.sh` already judged a timed-out agent's edits (exit 124/137); it now does
the same for exit 1 when `src/` or `include/` changed. A lane picks this up when it restarts.
Of the judge FAILs, 79 were "changed nothing under src/ or include/" and 44 "target did not rise".
The 12 "decomp_build.sh printed no All: line" are not a defect of their own: each sits under a
`ninja` compile or link failure in the same check that the build-fix round did not repair.
The review queue was read on the same day: of 87 items about 60 are measured compiler walls, 7 were
stale (their functions had since matched; removed, backup `review-queue.json.bak-20261001`), and the
three saved patches (`progress-prime1-csortedlists`, `-cplayergun`, `-cscriptactor`) no longer apply
to their units. Nothing in it was a correct change waiting to be landed.
