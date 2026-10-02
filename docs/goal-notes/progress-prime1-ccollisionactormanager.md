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
---

# Second run (lane 2, 2026-10-02) - `Update` reached 100%, unit 23 -> 24 / 30

**Result: `Update__22CCollisionActorManagerFfR13CStateManagerQ222CCollisionActorManager14EUpdateOptions`
91.503% -> 100.0% (a matched function). Unit 23 -> 24 / 30; `.text` 67.861% -> 69.39% fuzzy,
`matched_code` 35.61% -> 53.58%. `tools/goal_check.sh build/goal/item.json` -> **PASS**
(matched 12387 -> 12388, target 23 -> 24, no asm).** Diff is `src/MetroidPrime/CCollisionActorManager.cpp`
only, 4 hunks, all inside `Update`.

## Re-measured first

Clean tree at `60b3f97e` (the first run's commit `c5e85ec1` **is** an ancestor of this lane, so
`SetPhysicsActive` was already matched): **23 / 30**, `.text` 7480 B retail vs 7420 B ours,
67.861% fuzzy. Not `STALE:` - `Update` was still at 91.503%.

## What moved `Update` from 91.503 to 100

Every change is in `src/MetroidPrime/CCollisionActorManager.cpp`, `CCollisionActorManager::Update`.
Interim scores, all measured with `tools/fast_try.sh MetroidPrime/CCollisionActorManager`:

| # | change | after |
| --- | --- | --- |
| base | (state this lane started from) | 91.503 |
| a | OBB-else `LookAt(origin, ...)` -> `LookAt(pivotXf.GetTranslation(), ...)` | **93.383** |
| d | sphere `SetTranslation(origin + ...)` -> `SetTranslation(pivotXf.GetTranslation() + ...)` | **94.938** |
| e | final `else actor->SetTranslation(origin)` -> `SetTranslation(pivotXf.GetTranslation())` | **96.988** |
| f | named local for the orientation transform in the MayaPlugIn branch | **97.955** |
| g | `const float maxSep = desc.GetMaxSeparation();` in the SphereSubdivide-else branch | **100.0** |

**a, d, e - "write the expression, not the local".** Retail does not read the `origin` CVector3f
local in three of the four places ours does; it re-derives the vector straight out of `pivotXf`.
The proof is in the stack offsets, not the source: retail reads `0x45c / 0x46c / 0x47c` - **stride
16**, i.e. `pivotXf.m03 / m13 / m23` with `pivotXf` at `r1+0x450` - while our `origin` local sits at
`r1+0xcc` in three **stride-4** words. A `CVector3f` local is always stride 4, so a stride-16 read
can only be a re-read of the transform temp. Ours still keeps the local for the two paths that
genuinely need it (the `kUO_ObjectSpace` branch and the `+=` accumulation), and retail keeps it too
at `r1+0xf0`; the difference is only which of the two representations each use site picks.

**f - the orientation transform needs a home.** Retail emits, at `r1+0x390`, a
`__ct__12CTransform4fFRC12CTransform4f` copy of the `CTransform4f(desc.GetOrientation(),
desc.GetPivotPoint())` temporary *between* the `__ct__12CTransform4fFRC9CMatrix3fRC9CVector3f` call
and the first `__ml__12CTransform4fCFRC12CTransform4f`. Ours keeps that temporary in `r23`. One
named local (`orientXf`) makes mwcc spill it, and that is also where the missing 48 bytes of stack
frame came from - after (f) our frame went `0x4f0 -> 0x520` and `stmw r23, 0x4ec(r1)` matched retail
exactly. The previous run tried the same trick in the *constructor* and it scored worse there; it is
right in `Update`.

