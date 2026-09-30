# progress-rel-head-pirateragdoll

Target: `module:PirateRagDoll` (module 50). Kind: `progress`.
Outcome: **PASS** - `tools/goal_check.sh build/goal/item.json` prints `PASS
progress-rel-head-pirateragdoll`.

## What landed

Four functions, one `Matching` unit: `src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp`
claiming `config/G2ME01/rels/PirateRagDoll/splits.txt`'s new
`.text 0x000000..0x0000010C`, plus the `Rel("PirateRagDoll", ...)` block in `configure.py` that
the module did not have before. The four files a carve needs are all here: `configure.py`,
`config/G2ME01/rels/PirateRagDoll/splits.txt`, the source's own header, and the new source
(`files.cmake` is untouched on purpose - see below).

| retail | size | body |
| --- | --- | --- |
| `0x00 RELExit` | 0x24 | `fn_80227538(nullptr)` |
| `0x24 RELMain` | 0x20 | `bl fn_50_44` |
| `0x44 fn_50_44` | 0x30 | `lbl_50_bss_188 = fn_50_74 ; fn_80227538(&lbl_50_bss_188)` |
| `0x74 fn_50_74` | 0x98 | `__nw__FUlPCcPCc(0x11C, lbl_50_rodata_8C, 0)`, then `fn_50_1938` on the result |

**This module is not the accessors-and-predicate family the other heads are**, and that is what
shaped the claim. Its head is four *entry* functions, and the fifth, `fn_50_10C` (0x10C, 0x2A8),
is already the module's ragdoll constraint solver - it indexes two parallel 0x44-byte node arrays
off a `mulli`, calls `CVector3f::AsNormalized` and `CQuaternion::YRotation` and writes quaternion
and vector temporaries to its own frame. That needs the CActor hierarchy, so the claim stops at
the end of the head. Everything from 0x10C up is left unclaimed and filled from retail, which is
what keeps the module's sha1. Not added to `files.cmake`, for the reason the other heads measure:
the file calls `fn_50_1938` and `fn_80227538`, which the port cannot link.

## Measured

- `./tools/decomp_build.sh`: `All: 31.19% fuzzy, 23.48% matched, 11.80% linked (10171 / 28465)`.
  `main.dol` still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- **All 86 REL hashes hold** (the `config/G2ME01/config.yml` re-hash from
  `docs/RUNNING_THE_DECOMP.md`), and `build/G2ME01/PirateRagDoll/PirateRagDoll.rel` is
  `cmp`-equal to `orig/G2ME01/files/RelProd/PirateRagDoll.rel`.
- `build/report.json`: `PirateRagDoll/MetroidPrime/ScriptObjects/CPirateRagDollRel` **4/4
  functions matched, every function 100.0%, `complete: True`**. Module sum **7 -> 11 of 48**.
  Project **matched 10167 -> 10171, linked 4955 -> 4959**.
- `tools/unit_fit.sh MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp`: `.text claimed 268 ours
  268 retail 268 fits`, `no extra functions`. (The unit name has to be the configure.py path, not
  the `PirateRagDoll/...` objdiff name - the latter prints "not declared in any splits.txt".)
- `tools/audit_rel_claim.py PirateRagDoll`: `0x00000000..0x0000010C 4/4 functions`,
  `0 claim(s) with a problem`, and **`preplf 48 text symbols, plf 48, 0 dropped by
  -strip_partial`** - i.e. no dead-strip, which the module's own `ldscript.lcf` predicts (`fn_50_74`,
  `fn_50_1938` and `lbl_50_bss_188` are all in its FORCEACTIVE block; RELMain/RELExit are the entry
  points reached from `_prolog`/`_epilog`; `fn_50_44` is a direct `bl` from `RELMain`).
- `tools/check_decl_order.py --unit CPirateRagDollRel`: ok.
- `tools/check_symbol_names.py`: `checked 505 units; 0 declared names are missing`.
- `tools/check_raw_offsets.py`: ok, 152 sites in 61 files (this file adds no raw-offset site, so
  it needs no `docs/research/raw_offsets.md` section - the offsets it reaches are `0x11C` and the
  frame offsets, which that checker does not key on).
- `tools/goal_check.sh build/goal/item.json`: **PASS** (gate clean, target rose 7 -> 11, no
  function worse, no asm added).

## Three things worth knowing before the next lane spends time here

1. **The declaration-order rule bit me first time, and the diagnostic is not the obvious one.**
   I wrote the four definitions in *ascending* retail order (RELExit first, because 0x0 is the
   lowest offset). The object was then emitted in descending order, `mwldeppc` kept it verbatim,
   and the module's `.text` came out permuted - 262 differing bytes, all of them a 4-byte shift,
   while every function still paired by name. `check_decl_order.py` on the *object* is what
   catches it (`ok` now), and the symptom to recognise is a linked `.rel` whose bytes are right
   but *shifted*, not wrong. Note that the object mwldeppc actually links is
   `build/G2ME01/src/<unit>.o`, which is also the one `check_decl_order.py` reads; the copy under
   `build/G2ME01/<Module>/obj/` is a stale artifact of an earlier run and can disagree with it -
   I was misled by it for a few minutes.
