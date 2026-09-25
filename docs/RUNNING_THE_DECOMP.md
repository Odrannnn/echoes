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
| `tools/gate.sh [--baseline]` | **the whole acceptance test in one command**: configure, ninja (whose exit status *is* the hash check), an independent re-hash against `config.yml`, the per-function report diff, wiring, docs claims and the probe. Non-zero exit on any failure. `--baseline` records `build/report.base.json` from a clean tree. |
| `tools/report_diff.py <base> <new>` | per-function diff of two reports: `WORSE`, `GONE`, `UNLINKED`, and a fell linked total. Replaces the hand-typed comparison that missed things. |
| `tools/check_module_wiring.py` | is every module with sources in `src/` actually wired into the build - catches a `Rel(...)` block lost to a config clobber, and counts the modules that link our own code. |
| `tools/range_owner.py <section> <start> <end>` | which unit claims a split range, if any - before carving one for a new unit. |
| `tools/range_bounds.py <start> <end>` | does a proposed range start and end on real symbols in retail. |
| `tools/fnmap.py <unit>` | byte-identical function pairing between a unit's retail object and ours (the mechanical half of a port). |
| `tools/autorename.py <unit>` | rename every byte-identical `fn_` function after our own symbol, via the two above. |
| `tools/apply_rename.py` | apply `old=new` renames to `symbols.txt` from stdin, reporting any it could not find. |
| `tools/scaffold_rel_module.py` | the three artifacts needed to start a REL module, printed or `--write`. |
| `tools/probe_sources.sh` | the port build's syntax sweep: 115 files, must stay 0 failures. |

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
- **`87 files OK` from a *lane's clone* is not evidence.** That rule dates from the rig defect above,
  where the check read the master tree's files. In a real `build/`, ninja's `CHECK` edge runs
  `dtk shasum -c config/G2ME01/build.sha1`, which hashes the DOL and all 86 RELs and is the same
  check as `config.yml` - so **ninja's exit status is the acceptance test**. Reading hashes off disk
  afterwards is the unreliable part: a failed ninja leaves the previous `main.dol` in place, which is
  how an early commit here claimed a green DOL after a build that had failed.

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

### A third rig defect: the acceptance test could pass on nothing

`tools/flip_test.sh` reported `PASS` in two situations where it had tested nothing at all:

- **The unit is in no `splits.txt`.** Nothing claims its range, so our object is compiled and never
  linked; the flip is a no-op. It passed on `CScriptIngSwarm.cpp` in 0.6 s. (Two commits wired
  `IngSwarm` and `WallCrawlerSwarm` this way, into nothing.)
- **The unit is `Matching` with no source file.** `configure.py` prints `Missing source file` and
  links the **retail** object; the check then passes trivially. `flip_test` hid that line with
  `>/dev/null`.

Both are now refused with an explanation, and the script exits non-zero if anything failed or was
skipped, so a caller cannot read success off a partial run. It also keeps its `configure.py` backup in
`mktemp` rather than one shared `/tmp/opencode/cfg.before` - two lanes flipping at once used to
restore each other's file, which is a plausible cause of the lost `Rel(...)` blocks described below.

The general lesson, and it is the same one three times over today: **a verification tool must be
proven able to fail.** `gate.sh` exists so that the acceptance test cannot be run partially, and
`report_diff.py` exists because losing a function was invisible to every gate.

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
   FORCEACTIVE - tested, rejected, reverted. **This blocks the whole tail of `ScriptCoin`**
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
move, which is why it stays COMMON. Worth knowing before concluding "MWCC put my global in the wrong
section": check whether the initializer is a constant expression first.

Two more negatives from the lane that first hit this, so nobody spends a session on them:

- Adding `operator=(const T*)` to `rstl::single_ptr` to match retail's `__as__...FPQ2...` mangling is
  not viable: it makes the `= nullptr` idiom ambiguous tree-wide (MWCC stops at `CActor.cpp:255`,
  `CCubeMoviePlayer.cpp:412`, `:536`, `:588`, `:909`) and MWCC inlines the 8-byte body at `-O4,p`
  anyway, so the out-of-line symbol never appears.
- `decomp_build.sh <unit>`'s per-function percentages are the ground truth. A bare two-object
  `objdiff-cli diff` disagrees on units that set `reverse_fn_order` (it reports 99.6x% for functions
  the project counts as matched). Score with the tool, not with the raw diff.

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

### A DOL unit can be blocked by data, not by code

`Kyoto/Graphics/CGX` matched every function it could (51 of 54, 99.47%) and **is not promotable**,
because its *sections* cannot be reproduced by C++ source. **Updated 2026-09-25: the functions are
now 53 of 54 (99.69%) and the data blocker is fully characterised - but it is not the
"unsourceable constant pool" this section used to claim.** The corrected version is below, because
the correction is the useful part: the constants are not compiler-generated at all.

