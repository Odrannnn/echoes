# progress-prime1-canimtreetweenbase — 2026-09-30

Baseline `main/Kyoto/Animation/CAnimTreeTweenBase` was 10/20. Per-function measurements (before → after):

| Function | Score | Prime 1 transfer |
| --- | --- | --- |
| `CopyNodeMinusStartTime__12CBoolPOINodeFRC12CBoolPOINodeRC13CCharAnimTime` | 0.00% → 100.00% | Small adaptation: Echoes stores a name hash (`GetNameHash`) rather than Prime 1's string (`GetString`). The body was moved from `CAnimSourceReaderBase.cpp` to its retail unit; host-only definition remains under `TARGET_PC`. |
| `VGetOffset__18CAnimTreeTweenBaseCFRC6CSegId` | 68.96% → 100.00% | Prime 1 behavior adapted to local `CVector3f::Lerp`; using its unsuffixed `1.0` matched the retail double threshold. |
| `VGetRotation__18CAnimTreeTweenBaseCFRC6CSegId` | 19.81% → 100.00% | Small API adaptation to local `CQuaternion::SlerpLocal`, plus unsuffixed `1.0`. |
| `VGetSegStatementSet__18CAnimTreeTweenBaseCFRC10CSegIdListR16CSegStatementSetRC13CCharAnimTime` | 32.53% → 32.53% | Not adapted: Prime 1 uses `CStackSegStatementSet`/`GetData`, absent from Echoes headers; Echoes factors this through its optional-time `BlendSegStatementSet` helper. Needs local support/API reconstruction. |
| `VSimplified__18CAnimTreeTweenBaseFv` | 0.69% → 99.46% | Prime 1 logic adapted to local APIs. Remaining `lanediff` differences: boolean tests (`clrlwi.` vs `cmplwi`) and local `@stringBase0` vs retail `lbl_803AEE08` relocations. |

Verified: `./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS progress-prime1-canimtreetweenbase`; matched 10069 → 10072, linked 4918 unchanged, target 10 → 13 / 20, no regression, no asm. `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` → `no regression`. Declaration-order check: 1 unit checked, none out of retail order; symbol-name check: 504 units, 0 missing.

The unit remains `NonMatching`; no flip was attempted, as this is a progress item. No new queue item or `WALL:` filed.

# progress-prime1-canimtreetweenbase — 2026-09-30, second run (lane 6)

Re-measured first: the branch head already carried the first run's work, so the baseline was
**13/20**, not 10/20. `build/report.json` on the clean tree of `wt-mp2-goal-L6` reported
`main/Kyoto/Animation/CAnimTreeTweenBase: 61.64% fuzzy, 33.78% matched (15 / 20)` after this run's
change; before it, `59.95% fuzzy, 31.27% matched (13 / 20)`.

## What landed: 13 → 15 / 20

Both timed `VGet*` wrappers sat at **32.53%** and are now **100.00%**:

| Function | Before | After |
| --- | --- | --- |
| `VGetSegData__18CAnimTreeTweenBaseCFRC15CCharLayoutInfoR24CJointData_LinearStorageRC13CCharAnimTime` | 32.53% (0x44 = 68 bytes) | 100.00% (0x3c = 60 bytes) |
| `VGetSegStatementSet__18CAnimTreeTweenBaseCFRC10CSegIdListR16CSegStatementSetRC13CCharAnimTime` | 32.53% (0x44 = 68 bytes) | 100.00% (0x3c = 60 bytes) |

**Cause, measured.** `BlendSegData` / `BlendSegStatementSet` take their optional time **by value**,
so each wrapper materialises an `rstl::optional_object< CCharAnimTime >` temporary in its frame.
Retail (0x802AB094 / 0x802AB0D8):

```
stwu r1,-32(r1) / mflr r0 / lfs f0,0(r6) / li r7,1 / stw r0,36(r1) / lwz r0,4(r6)
addi r6,r1,8 / stb r7,16(r1) / stfs f0,8(r1) / stw r0,12(r1) / bl BlendSegStatementSet
```

With the generic `rstl::construct_impl` the temporary's copy goes through placement new and MWCC
guards it, and addresses the stores through a register:

```
addic. r7,r1,8 / stb r0,16(r1) / beq skip / lfs f0,0(r6) / lwz r0,4(r6)
stfs f0,0(r7) / stw r0,4(r7) / addi r6,r1,8 / bl ...
```

**Fix:** `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CCharAnimTime)` in `namespace rstl` at the top of
`src/Kyoto/Animation/CAnimTreeTweenBase.cpp` (with a comment saying why). `CCharAnimTime` is
`{ float, EType }` with no user-declared special members, so the specialised `construct_impl` is the
same copy without the guard. It is deliberately **TU-local**: putting it in
`include/Kyoto/Animation/CCharAnimTime.hpp` (the convention for the other types — `CVector3f.hpp`,
`CSegId.hpp`, …) touches a header almost every unit includes and would resize unrelated `.text`
sections. Measured: no other unit moved (report diff `no regression`, `All:` unchanged apart from
the +2).