2. **`tools/flip_test.sh` cannot verify a REL module unit in this tree, and that is pre-existing**
   (recorded for `FlyingPirate` too, so the second run does not repeat the diagnosis). It
   paren-counts from the last `MusyX(` in `configure.py` to find the source root, which is wrong
   for every `Rel(` block after it, so it FAILs with `no source file
   (extern/musyx/src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp)`. Fixing it is a `tools/`
   change, which this item may not make. The REL acceptance test is the module's sha1, which is
   green above.
3. **Two spellings mwcceppc 1.3.2 insists on, both measured here rather than looked up.**
   `extern "C" const char lbl_50_rodata_8C[];` is read as a *definition* and asks for an
   initializer - it needs the explicit `extern` keyword. And `nullptr` only exists because
   `REL/REL_Setup.h` includes `types.h`; a file that does not include it gets `undefined
   identifier 'nullptr'`.

## What the next item on this module would be

`.text 0xC2C..0xE10` is five more functions that read as small, self-contained pieces rather than
class code: `fn_50_C2C` (0x30, copies the x word of a 16-byte value and negates the other three -
a `CVector3f` negate plus a `w`), then `fn_50_C5C` (0xF0, a node lookup that indexes two tables off
an `lbz` - `slwi ×1` then `slwi ×6`, so a 2-byte stride and then 0x40 - and calls the unclaimed
`fn_50_D4C`), `fn_50_D4C` (0x34, compares two bytes against `'c'` and returns bool), `fn_50_D80`
(0x40, a three-float cross product, entirely inline) and `fn_50_DC0` (0x50). That is one contiguous
range, so one more source file and one more `Object(Matching, ...)`; `fn_50_74` and `fn_50_4F8` are
in FORCEACTIVE and the rest are reachable, so no dead-strip work. `fn_50_D80` alone looks like the
cheapest thing on the module. Not filed as `NEW:` - it is the same item's claim, just further
along, and it needs the node struct's layout to be worth a lane.
# Run 2 (lane 8, 2026-09-30) - re-landed, plus one function further

Outcome: **PASS**. `tools/goal_check.sh build/goal/item.json` prints
`PASS progress-rel-head-pirateragdoll`, target rose **7 -> 12** (the run above got 7 -> 11).

## Read the first section before acting: **the previous run's work was not in the tree**

On arrival, `src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp` did not exist, `configure.py`
had no `Rel("PirateRagDoll", ...)` block, and `splits.txt` claimed only the two shared `REL/*`
units. `git log --all -- '*PirateRagDoll*'` showed nothing but the upstream import commits. The
run-1 notes describe work that measured clean and passed the judge but **was lost to a reset**
before it was committed. So this run re-measured from zero and re-landed it, then went one
function further than run 1 did.

