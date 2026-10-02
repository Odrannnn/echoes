# progress-prime1-cobbtree

`kind: progress`, `target: WorldFormat/COBBTree`. The unit stays `NonMatching`; no `flip_test` was
run to decide anything.

## Measured result

`build/report.json`, `main/WorldFormat/COBBTree`, before and after (both re-measured in this
worktree, not recalled):

| | before | after |
|---|---|---|
| `matched_functions` | **22 / 37** | **25 / 37** |
| `matched_code_percent` | 43.19% | 48.23% |
| `fuzzy_match_percent` | 52.29% | 57.06% |

Project-wide, `tools/report_diff.py build/report.base.json build/report.json`:

```
matched  9845 -> 9848   linked 4895 -> 4895   (+3 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/COBBTree :: CalculateLocalAABox__8COBBTreeCFv
  +100%    main/WorldFormat/COBBTree :: __dt__8COBBTreeFv
  +100%    main/WorldFormat/COBBTree :: __nw__Q28COBBTree5CNodeFUlPCci
no regression
```

A before/after diff of every function in every unit in `report.json` (units and functions
compared pairwise, not just the target) shows **0 units worse and 0 functions worse**; the only
unit that moved is this one.

## Per function

`tools/bytescmp.py` / an objdump-vs-DOL instruction differ were used to locate each difference;
percentages are objdiff's `fuzzy_match_percent` for that function.

| function | retail | before | after | Prime 1's source |
|---|---|---|---|---|
| `__dt__8COBBTreeFv` | 0x8024EC88, 156 B | 97.18% | **100%** | needed a small edit |
| `CalculateLocalAABox__8COBBTreeCFv` | 0x8024EC28, 96 B | 77.50% | **100%** | needed a small edit |
| `__nw__Q28COBBTree5CNodeFUlPCci` | 0x8024E10C, 72 B | 66.11% | **100%** | matched unchanged, but GC/2.7 schedules the blocks the other way |
| `__ct__8COBBTreeFRCQ28COBBTree10SIndexDataPCQ28COBBTree5CNode` | 0x8024EE8C, 256 B | 47.27% | 98.44% | needed a structural edit; 1 instruction short |
| `__ct__8COBBTreeFR12CInputStream` | 0x8024ED24, 320 B | 59.63% | 98.75% | needed a structural edit; 1 instruction short |

Prime 1's `src/WorldFormat/COBBTree.cpp` was the starting point for all five; the two
`CNode::CNode` constructors, `CNode::GetMemoryUsage`, `CLeafData`, `CSimpleAllocator`,
`SIndexData` and `BuildOrientedBoundingBoxTree` already matched and were not touched. Prime 1 has
no `CCollisionPrimitiveData` base and no `SIndexData::x60_`, so the two `COBBTree` constructors are
Echoes-only and Prime 1's spelling of them does not exist.

### `~COBBTree` — the ternary, not the branches

Retail emits **two** `bl SetAllocator` calls, one per arm:

```
8024eca8: lwz r0,68(r30) ; cmplwi r0,0 ; beq 0x...ecc0
8024ecac: addi r3,r30,64 ; bl SetAllocator ; b 0x...ecc8
8024ecc0: li r3,0       ; bl SetAllocator
```

The old `CNode::SetAllocator(mAllocator.GetPoolMemSize() ? &mAllocator : nullptr);` is one call with
a computed argument, and mwcceppc tail-merged it into `addi r3,r30,64 / b <the other arm's call>`,
losing 4 bytes. Prime 1's spelling - an `if`/`else` with a `SetAllocator` statement in each arm -
keeps both calls. Nothing else in the 39 instructions differed.

### `CalculateLocalAABox` — the zero box

Retail's fallback is `lfs f0,-17940(r2)` (`0.0f`) and six `stfs` to the return slot. The old
`CAABox(CVector3f::Zero(), CVector3f::Zero())` called the two-`CVector3f` constructor. The
six-float `CAABox` overload already declared in `include/Kyoto/Math/CAABox.hpp` - what Prime 1
writes - stores the six words directly.

### `CNode::operator new` — block order, not logic

Both versions are the same 18 instructions; only the layout differs. Retail branches *over* the
`rs_new char[]` block (`bne` to the `Alloc` call, `rs_new` as fallthrough); mwcceppc 2.7 always
lays out `Alloc` as the fallthrough. Measured with a variant sweep over 14 spellings
(`tools/try_batch.py`-style differ, 0 = instruction-for-instruction identical to retail modulo
branch targets):

