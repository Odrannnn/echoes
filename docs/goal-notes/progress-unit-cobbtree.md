# progress-unit-cobbtree

`kind: progress`, `target: WorldFormat/COBBTree`. The unit stays `NonMatching`; no `flip_test` was
run to decide anything.

## Measured result

Re-measured the clean tree first (`./tools/decomp_build.sh WorldFormat/COBBTree` at `a374fce8`):
**31 / 37**, `fuzzy_match_percent` = `matched_code_percent` = 71.56%, `matched_code` 4600 / 6428. `item.json`'s list of ten `fn_*` functions at 0.00% was stale: four of
them had been renamed and matched by the previous run, six were left.

| | before | after |
|---|---|---|
| `matched_functions` | **31 / 37** | **35 / 37** |
| `matched_code` | 4600 / 6428 | **5600 / 6428** |
| `matched_code_percent` / `fuzzy_match_percent` | 71.56% | **87.12%** |

`tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  12386 -> 12390   linked 5863 -> 5863   (+4 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/COBBTree :: __ct__Q24rstl37vector<Ux,...>FRCQ24rstl37vector<Ux,...>
  +100%    main/WorldFormat/COBBTree :: __ct__Q24rstl51vector<14CCollisionEdge,...>FRCQ24rstl51vector<14CCollisionEdge,...>
  +100%    main/WorldFormat/COBBTree :: reserve__Q24rstl37vector<Ux,...>Fi
  +100%    main/WorldFormat/COBBTree :: reserve__Q24rstl51vector<14CCollisionEdge,...>Fi
no regression
```

**Do not read the tool's `RENAMED` lines on this unit** - it pairs the old and new names by
position in a list, and it prints `fn_8024E998 -> __ct__vector<u64> copy` /
`fn_8024EAE0 -> __ct__vector<CCollisionEdge> copy`, which is the opposite of what was applied.
`build/report.json` is the authority and agrees with the source: at 0x8024E998 it is the
`CCollisionEdge` copy constructor at 100.00%, at 0x8024EAE0 the `u64` one.

## Per function

All four are `rstl::vector<T>` instantiations; sizes are our object's before -> after, against
retail's. objdiff's `fuzzy_match_percent` was 0.00% for all four before and 100.00% after, because
they were `fn_`-named; the byte change below is what made the rename possible.

| function (now) | retail | before | after |
|---|---|---|---|
| `__ct__vector<CCollisionEdge>(const vector<CCollisionEdge>&)` | 0x8024E998, 328 B | 176 B | **328 B**, byte-identical |
| `__ct__vector<u64>(const vector<u64>&)` | 0x8024EAE0, 328 B | 176 B | **328 B**, byte-identical |
| `reserve<vector<CCollisionEdge>>` | 0x8024F6D8, 172 B | 180 B | **172 B**, byte-identical |
| `reserve<vector<u64>>` | 0x8024F62C, 172 B | 180 B | **172 B**, byte-identical |

Prime 1's `src/WorldFormat/COBBTree.cpp` was not needed for these four: they are template
instantiations of this repo's own `rstl::vector`.

## The one substantive fix: two types were not trivially *constructible*

`include/rstl/construct.hpp` declared `is_trivially_destructible<unsigned long long>` and
`include/WorldFormat/CCollisionEdge.hpp` declared it for `CCollisionEdge` (the previous run's
work), but neither type had `construct_impl`, so `construct()` still went to the primary
template's `new (dest) T(src)`. That emits a per-element null test of the destination inside every
copy loop, which is both instructions retail has not got and an optimisation barrier: MWCC's
8x element unroll of the block copy never fires.

Both headers now use `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`, which carries the destructor trait
with it, so no specialisation is lost. Measured effect on our object:

```
__ct__vector<u64>(const vector<u64>&)                     0xB0 -> 0x148   (retail 0x148)
__ct__vector<CCollisionEdge>(const vector<CCollisionEdge>&) 0xB0 -> 0x148  (retail 0x148)
reserve<vector<u64>>                                       0xB4 -> 0xAC    (retail 0xAC)
reserve<vector<CCollisionEdge>>                            0xB4 -> 0xAC    (retail 0xAC)
```

Retail's copy constructor (`0x8024EAE0`) is the proof of the unroll, not an assumption: it is
`srwi. r0,r4,3 / mtctr / bdnz` over a body of eight 8-byte copies with an `andi. r4,r4,7` tail.
Ours had five instructions and no unroll. Retail's `reserve` (`0x8024F62C`) is the proof of the
missing null test: its loop is `lwz/lwz/stw/stw` with nothing else, ours carried
`cmplwi r5,0 / beq` on every iteration.

