# progress-fn-names-cdecalmanager

`kind: progress`, target `MetroidPrime/CDecalManager`. **Result: 8 -> 9 matched functions
of 65**, and `./tools/goal_check.sh build/goal/item.json` printed
`PASS progress-fn-names-cdecalmanager`.

## What I did

One function reached 100%: `CDecalManager::AddToRenderer(const CStateManager&)`, which sat at
**99.29%** (1 of 252 bytes' worth of instructions differing) and is now an exact match.

The diff is three lines in `src/MetroidPrime/CDecalManager.cpp:235-247`. The cause was the
loop's register allocation, not the loop:

```cpp
// was: 15 differing instructions
const rstl::reserved_vector< int, 64 >::const_iterator end = mActiveIndexList.end();
for (rstl::reserved_vector< int, 64 >::const_iterator it = mActiveIndexList.begin(); it != end;
     ++it) {

// now: only relocation fields differ
for (const int* it = mActiveIndexList.begin(), *end = mActiveIndexList.end(); it != end; ++it) {
```

Retail puts the loop bound in **r29** and the decal pointer in **r31**
(`add r29,r4,r0` / `add r31,r30,r0` at +50 and +70 of the function). Ours put the bound in
r29 and the decal in r31 too - but the *order in which the two are introduced* differs.
Declaring `it` and `end` together in the `for`'s init-list, rather than `end` first as a
separate statement, makes MWCC introduce the bound second and allocate retail's registers.
`rstl::reserved_vector< int, 64 >::const_iterator` is already `const int*`, so this is the
same type written out; nothing is inlined or dropped.

`bytescmp.py` on the recompiled object shows the only remaining differences are the four
`lis`/`addi` pairs for the two statics, the two `bl` targets, and the `lwz gpRender@sda21` -
i.e. relocated fields, which objdiff ignores. That is what 100% means here.

### Measured, from `build/report.json`

| | before | after |
|---|---|---|
| `main/MetroidPrime/CDecalManager` matched_functions | 8 / 65 | **9 / 65** |
| All matched_functions | 11953 | **11954** |
| DOL units matched | 10405 | 10406 |

`linked` stayed 5728 -> 5728: the unit is still `NonMatching` (63 of our object's functions
are not in the retail unit object, and `.text` is 9104 bytes against a claimed 8096), so this
is progress on `report.json`'s per-function measurement, not a unit result. Nothing regressed.

## What is still in this unit

56 functions remain at 0% and are all `fn_800E6F14` ... `fn_800E8C3C` - unnamed in retail, and
`unit_fit.sh` reports 63 functions in our object that the retail unit object does not define
(mostly COMDAT weak copies of `CDecal`'s and `rstl::vector`'s inline constructors/destructors).
So the unit cannot flip until either those are traced or the split is carved. The three named
functions left in the 65:

| function | measured | note |
|---|---|---|
| `AddToRenderer` | **100.00%** | this run |
| `AddDecal` | 83.59% (756 B) | see below |
| `GatherWorldSurfaces` | 69.21% (1020 B) | not attempted; the largest single body here |

## AddDecal: what I tried, and the wall

`AddDecal` is 756 bytes and 177 of our 177 instructions differ, so it is a **shape** problem,
not register allocation. I did not reach a wall on a spelling - I ran out of runway on a
structural rewrite. Spellings measured with
`python3 tools/bytescmp.py build/G2ME01/src/MetroidPrime/CDecalManager.o AddDecal 800E6F88 756`
(ours vs retail 756 bytes; baseline is 177/177 differing, 708 bytes ours):

| change | ours | differing |
|---|---|---|
| baseline (unmodified) | 708 B | 177 / 177 |
| hoist `const CVector3f pos = xf.GetTranslation();` above the `surfaces` vector | 800 B | 115 / 200 |
| same, copy placed **inside** the `{ }` before the `TToken` copy | 800 B | 115 / 200 |
| same, non-`const` copy | 800 B | 115 / 200 |
| `const CVector3f& pos = xf.GetTranslation();` (reference, not copy) | 776 B | 171 / 194 |
| hoist the `TToken` copy and the two `GetValue` calls out of the `{ }` | no `AddDecal` symbol emitted | - |

**The copy is what makes the frame 432 bytes and emits the `f29`/`f30`/`f31` save/restore pair
retail has** - retail loads `lfs f29,0x2c(r27)` / `f30,0x1c(r27)` / `f31,0xc(r27)` (the three
components of `xf.GetTranslation()`) into callee-saved FPRs at +0x100..+0x110 and uses them at
+0x190, so the value must live across the two `GetValue` virtual calls. Every version that
keeps it in registers is 800 bytes, 44 over retail; every version that reloads it is 776-776
and has no FPR saves at all. So retail's is a *by-value* `CVector3f` local, and the 44 extra
bytes are something else I did not find - most likely a `CTransform4f` accessor that returns a
reference where retail's returns a copy, or an extra local in retail's `SDecal&` handling.

Two further concrete differences, both near the tail and both unexplained:

1. Retail's final `push_back` is **inlined into the loop's own frame** and ends with
   `bl fn_800E7D18` (a 4-argument function taking `r3` and `r4`, freeing `*(r3+0xc)` then
   `r3` itself - `rstl`'s block free, or `reserved_vector::operator=`). Ours inlines the same
   `mActiveIndexList` store but then runs a **hand-unrolled `CCollisionSurface` destructor
   loop** (`addi r7,r7,0x30` / `cmplw` / `bne` at +0x2CC) that retail does not have, and calls
   `Free__7CMemoryFPCv` instead. Retail's `fn_800E7D18` is 84 bytes and is one of the 0%
   `fn_*` in the unit, so the destructor's real shape is unknown.
2. Retail stores `mLastDecalCreatedIndex` (r30) and `mLastDecalCreatedAssetId` (r31) from the
   *pool index* and the *token's tag id* held in callee-saved GPRs across the whole function;
   ours keeps the same two values but in r31/r30 the other way round, and reloads
   `mFreeIndex` at +0x224 where retail keeps it in r30 from +0x0.

So: the body is right, the **frame is 16 bytes too big** and the tail's cleanup is
**structurally different**. Both are real work, not a spelling.

NEW: progress-fn-names-cdecalmanager-adddecal | progress | MetroidPrime/CDecalManager | AddDecal is 83.59% with all 177 instructions differing; keeping xf.GetTranslation() in callee-saved FPRs (as a by-value CVector3f) makes the frame 432 vs retail's 416, and the tail inlines a CCollisionSurface destructor loop where retail calls one out-of-line fn_800E7D18.

## Verifying

```
$ export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-fn-names-cdecalmanager (progress) target=MetroidPrime/CDecalManager
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11953 -> 11954   linked 5728 -> 5728
  ok    check_symbol_names.py
  ok    All:  33.77% fuzzy, 26.97% matched, 12.64% linked (11954 / 28465 functions)
  ok    target rose: main/MetroidPrime/CDecalManager: 8 -> 9 / 65 functions
  ok    no asm added
goal_check: PASS progress-fn-names-cdecalmanager
```

`docs/HANDOFF.md`'s state block was rewritten by `gate.sh` with the new counts
(11954 / 10406); I reverted it, because the judge rewrites those numbers from the tree itself
and the driver discards edits to that file. No commit made.