**g - one named `const float` was worth 2.05%.** This is the whole remaining gap, and it is pure
register allocation. Retail keeps `desc.GetMaxSeparation()` in **`f30`** across the `LookAt` call, so
it (i) saves and restores `f30` (`stfd`/`psq_st`/`psq_l`/`lfd`, 4 instructions), (ii) needs a
`0x530` frame instead of `0x520`, and (iii) loads `lfs f30, 0x28(r23)` *before* setting up the three
`LookAt` arguments instead of after the call. Written inline
(`origin += desc.GetMaxSeparation() * LookAt(...).GetColumn(kDY)`) mwcc schedules the load *after*
the call into a caller-saved register and never needs `f30`. Giving it a name makes mwcc hoist the
load over the call, which forces the callee-saved register. After (g) our frame is `0x530`, every
stack offset and branch target in the function matches, and the function is byte-identical.

**Why Prime 1's source did not help here.** Prime 1 has no `kCT_OBBFromMayaPlugIn`, no
`kUO_ObjectSpace` option and spells the members `mMovable` / `GetModelScale()`. The last three
changes are Echoes-only shapes that Prime 1's `Update` cannot express. Of Prime 1's shapes only
`const CTransform4f worldXf = act->GetTransform();` (already applied by the first run) survived.
The 91.503 -> 100.0 work is **all Echoes-specific and is not in Prime 1**.

## What I tried on the constructor this run and reverted (all measured, all worse)

The constructor is still 30.7% and I attacked it for ~20 minutes before reverting; these are the
numbers so the next run does not repeat them.