Note the guard `addic./beq` **is** retail's shape when the type is non-trivial — e.g. the
`optional_object< rstl::rc_ptr<...> >` copy in `ComputeSequenceFundamentals__15CSequenceHelperCFv`
(0x80299848, `ComputeSequenceFundamentals` has base 0x802993A0) has it, and that unit is `Matching`. So "placement new is always wrong" is not the rule;
"this particular 8-byte POD is copied directly" is.

## Verified

```
./tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 10392 -> 10394   linked 5048 -> 5048
  ok  check_symbol_names.py
  ok  target rose: main/Kyoto/Animation/CAnimTreeTweenBase: 13 -> 15 / 20 functions
  ok  no asm added
goal_check: PASS progress-prime1-canimtreetweenbase
```

`python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimTreeTweenBase` → `1 unit(s) checked,
none emits its functions out of retail order`. `tools/unit_fit.sh Kyoto/Animation/CAnimTreeTweenBase.cpp`
→ `.text` short by 804, `.data` over by 24, `.sbss` short by 8, 12 COMDAT/inline extra functions.

## Still unmatched (5) — what this run decoded, so the next one need not

No spellings were tried for these this run, so there is no `WALL:` for them. What follows is all
measured from `build/G2ME01/obj/Kyoto/Animation/CAnimTreeTweenBase.o`.

### Retail `.text` order (0x802AA750 base) — constrains declaration order

```
000 VGetWeightedReaders        0a0 VReverseSimplified       0cc VSimplified
600 ShouldCullTree              61c GetBlendingWeight        648 VGetRightChildWeight
668 VGetSegData(2)              694 VGetSegData(time)        6d0 BlendSegData
934 fn_802AB084 (0x54)          988 VGetSegStatementSet(time) 9c4 VGetSegStatementSet(2)
9f0 BlendSegStatementSet        e10 fn_802AB560 (0x54)       e64 VGetRotation
f7c VGetOffset                 10b4 VHasOffset              1138 __dt   1198 __ct
1218 CopyNodeMinusStartTime
```

mwcc emits in reverse source order, so **`fn_802AB084` must be declared immediately before
`BlendSegData` and `fn_802AB560` immediately before `BlendSegStatementSet`**. The current file
order already matches retail for the 17 named functions (that is why they pair up), so do not
reorder anything else.

### The two unnamed helpers are free functions (this was not obvious)

`fn_802AB084` / `fn_802AB560` are neither `CAnimTreeTweenBase` members nor virtuals: `r3` is a
`const rstl::ncrc_ptr< CAnimTreeNode >&` (`lwz r3,0(r3)` then `lwz r12,0(r3)` = vtable), and `r6` is
the `rstl::optional_object< CCharAnimTime >` (`lbz r0,8(r6)` = `m_valid`; `CCharAnimTime` is 8 bytes
so the flag is at +8). Slot decode, from `__vt__17CAnimTreeSequence`'s `.data` relocations
(slot 0 = `__dt__` at +0x08, so slot *i* is at vtable+8+4*i):

| helper | `m_valid` | slot 16 @0x48 | slot 15 @0x44 | slot 17 @0x4c | slot 18 @0x50 |
| --- | --- | --- | --- | --- | --- |
| `fn_802AB560` (0xe10) | != 0 | `VGetSegStatementSet(…,time)` | `VGetSegStatementSet(…)` | | |
| `fn_802AB084` (0x934) | != 0 | | | `VGetSegData(…,time)` | `VGetSegData(…)` |

So the shape is one static per family, e.g.

```cpp
static void GetSegData(const rstl::ncrc_ptr< CAnimTreeNode >& child, const CCharLayoutInfo& layout,
                       CJointData_LinearStorage& data,
                       const rstl::optional_object< CCharAnimTime >& time) {
  if (time.valid()) { child->VGetSegData(layout, data, *time); }
  else              { child->VGetSegData(layout, data); }
}
```

Retail outlines them because each has three call sites inside its `Blend*` body.

### `BlendSegData` (612 bytes) decoded

