# progress-prime1-ccollisionactormanager

`kind: progress`, target `MetroidPrime/CCollisionActorManager`. Re-measured first: the clean tree was
**22 / 30 functions matched**, `.text` 65.854% fuzzy. It is **23 / 30** now (`.text` 67.86% fuzzy).
`tools/goal_check.sh build/goal/item.json` -> **PASS** (matched 10354 -> 10355, target 22 -> 23, no asm).

## What I changed (`src/MetroidPrime/CCollisionActorManager.cpp` only, 4 hunks)

**1. `SetPhysicsActive`: pass the member, not the parameter - 73.617% -> 100% (a matched function).**

`actor->SetMovable(active)` / `actor->SetUseInSortedLists(active)` -> `... (mPhysicsActive)`.
Retail re-reads `this->mPhysicsActive` twice inside the loop
(`lbz r4,0x14(r28)` before the `rlwimi` and again before `bl SetUseInSortedLists`), which only happens
if the source reads the member. With the parameter the value stays in a register, the function needs a
5th callee-saved register, and mwcc emits `stmw r27,28(r1)` where retail emits four `stw`s.
After the edit our instruction sequence is byte-identical to retail's. Prime 1 has the same shape
(`SetMovable(mMovable)`), it just spells the member `mMovable`.

**2-4. `Update`: 84.02% -> 91.50%** (still short of 100%, see below).

| change | evidence in retail |
| --- | --- |
| `const CTransform4f& worldXf` -> `const CTransform4f worldXf` (copy) | an extra `bl __ct__12CTransform4fFRC12CTransform4f` with `addi r4,r31,36` before `bl Scale`; Prime 1 has `const CTransform4f worldXf = act->GetTransform();` |
| `MoveToOR(actor->GetTransform().TransposeMultiply(origin), dt)` -> named local `movement` | retail copies the `TransposeRotate` result `r1+112..120` to `r1+228..236` before `bl MoveToOR`; Prime 1 has the same named local |
| `pivotXf.Rotate(desc.GetPivotPoint())` -> `pivotXf.BuildMatrix3f() * desc.GetPivotPoint()` | retail calls `BuildMatrix3f__12CTransform4fCFv` then `__ml__9CMatrix3fCFRC9CVector3f`; `Rotate` is 104 bytes of inline `ps` asm and **cannot** be inlined. `CTransform4f::Rotate` is 100% matched in `main/Kyoto/Math/CTransform4f`, so retail's `Rotate` *is* `BuildMatrix3f() * v` - the caller just does not go through it. Behaviour identical. |
| product `worldXf * locatorXf * CTransform4f(...)` -> named local `xf` | retail emits a third `__ct__` copy of the product before `bl SetTransform` |

Interim scores for `Update`, all measured with `tools/fast_try.sh`: base 84.021 ->
worldXf-by-value + `movement` 89.637 -> `+ BuildMatrix3f()*` 90.586 -> `+ xf local` 91.503.
Two things I tried and **reverted** because they scored worse: a named local for
`CTransform4f(desc.GetOrientation(), desc.GetPivotPoint())` (91.440, though it does produce retail's
36th call), and `if (actor != nullptr) { ... }` instead of `if (actor == nullptr) continue;`
(89.637, no change).

## Per function, before -> after (`build/report.json`, objdiff fuzzy)

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `SetPhysicsActive__22CCollisionActorManagerFR13CStateManagerb` | 73.617 | **100.0** | `SetMovable` -> `mPhysicsActive`; adapted unchanged |
| `Update__22CCollisionActorManagerFfR13CStateManagerQ222CCollisionActorManager14EUpdateOptions` | 84.021 | 91.503 | 2 of 4 changes straight from Prime 1; the other 2 are Echoes-only |
| `SetActive__22CCollisionActorManagerFR13CStateManagerb` | 94.231 | 94.231 | Prime 1's nesting tried, no effect |
| `GetCollisionDescIndexFromUniqueId__22CCollisionActorManagerCF9TUniqueId` | 75.789 | 75.789 | no such function in Prime 1 |
| `__ct__22CCollisionActorManagerFR13CStateManager9TUniqueId7TAreaIdRCQ24rstl63vector<...>b` | 30.663 | 30.663 | Prime 1's is a fork, ~40% smaller; not attacked |
| `fn_801358B4` / `fn_8013639C` / `fn_801363D4` | unpaired | unpaired | - |

Unit `.text`: 7336 bytes -> 7420 (retail 7480).

## Two things that are not reachable from this unit's source

**The three `fn_` functions cannot be matched at all.** They are unpaired in objdiff's output
(`match_percent: null`, no base symbol), because retail's split starts at 0x801358B4 and those three
are partial functions: `fn_801358B4` is the 12-byte `lwz r0,kInvalidAreaId; stw r0,<this unit's .sbss>;
blr` that writes the unit's own `.sbss` word from `kInvalidAreaId`, and `fn_8013639C` / `fn_801363D4`
are the tails of `CCollisionActor::GetPrimitiveTransform` and `CCollisionActor::GetCollisionPrimitive`,
which live in `CCollisionActor.o`. objdiff pairs by symbol name and has nothing to pair them with, so
`total_functions` for this unit is 30 but only 27 are ever reachable. Do not spend a run on them.