The `CCollisionEdge` case is a `CHECK_SIZEOF`-checked class with an implicit copy assignment
operator, so the trait's `*dest = src` is the whole of `T(src)`; the previous note's destructor
measurement (`fn_8024E8B4` is the same 21 instructions as the instantiations retail *does* take the
trivial path for) covers the same property from the other side.

**Blast radius, measured.** `rstl::vector<u64>` exists in one place in the tree
(`include/WorldFormat/COBBTree.hpp:78`). `CCollisionEdge` reaches three other units, all of which
pass it as `const CCollisionEdge*` and never construct one - `rstl::construct` on it is
instantiated only by `vector<CCollisionEdge>`. `main.dol`'s sha1 is byte-identical (below), which
is the proof that no Matching unit moved.

## The four renames in `config/G2ME01/symbols.txt`

Reported as a list of intended changes, per the rule on config edits. No address gained a second
name; none of the four old names was referenced by any `.cpp`, generated stub or REL
(`grep -rl fn_8024E998` finds only `config/*/symbols.txt` and notes), so naming a symbol the DOL
had no name for cannot break a resolution that used to work.

```
fn_8024E998 = __ct__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>FRCQ24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>
fn_8024EAE0 = __ct__Q24rstl37vector<Ux,Q24rstl17rmemory_allocator>FRCQ24rstl37vector<Ux,Q24rstl17rmemory_allocator>
fn_8024F62C = reserve__Q24rstl37vector<Ux,Q24rstl17rmemory_allocator>Fi
fn_8024F6D8 = reserve__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>Fi
```

Each name is fixed by retail's own callers, decoded out of the DOL here, not inferred from the
byte pairing (`tools/autorename.py` only proposes the pairing; the caller is what names it):