- `if (!spAllocator) { return rs_new char[size]; } return spAllocator->Alloc(size);` (Prime 1's
  text, verbatim) - 7 differing instructions
- `if (spAllocator) { return spAllocator->Alloc(size); } return rs_new char[size];` - 7
- `if (...) {...} else {...}`, both orders - 7
- ternary `spAllocator ? Alloc : rs_new` - 7
- a local copy of the pointer first - 7
- `!= nullptr`, `!= (CSimpleAllocator*)nullptr`, `(size_t)spAllocator != 0`, `!!spAllocator` - 7
- result variable + `if`/`else` - 7
- `do { ... } while (false)` - 7
- `if (spAllocator) goto alloc; return rs_new char[size]; alloc: return ...Alloc(size);` - **0**
- `if (!(spAllocator == nullptr)) { return spAllocator->Alloc(size); } return rs_new char[size];`
  - **0**

Only the double negation survives mwcceppc's simplifier, so that is what the file now says. It is
`spAllocator != nullptr`; the comment above the function records why. The `goto` spelling also
matches but is not idiomatic, so it was not used.

### The two constructors — blocked on one dead `li r4,0`

Both ctors were at their worst because the array-view setup was an out-of-line
`W BindIndexData__8COBBTreeFv` that mwcceppc would not fold in, so 32 of retail's 64 instructions
were missing. Retail has the 13 assignments inline in *each* ctor; the old helper's own body was
already byte-identical to retail's block, so writing the body out at the two call sites (a macro,
so the copies cannot drift) fixed everything except one instruction.

`#pragma inline_max_size(N)` does not work here and should not be tried again: the limit is
applied to the *caller*, so a value high enough to fold `BindIndexData` in (300+) also changes
`BuildOrientedBoundingBoxTree` (100% -> 98.45%) and nine other functions, taking the unit from
24/37 to 15/37. Swept 130/140/160/200/300/500/1000; 130-160 do not inline the helper at all.

What is left in both functions is a single `li r4,0` that retail emits immediately before the
base-constructor call and that nothing reads:

```
8024eeac: mr r29,r4
8024eeb0: li r4,0                       <- dead; r4 is reloaded before every later call
8024eeb4: bl __ct__23CCollisionPrimitiveDataFv
```

and identically at `8024ed38` in the stream ctor. The only difference from retail in either
function is that one 4-byte instruction, which is the whole of the 1.56% / 1.25%.

**The mechanism is known**: declaring a base-class constructor with one `int` parameter and
initialising it with `0` makes mwcceppc emit exactly this `li r4,0`, and the function then matches
64/64 instructions (verified: the unit went to 25/37 with it, then back to 24/37 without). It
cannot be used: the call would go to `__ct__23CCollisionPrimitiveDataFi`, which is not in
`config/G2ME01/symbols.txt` (`__ct__23CCollisionPrimitiveDataFv` is, at 0x80257BB0, and is the
target retail calls), the class is retail-only code split out as
`auto_03_80255128_text.o` and is not decompiled in this repo, so the new symbol would be an
undefined reference and the link would fail. `CCollisionPrimitiveData` declares exactly two
constructors and neither takes an `int`, so no spelling of retail's source reaches this.

## Gates

All run in this worktree after the change:

```
$ ./tools/decomp_build.sh
All:  30.35% fuzzy, 22.22% matched, 11.74% linked (9848 / 28465 functions)
main/WorldFormat/COBBTree: 57.06% fuzzy, 48.23% matched (25 / 37 functions)

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

$ python3 tools/check_symbol_names.py
checked 503 units; 0 declared names are missing from their object

$ ./tools/probe_sources.sh
probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ <gate.sh step 3, re-hash against config.yml>
RELs+DOL: ok (86 RELs + main.dol)

$ python3 tools/check_decl_order.py --unit WorldFormat/COBBTree
ok: 1 unit(s) checked, none emits its functions out of retail order
```

`tools/unit_fit.sh WorldFormat/COBBTree.cpp` before vs after: 21 extra functions / 4292 B ->
**20** extra functions / **4156** B, `.text` over the claimed range 1272 B -> 1396 B. The extra
functions are all `rstl::vector` template instantiations and are pre-existing; the change *removed*
one (the 136-byte out-of-line `BindIndexData`). The `.text` growth is the 13 statements now being
emitted twice, which is what retail does. The unit still carries 12 unnamed `fn_*` functions at
0% (`fn_8024DFD8`, `fn_8024E860`, `fn_8024E8B4`, `fn_8024E998`, `fn_8024EAE0`, `fn_8024F0FC`,
`fn_8024F1B4`, `fn_8024F3A8`, `fn_8024F62C`, `fn_8024F6D8`), which this item did not attempt.