Two function-local statics (a counter and its init guard) in `.sbss`: counter `0x804198CC`, guard
`0x804198D0` (`BlendSegStatementSet` has the other pair, counter `0x804198C4`, guard `0x804198C8`;
`0x804198C0` is `sAdvancementDepth`). mwcc's static-init sequence is `lbz guard / extsb. / bne /
li r3,0 / lwz sym / stw sym / li r0,1 / stb guard`. Then:

1. `weight = GetBlendingWeight()`; `++counter`; `if (weight >= 1.0)` (`lbl_8041E3B0`) →
   `GetSegData(&mB /*+0x1c*/, layout, data, time)`;
2. `else if (counter > 3)` (`cmpwi r0,3 / ble`) → `child = (weight > 0.5f /*lbl_8041E3B8*/) ? mB : mA`,
   `rstl::rc_ptr< CAnimTreeNode > best = child->GetBestUnblendedChild();`, and the `!best` arm is
   `best = child` inlined as `cmpw best.ptr, child.ptr` + skip when equal, then `ReleaseData(&best)`
   and `++*child->mRefCount` — i.e. `rstl::rc_ptr`'s copy from an `ncrc_ptr`; then
   `GetSegData(best, …)` and `ReleaseData(&best)`;
3. `else` → `GetSegData(&mA, …)`, then three externs: `fn_802B2E34(&tmp, layout[0], 0)` (builds a
   44-byte `CJointData` at `r1+72`; it calls `fn_802B2ED4` and sets `+4/+8/+12/+16/+20/+24/+28`),
   then `if (data[12] & 0x80) tmp[12] |= 0x80`, then
   `fn_802B2AB4(data, &tmp, weight)`, then `fn_802B2DC0(&tmp, -1)`.
4. `--counter` on every exit.

Class layout measured from `VHasOffset` and the disassembly: `0x00` vptr, `0x04` `mName` (16),
`0x14` `mA`, `0x1c` `mB` (8-byte `ncrc_ptr` = `{ptr, refcount*}`), `0x24` `mFlags`,
`0x28` bitfield word; `CHECK_SIZEOF(CAnimTreeTweenBase, 0x2c)`.

### `BlendSegStatementSet` (1056 bytes) decoded

Same three branches, with `CStackSegStatementSet setA` at `r1+104` and `setB` at `r1+92`
(`fn_802B1E80` is the constructor; each object is `{u32 size; CSegStatement* data;}` with inline
storage after it). Then the loop over `list` (`mStart` at `+4`, `mEnd` at `+12`):

```c
f31 = 1.0 /*lbl_8041E3A8*/ - weight;      // used as fmadds weight on setB + (1-w)*setA
id  = *it;  off = id * 44;                 // sizeof(CSegStatement) == 44
fn_802A05EC(&tmp /*r1+28*/, &setA[id], &setB[id], off, weight);
// copy tmp orientation into setOut[id], then setOut[id][0x28] |= 0x80
// if setA[id][0x28] & 0x80 && setB[id][0x28] & 0x80 → lerp offsets with Slerp/Lerp
```

`CSegStatement` is 44 bytes: orientation `0x00-0x0F`, offset `0x10-0x1B`, flag byte `0x28`
(bit 7 = "present").

### The externals live in unclaimed carve units, so they need `extern "C"`

`fn_802B2AB4`, `fn_802B2DC0`, `fn_802B2E34`, `fn_802B286C`, `fn_802B2CB4`, `fn_802B2CE0`,
`fn_802B2D70`, `fn_802B2ED4`, `ResetScales__24CJointData_LinearStorageFv` →
`build/G2ME01/obj/auto_03_802B286C_text.o`; `fn_802B1E80` → `auto_03_802B1DC0_text.o`;
`fn_802A05EC` → `auto_03_802A042C_text.o`. They are unmangled, so a plain C++ declaration would
mangle; the repo's precedent is `extern "C" void fn_800E4E9C(...)` in
`src/MetroidPrime/CModelDataModelSlots.cpp` and `PortBoot.cpp`. Their bodies are still raw bytes, so
their signatures have to be read off the carve before writing anything.

### `VSimplified` (99.46%) is blocked by a dtk split, not by source

The two remaining diffs are `cmplwi rX,0` vs `clrlwi. r0,rX,24` and
`R_PPC_ADDR16_HA lbl_803AEE08` vs `R_PPC_ADDR16_HA @stringBase0`. `lbl_803AEE08` is **not** in this
unit's claimed ranges: it lives in the unclaimed rodata carve
`build/G2ME01/obj/auto_06_803AEDF0_rodata.o` and holds the bytes `3f 3f 28 3f 3f 29 00` = `"??(?)"`,
which is exactly what our object's own `.rodata` already holds. It is mwcc's `-str pool` type-name
string for the `rstl::rc_ptr` refcount allocation (`__nw__FUlPCcPCc(4, "??(?)", 0)` then
`*(int*)p = 1`); retail's linker kept the first copy and dropped ours, so the relocation points
elsewhere. No source spelling can change that — treat 99.46% as the ceiling for this function.

### Flip blockers recorded by `tools/unit_fit.sh` (for whoever takes it to `Matching`)

- `.data` over by 24: the first run's move of `CBoolPOINode::CopyNodeMinusStartTime` into this TU
  makes mwcc emit `__vt__8CPOINode` and `__vt__12CBoolPOINode` here as well; retail's `.data` is
  136 bytes and holds `__vt__18CAnimTreeTweenBase` alone.
- `.sbss` short by 8: retail's two function-local static pairs (above) are still missing.
- `.text` short by 804: `BlendSegData` + `BlendSegStatementSet` + the two outlined helpers.

## Not filed

No `WALL:` — nothing here sat at a fixed score across spellings tried this run; the five functions
above were decoded, not attempted. No `NEW:` — what remains is this item's own target, so requeue
`progress-prime1-canimtreetweenbase` rather than opening a new one.