| change | ctor |
| --- | --- |
| baseline (this lane's clean tree) | **30.71** |
| `const CVector3f scale = actor->GetModelData()->GetScale();` (by value) | 30.71 (neutral) |
| `const CTransform4f worldXf = actor->GetTransform();` (by value) | 30.73 (noise, +0.02) |
| both of the above together | 30.44 |
| + `pivotXf.Rotate(p)` -> `pivotXf.BuildMatrix3f() * p` | 30.34 |
| + `mgr.AddObject(*colActor)` / `(*newActor)` (retail calls the `CEntity&` overload) | 30.34 (neutral) |

**Do not bother with `BuildMatrix3f` in the constructor.** Retail's ctor really does call
`BuildMatrix3f__12CTransform4fCFv` + `__ml__9CMatrix3fCFRC9CVector3f` (`R60`,`R61`) where ours calls
`Rotate__12CTransform4fCFRC9CVector3f` (`O19`), so the substitution is semantically right, but it
costs 0.4% - the same substitution was worth +1.9% in `Update`. **The `Rotate` -> `BuildMatrix3f() *`
rule is path-dependent; apply it only where the call sequence already matches.**

The call sequences otherwise line up one-for-one, and the ctor's real gap is elsewhere: **we emit
~17 out-of-line `internal_dereference__Q24rstl66basic_string<...>Fv` calls that retail never makes**
(rsp. `O30`,`O34`,`O44`,`O52`,`O54`,`O58`,`O67`,`O71`,`O75`,`O87`), i.e. retail inlines the
`rstl::string` temporary's destruction in the `SphereCollision` / `SphereSubdivideCollision` returns
and we call out of line. That is the same defect as the `__dt__26CJointCollisionDescriptionFv` and
`__dt__rstl::basic_string` (80 B) symbols the first run listed as "emitted but not in retail", and it
plausibly accounts for most of the ctor's 69% gap. **Spelling `rstl::basic_string`'s destructor so
mwcc inlines it is the next thing to try, and it is what keeps the ctor from flipping.**

## Per function, before -> after (`build/report.json`, objdiff fuzzy)

| function | before | after | notes |
| --- | --- | --- | --- |
| `Update__22...FfR13CStateManagerQ222CCollisionActorManager14EUpdateOptions` | 91.503 | **100.0** | 5 hunks, all Echoes-only; Prime 1's source did not help |
| `SetPhysicsActive__22CCollisionActorManagerFR13CStateManagerb` | 100.0 | 100.0 | landed by the first run (`c5e85ec1`) |
| `__ct__22CCollisionActorManagerFR13CStateManager9TUniqueId7TAreaIdRCQ24rstl63vector<...>b` | 30.71 | 30.71 | 6 variants tried this run, see table; all reverted |
| `SetActive__22CCollisionActorManagerFR13CStateManagerb` | 94.231 | 94.231 | not touched this run; re-read the diff, still register-only |
| `GetCollisionDescIndexFromUniqueId__22CCollisionActorManagerCF9TUniqueId` | 75.789 | 75.789 | not touched this run |
| `fn_801358B4` / `fn_8013639C` / `fn_801363D4` | unpaired | unpaired | unreachable, as the first run established |

Unit `.text`: **7420 B -> 7480 B** (`build/binutils/powerpc-eabi-size -A
build/G2ME01/obj/MetroidPrime/CCollisionActorManager.o`), i.e. **our object is now exactly retail's
size**. `Update` grew 1260 -> 1344 B and nothing else moved; the constructor is unchanged.
`report.json`: `matched_code` 2664 -> 4008 of 7480, `fuzzy_match_percent` 67.861 -> 69.388,
`matched_functions` 23 -> 24 of 30.

## Remaining, per function

- **`__ct__` 30.71%** - see the `basic_string` destructor note above. That is the whole ballgame.
- **`SetActive` 94.231%** - still byte-for-byte the same instruction sequence, same size (208 B),
  same branch structure; only the six callee-saved registers differ. Retail
  `{r26=norm(active), r27=byte-offset, r28=this, r29=mgr, r30=active, r31=i}`;
  ours `{r26=this, r27=mgr, r28=active, r29=i, r30=norm, r31=byte-offset}`. mwcc gave retail's two
  *temporaries* the two lowest registers and ours gave them the two highest. No new spelling tried
  this run, so **no new `WALL:` line** - the first run's 16 spellings still stand.
- **`GetCollisionDescIndexFromUniqueId` 75.789%** - unchanged and untouched.

## New facts worth keeping

- **Reading `x.GetTranslation()` straight out of the expression instead of through a local
  `CVector3f` is a real, measurable choice, not a style preference.** mwcc keeps a fresh
  `CVector3f` temp at stride 4 but re-reads a `CTransform4f`'s translation column at stride 16
  (`base+0xc`, `base+0x1c`, `base+0x2c`). A stride-16 read of three floats in retail's disassembly
  means "this value is `someXf.GetTranslation()`, written inline"; stride 4 means "this is a named
  `CVector3f` local". This is the cheapest way to tell which of the two retail wrote, and it was
  worth +5.5% on `Update` here. (Extends the first run's stride-16 note.)
- **In this tree `GetColumn(kDZ)` compiles to `base+0x8/+0x18/+0x28` and `GetTranslation()` to
  `base+0xc/+0x1c/+0x2c`** - i.e. `kDZ` is column index 2 and the translation is the 4th column.
  Confirmed against both `Update` and the ctor. Useful when mapping a retail stack offset back to a
  source expression.
- **A named `const float` (or any local) whose value must survive a call is how you make mwcc
  allocate a callee-saved FP register**, and the frame grows by 16 to hold the save/restore pair.
  This was the last 2.05% of `Update`. Generalises the first run's "members not parameters" note:
  mwcc's decisions here are steered by whether a value has a *name*, not by what it means.
- **`objdiff-cli diff -p . -u <unit> <symbol> --format json-pretty -o -` gives a per-instruction
  alignment diff** (`instruction.formatted`, `target_symbol.symbol_index`, `diff_kind`). Row index
  != instruction index once anything is inserted/deleted, but it shows exactly which instructions
  are retail-only / ours-only and which are pure operand mismatches, which is far faster than
  eyeballing two objdumps. ~40 s per source variant with `tools/fast_try.sh`.

No `NEW:` line: everything above is this item's own functions or a general codegen rule, and the
remaining ctor work is the same item's own function.