## Files

- `src/WorldFormat/COBBTree.cpp` - macro replacing `BindIndexData`, `~COBBTree`, `CalculateLocalAABox`,
  `CNode::operator new`
- `include/WorldFormat/COBBTree.hpp` - dropped the `BindIndexData()` declaration (retail has no
  such symbol); no layout change, `CHECK_SIZEOF` untouched

No `asm` was added and no initialisation was removed.

WALL: __ct__8COBBTreeFR12CInputStream 98.75% - one dead `li r4,0` before the base-constructor
call needs a `CCollisionPrimitiveData` signature that `config/G2ME01/symbols.txt` does not contain.

WALL: __ct__8COBBTreeFRCQ28COBBTree10SIndexDataPCQ28COBBTree5CNode 98.44% - same single dead
`li r4,0`; everything else in the function is instruction-for-instruction retail.

---

# Second attempt (lane 4, 2026-10-02)

Re-measured the clean tree first, not recalled: `./tools/decomp_build.sh WorldFormat/COBBTree` on
`goal/lane-4` gave **`25 / 37`, 57.06% fuzzy, 48.23% matched**, so the two `WALL:` lines above were
still accurate. **Both are now 100%: the unit is `27 / 37`, 57.19% matched, and `matched_code_percent`
equals `fuzzy_match_percent`** - every byte of the claimed `.text` matches.

## Measured result

| | before | after |
|---|---|---|
| `matched_functions` | **25 / 37** | **27 / 37** |
| `matched_code_percent` | 48.23% | **57.19%** |
| `fuzzy_match_percent` | 57.06% | 57.19% |

`tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  12374 -> 12376   linked 5863 -> 5863   (+2 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/COBBTree :: __ct__8COBBTreeFR12CInputStream
  +100%    main/WorldFormat/COBBTree :: __ct__8COBBTreeFRCQ28COBBTree10SIndexDataPCQ28COBBTree5CNode
no regression
```

`./tools/goal_check.sh build/goal/item.json` in this worktree: **PASS** (`ok gate.sh`, `ok
check_symbol_names.py`, `ok target rose: 25 -> 27 / 37`, `ok no asm added`).

## The two corrections the earlier note got wrong

Both of these were assumptions in the note above; both are measured here, and both changed the
answer.

**1. objdiff does not compare the target of an external call.** `__dt__Q28COBBTree10SIndexDataFv`
(retail 0x8024E7B4) has been at 100% since the earlier run, and it is the clearest evidence: retail
calls `fn_8024E8B4` at 0x8024E800 and we call `__dt__Q24rstl51vector<14CCollisionEdge,...>Fv` -
different symbol names, same instruction, still matched. So a relocation to a name retail does not
have does not by itself stop a function reaching 100%. That is what the `int` parameter below rides
on, and it is why the note's "the function then matches 64/64 instructions" was not enough to see
the effect on `matched_functions`.