**What retail's object actually does** (`build/G2ME01/obj/Kyoto/Graphics/CGX.o`, six symbols):

| symbol | section, address | referenced from | defined in |
| --- | --- | --- | --- |
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

**17 units are permuted right now** (18 before `CGX` was reordered), all of them `NonMatching` -
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

### The port's link gap is 732 symbols, and most of it is bulk work, not decompilation

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
- `./tools/probe_sources.sh` green (115 files, 0 failures)
- `python3 tools/check_symbol_names.py` reports 0 missing names
- `All:` matched count from the report does not fall
- `config/G2ME01/splits.txt` and `configure.py` only change when the task is explicitly a
  config task (REL modules), never as a side effect
- `python3 tools/check_docs_claims.py` reports no disagreement (it derives the docs' numbers from the
  report; a doc claim that cannot be checked is a claim that will drift)
- `python3 tools/check_module_wiring.py` reports nothing UNWIRED (a module whose sources no
  `configure.py` entry declares is in no link, whatever the report shows)
- `python3 tools/check_raw_offsets.py` is clean (a raw offset is a documented stopgap, not a
  decompilation - see `docs/research/raw_offsets.md`)
- `python3 tools/check_decl_order.py` agrees with `docs/research/decl_order.md` (a permuted unit
  compiles, links, scores 100% and breaks the hash on a few bytes - see "Declare in reverse")

## Attempted modules (keep this list current)

