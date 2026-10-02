# progress-rel-head-lumite — Lumite's (module 39) head, .text 0x0..0x190

## What I did

Wrote the module head in C++ and claimed exactly the contiguous range it reproduces, as
`CMysteryFlyerRel.cpp` / `CAtomicAlphaRel.cpp` do. Four files:

- **new** `src/MetroidPrime/ScriptObjects/CLumiteRel.cpp` — 17 functions, `.text 0x0..0x190`.
- `config/G2ME01/rels/Lumite/splits.txt` — new claim
  `MetroidPrime/ScriptObjects/CLumiteRel.cpp: .text start:0x00000000 end:0x00000190`
  (inserted above the existing `REL/REL_Setup.cpp` claim; `Sections:` untouched).
- `configure.py` — `Rel("Lumite", [Object(Matching, "MetroidPrime/ScriptObjects/CLumiteRel.cpp")])`,
  placed after `SnakeWeedSwarm` with a comment in the house style.
- `docs/research/raw_offsets.md` — the section `tools/check_raw_offsets.py` requires for the
  file's one keyed site (1 site, `+0x54`).

Deliberately **not** touched: `files.cmake` (the file defines `RELMain`/`RELExit`, so it stays in
`check_files_cmake.py`'s excluded set, as every other module head does), `docs/HANDOFF.md`,
`docs/RUNNING_THE_DECOMP.md`. Nothing from `fn_39_190` (0x190, 0x5A8) up is claimed, so dtk fills
the rest from retail.

## The claim, from `config/G2ME01/rels/Lumite/symbols.txt`

    0x000 fn_39_0   0x68  optional_object<CAABox> from GetBoundingBox (inlined, no call)
    0x068 fn_39_68  0x10  lbl_8041AAB8 -> *((float*)(self + 0x448))
    0x078 fn_39_78  0x08  the byte at +0x44f
    0x080/88/90    0x08  three `li r3,0`
    0x098 fn_39_98  0x10  *self = kInvalidUniqueId
    0x0A8 fn_39_A8  0x0C  byte at +0x34c, bit 3
    0x0B4 fn_39_B4  0x08  address of the member at +0x754
    0x0BC fn_39_BC  0x08  `li r3,1`
    0x0C4/CC       0x08  two more `li r3,0`
    0x0D4 fn_39_D4  0x1C  three floats from self+0x54 -> *out
    0x0F0 fn_39_F0  0x2C  vtable slot 0x38
    0x11C RELExit   0x24  `li r3,0` / `bl fn_80218BFC`
    0x140 RELMain   0x20  `bl fn_39_160`
    0x160 fn_39_160 0x30  `lbl_39_bss_40 = fn_39_190; fn_80218BFC(&lbl_39_bss_40)`

Callees are named by what they are, not invented:

- `fn_80218BFC` is the plain name `config/G2ME01/symbols.txt:9490` already gives the DOL's
  0x80218BFC (`size:0x8`, the two-instruction setter `stw r3, gLoader_Lumite; blr`;
  `gLoader_Lumite` is `:20745`, `.sbss:0x80419428`). No `symbols.txt` rename and no long
  `SetLoader_...` mangling is needed, unlike `CIngSnatchingSwarmRel.cpp`. Declared
  `extern "C"` because REL modules import it by that retail name.
- `lbl_39_bss_40` is `.bss:0x40, size:0x4 data:4byte` — **not** `lbl_39_bss_0`: this module's
  `.bss` holds five objects (`lbl_39_bss_0` 8, `_8` 0x18, `_20` 8, `_28` 0x18, `_40` 4). The
  split claims `.text` only, so dtk's `.bss` object defines it; hence `extern` under
  `__MWERKS__` and a host definition, the arrangement `CMysteryFlyerRel.cpp` uses.
- `fn_39_190` is the module's own entity loader, **unclaimed** (0x190 onward), declared `extern "C"`
  so the registration can store its address.

## The one thing that is not the family arrangement: `fn_39_0`

The other heads' `GetBoundingBox` wrapper is 0x3C bytes ending in a call to an out-of-line
`optional_object<CAABox>` converting ctor (MysteryFlyer's `fn_45_10` → `bl fn_45_2BBC`).
**Lumite has no such symbol: `fn_39_0` is 0x68 bytes with the whole conversion inlined** — the
flag store at +0x18 and the 24-byte `CAABox` copy are in the body.

Measured, and this is the spelling that works:

```cpp
rstl::optional_object<CAABox> fn_39_0(const CPhysicsActor* self) { return self->GetBoundingBox(); }
```

with `#include "rstl/optional_object.hpp"`. MWCC emits `li r0,1; stb r0,0x18(r31)` then the six
`lwz`/`stw` pairs with no hand-written copy, because `CAABox` carries
`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`. The one-method `CPhysicsActor` stand-in is load-bearing:
`CAABox GetBoundingBox() const` takes `this` in r4 — the register `self` already arrives in — and
returns through r1+0x8, which is the 0x30 frame and the 0x34 saved-LR slot retail has. Do **not**
include `MetroidPrime/CPhysicsActor.hpp`: it reaches `CMaterialList.hpp` and its file-scope
`static EMaterialTypes SolidMaterial` puts 0x28 bytes of `.data` in the object.

**A hand-written equivalent does not compile to this.** My first version spelled the flag store
and the copy out (`*reinterpret_cast<CAABox*>(out) = box;` after `static_cast<unsigned
char*>(out)[0x18] = 1;`); it produced a **0x100-byte** `fn_39_0` instead of 0x68, because
MWCC materialised the box on the stack and copied it through f0/f1/f2/f3/f4. That shifted the
other sixteen functions and broke the claim. The real return type is the fix.

The other thirteen accessor bodies are the ones `CAtomicAlphaRel.cpp` carries (there at
0x10..0xC8), so no spelling had to be discovered. `fn_39_D4` reuses f0 for all three loads, so it
stays subscript stores, not a `CVector3f` copy. Definitions are in descending retail offset
(mwcceppc emits in reverse source order); `tools/check_decl_order.py` is clean.

## What I measured

**1. The claimed range is byte-identical to retail.** `build/G2ME01/Lumite/obj/.../CLumiteRel.o`
is *not* the object the link uses — that directory holds a stale artifact from an earlier run,
and comparing it reports 0 diffs for a source that does not match. The linked object is
`build/G2ME01/src/MetroidPrime/ScriptObjects/CLumiteRel.o`. Comparing **that** `.text` against the
retail bytes parsed out of `build/G2ME01/Lumite/asm/auto_00_00000000_text.s`:

    our .text size 400 (retail claim 0x190 = 400)
      reloc  +0x018 -> GetBoundingBox__13CPhysicsActorCFv
      reloc  +0x12C -> fn_80218BFC
      reloc  +0x14C -> fn_39_160
      reloc  +0x17C -> fn_80218BFC
    real diffs: 0

400 bytes exactly, no padding, and the only four differing words are the `bl` fields, which are
relocations to the retail symbol names.

**2. `tools/audit_rel_claim.py Lumite`**

    ok   MetroidPrime/ScriptObjects/CLumiteRel.cpp            0x00000000..0x00000190  17/17 functions
    ok   REL/REL_Setup.cpp                                    0x000075E0..0x00007784  5/5 functions
    0 claim(s) with a problem
    Lumite: preplf 164 text symbols, plf 164, 0 dropped by -strip_partial

164 in, 164 out, none dead-stripped — so no `force_active:` entry is needed in
`config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit).

**3. Clean:** `check_raw_offsets.py` (0), `check_symbol_names.py` ("checked 484 units; 0
declared names are missing"), `check_decl_order.py` ("939 unit(s) checked, 28 permuted, all 28
accounted for in decl_order.md"), `check_files_cmake.py` (0),
`check_module_wiring.py` (0 — and it now reports **87 units of our own code in 67 modules**,
with Lumite among them), `probe_sources.sh` ("736 files, 0 failed, 0 errors; link: LINKED
(254 undefined, 0 duplicates)").

## BLOCKER: the module sha1 could not be measured in this tree, and the reason is pre-existing

`./tools/decomp_build.sh` fails at step 5/11 on `build/G2ME01/main.elf`, **at HEAD with my
change stashed**, so it is not mine:

    ### mwldeppc.exe Linker Error:
    #   undefined: 'sndStreamMixParameter'
    #   Referenced from: 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o

Reproduced on the clean tree:

    $ git stash push -u -m lumite-wip && ./tools/decomp_build.sh
    ... undefined: 'sndStreamMixParameter' ...        # identical failure
    $ git stash pop

Cause: `Kyoto/Audio/CDSPStreamManager.cpp` is `NonMatching`, so the link uses the **retail**
object `build/G2ME01/obj/Kyoto/Audio/CDSPStreamManager.o`, which calls `sndStreamMixParameter`.
That DOL symbol (`config/G2ME01/symbols.txt:16524`, `.text:0x8038BBF0 size:0x564`) lies inside
`musyx/runtime/stream.c`'s claim, which is `Matching` (configure.py:1275), so the definition has
to come from `extern/musyx/src/musyx/runtime/stream.c:759` — where it sits behind
`#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)`. The built object has the sibling but not it:

    $ build/binutils/powerpc-eabi-nm build/G2ME01/src/musyx/runtime/stream.o | grep -i MixParameter
    00001e60 T sndStreamMixParameterEx

Because `main.elf` never links, ninja's `makerel` step (order-only dep `post-link`) never runs,
so **no** REL module's sha1 can be checked on this branch. I confirmed that is the only way to
get a `.rel` out: `dtk rel make -c config/G2ME01/config.yml -n Lumite .../Lumite.plf` reports
"resolved 0 symbols" and writes nothing, and passing `main.dol` gives "Failed to load ... Unknown
file magic" — it wants the ELF.

I tried defining the symbol temporarily to unblock the link and measure the hash, and **backed it
out**: it shifts the DOL, which invalidates every module's import resolution (all 86 RELs then
report "WARNING: 87 computed checksum(s) did NOT match"). Both temporary edits are reverted —
`git status` shows only my four intended paths, and
`grep -c sndStreamMixParameter src/Runtime/MetroTRKConsoleStubs.cpp` is 0. I then restored all
86 `build/G2ME01/*/*.rel` from the read-only disc, so nothing false is left in the tree.

So the hash claim is **inferred, not measured here**. The inference is strong — the entire claim
is byte-identical to retail and the module's symbol count is unchanged at 164 with none dropped,
which is what the recipe requires — but `config.yml`'s `1c53f8b7…` was not re-verified in this
run, and a lane should re-check it once the link works.

`build/goal/judge/` holds a **prior run of this same item on this lane** (logs 22:10-22:16,
`rebase.patch` is its diff) that reached the same 0x0..0x190 claim and whose judge recorded:

    ok  target rose: module:Lumite: 5 -> 22 / 164 functions
    ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    goal_check: PASS progress-rel-head-lumite

`22 = 5 REL_Setup + 17 ours`, the same number my claim reaches. That is corroboration from a
*different* run, not a substitute for the measurement, and its own rebase-build at 22:16 shows
this same `main.elf` failure — so the link broke when the driver rebased onto `ada6d97`.

## Notes for the next attempt

- `check_docs_claims.py` exits 1 on the derived counts only: the HANDOFF state block's
  `DOL units 8095 / 16726 functions`, `**87 units of our own code in 67 modules**`, and
  "module Lumite links our code but is not named in the docs". All three are the judge's to
  rewrite (per the lane prompt), so I left HANDOFF.md alone. The module list in
  `docs/RUNNING_THE_DECOMP.md`'s state block is likewise the judge's.
- Nothing here needs a new accessor spelling for a retry: all sixteen bodies are
  `CAtomicAlphaRel.cpp`'s, and `fn_39_0` is settled above with the one spelling that compiles to
  retail's bytes.

NEW: musyx-dsp-sndstreammixparam | match | musyx/runtime/stream.c | the DOL linker symbol sndStreamMixParameter (0x8038BBF0, symbols.txt:16524) is compiled out by `#if MUSY_VERSION <= 2.0.2` in extern/musyx/src/musyx/runtime/stream.c:759, so main.elf fails to link and every REL module's sha1 becomes uncheckable on this branch

---

# Run 2 - 2026-09-29, lane 4, HEAD `b04f1a9` ("match: match-cmetaanimrandom")

## What I did

Run 1's head claim was still correct, so I re-applied it verbatim and re-measured it rather than
re-deriving it: `src/MetroidPrime/ScriptObjects/CLumiteRel.cpp` (17 functions, `.text 0x0..0x190`),
its `config/G2ME01/rels/Lumite/splits.txt` entry, the `Rel("Lumite", ...)` block in `configure.py`
and the `docs/research/raw_offsets.md` section - all four paths, all byte-for-byte as run 1 left
them. Its object reproduces retail's 400 bytes with 0 real diffs, again.

**New this run: a second claim in the module, `CLumiteRelDel.cpp`, `.text 0x778..0x7C0`, two
functions** (`fn_39_778`, `fn_39_798`) - so six paths in all, the two extra being a second
`Object(Matching, ...)` in the existing `Rel("Lumite")` list and one line in `files.cmake`.
**module:Lumite goes 5 -> 24 / 164** (5 `REL_Setup` + 17 + 2), measured with goal_check's own rule
(sum of `matched_functions` over units named `Lumite/...`).

## What I measured

**1. Both claims are byte-identical to retail.** Same method as run 1 - the *linked* object
(`build/G2ME01/src/...`, not the stale `build/G2ME01/Lumite/obj/...`), `.text` against the bytes
parsed out of `build/G2ME01/Lumite/asm/auto_00_00000000_text.s`:

    CLumiteRel.o     .text 400 (0x190) vs retail 0x190   7 differing bytes, all `bl` fields
                     reloc +0x018 GetBoundingBox__13CPhysicsActorCFv, +0x12C fn_80218BFC,
                     +0x14C fn_39_160, +0x17C fn_80218BFC
    CLumiteRelDel.o  .text  72 (0x48)  vs retail 0x48    2 differing bytes, both `bl` fields
                     reloc +0x00C fn_39_798, +0x034 fn_39_7C0

**2. `python3 tools/audit_rel_claim.py Lumite`**

    ok   MetroidPrime/ScriptObjects/CLumiteRel.cpp     0x00000000..0x00000190  17/17 functions
    ok   MetroidPrime/ScriptObjects/CLumiteRelDel.cpp  0x00000778..0x000007C0   2/2 functions
    ok   REL/REL_Setup.cpp                             0x000075E0..0x00007784   5/5 functions
    0 claim(s) with a problem
    Lumite: preplf 164 text symbols, plf 164, 0 dropped by -strip_partial

**3. `build/report.json`, regenerated by hand - see the trap below - gives
`module:Lumite: 5 -> 24 / 164`, global `matched_functions` 9527 / 28465, fuzzy 29.190672.**

**4. Clean:** `check_symbol_names.py` ("checked 484 units; 0 declared names are missing"),
`check_decl_order.py` ("939 unit(s) checked, 28 permuted, all 28 accounted for in decl_order.md"),
`check_raw_offsets.py` ("153 raw-offset site(s) in 62 file(s), all documented" - the new file has
**no** raw offset, its only one is a subscript the checker deliberately does not key on),
`check_module_wiring.py` (0; now **88 units of our own code in 67 modules**),
`check_files_cmake.py` ("every configured DOL object is either in files.cmake or excluded with a
reason"), `probe_sources.sh` ("737 files, 0 failed, 0 errors; link: LINKED (254 undefined,
0 duplicates)" - 737 not 736, and the undefined count unchanged at 254). No `asm` added.

## TRAP: `build/report.json` is stale whenever the build stops, and it looks like a real answer

`tools/decomp_build.sh` runs `"$NINJA"` under `set -e` and only then runs
`objdiff-cli report generate -o build/report.json`. With `main.elf` broken (below) ninja exits
non-zero, so **the report is never regenerated and the file left in the tree is the previous
run's**. Mine read `module:Lumite 5 / 164` and global `matched_functions 9508` - byte-identical to
`build/goal/judge/report.base.json`, which is exactly what a claim that did nothing looks like.
The build log says `Generating report for 2028 units` and writes `build/G2ME01/report.json`, which
*is* fresh; it is the copy at `build/report.json` that is stale. Run
`./build/tools/objdiff-cli report generate -o build/report.json` yourself, or you will measure the
previous run and conclude your claim is dead.

## BLOCKER (unchanged, pre-existing): `main.elf` does not link, so no module sha1 is checkable

    ### mwldeppc.exe Linker Error:
    #   undefined: 'sndStreamMixParameter'
    #   Referenced from: 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o

Measured **at the clean baseline too**: `build/goal/judge/record-gate.log`, recorded by the judge
at `b04f1a9` with my tree stashed, shows the identical error and `GATE FAIL: ninja`. So this is
not mine and not the claim's; it is the branch. `build/G2ME01/main.dol` in this tree is a stale
artifact, `4cc3868bbc84fa285d8bfebe188ffa68dd49234e`, not the pinned `6ef9b491...`, and cannot be
rebuilt while the link is broken. `hashes vs config.yml` is `skipped` for the same reason, so the
two claims above are verified byte-exact and by `audit_rel_claim.py` but **not** by a module sha1.

### Run 1 blamed the wrong line. The cause is the `-D` flag, not the `#if`

Run 1's note says the definition "sits behind `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)`"
and points at `extern/musyx/src/musyx/runtime/stream.c:759`. That `#if` is correct and evaluates
true; the function is compiled out because the **build flag says 2.0.3**:

    configure.py:358  def MusyX(objects, mw_version="GC/1.3.2", major=2, minor=0, patch=3)
    build.ninja:18530  cflags = ... -DMUSY_VERSION_MAJOR=2 -DMUSY_VERSION_MINOR=0
                              -DMUSY_VERSION_PATCH=3

so `MUSY_VERSION` is `0x020003` and `0x020003 <= 0x020002` is false. Nothing is wrong with
`stream.c`. Measured both ways:

    $ build/binutils/powerpc-eabi-nm build/G2ME01/obj/musyx/runtime/stream.o   # retail's
    00001e4c T sndStreamMixParameter
    $ build/binutils/powerpc-eabi-nm build/G2ME01/src/musyx/runtime/stream.o   # ours
    00001e60 T sndStreamMixParameterEx

and retail's symbol table agrees with the retail object: `config/G2ME01/symbols.txt:16524` is
`sndStreamMixParameter = .text:0x8038BBF0 size:0x564`, and `sndStreamMixParameterEx` appears
**nowhere** in the DOL (0 hits). So retail is `<= 2.0.2` and the `patch=3` default is the thing
that has to change - and it is the default of the one `MusyX(...)` call at `configure.py:1269`,
which carries ~20 objects, so the fix is a real `match` item and not a one-liner. The *host* port
does not see any of it: `extern/musyx-port/src/musyx/runtime/stream.c:695` has the function with
no guard, which is why `probe_sources.sh` still links at 254.

I did **not** touch it. It is a different SDK library, and the lane brief says to file a `NEW:`
line rather than fix an unrelated thing inside this item's diff.

## MEASURED WALL: `fn_39_738` (0x738, 0x40) is two instructions from retail and no spelling reaches it

This is the teardown guard, and it is the one function of the three I could not land. Retail:

    stwu / mflr / stw r0,0x14 / **lbz r0,0x4c(r4)** / **stw r31,0xc / mr r31,r3** / cmplwi r0,0 /
    stb r0,0x4c(r3) / beq .L / bl / .L: lwz r0,0x14 / mr r3,r31 / lwz r31,0xc / mtlr / addi / blr

The flag at `+0x4C` is read out of the **second** argument and stored through the first, and the
epilogue's `mr r3, r31` says the function returns `self` - returning it is what forces the r31
spill. Measured at GC/1.3.2 with the flags `configure.py` gives a REL unit:

| spelling | `.text` | result |
|---|---|---|
| `void f(void* s, const void* o)` (no return) | 0x30 | 12 instrs, `lbz` in retail's slot 3, but the r31 pair and `mr r3,r31`/`lwz r31` are gone |
| `void* f(...)` + `if (f != 0) call(); return self;` | **0x40** | 16 instrs, **right count, `stw r31,0xc / mr r31,r3` land before the `lbz`**, retail has them after |
| same, with a `bool` temp, with a local `void* p`, with `if (f == 0) return self; else call();` | 0x40 / 0x40 / 0x4C | the early-return forms add a `b` and a second `mr r3,r31` (duplicate epilogue) |
| typed `CLumiteDel*` params and members, and `reinterpret_cast` forms | 0x40 | identical to row 2 - typing does not move the pair |

Every spelling that carries the spill puts the spill first; the one that puts the `lbz` where
retail has it has no spill. That looks like a scheduling difference between MWCC GC/1.3.2 and
whichever version built the retail REL, not something the source reaches. I left 0x738..0x778
retail rather than claim a range a byte off, which is also why the shipped claim starts at 0x778.

WALL: fn_39_738 0x40-vs-0x40 - MWCC GC/1.3.2 always emits the r31 spill before the lbz, and the spelling that puts the lbz in retail's slot has no spill at all.

## One spelling that did have to be discovered, for `fn_39_798`

`if (s) { fn_39_7C0(s, o); }` and `if (s == 0) { return; } fn_39_7C0(s, o);` are the same program,
and MWCC schedules them differently - the first puts `cmplwi r3,0` *after* the saved LR, the second
puts it *before*, which is where retail has it:

    if (s) {...}          stwu / mflr / stw r0,0x14 / cmplwi r3,0 / beq / bl / ...   wrong
    if (s == 0) return;   stwu / mflr / cmplwi r3,0 / stw r0,0x14 / beq / bl / ...   retail

Both measured, same flags, same TU. Recorded in the source because the conditional form is the one
anyone would write.

## `check_files_cmake.py` only exempts a REL unit that defines RELMain/RELExit - worth knowing

A second claim in a REL module is not like a DOL carve: it needs a fourth and fifth file, and
`files.cmake` is the fifth. `tools/check_files_cmake.py` walks every `Object(Matching, "…cpp")` in
`configure.py` and requires the path to be either listed in `files.cmake` or in its own `EXCLUDED`
set - **and `EXCLUDED` is judge-owned, so a lane cannot add to it.** The only automatic exemption is
`MODULE_ENTRY = \bvoid RELMain\(|\bvoid RELExit\(`, which is why every landed REL *head* passes
(33 such units on this branch) and why `CSplitterRelMain.cpp` passes: that file happens to claim
RELMain and RELExit, not because it is a second unit. My first attempt at the second claim failed
`files.cmake  ok` -> `omitted: …/CLumiteRelDel.cpp is a Matching object … and is neither in
files.cmake nor in check_files_cmake.py's EXCLUDED list`.

The way through is to list the file and make its host branch empty, which is the same arrangement
`CLumiteRel.cpp` uses for RELMain/RELExit: the two functions are inside `#ifdef __MWERKS__` and the
`#else` defines nothing at all, so the port compiles the file and its undefined count does not move
(measured: still 254, 0 duplicates, 737 files). Nothing is stubbed for the matching build - the
MWCC branch is the real body and reproduces retail's bytes.

## Row for `docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table

Not added - the lane brief says the driver discards edits to that file. The row's content:

> `Lumite` | two units | `.text 0x0..0x190` (17) and `.text 0x778..0x7C0` (2) | head is `CDarkCommandoRel.cpp`'s with its first two functions removed and one predicate added, and `fn_39_0` **inlines** the `optional_object<CAABox>` conversion (no out-of-line ctor, unlike `CMysteryFlyer`'s `fn_45_10`); the loader record is `.bss:0x40`, not `.bss:0x0`; the import is the plain DOL symbol `fn_80218BFC`, no `symbols.txt` rename. Second unit is the two forwarders into `fn_39_7C0` and needs the early-return spelling. Blocked: `fn_39_738` and everything from `fn_39_190` (the entity loader, 0x5A8) up, which needs the CActor/CPatterned hierarchy. 5 -> 24 / 164, module sha1 unmeasured - `main.elf` does not link on this branch.

## NEW: line

Re-filed with the corrected cause; run 1's version of this line pointed at the wrong line of
`stream.c` and would have sent a lane looking in the wrong place.

NEW: musyx-version-patch | match | musyx/runtime/stream.c | `MusyX()` defaults `patch=3` (configure.py:358) so `-DMUSY_VERSION_PATCH=3` makes MUSY_VERSION 2.0.3 and compiles out `sndStreamMixParameter` (stream.c:759), which retail has and which CDSPStreamManager.cpp's retail object calls - main.elf will not link, so no REL module's sha1 is checkable on this branch

---

# Run 3 - 2026-10-02, lane 12, HEAD `a3bbe16d` ("match: carve-800f1a10")

## What I did

Re-derived and re-measured the head claim 0x0..0x190 (17 functions) on this tree, and landed the
tail as **0x738..0x7C0 (3 functions)** rather than run 2's 0x778..0x7C0 (2): `fn_39_738` is
matchable, and the wall run 2 recorded was a compiler-version wall, not a spelling one.

Six paths (run 2's CLumiteRelDel.cpp is re-created here as CLumiteRelTail.cpp, same two functions
plus the third):

- **new** `src/MetroidPrime/ScriptObjects/CLumiteRel.cpp` - 17 functions, `.text 0x0..0x190`
- **new** `src/MetroidPrime/ScriptObjects/CLumiteRelTail.cpp` - 3 functions, `.text 0x738..0x7C0`
- `config/G2ME01/rels/Lumite/splits.txt` - both claims, above the REL_Setup claim
- `configure.py` - `Rel("Lumite", ...)` after `SnakeWeedSwarm`; the tail object carries
  `mw_version="GC/2.7"` (the same per-object override `CGameOptions.cpp` uses)
- `files.cmake` - `CLumiteRelTail.cpp`, host branch empty by design
- `docs/research/raw_offsets.md` - the head file's 1 keyed site (`+0x54`)

## The measurement runs 1 and 2 could not make: the module sha1 holds here

    $ cmp build/G2ME01/Lumite/Lumite.rel orig/G2ME01/files/RelProd/Lumite.rel && echo IDENTICAL
    IDENTICAL
    $ sha1sum build/G2ME01/Lumite/Lumite.rel
    1c53f8b7662fc828cddb56d978910533629b5fe9      # == config/G2ME01/config.yml's pin
    $ python3 tools/audit_rel_claim.py Lumite
    ok   MetroidPrime/ScriptObjects/CLumiteRel.cpp      0x00000000..0x00000190  17/17 functions
    ok   MetroidPrime/ScriptObjects/CLumiteRelTail.cpp  0x00000738..0x000007C0   3/3 functions
    ok   REL/REL_Setup.cpp                              0x000075E0..0x00007784   5/5 functions
    0 claim(s) with a problem
    Lumite: preplf 164 text symbols, plf 164, 0 dropped by -strip_partial

**Run 2's NEW: blocker is STALE, measured, not assumed**: `main.elf` links on this branch, so the
sha1 gate runs. `MusyX()` still defaults `patch=3` (configure.py:358), but our musyx object now
defines the symbol - `build/binutils/powerpc-eabi-nm build/G2ME01/src/musyx/runtime/stream.o` gives
`00001e4c T sndStreamMixParameter`, main.dol is the pinned `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
and gate.sh prints `hashes vs config.yml ok` over all 86 RELs. Nothing to re-file, and the sha1 is
no longer "inferred".

## fn_39_738 was GC/1.3.2's scheduling, not the source

**Run 2's `WALL: fn_39_738` line above is superseded - do not park on it.** Retail reads the flag
out of r4 *before* saving r31:

    lbz r0,0x4c(r4) / stw r31,0xc(r1) / mr r31,r3 / cmplwi r0,0 / stb r0,0x4c(r3) / beq / bl / ... mr r3,r31

Measured this run, same TU, same cflags, one compiler version apart (136-byte object vs the bytes
parsed out of `build/G2ME01/Lumite/asm/auto_00_00000000_text.s` at 0x738):

    GC/1.3.2  14 differing bytes - the `stw r31` / `mr r31` save pair sits above the lbz
    GC/2.7     3 differing bytes - all three in `bl` displacement fields, which the linker patches
               from relocations (the field is not final in the .o); instruction order is retail's
    sweep: 1.0 / 1.1 / 1.1p1 / 1.2.5 / 1.2.5n fail to build; 1.3 / 1.3.2 / 1.3.2r 14 bytes;
           2.0 / 2.0p1 / 2.5 / 2.6 / 2.7 3 bytes; 3.0a3 / 3.0a5.2 28 bytes

The proof is the linked module, not the object: with `fn_39_738` claimed and compiled at GC/2.7 the
REL is byte-identical and the audit says 3/3. The other two functions in that object are identical
under both versions, so the override costs nothing. Spelling, for the record (natural - the source
was never the problem):

    void* fn_39_738(void* self, const void* other) {
      const unsigned char flag = static_cast< const unsigned char* >(other)[0x4C];
      static_cast< unsigned char* >(self)[0x4C] = flag;
      if (flag != 0) { fn_39_778(self); }
      return self;
    }

The generalisable lesson (**put it in a notes file, not a NEW item**): when a function's remaining
diff is only instruction *scheduling* and no spelling moves it, the per-object `mw_version` knob is
a real axis of variation - `configure.py` already uses it for `CGameOptions.cpp` (2.0p1) for exactly
this reason. A version sweep is one compile per version; this run used a scratch wrapper around the
same cflags (`.tmp/opencode/mwcc.sh`) and compared raw `.text` bytes.

## Judge

    goal_check: PASS progress-rel-head-lumite
      ok  no judge-owned path touched
      ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok  counts: matched 12495 -> 12515   linked 5871 -> 5891
      ok  check_symbol_names.py
      ok  All:  35.32% fuzzy, 29.15% matched, 12.91% linked (12515 / 28465 functions)
      ok  target rose: module:Lumite: 5 -> 25 / 164 functions
      ok  no asm added

`gate.sh` also records the split explicitly: `per-function diff  SPLIT  Lumite/auto_00_00000000_text:
158 function(s) accounted for across 4 new unit(s) in Lumite (exact count match - a split, not a
loss)`. Clean too: `unit_fit.sh` for both units ("fits", "no extra functions"), `check_decl_order.py`
(988 units, 28 permuted, all accounted for; both our units in retail order),
`check_raw_offsets.py` (168 sites in 72 files, all documented), `check_files_cmake.py` 0,
`check_module_wiring.py` 0 (101 units of our code in 78 modules, Lumite among them).

I reverted the two judge-owned docs (`docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md`) that
`goal_check.sh`'s own `MP_GATE_DOCS_WRITE=1` gate run rewrote, so the diff is only the six paths
above; the driver's own judge run rewrites them again.

## What is still blocked in this module (measured, unchanged by this run)

**139 of Lumite's 164 text symbols are unclaimed.** `fn_39_190` (0x190, 0x5A8) is the module's
entity loader; `fn_39_7C0` (0x7C0, 0x13C) is the module's copy/steal constructor - three floats, a
flag+word pair, three `optional_object<CToken>` members copied through `__ct__6CTokenFRC6CToken`
then `Lock__6CTokenFv`, one of them clearing the source's flag byte; and the 137 functions above it
are the class's own methods. Reading 0x7C0..0x968 this run showed deleting destructors of
unmodelled helper classes (`__dt__6CTokenFv`, `Free__7CMemoryFPCv`, `__dt__9CAnimDataFv`), not
wrappers. All of it needs the CActor/CPatterned hierarchy and the CToken/optional_object member
types. Nothing above 0x7C0 was attempted because a claim is contiguous: the run from 0x7C0 to
0x72C4 has to match as a whole, and one member of it (`fn_39_72C4`, 0x31C) is a compiler-generated
static initialiser over `lbl_39_data_*`.

## Row for `docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table (driver-owned, not added)

> `Lumite` | two units | `.text 0x0..0x190` (17) and `.text 0x738..0x7C0` (3) | head is
> `CDarkCommandoRel.cpp`'s with the `GetBoundingBox` wrapper at 0x0: its `optional_object<CAABox>`
> conversion is **inlined** (there is no out-of-line ctor in the module's symbols), the same split
> DarkCommando's `fn_3_14` has, unlike MysteryFlyer's `fn_45_10`; the predicate run is three
> `li r3,0` in a row at 0x80/0x88/0x90 with the block's only `li r3,1` at 0xBC, not the family's
> alternation; the loader record is `.bss:0x40`, not `.bss:0x0`; the setter import is the plain DOL
> symbol `fn_80218BFC`, no `symbols.txt` rename. The tail object is `fn_39_778`/`fn_39_798` (the
> null guard needs the early-return spelling) **plus `fn_39_738`, which needs GC/2.7** - under the
> REL default 1.3.2 the r31 save is emitted above the `lbz` retail has first, and 2.7 reproduces the
> function instruction for instruction. Blocked: `fn_39_190` and everything from `fn_39_7C0` up
> (139 functions), which needs the CActor/CPatterned hierarchy. 5 -> 25 / 164, and the module sha1
> `1c53f8b7662fc828cddb56d978910533629b5fe9` is **measured** this run, REL `cmp`-equal to retail.
