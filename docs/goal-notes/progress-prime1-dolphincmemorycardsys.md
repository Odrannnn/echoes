# progress-prime1-dolphincmemorycardsys

`kind: progress`, target `Kyoto/DolphinCMemoryCardSys` (the DOL unit `main/Kyoto/DolphinCMemoryCardSys`).
Re-measured on the clean tree first: **73 / 78 functions matched**, 99.08% fuzzy. After the change below:
**74 / 78**, 99.09% fuzzy, and the whole tree **10394 -> 10395** matched functions.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (DOL sha1, 86 RELs, wiring, docs claims, port probe all
clean; no judge-owned path touched; no `asm` added; no function got worse).

## What I changed

One statement-order swap in `src/Kyoto/DolphinCMemoryCardSys.cpp`, `CCardFileInfo::WriteSaveSlot(int)` (line ~135):

```cpp
 ECardResult CMemoryCardSys::CCardFileInfo::WriteSaveSlot(int slot) {
-  void* data = mSlots[mSlot].mData.data();
   const int offset = slot * mSlotSize + 0x2000;
+  void* data = mSlots[mSlot].mData.data();
   DCStoreRange(data, mSlotSize);
```

Before this, the two values that live across `DCStoreRange` were given callee-saved registers in the wrong order:
ours `r31 = data`, `r30 = offset`; retail `r30 = data`, `r31 = offset`. The instruction *sequence* was already
identical - only the two register numbers in three instructions differed (`lwz`, `addi`, and the two `mr`s feeding
`CARDWriteAsync`). Declaring `offset` first and `data` second swaps the allocation and the function matches
exactly, 99.17% -> **100.00%**.

This is a general MWCC rule worth keeping: **mwcceppc allocates callee-saved registers to values live across a
call in reverse order of declaration**, so swapping two local declarations can flip a whole function from
99.17% to 100% with no other change. `WriteSaveSlot` and `SelectSaveSlot` both showed it (see below).

## Per function, as the item asked

| function | before | after | note |
| --- | --- | --- | --- |
| `WriteSaveSlot__Q214CMemoryCardSys13CCardFileInfoFi` | 99.17% | **100.00%** | Prime 1 has no equivalent (its save layout is a single 8 KiB-aligned buffer, not two alternating slots), so this one is Echoes-only. Fixed by the declaration-order swap above. |
| `WriteIconData__Q214CMemoryCardSys13CCardFileInfoFR13COutputStream` | 95.12% | 95.12% | Prime 1's source compiles **unchanged** here (same `mIconToks.size()` loop, same `**mIconToks[i].mTex`, same `GetPaletteData()`); the remaining diff is a single instruction's position, not the code. |
| `SelectSaveSlot__Q214CMemoryCardSys13CCardFileInfoFv` | 93.05% | 93.05% | Echoes-only, no Prime 1 counterpart. Best measured this run: **98.44%** (not 100%). |
| `destroy_elements__Q24rstl57reserved_vector<...Icon,8>>Fv` | 92.50% | 92.50% | Emitted from `include/rstl/reserved_vector.hpp`, which every `reserved_vector` user shares. Not touched: the header note there records that a small change to it moves eight `Matching` units. |
| `WriteBannerData__Q214CMemoryCardSys13CCardFileInfoFR13COutputStream` | 72.97% | 72.97% | Prime 1's source compiles **unchanged** here. |

## What did not work (spelling -> measured score), so the next run skips them

`SelectSaveSlot` (all on top of the tree as committed, i.e. **93.05%** unless stated):

- `mSlots[0].CheckCrc(); mSlots[1].CheckCrc();` with the `slotA`/`slotB` references declared after the calls
  instead of before: **95.10%**. This is the one that fixes retail's habit of computing `&slotB` *after* the
  first `CheckCrc` call.
- ...and with `rstl::vector<uchar>& loaded = mLoadedData;` hoisted above the use, plus
  `mGeneration = slot == 0 ? 1 - slotA.mGeneration : slotB.mGeneration;` (polarity of the ternary matters -
  `slot != 0 ? ...` is worse) and the `result`/`slot` pair kept above the chain: **98.44%**, the best measured.
  That last one is four lines of churn for no extra matched function, so it is **not** in the diff.