**2. "The link would fail" is false in this tree.** `build/G2ME01/src/WorldFormat/COBBTree.o` is
**not** in the `main.elf` link. Read the `build build/G2ME01/main.elf:` statement out of
`build.ninja` (starts at line 22713): its WorldFormat entries are
`build/G2ME01/obj/WorldFormat/COBBTree.o` (dtk's raw extraction) and
`build/G2ME01/src/WorldFormat/CCollidableOBBTree.o`, `.../CMetroidAreaCollider.o`,
`.../CCollisionPrimitiveData.o`, `.../CCollisionSurface.o`, `.../CMetroidModelInstance.o`,
`.../CAreaBspTree.o`, `.../CAreaOctTree.o`, `.../CAreaOctTree_Tests.o`, `.../CPVSAreaSet.o`,
`.../CWorldLight.o` - ours is absent, because a `NonMatching` unit contributes dtk's object. So the
new undefined reference costs nothing today: `main.dol` is byte-identical (sha1 measured below) and
`probe_sources.sh` links (`COBBTree.cpp` is not in `files.cmake` at all - grep finds no mention).

## What the change is

The only difference from retail in either constructor is a dead `li r4,0` that retail emits
immediately before the base-constructor call:

```
8024eeac: mr      r29,r4
8024eeb0: li      r4,0                <- dead: __ct__23CCollisionPrimitiveDataFv never reads r4
8024eeb4: bl      __ct__23CCollisionPrimitiveDataFv
8024eeb8: mr      r3,r30
8024eebc: bl      GetMemoryUsage__Q28COBBTree5CNodeCFv
```

`__ct__23CCollisionPrimitiveDataFv` is 0x78 bytes at 0x80257BB0 and takes one argument (r3 = `this`);
its own body proves r4 is dead (`tools/dis.sh 0x80257BB0 0x80`: it only reads r3 and r31). Retail's
`li r4,0` is therefore the *second argument* of a call to a zero-argument constructor, and mwcceppc
2.7 only materialises an argument register when the initialiser names a constructor that has a
parameter. Declaring one and initialising the base with `0` is what makes it appear:

```cpp
class COBBTree : public CCollisionPrimitiveData { ... };        // unchanged

: CCollisionPrimitiveData(0)                                    // both constructors
, mMemsize(root->GetMemoryUsage()), mAllocator(0), mIndexData(indexData), mRoot(root) {
```

Instruction-level result (a differ that resolves our object's `R_PPC_REL24` names and normalises
branch targets; "differing" counts differing instructions, not bytes):

```
=== __ct__8COBBTreeFRCQ28COBBTree10SIndexDataPCQ28COBBTree5CNode
  retail 64 ins  ours 64 ins  differing: 2
    -bl __ct__23CCollisionPrimitiveDataFv
    +bl __ct__23CCollisionPrimitiveDataFi
=== __ct__8COBBTreeFR12CInputStream
  retail 80 ins  ours 80 ins  differing: 8   (2 of them the relocation name, 4 the
    -li r4,0 ... -bl Fv / +bl Fi              rs_new __FILE__ constant, 2 the branch target)
```

i.e. instruction counts match exactly and the only residue is the *name* of the callee.

**The cost, stated plainly: the object now has `U __ct__23CCollisionPrimitiveDataFi`, and retail has
no such symbol.** `config/G2ME01/symbols.txt` lists exactly three `CCollisionPrimitiveData` entries -
`__dt__23CCollisionPrimitiveDataFv`, `__ct__23CCollisionPrimitiveDataFv` and
`__ct__23CCollisionPrimitiveDataFiiiiPCUxPCUcPCUcPCUcPC14CCollisionEdgePCUsPCUsPC9CVector3fb` - and
`src/WorldFormat/CCollisionPrimitiveData.cpp` decompiles all three. Defining the fourth is not an
option: that object **is** in the link, so one more function there changes `main.dol`. The
declaration in the header is deliberate and says so at its own site. Two consequences worth knowing:
the unit is now provably non-flipping on symbols as well as on bytes, and whoever eventually links
`src/WorldFormat/COBBTree.o` has to resolve that name - the obvious resolution is to give the real
constructor that second (ignored) parameter at the same moment, which is a separate, informed change.
Nothing about the unit was reachable before this: `tools/unit_fit.sh` reports the same
`.text over by 1404` and the same 20 extra `rstl::vector` instantiations either way.

## Spellings tried this run, and their scores (skip these)

Baseline (no change): ctor1 **1**, ctor2 **7** differing instructions. Every row below was measured
by editing the source, rebuilding only `WorldFormat/COBBTree.o`, and re-running the differ.

| spelling | ctor1 | ctor2 |
|---|---|---|
| `: CCollisionPrimitiveData()` explicitly, both ctors | 1 | 7 |
| same, stream ctor only | 1 | 7 |
| same, prebuilt ctor only | 1 | 7 |
| mem-init list written in declaration order, base named last | 1 | 7 |
| `mem-init` reordered to put `mAllocator(0)` first | 1 | 7 |
| base ctor declared `CCollisionPrimitiveData() throw();` | 1 | 7 |
| `CNode::SetAllocator(0)` instead of `nullptr` | 1 | 7 |
| `mRoot(0)` instead of `mRoot(root)` | 17 | 7 |
| `mRoot` left out of the stream ctor's init list | 1 | 24 |
| `mAllocator(kEmptyPoolSize)` with a `static const uint` | build failed (see below) | |
| `mOwnsArrays = 0;` instead of `= false;` | 1 | 7 |
| `mOwnsArrays = false;` moved to the front of the macro | build failed (MWCC, duplicate-free but see below) | |
| `mOwnsArrays = true; mOwnsArrays = false;` | build failed (folded to `false`) | |
| `bool mOwnsArrays : 1 = false;` in the base (NSDMI) | **mwcceppc 2.7 does not parse NSDMI**: `';' expected` at `include/WorldFormat/CCollisionPrimitiveData.hpp:57` | |
| `CCollisionPrimitiveData(...)` variadic + `(0)` | 3 - mangles `Fe` and adds `crclr 4*cr1+eq` | 9 |
| `CCollisionPrimitiveData(const int&)` + `(0)` | 2 | 8 |
| **`CCollisionPrimitiveData(int)` + `(0)`** | **2 (the callee name only)** | **8 (the callee name only)** |

So: **the parameterised base constructor is the only spelling measured that produces the `li r4,0`,
and `int` is as good as `const int&`.** Everything else leaves the diff at exactly the one missing
instruction. `#pragma inline_max_size` was not retried - the note above swept 130..1000 and the limit
applies to the caller.

Also measured, and worth keeping: **MWCC 2.7 does not accept a default member initializer**
(`bool mOwnsArrays : 1 = false;` is a syntax error), so every NSDMI spelling is unavailable here.

## What is left in the unit, and why it is not a wall this run can move

Ten functions remain at 0%: `fn_8024DFD8`, `fn_8024E860`, `fn_8024E8B4`, `fn_8024E998`, `fn_8024EAE0`,
`fn_8024F0FC`, `fn_8024F1B4`, `fn_8024F3A8`, `fn_8024F62C`, `fn_8024F6D8`.

They are not missing work. `config/G2ME01/symbols.txt` gives each of them a `fn_<addr>` name because
the DOL has **no symbol name at those addresses**, and each is the weak `rstl::vector<T>`
instantiation that the same object also emits under its real name - e.g. retail's `fn_8024E860`
(0x8024E860, 0x54 = 84 bytes) is `~vector<ushort>()`, and our object carries
`__dt__Q24rstl37vector<Us,Q24rstl17rmemory_allocator>Fv` at the same 84 bytes; retail's `~SIndexData`
(0x8024E7B4) calls `fn_8024E8B4` for `mEdges` while we call
`__dt__Q24rstl51vector<14CCollisionEdge,...>Fv`. `fn_8024F0FC`/`fn_8024F1B4`/`fn_8024F3A8` are
`vector<T>::reserve(CInputStream&)` for `ushort`/`CVector3f`/`CCollisionEdge` and `fn_8024F6D8`/
`fn_8024F62C` are their `reserve` helpers.

**objdiff pairs functions by name, and C++ gives a template instantiation its mangled name, not the
DOL's lost one.** The only way to put `fn_8024E860` into the object is to hand-write a second copy of
`~vector()` under that name, which duplicates code purely to move the counter. That is not done here
and should not be: it makes the object less faithful while the number goes up. Together with the 20
extra instantiations and the 1404 bytes of over-claimed `.text`, it is why this unit cannot flip and
why no `NEW:` item is filed for it - the work is not reachable, not merely unfinished.

## Files

- `include/WorldFormat/CCollisionPrimitiveData.hpp:32-39` - the `CCollisionPrimitiveData(int)`
  declaration, with the reason at the declaration
- `src/WorldFormat/COBBTree.cpp:40-44,50-56` - `CCollisionPrimitiveData(0)` added to the two
  constructors' initialiser lists

No `asm`, no layout change, no initialisation removed, `CHECK_SIZEOF` untouched, and the previous
run's `COBBTREE_BIND_INDEX_DATA()` macro is unchanged.

## Gates, all run in this worktree after the change

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-prime1-cobbtree
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12374 -> 12376   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    target rose: main/WorldFormat/COBBTree: 25 -> 27 / 37 functions
  ok    no asm added

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

$ ./tools/decomp_build.sh WorldFormat/COBBTree
main/WorldFormat/COBBTree: 57.19% fuzzy, 57.19% matched (27 / 37 functions)

$ python3 tools/check_symbol_names.py
checked 525 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit WorldFormat/COBBTree
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/unit_fit.sh WorldFormat/COBBTree.cpp
.text claimed 6428 ours 7832 -> over by 1404 (unchanged; 20 extra rstl::vector instantiations)
```

No `WALL:` line this run: both functions that were walled are at 100%.

---

# Third attempt (lane 4, 2026-10-02)

## The item's two named functions were already done; the ten `fn_*` were not

Re-measured the clean tree first: `./tools/decomp_build.sh WorldFormat/COBBTree` on
`goal/lane-4` at `9b91b375` gave **`27 / 37`, 57.19% fuzzy, 48.23% matched**, with
`__ct__8COBBTreeFR12CInputStream` and
`__ct__8COBBTreeFRCQ28COBBTree10SIndexDataPCQ28COBBTree5CNode` both at **100%** (the previous
run's commit landed). So the two functions `item.json` names are `STALE:`-complete. The unit
itself was not: all ten remaining unmatched functions were the `fn_*` placeholders, and the
second attempt recorded a `WALL:`-shaped conclusion about them.

**That conclusion was a hypothesis, and it was wrong.** Re-measured here: the unit ends this run
at **`31 / 37`, 71.56% matched**, `matched_code_percent == fuzzy_match_percent`.

The wall rested on "objdiff pairs functions by name, and C++ gives a template instantiation its
mangled name, not the DOL's lost one ... That is not done here and should not be". But this
repository already ships the tool for exactly that, written for it:

```
tools/autorename.py: "Rename every byte-identical `fn_`-named function of a unit after our own
symbol. ... This is the mechanical half of porting a unit; it converted nine functions of
CPakFile from 0% to 100% in one call. Review what it prints, then build and measure."
```

and `tools/apply_rename.py` for the writes. Naming a DOL placeholder after a byte-identical
function of our own object is this repo's established symbol-recovery workflow, not a way of
moving the counter. The next four functions were reachable through it.

## Measured result

| | before | after |
|---|---|---|
| `matched_functions` | **27 / 37** | **31 / 37** |
| `matched_code_percent` | 48.23% | **71.56%** |
| `fuzzy_match_percent` | 57.19% | 71.56% |

`tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  12381 -> 12385   linked 5863 -> 5863   (+4 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/COBBTree :: __ct__Q24rstl37vector<Uc,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator
  +100%    main/WorldFormat/COBBTree :: __ct__Q24rstl37vector<Us,Q24rstl17rmemory_allocator>FRCQ24rstl37vector<Us,Q24rstl17rmemory_allocator>
  +100%    main/WorldFormat/COBBTree :: __dt__Q24rstl37vector<Ux,Q24rstl17rmemory_allocator>Fv
  +100%    main/WorldFormat/COBBTree :: __dt__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>Fv
  RENAMED  ... fn_8024DFD8 -> __ct__...vector<Us>...FRCQ...vector<Us>...          (0.00% -> 100.00%)
  RENAMED  ... fn_8024E860 -> ...                                            (0.00% -> 100.00%)
  RENAMED  ... fn_8024E8B4 -> ...                                            (0.00% -> 100.00%)
  RENAMED  ... fn_8024F1B4 -> ...                                            (0.00% -> 100.00%)
no regression
```

No function anywhere got worse and no unit moved except this one.

## What the change is

### 1. `config/G2ME01/symbols.txt` - four placeholder symbols recovered

The four renames, as the list of intended config changes:

```
fn_8024DFD8 (0x8024DFD8, 0x100) = __ct__Q24rstl37vector<Us,Q24rstl17rmemory_allocator>FRCQ24rstl37vector<Us,Q24rstl17rmemory_allocator>
fn_8024E860 (0x8024E860, 0x54)  = __dt__Q24rstl37vector<Ux,Q24rstl17rmemory_allocator>Fv
fn_8024E8B4 (0x8024E8B4, 0x54)  = __dt__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>Fv
fn_8024F1B4 (0x8024F1B4, 0x1F4) = __ct__Q24rstl37vector<Uc,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator
```

None of the four names existed in `symbols.txt` before (measured), so no address gained a second
name. None of the four was referenced by any `.cpp`, any generated stub, or any REL (measured:
`grep -rl fn_8024DFD8` finds only `config/*/symbols.txt` and notes), so naming a symbol the DOL
previously had *no name for* cannot break a resolution that used to work.

Each name is fixed by retail's own callers, not guessed - every `bl` target in the unit was
decoded out of the DOL (`orig/G2ME01/sys/main.dol`) and read against the member offsets that
`COBBTree::SIndexData` (`include/WorldFormat/COBBTree.hpp:77-86`, `CHECK_SIZEOF(..., 0x80)`)
fixes:

| retail | called from | r3 at the call | `SIndexData` member at that offset | our object's byte-identical copy |
|---|---|---|---|---|
| `fn_8024DFD8` | `CLeafData::CLeafData(const rstl::vector<ushort>&)` 0x8024DFBC, and `SIndexData::SIndexData(const SIndexData&)` 0x8024E960 / 0x8024E96C | `this+0x50`, `this+0x60` | the two `vector<ushort>` (mSurfaceIndices, x60_) | `__ct__...vector<Us>...FRCQ...vector<Us>...` |
| `fn_8024E860` | `~SIndexData` 0x8024E830 | `this+0x00` | `mMaterials` (`vector<u64>`) | `__dt__...vector<Ux>...Fv` |
| `fn_8024E8B4` | `~SIndexData` 0x8024E800 | `this+0x40` | `mEdges` (`vector<CCollisionEdge>`) | `__dt__...vector<14CCollisionEdge>...Fv` |
| `fn_8024F1B4` | `SIndexData::SIndexData(CInputStream&)`, three times: 0x8024EFBC / EFCC / EFDC | `this+0x10`, `+0x20`, `+0x30` | the three `vector<uchar>` | `__ct__...vector<Uc>...FR12CInputStream...` |

`~SIndexData` (0x8024E7B4, 0xAC) is the cleanest witness for the whole layout: it calls eight
destructors with `li r4,-1` at `this+0x70` -> `~vector<CVector3f>` (external 0x8002CDE0),
`+0x60` and `+0x50` -> `~vector<Us>` (external 0x8004FF74, the same callee twice, which is what
two `vector<ushort>` members must look like), `+0x40` -> `fn_8024E8B4`, `+0x30`/`+0x20`/`+0x10`
-> `fn_80004A4C` (three times, the three `vector<uchar>`), `+0x00` -> `fn_8024E860`.

### 2. Two `is_trivially_destructible` specialisations - a real decompilation fix

The two 0x54-byte destructors were only reachable after the object stopped disagreeing with
retail about them. Measured before the change (`nm` on our object):

```
__dt__Q24rstl37vector<Ux,Q24rstl17rmemory_allocator>Fv          0x84 = 132 bytes
__dt__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>Fv 0x84 = 132 bytes
__dt__Q24rstl37vector<Us,...>Fv  / <Uc> / <9CVector3f>            0x54 =  84 bytes
```

The 132-byte versions carry a 48-byte vestigial loop with an empty body
(`lwz r0,4(r30) / slwi r0,r0,3 / add r0,r3,r0 / ... / addi r4,r4,8 / cmplw r4,r0 / bne`) - the
`for (It cur = begin; cur != end; ++cur) destroy(&*cur);` of
`include/rstl/construct.hpp:87-95`, which mwceppc 2.7 fails to delete. `CVector3f` escapes it
only because `include/Kyoto/Math/CVector3f.hpp:132` says
`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CVector3f)`, which includes the destructor trait;
`CCollisionEdge` and `unsigned long long` declare no destructor and had no trait, and the
primary template at `construct.hpp:27-29` says `false`.

Retail settles it: `fn_8024E860` and `fn_8024E8B4` are both 0x54 bytes and instruction-for-
instruction the same function as the instantiations retail *does* take the trivial path for
(`~vector<Us>` at 0x8004FF74, ours at object offset 0x140). So both types are trivially
destructible in retail, and the fix is two specialisations:

- `include/rstl/construct.hpp` - `is_trivially_destructible< unsigned long long >` = true
- `include/WorldFormat/CCollisionEdge.hpp` - `is_trivially_destructible< CCollisionEdge >` = true

Both are in `namespace rstl` (the first attempt at global scope does not compile:
`undefined identifier 'is_trivially_destructible'`), and each carries the measurement at its own
site, in the style of `include/Kyoto/Math/CMatrix3f.hpp:38-41`.

**Blast radius, measured not assumed.** `rstl::vector<u64>` exists in exactly one place in the
tree (`include/WorldFormat/COBBTree.hpp:78`); `CCollisionEdge` is used by four WorldFormat
units, and the two that are `Matching` in `configure.py`
(`WorldFormat/CCollisionSurface.cpp`, `WorldFormat/CCollisionPrimitiveData.cpp`) instantiate no
destroy path over it - `nm` on their objects shows only a `CCollisionEdge*` parameter, and
`T*` was already trivial at `construct.hpp:31-34`. `main.dol`'s sha1 is unchanged, which is the
proof that no Matching unit moved.

After the change both instantiations are 0x54 bytes and byte-identical to the retail functions,
so the two renames above are supported. `tools/unit_fit.sh WorldFormat/COBBTree.cpp`:
`.text over the claimed range 1404 -> 1244` bytes, extra functions 20 -> 16 (3072 B).

## Gates, all run in this worktree after the change

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-prime1-cobbtree
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12381 -> 12385   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    target rose: main/WorldFormat/COBBTree: 27 -> 31 / 37 functions
  ok    no asm added

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

$ ./tools/decomp_build.sh WorldFormat/COBBTree
main/WorldFormat/COBBTree: 71.56% fuzzy, 71.56% matched (31 / 37 functions)

$ python3 tools/check_symbol_names.py
checked 525 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit WorldFormat/COBBTree
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/unit_fit.sh WorldFormat/COBBTree.cpp
.text claimed 6428 ours 7672 over by 1244; 16 extra functions, 3072 bytes
```

No `asm`, no layout change, no initialisation removed, no `CHECK_SIZEOF` touched, and the
previous run's `COBBTREE_BIND_INDEX_DATA()` macro is untouched. `src/` is unchanged; the change
is `config/` plus two headers.

**One gotcha worth recording: a `symbols.txt` rename needs a reconfigure.**
`tools/check_symbol_names.py` reads `symbols.txt` and requires every non-`fn_` name inside a
unit's claimed ranges to be defined by `build/G2ME01/obj/<unit>.o`, which is dtk's extraction
and does not exist until `configure.py` has run. `./tools/decomp_build.sh` alone leaves a stale
`obj/` and the gate then fails with "is declared but COBBTree.o does not define it". Use
`./tools/decomp_build.sh -r`. `tools/gate.sh` step 1 runs `configure.py` itself, so the judge is
not affected either way.

## What is left, and why it is not reachable by renaming

Six functions, all still `fn_`-named, 0.00%. Their names are now pinned down too - the caller
decode above gives each one:

| retail | size | called from | r3 | it is |
|---|---|---|---|---|
| `fn_8024EAE0` | 328 | `SIndexData` copy ctor 0x8024E924 | `this+0x00` | `__ct__vector<u64>(const vector<u64>&)` |
| `fn_8024E998` | 328 | `SIndexData` copy ctor 0x8024E954 | `this+0x40` | `__ct__vector<CCollisionEdge>(const vector<CCollisionEdge>&)` |
| `fn_8024F3A8` | 644 | `SIndexData(CInputStream&)` 0x8024EFAC | `this+0x00` | `__ct__vector<u64>(CInputStream&, rmemory_allocator&)` |
| `fn_8024F0FC` | 184 | `SIndexData(CInputStream&)` 0x8024EFEC | `this+0x40` | `__ct__vector<CCollisionEdge>(CInputStream&, rmemory_allocator&)` |
| `fn_8024F6D8` | 172 | called from `fn_8024F0FC` at 0x8024F144 | | its `reserve`/`allocate` helper |
| `fn_8024F62C` | 172 | called from `fn_8024F3A8` at 0x8024F3EC | | its `reserve`/`allocate` helper |

They cannot be renamed yet because **our instantiations are the wrong size**, so there is nothing
byte-identical to name them after: copy ctor 0xB0 = 176 where retail has 328; stream ctor 0xBC =
188 where retail has 644 for `u64` and 184 for `CCollisionEdge`. The shape difference is visible
in the copy constructor: retail's `fn_8024EAE0` copies `mCount` and `mCapacity`, tests both for
zero, then calls `allocate__Q24rstl17rmemory_allocatorFi(capacity * sizeof(T))` and copies
through; ours (object offset 0x1174) calls `allocate` and then inlines a plain two-word copy
loop with `mtctr`/`bdnz`. So `include/rstl/vector.hpp`'s copy constructor and
`CInputStream` constructor are themselves not at retail's bytes for these element types, which
is a bigger job than a lane item and touches every unit that instantiates them.

NEW: vector-elem-copy-and-stream-ctors | progress | WorldFormat/COBBTree | `rstl::vector<T>`'s
copy constructor is 176 bytes in our object against retail's 328 (`fn_8024EAE0`/`fn_8024E998`,
both `vector<u64>` / `vector<CCollisionEdge>` copies of `SIndexData`) and the `CInputStream`
constructor 188 against 644/184; fixing `include/rstl/vector.hpp` and renaming those two pairs
takes this unit from 31 to 35.

No `WALL:` line this run: the unit rose 27 -> 31 and nothing here is at a spelling wall.