**Our object emits three functions retail does not** (pre-existing, not from my change -
`tools/unit_fit.sh` reports them): `destroy<pointer_iterator<...>>` 100 B, `__dt__26CJointCollisionDescriptionFv`
92 B, `__dt__rstl::basic_string` 80 B. Retail inlines them. Also `.ctors` 4 B and `.sbss` 8 B exist in
retail's object and not in ours, and we carry a 40 B `.sdata2` that retail does not. Until the vector
destructor is spelled the way retail spells it, `flip_test` cannot pass this unit even with all 27
reachable functions matched.

## What is left, per function

**`SetActive` (94.231%, 208 B) - register allocation only.** The instruction sequence, the branch
targets and the size are already identical to retail; only the callee-saved assignment differs.
Retail: `mr r30,r5` (raw `active`) / `mr r28,r3` (this) / `clrlwi r26,r5,24` (normalised) /
`mr r29,r4` (mgr) / `li r31,0` (i) / `li r27,0` (byte offset). Ours: `mr r28,r5` / `mr r26,r3` /
`mr r27,r4` / `clrlwi r30,r5,24` / `li r29,0` / `li r31,0`. Both save r26-r31 with one `stmw r26,24(r1)`.
16 spellings tried, list below.

**`GetCollisionDescIndexFromUniqueId` (75.789%, 76 B) - register allocation + one hoisted load.**
Retail hoists `lwz r5,12(r3)` (the vector base) above the loop and uses six registers
(r3 this, r4 id, r5 base, r6 offset, r7 count, r8 i); ours reloads the base inside the loop and reuses
r5 for both count and base. Same instruction count, same size. 15 spellings tried, list below.

**`Update` (91.503%) - the call sequence now matches retail's 35 calls exactly** except retail has one
extra `__ct__` copy after `__ct__12CTransform4fFRC9CMatrix3fRC9CVector3f`; ours is 1260 bytes to
retail's 1344, and the residue is spread as register/stack-layout differences (retail keeps `origin`
materialised at `r1+240` in one path and re-derives it from `pivotXf`'s m03/m13/m23 in another, we
always materialise it).

## Spellings tried (all measured this run)

`SetActive` - all stayed at 94.231 unless noted: nested `if (entity != nullptr) { if (...) {...} }`,
`if (active != false)`, `static_cast<CActor*>` instead of `CEntity*`, `for (uint i = ...)`, a local
`const bool act = active`, `if (entity == nullptr || ... ) continue;`, `SetActive` outside the inner
`if`, `for (int i = 0, n = size(); ...)`, a `desc` reference local, `active != entity->GetActive()`,
`this->mActive = active`, `if (active == true)`, `CEntity* const entity`, braces around the
`Update(0.f, ...)` call. Worse: `for (uint i...)` 93.327, `for (int i = 0, n = ...)` 90.154,
`if (active == true)` 92.115, named `const bool act` 91.692.

`GetCollisionDescIndexFromUniqueId`: comparison swapped 75.263; `const CJointCollisionDescription*
desc = mJointDescriptions.data()` 83.947; same plus a cached `int n` 84.211 (**not kept** - it changes
the loop into a single walking pointer, 68 B, which is structurally further from retail than the
original 76 B version); `&mJointDescriptions[0]` 83.947; `desc` reference local 75.789;
`for (uint i...)` 67.526; cached size + indexed 75.789; `rstl::vector&` alias 75.789;
`for (int i = 0, n = ...)` 75.789; `while` form 75.789; explicit byte-offset counter 75.789;
`id == desc.GetCollisionActorId()` 75.263; `const TUniqueId`/`const uint` variants 67.526;
`desc`/`end` pointer-walk 41.526.

## New facts worth keeping

- **`SetPhysicsActive`-style members, not parameters, are the recurring Echoes idiom here.** Retail
  re-reads the member from memory instead of keeping the argument live; that removes a whole
  callee-saved register. Worth trying on every other function of this unit that forwards a `bool`.
- **`CTransform4f::Rotate` is inline `ps` asm and can never be inlined** (104 B, 100% matched in
  `main/Kyoto/Math/CTransform4f`). Any retail code that needs `matrix * vector` and shows
  `BuildMatrix3f` + `__ml__9CMatrix3fCFRC9CVector3f` is written as `x.BuildMatrix3f() * v`, *not* as
  `x.Rotate(v)`. This is the general rule; it may be wrong elsewhere in the tree, so check the call
  sequence before assuming it.
- **Stack offsets are a usable signal for source shape.** Retail stores `CTransform4f` at `r1+1104`
  and reads `1116 / 1132 / 1148` for its translation - that stride-16 pattern is a 3x4 `CMatrix3f`'s
  4th column, i.e. `GetTranslation()` = `m03, m13, m23`. A plain `CVector3f` copy is stride 4
  (`76, 80, 84`). Seeing stride 16 means the compiler re-derived a vector from a transform temp
  instead of copying it.

WALL: SetActive__22CCollisionActorManagerFR13CStateManagerb 94.231% - identical instruction sequence
and size to retail, only the callee-saved register assignment differs, and 16 spellings did not move
it.

WALL: GetCollisionDescIndexFromUniqueId__22CCollisionActorManagerCF9TUniqueId 75.789% - retail hoists
the vector base out of the loop and uses six registers; 15 spellings could not make mwcc hoist it.

No `NEW:` line: everything above is either this item's own functions or a general codegen rule.