- 120 permutations of the declaration order of `slotA, slotB, result, slot, loaded` with all five in one block
  after the `CheckCrc` calls: 88.70 - 92.15%, all worse than 98.44%. Declaration position is not free - the good
  shape is *refs right after the calls, `loaded` after the if-chain*.
- `const int size = mSlotSize - 8;` hoisted above `mSlot = 1 - slot;` 88.70%, inlined (no local) 96.39%,
  `mSlot = 1 - slot;` moved after the `memcpy` 93.86%, `slotA`/`slotB` declared after the chain 98.44%
  (same as the best), `if`/`else` instead of the ternary 95.15/95.26%, xor operands swapped 95.63%,
  `int slot = -1;` or `ECardResult result = kCR_READY;` moved above the `CheckCrc` calls 96.33/96.34%.

`WriteIconData` (base 95.12%): `for (int i = 0, n = mIconToks.size(); i < n; ++i)` (Prime 1's shape, no `count`
local) **90.61%**; `const int* icon = mIconToks.data()` with `icon[i]` **95.12%**; `const void* palette = 0;`
**95.12%**; `uint size` non-const **95.12%**; `int i = 0;` before the loop **95.12%**. Nothing moves the
`lwz r31, 0x64(r3)` (the `mCount` load) up to the second instruction, which is all that differs.

`WriteBannerData` (base 72.97%): `CAssetId bannerTex = mBannerTex;` local **72.97%**; `static_cast<uint>(mBannerTex)`
compare **72.97%**; `if (mBannerTex == kInvalidAssetId) return;` with a naked block **72.97%**; non-const `uint size`
**72.97%**; `format != kTF_RGB5A3 ? 3072 : 6144` **72.78%**. (`**mBannerTok` instead of `**mBannerTok.data()` does
not compile here - `TLockedToken` has no `operator*`; `**mBannerTok.data()` is the right spelling for this repo.)

## The remaining diffs, character by character

- `WriteBannerData` (72.97%) - **scheduling only, 37 instructions either way, identical after the `beq`**.
  Retail's entry block interleaves the prologue stores with the condition: `mflr / lwz r5,0x50(r3) / stw r0 /
  addis / stw r31 / cmplwi / stw r30 / stw r29 / mr r29,r4 / beq`. Ours emits all four stores, then `mr`, then the
  condition. Body identical (`GetConstBitMapData` -> `li r5,0xc00|0x1800` -> `DoPut` -> `li r5,0x200` -> `DoPut`).
- `WriteIconData` (95.12%) - **scheduling only**: retail `mr r25,r4 / lwz r31,0x64(r3) / addi r29,0x68 / li r28,0 /
  li r27,0 / b`; ours moves the `lwz` after the two `li`s. Loop body and registers otherwise identical.
- `destroy_elements<reserved_vector<Icon,8>>` (92.50%) - retail `addic. r3,r31,8` then re-tests it with
  `cmplwi r3,0`; ours recomputes `addic. r0,r31,8` for the second test. Both then `addi r3,r31,8; li r4,0;
  bl ~CToken`. Lives in a shared header.
- `SelectSaveSlot` (93.05%, 98.44% with the four-line variant above) - **register allocation only**, no
  instruction is added or dropped. Retail: `this=r26, slot=r27, &mLoadedData=r28, result=r29, slotB=r30,
  slotA=r31`. Ours (93.05%): `this=r28, slot=r27, result=r29, slotA=r31, slotB=r30`, no `&mLoadedData` temp
  because `this` is still live. The 98.44% variant gets `this=r26` and the temp right but shuffles the other four
  (`slot=r28, slotA=r29, result=r30, slotB=r31`).

WALL: WriteBannerData 72.97% - body is byte-identical; only the position of the four prologue `stw`s relative to the `mBannerTex` condition differs, and 5 source spellings did not move it.
WALL: WriteIconData 95.12% - only the `mCount` load's position in the preamble differs, and 5 spellings (including Prime 1's loop shape) did not move it.
WALL: SelectSaveSlot 98.44% - last diff is purely which callee-saved register each of the 5 live values gets; ~25 declaration/ternary spellings, none reaches the retail assignment.

No `NEW:` lines: nothing here is a blocker on new work, only spellings already measured above.
