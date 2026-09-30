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