| module | what happened |
| --- | --- |
| `AIMannedTurret` | **Landed, 2026-09-25** - the first module whose unit genuinely flips, and the failure this table recorded for several sessions was real but was not a blocked module. Declared ascending, the unit broke the module's hash (85/86, exactly as measured); the cause was **declaration order**, not a rename, a symbol, a data section or extra functions. See "Declare in reverse" below. With the order fixed: unit `Matching`, `flip_test.sh` PASS, sha1 `949b8c21caf1112b10d07748dbe8c32d3bd7efac` verified against `config.yml`, DOL and all 86 RELs unchanged. The first modules to link our own code are still `ScriptRiftPortal` and `Metaree`; `AIMannedTurret` is the first whose unit **flips**. |
| `IngSwarm`, `WallCrawlerSwarm` | wired; no class code at all (all `REL_Setup`), so nothing to decompile. |
| `SkyRipple` | scaffold broke the hash (85/86 RELs) - claimed ranges did not match the object. Reverted. |
| `FogOverlay` | "completed" by transcribing 1,014 instructions into a `.s` unit. Rejected as not a decompilation. |
| `ScriptRsfAudio` | wiring, 7 correct symbol names, and an empty source; hash "matched" only because the unit was `NonMatching`. The symbol names are worth keeping; the rest proves nothing. **Superseded, 2026-09-25**: a later lane added exact `RELMain`/`RELExit`, loader registration and the setup range - 8 functions, unit `Matching`, sha1 `af0941ce5e81230282eda9cfb59e1839dc45443a` verified against config.yml. The remaining 14 module functions stay retail/unclaimed. |
| `ScriptPlayerProxy` | 9 functions (loader registration, `RELMain`/`RELExit`, an unnamed setup function and one field accessor) plus all 5 `REL_Setup` ones - unit `Matching`, sha1 `19ea68a377b4908848b9d640245842526a8dd968` verified. The remaining 48 class functions stay retail/unclaimed; this is the cheapest module shape yet found (its writable code is all `.text` wiring). |
| `DarkSamusBattleStage` | scaffolded split produced a 5,184-byte REL and an assembly object would not link. Correctly reverted with 0 functions. |
| `ScriptCoin` | **Landed, 2026-09-25** - 6 functions in 3 `Matching` units, module hash held, **+6 linked**. *Superseded:* the row above used to say "3 real functions written (a class, `Render`, `GetTouchBounds`) ... does not hold its hash yet", and **no `CScriptCoin.cpp` and no `Rel("ScriptCoin", ...)` block existed in the tree at all** - the claim was written from memory about a lane that never landed. The 6 functions are `RegisterCoinLoader`, `RELMain`, `RELExit` and a vtable slot in `CScriptCoinRel.cpp`, `CScriptCoin::Render` in `CScriptCoin.cpp`, and `CActor::GetTouchBounds` in `CScriptCoinTouchBounds.cpp`; all 100%, all with the unit `Matching`. The rest of the module stays retail and unclaimed. |
| `Ripper` | blocked with evidence: no `CRipper`, no `CPatterned`, no `include/MetroidPrime/Enemies/` at all. Reverted the scaffold rather than claim ranges it could not fill. The range check passed, so the block is the missing base classes, not the splits. |
| `Tweaks` | 2 generated constructors brought to exactly 100% (`SLdrTweakTargeting_Scan`, `SLdrTweakTargeting_VulnerabilityIndicator`) and 3 more moved 5-40 points closer, by moving the member assignments from the constructor body into the mem-init list. Not promoted - the other 12 units are blocked (seven `LoadTypedef*` at a 99.2% register-allocation wall, three on float-literal pooling, and the module's `.rodata` cannot be split per unit). |
| `CRumbleVoice`, `CRumbleGenerator` | `CRumbleVoice` now matches **five** of them (8/16 -> 13/16) after the fix below; 0 of `CRumbleGenerator`'s. The unmatched `fn_8032*` functions are TU-local weak `rstl::vector<SAdsrDelta>`/`<SAdsrData>` instantiations with no name in the retail object, so objdiff scored them 0% even when the bodies were byte-identical. **Solved for pairing** by writing explicit specialisations in the source and renaming the retail symbols in `symbols.txt` to the mangled names MWCC emits (read them from our own object with `nm`) - see "Pairing a function the retail symbol table has no name for". Neither unit can be promoted yet: `CRumbleVoice` emits 180 bytes the retail unit object does not have, `CRumbleGenerator` 452. |
| `CScriptStreamedMusic`, `CStaticAudioPlayer` | **Superseded for `CStaticAudioPlayer`, re-measured 2026-09-25.** The old reading - "pure register allocation, and 868 bytes of extra emitted functions on top" - was half right and has been corrected. `CStaticAudioPlayer` is now **23/24 at 99.87%**, and the "extra functions" are *not* the blocker: the DOL link passes `-strip_partial`, so mwldeppc deletes the 8 duplicate weak copies out of the middle of our `.text` and the flipped DOL comes out **exactly the same size as retail** (3 969 024 bytes both), with the bytes coming back out of the three objects that hold retail's copies (`CFilePreload`, `CCubeMoviePlayer`, `auto_03_8018A188_text`). What now blocks the flip is the **emission order of the out-of-line template instantiations** - see the new section "An emission-order wall: out-of-line template instantiations". `Decode` went 99.39% -> 100% on a one-statement `const` local; `DecodeMonoAndMix` 97.50% -> 98.70% and is stopped at 18 differing instructions. `CScriptStreamedMusic` was not re-measured. |
| `CGX` (DOL, not a module) | **53 of 54 and still not promotable, and the reason is data, not code.** The permutation went first (five local moves, ~15 lines - it was the unit `docs/research/decl_order.md` called the best value per line moved, and that is now paid out), then `SetDstAlpha` 99.43% -> 100% by assigning a widened local back to a `uchar` member, and `__sinit_CGX_cpp` 76.92% -> 100% by routing a constant initializer through an `inline` function. `.text` now measures 5936 against a claimed 5936, "fits", no extra functions. **Not flipped**, and a hand flip was measured rather than assumed: `main.dol` grows 32 bytes, `lbl_8041E4A0` moves to 0x8041E480, and `sGXState` (COMMON for us, `.bss` in retail) lands at 0x804170E0 against a claimed 0x803DF828. Three separate problems remain - six data symbols that must be *imports* rather than compiler-generated constants, `sGXState`'s COMMON-vs-`.bss` placement, and `SetVtxDescv_Compressed` on the register-allocation wall. Full symbol/address table and the DOL evidence in "A DOL unit can be blocked by data, not by code". The intended config changes, not applied here, are three `splits.txt` lines plus the six `extern` declarations - see the report. |

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
| `AIMannedTurret` | 3 functions (`fn_1_0`, `fn_1_8`, `fn_1_10`, all `extern "C"`) | unit `Matching`, sha1 `949b8c21…` verified; the first module whose unit flips - see "Declare in reverse" |
| `ScriptRiftPortal` | 3 functions (`SetFuncPtrs`, `RELMain`, `RELExit`) | first with a three-way split; sha1 `a0fa6c69…` verified against config.yml |
| `Metaree` | 23 named functions exact (18 ours + 5 setup), of 59 total; the rest unclaimed | first creature-family module; ranges unclaimed rather than named |
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
| `CPakFile` | 0 of 33 in the link - **not** a module | DOL unit, not REL: 24/33 at 100% after 2026-09-25, still `NonMatching`, `.text` 1904 bytes over its claimed range |
| `WallCrawler` | 18 functions | verified; no `LoadWallCrawler` or Think to attach to yet |