| retail | called from | r3 at the call | `SIndexData` member there | the callee itself |
|---|---|---|---|---|
| `fn_8024E998` | `SIndexData(const SIndexData&)` 0x8024E954 | `this+0x40` | `mEdges`, `vector<CCollisionEdge>` | `slwi r3,r0,2` = x4 |
| `fn_8024EAE0` | `SIndexData(const SIndexData&)` 0x8024E924 | `this+0x00` | `mMaterials`, `vector<u64>` | `slwi r3,r0,3` = x8 |
| `fn_8024F6D8` | `fn_8024F0FC` 0x8024F144 (`vector<CCollisionEdge>`'s stream ctor) | - | - | `slwi r3,r30,2` = x4 |
| `fn_8024F62C` | `fn_8024F3A8` 0x8024F3EC (`vector<u64>`'s stream ctor) | - | - | `slwi r3,r30,3` = x8 |

`SIndexData`'s two constructors also pin the last two: 0x8024EFAC calls `fn_8024F3A8` with
`r3 = this+0x00` and 0x8024EFEC calls `fn_8024F0FC` with `r3 = this+0x40`.

## `CInputStream::Get<u64>` - right shape, one instruction of scheduling short

Retail reads a `u64` as one 8-byte load with a single bump of the read pointer; our
`Get<u64>` was two `ReadInt32`s (two bumps of 4, a load off each). `fn_8024F3A8`'s element loop is
the witness: `lwz r4,8(r30) / addi r0,r4,8 / stw r0,8(r30) / lwz r7,0(r4) / lwz r8,4(r4)`.
`Get<u64>` now returns `ReadInt64()`, which is that one-bump read under mwcceppc (the host build
takes the `unsigned long` specialisation and is unaffected). The function goes from **336 bytes to
644 - retail's exact size** - and from two-bump reads to one, but is still not byte-identical.

`python3 tools/bytescmp.py build/G2ME01/src/WorldFormat/COBBTree.o 'vector<Ux,...>FR12CInputStream' 0x8024F3A8 0x284`:

```
7 differing instructions of 161 (644 bytes ours vs 644 retail)
```

One is the `bl reserve` relocation field (objdiff ignores it). The other six are **one instruction
placed four slots later** in the second unrolled copy - retail emits the dead `addi r3,r3,8`
between `stw r8,4(r4)` and `stw r7,0(r4)`, we emit it after `lwz r5,4(r29)`. Every other byte of
all 161 instructions agrees, including the 8x unroll, the `cmpwi r31,8 / addi r4,r31,-8` guard and
the tail loop.

Spellings measured for `fn_8024F3A8`, each a rebuild plus `bytescmp.py` (skip these):

| spelling | differing |
|---|---|
| `return ReadInt64();` | **7 of 161** (one is the relocation field) |
| `const u64 value = *reinterpret_cast<const u64*>(mPtr); mPtr += sizeof(u64); return value;` | 68 of 161 |
| as above, plus `const T value = in.Get<T>(); push_back_unsafe(value);` in the stream ctor's loop | 7 of 161 (unchanged) |

`fn_8024F3A8` is still `fn_`-named, so it scores 0.00% either way; **this change raises no count**.
It is in the diff because it is the item's own named function and it takes it from a loop with the
wrong number of reads and the wrong number of pointer bumps to retail's - one scheduling
instruction away.

## The last function, and why it is not reachable from here

`fn_8024F0FC` (0x8024F0FC, 184 B) is `__ct__vector<CCollisionEdge>(CInputStream&, const Alloc&)`.
Retail calls a per-element helper **out of line** - `bl fn_8024747C` (0x8024747C, 0x2C bytes: two
`lhz`/`sth` pairs with a `+2` bump each) - into a stack temporary, then copies the temporary into
the slot. Ours is **464 bytes**: `CCollisionEdge(CInputStream&)` is defined in the class body, so
it is implicitly inline, mwcceppc inlines it, and no symbol survives. Measured: `#pragma
dont_inline on` around the class (the mechanism `src/MetroidPrime/CAnimData.cpp:41` uses) changes
nothing - still 464 bytes, no out-of-line copy emitted. Getting the call needs the constructor
defined out of class in a `.cpp`, which is a different unit and a carve (four files), not a
spelling.

## Gates, all run in this worktree after the change

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-unit-cobbtree
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12386 -> 12390   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  35.02% fuzzy, 28.69% matched, 12.90% linked (12390 / 28465 functions)
  ok    target rose: main/WorldFormat/COBBTree: 31 -> 35 / 37 functions
  ok    no asm added

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

$ python3 tools/check_decl_order.py --unit WorldFormat/COBBTree
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/unit_fit.sh WorldFormat/COBBTree.cpp
.text over the claimed range by 2305 bytes; 12 function(s) in ours but not the retail unit
object, 3092 bytes total (all pre-existing rstl::vector instantiations)
```

No `asm`, no layout change, no initialisation removed, `CHECK_SIZEOF` untouched. `src/` is
unchanged: the change is `config/` plus three headers. A `symbols.txt` rename needs
`./tools/decomp_build.sh -r` - `decomp_build.sh` alone leaves a stale `obj/` and
`check_symbol_names.py` then fails with "declared but COBBTree.o does not define it".

`docs/HANDOFF.md` is rewritten by `gate.sh`'s docs-claims step when it runs; that edit is not in
this change (reverted), and the driver regenerates the state block from the tree.

## Files

- `include/rstl/construct.hpp:31-43` - `is_trivially_destructible<unsigned long long>` becomes
  `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(unsigned long long)`, with the copy-side measurement added
- `include/WorldFormat/CCollisionEdge.hpp:7-14,46` - the hand-written destructor
  specialisation becomes `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CCollisionEdge)`; comment records
  the out-of-line-read shape retail uses and that `dont_inline` does not produce it
- `include/Kyoto/Streams/CInputStream.hpp:157-172` - `Get<u64>` returns `ReadInt64()`
- `config/G2ME01/symbols.txt:10378-10379,10391-10392` - the four renames above

## Lesson worth keeping

A trait that says "trivially destructible" but not "trivially constructible" gets you a *shorter*
destructor and a *wrong* copy: the placement new of the primary template's `construct_impl` tests
the destination inside every loop, which is both 8 bytes of code retail has not got and a barrier
to MWCC's block-copy unroll (176 bytes where retail has 328). The destructor measurement alone did
not predict the copy, and the copy measurement is what made four functions renamable.

WALL: fn_8024F3A8 644 bytes, 6 real instructions differ - `__ct__vector<u64>(CInputStream&, const
Alloc&)` is retail's size and shape but one dead `addi r3,r3,8` lands four instructions late in the
second unrolled copy; three read spellings measured (7, 68, 7 differing of 161), nothing moved.

NEW: cobbtree-ccollisionedge-stream-ctor | progress | WorldFormat/COBBTree | fn_8024F0FC (184 B) needs
`CCollisionEdge(CInputStream&)` out of class in a .cpp so the stream ctor calls it like retail's
does; defined in the class it is implicitly inline and the instantiation is 464 bytes.