**What survived the reset, and it is worth knowing**: `build/` is not tracked, so run 1's *compiled
object* was still in this worktree - `build/G2ME01/PirateRagDoll/asm/MetroidPrime/ScriptObjects/CPirateRagDollRel.s`,
`build/G2ME01/src/.../CPirateRagDollRel.o`, and the module's own `ldscript.lcf`. That is a free
ground truth: `powerpc-eabi-nm -u` on the leftover object reads
`fn_50_1938, fn_80227538, lbl_50_bss_188, lbl_50_rodata_8C, __nw__FUlPCcPCc` and nothing else, which
is the exact set of externs run 1 used, and the dtk listing is retail's bytes for the range. **If a
lane's notes claim a change passed and the tree does not have it, look in `build/` before writing
anything from scratch.** (Do not trust `build/G2ME01/<Module>/obj/` - it is a stale copy, noted in
run 1's point 1 and still true.)

## What landed

Five functions in two `Matching` units, both new this run.

| unit | range | functions |
| --- | --- | --- |
| `src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp` | `.text 0x0..0x10C` | 4: RELExit, RELMain, `fn_50_44`, `fn_50_74` |
| `src/MetroidPrime/ScriptObjects/CPirateRagDollCross.cpp` | `.text 0xD80..0xDC0` | 1: `fn_50_D80` |

Four files of the carve: `configure.py` (the module's first `Rel(...)` block ever, two
`Object(Matching, ...)` lines, one per line), `config/G2ME01/rels/PirateRagDoll/splits.txt` (two
new claims), `files.cmake` (+1), and the two sources' own header comments.

## Measured

- `./tools/decomp_build.sh`: `All: 31.21% fuzzy, 23.50% matched, 11.82% linked (10223 / 28465)`.
  `main.dol` still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, and **87 files OK**.
- All 86 REL sha1s against `config/G2ME01/config.yml` hold;
  `build/G2ME01/PirateRagDoll/PirateRagDoll.rel` is `cmp`-equal to `orig/.../PirateRagDoll.rel`.
- `build/report.json`: both new units **complete: True**, every function **100.0%**,
  4/4 and 1/1. Module sum **7 -> 12 of 48**. Project matched **10218 -> 10223**,
  linked **5004 -> 5009**.
- `tools/unit_fit.sh` on both: `.text claimed 268 ours 268 retail 268 fits` and
  `claimed 64 ours 64 retail 64 fits`, `no extra functions` each.
- `tools/audit_rel_claim.py PirateRagDoll`: both claims `ok`, `0 claim(s) with a problem`,
  `preplf 48 text symbols, plf 48, 0 dropped by -strip_partial`.
- `tools/check_decl_order.py --unit CPirateRagDoll`: `ok: 2 unit(s) checked`.
- `tools/check_symbol_names.py`: `checked 505 units; 0 declared names are missing`.
- `tools/probe_sources.sh`: `750 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)`. The undefined count is **unchanged at 250** against a baseline of 250 - the
  `files.cmake` line costs the port nothing, measured.
- `python3 tools/check_docs_claims.py`: `docs claims agree with the tree`.
- `tools/goal_check.sh build/goal/item.json`: **PASS** (gate clean, target 7 -> 12, no function
  worse, no asm added).

## Two things that cost time here, both worth the next lane's minute

1. **The declaration-order rule bites the *second* definition too, not just the first.** Run 1
   documented ascending order; the sharper form is that the order must be **strictly descending by
   retail offset, all four of them**. Writing RELExit / RELMain / `fn_50_44` / `fn_50_74` - which
   *looks* descending at a glance because 0x0 is lowest - emits
   `RELExit, RELMain, fn_50_74, fn_50_44`, because `fn_50_44` calls `fn_50_74` and the compiler
   groups a callee after its caller. The `.rel` then comes out with 192 differing bytes and the
   same 18488 length, the failure looking like a bad `bl` displacement at `.text:0x30` rather than
   a permutation. The diagnostic is still `check_decl_order.py`, but note it prints
   `0 unit(s) checked` before the object is rebuilt - run it **after** the build, not before.
2. **`tools/check_files_cmake.py` is a real gate for a second `Object(...)` in a REL block, and
   the fix is the opposite of what the heads do.** Every REL head in this family is deliberately
   **absent** from `files.cmake` (it calls `fn_50_1938` and `fn_80227538`, which the port cannot
   link). The checker exempts those because they define `RELMain`/`RELExit`
   (`MODULE_ENTRY` in `tools/check_files_cmake.py:562`). `CPirateRagDollCross.cpp` defines neither,
   so the gate **FAILs `files-cmake`** until it is listed - and the first `goal_check` run failed
   on exactly that while every other check was green. It is safe to list because
   `powerpc-eabi-nm -u` on its object prints nothing: one self-contained float function, zero
   externals, so the port's undefined count is unchanged. General rule: **a REL unit that defines
   no entry point and calls nothing has to go in `files.cmake`; one that calls the DOL cannot.**

## The one function that went further: `fn_50_D80`, and the spelling that reached it

`.text 0xD80..0xDC0`, 0x40 bytes, a three-float cross product. It is `a.Cross(b)` - argument order
fixed by which operand r4 holds - and `-fp_contract on` (which the REL cflags carry) folds each
`x*y - z*w` into one `fmsubs`. Two things had to be right, and both are invisible in the
mathematics:

- **The three loads must be hoisted into locals first.** Written as `av[1]*bv[2] - av[2]*bv[1]` etc.
  straight off the pointers, the compiler re-loads each operand per component and emits **0x58
  bytes** with three separate `fmuls`+`fmsubs` pairs. `const float ax = av[0], ay = av[1], ...`
  gives exactly 0x40 with retail's register assignment (f3/f4/f5 = a, f6/f7/f2 = b).
- **The operand order inside two of the three `fmuls` is load-bearing.** Retail emits
  `fmuls f0,f7,f5`, `fmuls f1,f2,f3`, `fmuls f0,f6,f4` - the first and third products are written
  **b-component first**, only the second a-component first. All three textbook
  (`ay*bz - az*by`, `az*bx - ax*bz`, `ax*by - ay*bx`) give the same 0x40 bytes, the same arithmetic
  and objdiff at 100%, and **still break the module's sha1 on four bytes**. Multiplication is
  commutative, so only the encoder's operand order distinguishes them, and no objdiff percentage
  or `unit_fit` line says so. The landed source is
  `ay*bz - by*az`, `az*bx - bz*ax`, `ax*by - bx*ay`.

WALL: fn_50_D4C (retail .text 0xD4C, 0x34) - the adjacent neighbour is 13/14 instructions correct in
every spelling tried and no C++ source reached retail's fused `lbzu r0,0x8(r3)` + `lbz r0,0x1(r3)`
pair; the remaining diff is one load-update instruction, not register allocation.

## `fn_50_D4C`: six spellings, all measured, none reached it

Retail is `lbz r0,0(r4) / li r4,0 / slwi r0,r0,1 / add r3,r3,r0 / lbzu r0,0x8(r3)` then
`cmplwi r0,0x63` and `lbz r0,0x1(r3)`. **The index arrives as a pointer** (`lbz 0(r4)`), the
stride is 2 bytes off the table base, and the two bytes tested are at +8 and +9. All six below
compile and link; the sizes and shapes are what was measured:

| spelling | emitted | result |
| --- | --- | --- |
| `bool fn(void* table, unsigned char index)`, `entry[8]/entry[9]` | `rlwinm r4,r4,1,23,30` + `addi r5,r4,8` | wrong - promotes the index to int and uses a 2nd register |
| `const unsigned char* index` (by pointer), `entry[8]/entry[9]` | **0x34 bytes**, `lbz r0,8(r3)` / `lbz r0,9(r3)` | right size, right registers, **2 instructions differ** |
| same, `+ 8` folded into the pointer | 0x38 bytes, extra `addi r5,r5,8` | wrong size |
| same, post-increment `const unsigned char c0 = *entry++` | `lbz r0,0(r3)` / `lbz r0,1(r3)` | 0x34, one byte short of retail |
| same, post-increment **and** `+ 8` | 0x38, `slwi r5`/`addi r5,r5,8`/`add r5,r3,r5` | wrong size and register |
| struct with `char name[2]` at +8, `entry->name[0]/[1]` | **0x34 bytes**, `lbz r0,8(r3)` / `lbz r0,9(r3)` | same as row 2 |

The best two (rows 2 and 6) are **byte-for-byte the same object** - mwcceppc normalises both to a
base+displacement pair - and the whole remaining difference from retail is that retail folds the
+8 into the load as `lbzu` (which also bumps r3) and then reads offset 1, where mwcceppc emits
`lbz` at offsets 8 and 9. Note the compiler *will* emit `lbzu` for a `char*` walk
(`CGeomBlobV2.cpp` has plenty), so this is not a missing feature: what is missing is a source
shape that makes the **same** register both the load base and the post-increment base. The
interleaved form that would produce it is what `lbzu` *is*, so this may not be reachable in C++ at
all. **Not worth another lane on its own** - one function, and the six spellings above are the
whole search space a reader needs to skip.

## What the next item on this module would be

`.text 0xC2C..0xD4C` - `fn_50_C2C` (0x30, loads the four words of a 16-byte value, keeps x, negates
y/z/w: a `CVector3f` negate plus a `w`) and `fn_50_C5C` (0xF0, the node lookup: `lbz` the index,
`slwi x1` then `slwi x6` off two tables, a `CVector3f` difference written to the frame, and a call
to `fn_50_D4C`). `fn_50_C2C` is self-contained and is the cheapest thing left; `fn_50_C5C` needs
the 0x44-byte node struct's layout. Both are reachable from `fn_50_4F8`, which is force-active, so
there is no dead-strip work. Not filed as `NEW:` - it is this same item's claim, and `fn_50_D4C`
above it stays a wall, so the contiguous-range rule would put all three in one unit that cannot
be completed.

**One process note for the driver, not a blocker:** this item is `fails: 0` and was requeued with
its notes claiming a PASS, because the change it described was not in the tree. A `progress` item
whose notes say `PASS` but whose target is unmoved is worth a re-read of `build/` before spending
a lane on it.

# Run 3 (lane 8, 2026-09-30) - re-landed a third time, plus `fn_50_C2C` tried and walled

Outcome: **PASS**. `./tools/goal_check.sh build/goal/item.json` prints
`PASS progress-rel-head-pirateragdoll`, target rose **7 -> 12** (run 2 got the same figure).

## Run 2's work was in the tree; run 1's never was. Read this before acting.

On arrival `git status` was **clean** and neither `CPirateRagDollRel.cpp` nor
`CPirateRagDollCross.cpp` existed, `configure.py` had no `Rel("PirateRagDoll", ...)` block, and
`splits.txt` claimed only the two shared `REL/*` units. So **the same reset that lost run 1's
work took run 2's**, one loop later. Two runs of this item have now been thrown away after
passing. Run 2's note that `build/` survives a reset is what saved this run: the leftover
objects and dtk listings under `build/G2ME01/PirateRagDoll/` and
`build/G2ME01/src/MetroidPrime/ScriptObjects/CPirateRagDoll{Rel,Cross,Helpers}.o` gave retail's
exact bytes for the head and for `fn_50_D80` without re-deriving a single spelling. **This item
is worth one more lane only because of that file, not because of its notes.**

One leftover was *not* from run 2 and is the interesting part: `CPirateRagDollHelpers.s` claims
`.text 0xD4C..0xDC0`, i.e. `fn_50_D4C` **plus** `fn_50_D80` in one unit. Run 2 split those
because `fn_50_D4C` walls; the leftover shows the combined attempt was built and abandoned. Its
`.text` is 0x74 bytes and the D4C half is retail's own listing, so it is an abandoned attempt,
not a third landing.

## What landed

The same five functions as run 2, re-derived from the leftover objects and the notes:

| unit | range | functions |
| --- | --- | --- |
| `src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp` | `.text 0x0..0x10C` | 4: RELExit, RELMain, `fn_50_44`, `fn_50_74` |
| `src/MetroidPrime/ScriptObjects/CPirateRagDollCross.cpp` | `.text 0xD80..0xDC0` | 1: `fn_50_D80` |

Four files of the carve: `configure.py` (the module's first `Rel(...)` block ever, two
`Object(Matching, ...)` lines, one per line), `config/G2ME01/rels/PirateRagDoll/splits.txt` (two
new claims), `files.cmake` (+1), and the two sources' own header comments.

## Measured

- `./tools/decomp_build.sh`: `All: 31.23% fuzzy, 23.52% matched, 11.82% linked (10248 / 28465)`.
  `main.dol` still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, and **87 files OK**.
- All 86 REL sha1s against `config/G2ME01/config.yml` hold;
  `build/G2ME01/PirateRagDoll/PirateRagDoll.rel` is `cmp`-equal to `orig/.../PirateRagDoll.rel`
  and `sha1 c43a3088261b9095ba8ce51eb1daea00e48d1e81` on both sides.
- `build/report.json`: both new units **complete: True**, every function **100.0%**, 4/4 and
  1/1. Module sum **7 -> 12 of 48**. Project matched **10243 -> 10248**, linked **5025 -> 5030**.
- `tools/unit_fit.sh` on both: `.text claimed 268 ours 268 retail 268 fits` and
  `claimed 64 ours 64 retail 64 fits`, `no extra functions` each.
- `tools/audit_rel_claim.py PirateRagDoll`: both claims `ok`, `0 claim(s) with a problem`,
  `preplf 48 text symbols, plf 48, 0 dropped by -strip_partial`.
- `tools/check_decl_order.py --unit CPirateRagDoll`: `ok: 2 unit(s) checked`.
- `tools/check_symbol_names.py`: `checked 505 units; 0 declared names are missing`.
- `tools/probe_sources.sh`: `751 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)` - **unchanged at 250** against a baseline of 250.
- `tools/check_raw_offsets.py`: `ok, 156 raw-offset site(s) in 65 file(s)` (this run adds no
  raw-offset site, so it needs no `docs/research/raw_offsets.md` section).
- `tools/goal_check.sh build/goal/item.json`: **PASS** (gate clean, target 7 -> 12, no function
  worse, no asm added).

Run 2's two findings both reproduced exactly and both cost time to rediscover: the
declaration-order rule needs **strictly** descending retail offsets (ascending emits
`RELExit, RELMain, fn_50_74, fn_50_44` because the compiler groups a callee after its caller),
and `tools/check_files_cmake.py` requires a REL unit that defines neither `RELMain` nor
`RELExit` to be in `files.cmake`. Its `MODULE_ENTRY` exemption is why the head is not.

## WALL: fn_50_C2C (retail .text 0xC2C, 0x30) - the allocator, not the source, is what differs

It is the cheapest function left on the module: self-contained, four floats, no callee, and its
own contiguous claim. **Every spelling reaches exactly 0x30 bytes and 12 instructions**, and
every one is a reordering from retail. Retail:

    lfs f1,0x4(r4) / lfs f0,0x0(r4) / lfs f2,0x8(r4) / fneg f1,f1 / stfs f0,0x0(r3) /
    lfs f0,0xc(r4) / fneg f2,f2 / stfs f1,0x4(r3) / fneg f0,f0 / stfs f2,0x8(r3) /
    stfs f0,0xc(r3) / blr

Three float registers, and **f0 is reused for `w` after `out.x` has been stored**. Every
spelling measured loads all four first and uses four or five registers.

**mwcceppc's scheduler decides this, not statement order.** Measured this run: all 24
permutations of the four `out[i] = ...` statements; all **630** orderings of the seven statements
`ny=-in[1]; out[0]=in[0]; nz=-in[2]; out[1]=ny; nw=-in[3]; out[2]=nz; out[3]=nw;` with the three
temporaries declared up front *and* declared where used; a further 51 valid interleavings; and
the flat-`float*`, struct-of-4-floats, nested-`CVector3f`, union, `in-place`, `0.f - x`,
`x * -1.f`, `static inline` helper and `CQuaternion`-constructor shapes. All of it collapses to
**two** emitted bodies and neither is retail's. **Best positional agreement over the whole sweep:
1 of 12.**

Two spellings got closest and are the whole search space a later reader needs:
- `float y = -in[1]; float x = in[0]; float nz = -in[2]; out[0] = x; float nw = -in[3]; ...`
  emits `lfs f1,4(r4) / lfs f0,0(r4) / fneg f1,f1 / stfs f0,0(r3) / ...` - **the right registers**
  (y in f1, x in f0) but the load of `0x8` is issued after the store instead of before it.
- three hoisted `const float` locals: four registers, all four loads batched at the top.

**Not worth a lane.** A claim here would also span a gap (0xC5C..0xD4C is `fn_50_C5C` and
`fn_50_D4C`, neither claimed) and the contiguous-range rule would pull in `fn_50_D4C`, which is
already a wall. The finding is a codegen property, so per the brief it goes here and not in the
queue.

## One process correction for the driver, not a blocker

Run 2's first section says to trust `build/` over the tree. That advice is right but it cuts
both ways and cost this run about ten minutes: **`build/G2ME01/<Module>/obj/` and the `asm/`
directory under it are stale copies that disagree with `build/G2ME01/src/`.** Run 2 already
warned about `obj/`; this run's abandoned `CPirateRagDollHelpers.s` is a *third* claim shape for
the same 0xD4C..0xDC0 range that never landed, and reading it as the current state would put a
dead claim back into `splits.txt`. Read `build/G2ME01/src/<unit>.o` and the *current* build's
`asm/` output; treat anything older than the last `decomp_build.sh` as history.

## What the next item on this module would be

Unchanged from run 2, and still `fn_50_C5C` (0xC5C, 0xF0) - the node lookup. It needs the
0x44-byte node struct's layout, and `fn_50_D4C` (0xD4C, 0x34) sits between it and `fn_50_D80`
and stays a wall from run 2, so the contiguous-range rule puts all three in one unit that
cannot be completed. `fn_50_DC0` (0xDC0, 0x50) is the next one above and it *is* in FORCEACTIVE:
it is an `rstl::basic_string` destructor (calls `internal_dereference__Q24rstl...` then
`Free__7CMemoryFPCv`, with an `extsh r31` size test), which needs `rstl/basic_string.hpp` and a
`CMemory` declaration. Not filed as `NEW:` - it is this same item's claim, further along.

# Run 4 (lane 8, 2026-09-30) - re-landed a fourth time, and `fn_50_DC0` characterised

Outcome: **PASS**. `./tools/goal_check.sh build/goal/item.json` prints
`PASS progress-rel-head-pirateragdoll`, target rose **7 -> 12 of 48** (runs 2 and 3 got the same).

## Runs 1-3's work was in the tree's `build/` *and* recoverable from the transcripts

On arrival `git status` was **clean** again and neither `CPirateRagDollRel.cpp` nor
`CPirateRagDollCross.cpp` existed, `configure.py` had no `Rel("PirateRagDoll", ...)` block, and
`splits.txt` claimed only the two shared `REL/*` units. That is the **fourth** time (runs 1, 2, 3
and now this one) for this item, and the driver log names the cause rather than a reset:
`build/goal/run.log:936,967,998` all read `progress-rel-head-pirateragdoll does not apply on
<sha> - releasing it for a fresh attempt` after `goal/decomp` moved under it. **The judged change
is a patch that fails to rebase, so it is thrown away, not the work being wrong.**

**This run did not re-derive any spelling**, which is the practical lesson. Two sources of
ground truth, both cheaper than decompiling:

1. `build/G2ME01/PirateRagDoll/asm/` and `build/G2ME01/src/**/CPirateRagDoll*.o` survive the
   reset, so retail's exact bytes for the head and for `fn_50_D80` are on disk.
2. **The agent transcripts are the whole source, verbatim.**
   `../wt-mp2-goal/build/goal/agent/progress-rel-head-pirateragdoll-L8-<n>-*.jsonl` records every
   `write`/`edit` tool call with its full `state.input`. Reading the `write` calls for
   `CPirateRagDollRel.cpp` and `CPirateRagDollCross.cpp` out of run 3's transcript and replaying
   the four `edit`s recovered all four files of the carve byte-for-byte, and it built and hashed
   clean on the first try. **If a lane's notes claim a change passed and the tree does not have
   it, look in `build/goal/agent/*.jsonl` before writing anything from scratch** - it is the
   highest-value habit this item has produced, and it generalises to every item the loop has ever
   lost. (Run 3's warning still stands for `build/G2ME01/<Module>/obj/` and `asm/`: treat anything
   older than the last `decomp_build.sh` as history.)

## What landed

The same five functions as runs 2 and 3, in two `Matching` units:

| unit | range | functions |
| --- | --- | --- |
| `src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp` | `.text 0x0..0x10C` | 4: RELExit, RELMain, `fn_50_44`, `fn_50_74` |
| `src/MetroidPrime/ScriptObjects/CPirateRagDollCross.cpp` | `.text 0xD80..0xDC0` | 1: `fn_50_D80` |

Four files of the carve: `configure.py` (the module's first `Rel(...)` block, two
`Object(Matching, ...)` lines, one per line), `config/G2ME01/rels/PirateRagDoll/splits.txt` (two
claims), `files.cmake` (+1), and the two sources' own header comments.

## Measured

- `./tools/decomp_build.sh`: `All: 31.27% fuzzy, 23.62% matched, 11.83% linked (10293 / 28465)`.
  `main.dol` still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, and **87 files OK**.
- All 86 REL sha1s against `config/G2ME01/config.yml` hold (the `docs/RUNNING_THE_DECOMP.md`
  re-hash, run directly): `86 REL sha1s checked, 0 mismatched`.
  `build/G2ME01/PirateRagDoll/PirateRagDoll.rel` is `cmp`-equal to `orig/.../PirateRagDoll.rel`.
- `build/report.json`: both new units 100.0% on every function, 4/4 and 1/1. Module sum
  **7 -> 12 of 48**. Project matched **10288 -> 10293**, linked **5043 -> 5048**.
- `tools/unit_fit.sh` on both: `.text claimed 268 ours 268 retail 268 fits` and
  `claimed 64 ours 64 retail 64 fits`, `no extra functions` each.
- `tools/audit_rel_claim.py PirateRagDoll`: both claims `ok`, `0 claim(s) with a problem`,
  `preplf 48 text symbols, plf 48, 0 dropped by -strip_partial`.
- `tools/check_decl_order.py --unit CPirateRagDoll`: `ok: 2 unit(s) checked`.
- `tools/check_symbol_names.py`: `checked 505 units; 0 declared names are missing`.
- `tools/probe_sources.sh`: `751 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)` - **unchanged at 250** against a baseline of 250.
- `tools/check_raw_offsets.py`: `ok, 157 raw-offset site(s) in 66 file(s)`.
- `tools/goal_check.sh build/goal/item.json`: **PASS** (gate clean, target 7 -> 12, no function
  worse, no asm added).

Run 2's two findings both reproduced exactly: the declaration order has to be **strictly**
descending by retail offset (ascending emits `RELExit, RELMain, fn_50_74, fn_50_44` because the
compiler groups a callee after its caller), and `tools/check_files_cmake.py` requires a REL unit
that defines neither `RELMain` nor `RELExit` to be in `files.cmake`.

## `fn_50_DC0` (0xDC0, 0x50): the bytes are found, and the wall is the *name*, not the source

This is the one function above the Cross claim, and it is **contiguous** with it (0xD80..0xDC0,
then 0xDC0..0xE10), so it was the natural next step. It is not in any previous run's notes. It is
`rstl::string`'s deleting destructor: `bl internal_dereference__Q24rstl66basic_string<...>` then
`extsh. r0,r31 / ble / mr r3,r30 / bl Free__7CMemoryFPCv`, and `fn_50_2EE4` (0x2EE4) passes it to
`__register_global_object` for each of the fifteen 0xC-byte `lbl_50_bss_*` string globals.

**The body is reached exactly, and this is the useful part.** Retail is 0x50 bytes, 20
instructions. A file-scope `rstl::string` compiled with the REL cflags emits
`__dt__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv` as a
**weak** symbol and it is **byte-for-byte retail's 20 instructions** - same registers, same
`mr. r30,r3` / `beq` / `extsh. r0,r31` / `ble` deleting-destructor frame, same two externs
(`nm -u` on the retail unit object prints exactly `Free__7CMemoryFPCv` and
`internal_dereference__Q24rstl66basic_string<...>Fv`, and so does ours). Retyping it as a
*wrapper* class is what breaks it, and the reason is worth keeping: a class with a `rstl::string`
**member** gets a per-member null check (`addic. r0,r30,off; beq`) and comes out **0x58** bytes,
while a class that merely *derives* from it emits `li r4,0` and a `bl` to a separate base
destructor. So the shape is only reachable by instantiating the template's own destructor.

**Two blockers, both measured, and neither is a spelling problem.**

1. **The name.** dtk's `config/G2ME01/rels/PirateRagDoll/symbols.txt:12` calls it
   `fn_50_DC0 = .text:0x00000DC0; // type:function size:0x50` - an *unnamed* retail symbol - and the
   retail unit object defines it `T` (global). Our object can only ever emit the mangled
   `__dt__...` name, so dtk pairs nothing and the claim fails two ways at once. Renaming the
   symbols.txt entry to the real mangled name **does work in isolation** (measured: the rename
   alone, with no new source, is `87 files OK`), and with a source claiming 0xDC0..0xE10 the link
   resolves. **But the rename is not the last step:** claiming the range removes `fn_50_DC0` from
   the module, and `fn_50_2EE4` still references it, so the link fails with
   `Failed to find symbol fn_50_DC0 in any module` unless the rename is made too. Both together
   then link, so this half is *solved* - a `symbols.txt` rename is a legitimate, already-used
   mechanism here (`wire_rel_setup.py` does exactly this for `_epilog`/`_prolog`, and
   `ScriptCannonBall`, `Tweaks` and `ForgottenObject` all carry this very `__dt__` name correctly).
2. **The forcing construct costs bytes the module has no room for.** A file-scope
   `rstl::string` is the only construct measured that forces the destructor out of line, and it
   also emits `__sinit_<file>` (0x4C) plus a 0x10-byte `.bss` object and a 4-byte `.ctors` entry.
   `__sinit` is **dead-stripped** - it does not appear in the linked `.rel` at all, so
   `unit_fit.sh` reports it as a harmless extra - but the `.ctors` and `.bss` are not, and there
   is nowhere to claim them: the module's `.bss` is 0x0..0x18C and every byte of it is retail
   (`lbl_50_bss_0` .. `lbl_50_bss_188`). Result measured: the module's `.data`/`.bss` offsets
   move by 0x20 and `PirateRagDoll.rel` comes out **152 bytes long** (18640 against 18488), and
   `DarkCommando`, `CommandoPirate`, `DarkTrooper` and `SpacePirate` fail their hashes too, on
   **one byte each**.

Constructs measured and rejected for the forcing role (all compile, none emit the standalone
`~basic_string` with no extras):

| construct | emits | usable |
| --- | --- | --- |
| file-scope `rstl::string` | `__dt__` 0x50 **+ `__sinit` 0x4C + 0x10 `.bss` + 4 `.ctors`** | no - see blocker 2 |
| member `rstl::string` in a wrapper class | wrapper's own dtor, **0x58** | no - per-member null check |
| derive from `rstl::string` | `li r4,0` + `bl` to a second dtor | no |
| `void (*p)(S*,int) = &S::~basic_string;` (file scope, `volatile` or not) | **compile error** | no |
| `&S::~basic_string` in a template non-type argument, in a cast, in a `(void)&` expression, as a default argument | **compile error** | no |
| unreferenced `static` function calling `new C()` | the dtor **plus** a 0x50-byte unused maker | no |
| class with a virtual destructor and nothing else | its own dtor + a 0x0C vtable in `.data` | no |
| class deriving from `rstl::string` with a virtual destructor | **0x60** - the dtor was inlined into the derived dtor | no |
| explicit instantiation `template struct Reg<...>` of anything | the *wrapper's* code, not the dtor | no |
| `#pragma instantiate rstl::basic_string<...>` | **nothing at all** | no |
| out-of-class `~basic_string` definition | **nothing** - not emitted unless used | no |

**mwcceppc 1.3.2 cannot take a destructor's address in any form**, so there is no way to force the
out-of-line copy that does not also create a static object. That is the wall, and it is a property
of the compiler and the module's `.bss` layout, not a missing spelling.

WALL: fn_50_DC0 (retail .text 0xDC0, 0x50) - the body is reached byte-for-byte (it *is*
`~basic_string` emitted out of line, 0x50, same registers, same two externs), but the only
construct that forces that emission also creates a `.bss`/`.ctors` the module has no room for,
and mwcceppc 1.3.2 cannot take a destructor's address to force it without one.

**If a later lane wants it, the two halves are separable and only one is new:** the
`symbols.txt` rename to `__dt__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`
is already measured safe on its own (`87 files OK`) and is required - `fn_50_2EE4` references the
symbol by name, so a claim without the rename cannot link. What is still unsolved is making the
object emit that destructor without static storage.

## What the next item on this module would be

Unchanged from runs 2 and 3: `.text 0xC5C..0xD4C` - `fn_50_C5C` (0xF0, the node lookup) and
`fn_50_D4C` (0x34, still a wall from run 2), with `fn_50_C2C` (0xC2C) a wall from run 3 between
them and the Cross claim. The contiguous-range rule puts all three in one unit that cannot be
completed. `fn_50_E10` (0xE10, 0x800) is the next claimable range above: it is in FORCEACTIVE, is
0x800 bytes of the module's own setup (`psq_l`/`stfd` f31-f29 pairs in the prologue, so it is
`CRagDoll`'s own method) and calls the unclaimed `AddJointConstraint__8CRagDollFiiiiii` family,
which the DOL does provide. That is more work than this item's slice, so it is not filed as
`NEW:` - it is this same item's claim, further along.
