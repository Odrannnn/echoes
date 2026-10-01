# progress-unit-cscripteffect — `MetroidPrime/ScriptObjects/CScriptEffect` 15/35 -> 17/35

**Result: `Render` and `UpdateSpline` taken to 100.00%, `PreRenderAllViewports` 84.84% -> 99.84%.
`./tools/goal_check.sh build/goal/item.json` -> `PASS` (matched 11686 -> 11688, target rose
15 -> 17, linked 5625 unchanged, no `asm` added).** The unit stays `NonMatching`, as a
`progress` item requires.

Only `src/MetroidPrime/ScriptObjects/CScriptEffect.cpp` changed (+29/-28). No header,
no `config/`, no other unit touched.

## Measured

| | before | after |
|---|---|---|
| unit `matched_functions` | 15 / 35 | **17 / 35** |
| unit `matched_code` | 2116 / 11284 (18.75%) | **2952 / 11284 (26.16%)** |
| unit `fuzzy_match_percent` | 56.02% | **57.07%** |
| DOL `matched_functions` | 11686 / 28465 | **11688 / 28465** |
| DOL `linked` | 5625 | 5625 (unchanged) |
| `All:` | 33.31% fuzzy, 26.25% matched | 33.31% fuzzy, **26.26%** matched |

Per function (`build/report.json` `fuzzy_match_percent`, our compiled size against retail's):

| function | before | after | size before -> after (retail) |
|---|---|---|---|
| `Render__13CScriptEffectCFRC13CStateManager` | 96.07% | **100.00%** | 172 -> **168** (168) |
| `UpdateSpline__13CScriptEffectFf` | 92.95% | **100.00%** | 632 -> **668** (668) |
| `PreRenderAllViewports__13CScriptEffectFR13CStateManager` | 84.84% | **99.84%** | 412 -> **436** (436) |
| everything else in the unit | unchanged | unchanged | — |

## What I changed, per function

### 1. `Render` 96.07% -> 100.00% — the `ps` local was the whole difference

Retail's tail is `lwz r0,mNumParticlesDrawing ; add r0,r0,r3 ; stw r0,mNumParticlesDrawing ;
lwz r3,348(r31) ; ... ; bctrl`. Ours hoisted `lwz r4,348(r31)` **before** the read-modify-write
and then needed an extra `mr r3,r4`, which is the 4 bytes we were over (172 vs 168).

Holding the pointer in a named local (`CParticleGen* ps = mParticleSystem.get(); ps->Render();`)
is what makes mwcceppc hoist the load. Calling `mParticleSystem->Render()` directly re-loads it
after the static update, exactly as retail does. One line pair, no logic touched.

### 2. `UpdateSpline` 92.95% -> 100.00% — three structural spellings plus one named temp

Measured one change at a time (each is a separate build):

- **early-out shape.** Ours was `if (!mHasSpline || !mEmitting) { return; } ... rest`, which
  compiles to `beq ret ; bne body ; b ret` (an extra unconditional branch). Retail has
  `beq ret ; beq ret` — two tests branching to the *same* epilogue. Wrapping the body in
  `if (mHasSpline && mEmitting) { ... }` gives exactly that.
- **the loop-clamp's false branch recomputes the duration.** Retail does
  `addi r3,r30,408 ; bl GetDuration ; lfs f0,56(r3) ; stfs f0,708(r30)` *inside* the
  `mLoopSpline == 0` arm; ours reused the value already in a float register. Writing
  `mSplineTime >= mSpline.GetPositionSpline().GetDuration()` (the expression textually twice,
  no `duration` local) makes mwcceppc rematerialise the call in the second arm.
- **comparison operand order.** Retail is `fcmpo cr0,f1,f0 ; cror eq,gt,eq ; bne` with f1 =
  `mSplineTime` and f0 = `duration`. The spellings tried, all measured: `duration <= mSplineTime`
  gives the right `fcmpo` operands but `cror eq,lt,eq` (2 instructions different);
  `mSplineTime >= duration` gives the right `cror` but `fcmpo cr0,f0,f1`. Only spelling the
  comparison with the call **inline** in the left operand (`mSplineTime >= mSpline.GetPositionSpline().GetDuration()`)
  gives both. This is the same mwcceppc register-assignment rule as `fn_801426E0` in
  `RUNNING_THE_DECOMP.md`, one level up: the call result's register depends on the textual
  order of the operands, so a hoisted local changes the `fcmpo`.
- **one extra stack temporary for `SetTranslation`.** Retail materialises
  `GetPositionByTime`'s return at `r1+100`, then *copies* it to `r1+152` and passes that
  (6 instructions, 24 bytes) — the frame is 240 against our 224. A named
  `const CVector3f position = ...; SetTranslation(position);` is what forces the copy; passing
  the call inline lets mwcceppc elide it.

### 3. `PreRenderAllViewports` 84.84% -> 99.84% — see the wall below

Two of three differences fixed, one left:

- **The `optional_object` ternary is written "condition ? non-null : empty".** Retail tests
  `cmplwi r4,0 ; beq <empty arm>` with the non-null arm falling through; ours had the arms
  the other way round (`bne` + an extra `b`) because the source spelled the test as
  `mParticleSystem.null() ? optional_object() : GetBounds()`. Writing
  `mParticleSystem.get() != nullptr ? mParticleSystem->GetBounds() : rstl::optional_object<CAABox>()`
  inverts it and every instruction now matches.
- **The empty box's centre is a named local.** Retail stores `GetTranslation()`'s three floats
  to `r1+8` and passes that address as *both* arguments to `CAABox(const CVector3f&, const CVector3f&)`;
  ours passed `&this->mTranslation` (`addi r4,r30,84 ; mr r5,r4`) and elided the temporary.
  `const CVector3f position = GetTranslation(); const CAABox emptyBounds(position, position);`
  restores the 6 copy instructions and the 24 bytes they cost. (This was worth 412 -> 436 bytes.)

## Walls measured in this run (spellings and scores, so the next run skips them)

**`PreRenderAllViewports__13CScriptEffectFR13CStateManager` 99.84%, 4 of 109 instructions, and
every one of them is a stack displacement.** The instruction sequence is now identical to
retail's; the frame layout is not:

| slot | retail | ours | what |
|---|---|---|---|
| `r1+8` | `position` | `position` | 12 B |
| `r1+20` | `CAABox emptyBounds` | ternary arm 1 | 24 B / 28 B |
| `r1+44` | ternary arm 1 | ternary arm 2 | 28 B |
| `r1+72` | ternary arm 2 | `CAABox emptyBounds` | 28 B |
| `r1+100` / `124` | payload copy / bool | `r1+88` / `112` | |

Retail allocates the else-block's two named locals *before* the two ternary-arm temporaries; we
allocate the arms first and the block's locals after. Four spellings measured, all 99.84%:
ternary arms in either order (inverting the arms flips the block order too and stays at 99.84%
with the same four displacements); `const` on `bounds` vs not (no effect); swapping the
`if (bounds.valid())` / `else` branch order (block order flips, 4 displacements move);
`rstl::optional_object<CAABox> bounds; if (...) bounds = ...;` instead of the ternary
(396 bytes - 40 short - because `operator=` drops the payload copy `operator*` forces).

WALL: PreRenderAllViewports__13CScriptEffectFR13CStateManager 99.84% - instruction sequence is retail's; the CAABox temp is allocated after the ternary arms instead of before them, 4 stack displacements

**`__ct__15CGameSplineDescFRC11CMayaSplineQ213CMotionSpline11ESplineTypefb` 92.31% (104 B, both
sides) - one instruction: the epilogue's `lwz r0,36(r1)`.** Everything up to and including the
last `stb r0,76(r29)` is byte-identical. Retail restores in the order it saved
(`lwz r0 ; lfd f31 ; lwz r31 ; lwz r30 ; lwz r29 ; mtlr`); we restore `r0` last, immediately
before `mtlr`. This is the wall already recorded in `RUNNING_THE_DECOMP.md` (`fn_8014601C`,
`GetTouchBounds`, `fn_8014680C`) - our 100%-matched functions with an `f31` save (`Think`,
`UpdateSpline`) all have a `psq_st`/`psq_l` pair, which is what puts their `lwz r0` early; this
constructor has no vector pair and nothing tried moves the reload. Two spellings measured, both
92.31%: mem-init list (original) and assignments in the constructor body.

**`UpdateGeneratorRate__13CScriptEffectFR13CStateManager` - worse than the shape we already had
(85.62%), do not re-try these.** Retail's loop seeds `distanceSq` with `lfs f31,-30788(r2)`
(0.0f at `0x80081AF0`), peels the first iteration (unconditional `fmr f30,f0`, no compare) and
tests `cmplw r30,r0 ; blt`, and keeps the running max in **f30** while we keep it in **f31**.
Four restructurings measured:
`float distanceSq = 0.f; for (int i = 0; ...)` with `max_val(distanceSq, next)` -> **71.91%**
(452 B, the loop becomes a pre-test loop, the peel is lost);
the same with `max_val(next, distanceSq)` -> 71.91%, same;
`for (unsigned i = 0; ...)` -> **~72%** and the array access degrades to `addi r0,r31,n ;
lwzx r3,r29,r0` instead of retail's pointer step;
the original (`camera(0)` then `for (int i = 1; ...)`, `max_val(distanceSq, next)`) -> **85.62%**
(532 B) and is what the tree still has. The residue is `cmplw` vs `cmpw` (`GetNumPlayers()`
returns `int`; retail compares unsigned) and f30/f31 allocation.

**`PreRender__13CScriptEffectFR13CStateManager` 79.50% (456 B against retail's 540).** Two
things block it and both are outside this file:

1. `CStateManager::fn_800366e4` is declared `int` (`include/MetroidPrime/CStateManager.hpp:275`)
   and mwcceppc therefore tests it with `cmpwi r3,0`. Retail tests it with
   `clrlwi. r0,r3,24 ; beq`, which is the **bool** idiom — the same one our 100%-matched
   `IsSystemDeletable` emits for a `bool` return. Changing the declaration to `bool` does not
   change the mangled name (return types are not mangled), but it would change the generated
   code of `CActor.cpp:297` and `CEnergyProjectile.cpp:137`, the only other callers, so it needs
   its own measured change rather than being folded into a `progress` item on this unit.
2. Retail computes `addi r31,r28,204` (the address of `GetOtherBounds()`) *before* the
   `fn_800366e4` call and keeps it in a callee-saved register across the visor switch, so it
   needs `r28,r29,r30,r31` (this, mgr, visible, bounds); we compute it after the switch and use
   `r28` for it, so every register is one number higher and the three temporaries are at
   `r1+16/28/40` against retail's `r1+28/40/52`. That needs the `const CAABox&` bound earlier
   in the function than the `if (!mCanRender)` test.

## For the orchestrator (not filed as `NEW:`, it is a restatement of this item's target)

**11 of this unit's 20 remaining functions are at 0.00% and none of them is ours.** `report.json`
lists them in this unit's split range (`.text 0x800802D4..0x80082EE8`) but our object does not
emit them at all, and `nm` confirms it: `fn_80080394` (1772 B), `fn_80080BE8` (464),
`fn_80080FCC` (248), `fn_80080E64` (224), `fn_80080DB8` (172),
`reserve__Q24rstl52vector<15CMayaSplineKnot,...>Fi` (152), `fn_80080F50` (124), `fn_80082E74`
(92), `__dt__15CGameSplineDescFv` (84), `fn_80082ED0` (24), `fn_80080F44` (12).

Reading them (`./tools/dis.sh`, no guessing beyond what the bytes say): `fn_80080F44` is
`li r0,0 ; stw r0,4(r3) ; blr`; `fn_80080F50` destroys a `CMayaSpline` at `this+0xB8` plus
`this+0xB4`, `this+0x70` and an `SLdrEditorProperties` at `this+0x00`, then calls
`Free__7CMemoryFPCv` when the `extsh` flag is non-zero - a deleting destructor of whichever
class owns one; `fn_80082E74` is a 28-byte-stride copy loop over `SLdrSpline`-shaped records
that returns the **advanced destination** (`mr r3,r5` after `addi r5,r5,28`), not the original;
`fn_80082ED0` is `lfs f0,0.0f ; lis r3,0x8046 ; stfsu f0,-0x5700(r3)` then `stfs f0,4(r3)` and
`stfs f0,8(r3)`, i.e. it zeroes a float at `0x8045A900` and two at `0x8045A908`/`0x8045A90C`.
They are the
spline/stream classes' members, and `src/Kyoto/Math/CMayaSpline.cpp` and
`src/Kyoto/Streams/*` already exist and are built - but a definition emitted from *those*
translation units lands in *their* object, and objdiff pairs per unit, so it would not count for
`CScriptEffect`. Writing them into `CScriptEffect.cpp` under their dtk names would pair them here
(and the unit is `NonMatching`, so retail's object stays in the link and no symbol collides), but
that is a claim question - where a class's members belong - and not mine to decide. Worth a look
from whoever owns the claim layout: 11 functions, 3237 bytes, is more than the remaining four
named functions in this unit put together.

## Gates

`./tools/goal_check.sh build/goal/item.json` -> **`PASS`**:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11686 -> 11688   linked 5625 -> 5625
  ok    check_symbol_names.py
  ok    All:  33.31% fuzzy, 26.26% matched, 12.24% linked (11688 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptEffect: 15 -> 17 / 35 functions
  ok    no asm added
```

No `tools/`, no `config/`, no `docs/`, no `build/goal/` file was edited; nothing committed.
---

# progress-unit-cscripteffect — run 2 (`wt-mp2-goal-L7`): 17/35 -> 18/35

**Result: `CreateSystem` taken to 100.00% (57.96% -> 100.00%, three spellings),
`AcceptScriptMsg` 46.72% -> 48.56%. `./tools/goal_check.sh build/goal/item.json` -> `PASS`
(matched 12131 -> 12132, target rose 17 -> 18, linked 5860 unchanged, no `asm` added).**
The unit stays `NonMatching`, as a `progress` item requires.

Only `src/MetroidPrime/ScriptObjects/CScriptEffect.cpp` changed (+13/-11). No header, no
`config/`, no `tools/`, no other unit touched. Nothing committed.

Re-measured on this tree first: the unit really was at 17/35, `matched_code` 2952/11284,
so the previous run's numbers were still current and nothing had landed upstream.

## Measured

| | before | after |
|---|---|---|
| unit `matched_functions` | 17 / 35 | **18 / 35** |
| unit `matched_code` | 2952 / 11284 | **3308 / 11284 (29.32%)** |
| unit `fuzzy_match_percent` | 57.09% | **58.73%** |
| DOL `matched_functions` | 12131 / 28465 | **12132 / 28465** |
| DOL `linked` | 5860 | 5860 (unchanged) |

| function | before | after | our size -> retail |
|---|---|---|---|
| `CreateSystem__13CScriptEffectFRC9CVector3fRC6CColor` | 57.96% | **100.00%** | 356 -> 356 (356) |
| `AcceptScriptMsg__13CScriptEffectFR13CStateManagerRC10CScriptMsg` | 46.72% | **48.56%** | 1872 -> 1872 (1872) |
| everything else in the unit | unchanged | unchanged | — |

## `CreateSystem` 57.96% -> 100.00%: three independent spellings

The previous run left it alone. Read with `./tools/dis.sh 0x80081770`-style ranges plus a
byte-exact per-instruction compare of `build/G2ME01/{src,obj}/.../CScriptEffect.o`, the whole
gap was three *spelling* differences, none of them a logic change.

1. **`const CVector3f& localScale = CVector3f(1.f, 1.f, 1.f);` replaces the trailing
   `CVector3f::One()` argument** (57.96% -> 89.72% on its own). Retail materialises a
   three-float stack temporary and keeps its address in a callee-saved register across the
   whole `ConstructChildParticleSystem` call (`addi r31,r1,32` before the transform copies,
   `stw r31,24(r1)` at the call). `CVector3f::One()` is `static const CVector3f&` returning
   `sOneVector`, so we passed a *global's* address and emitted no temporary at all. Binding
   the temporary to a `const&` named local is what makes mwcceppc allocate the frame slot for
   it. Measured and rejected on the way: `const CVector3f localScale(1.f,1.f,1.f)` (by value,
   89.72% — same score, but the slot is still allocated; it is the `&` that survives to
   100%), `= CVector3f::One()` (84.61%, emits `lfsu f2,0(sOneVector)` into f5/f4 instead),
   declared before `position` (80.38%), non-`const` by value (89.72%).
2. **`ClearTrans(GetTransform())` inline as the `orientation` argument** replaces
   `CTransform4f orientation = GetTransform(); orientation.SetTranslation(...)`.
   Retail calls `__ct__CTransform4f` **twice** (once into `r1+56`, once into `r1+104`) with
   `sZeroVector` as the third argument of the second — the by-value-return shape the file's
   own `ClearTrans` helper already produces for `Think`. Measured: as a **named local**
   (`const CTransform4f orientation = ClearTrans(GetTransform());`) it is 61.13% — the extra
   copy is elided. Passing it **inline in the call** keeps the second `__ct__`. Also
   measured and worse: `const CVector3f position = xf.GetTranslation()` from a cached
   `const CTransform4f& xf` (54.34%), `CTransform4f xf = GetTransform()` by value then
   `ClearTrans(xf)` (82.70%), swapping the two `mUseLocalTranslation` ternaries (84.34%),
   and hoisting both ternaries into named locals (36.83%).
3. **`mEffectLights.get() != nullptr` instead of `!mEffectLights.null()`** — the last 10
   instructions, and the one that actually reaches 100 (94.43% -> 100.00%). Retail's
   `lwz r12,396(r28)` / `neg r11,r12` / `or r0,r11,r12` / `srwi r7,r0,31` sequence loads the
   raw pointer into `r12` and normalises it through `r0`. `single_ptr::null()` makes
   mwcceppc compute the negation through a *different* register (`or r8,r7,r8` /
   `srwi r8,r8,31`), so the whole 8-instruction idiom is the same shape in a different
   register assignment. **Generalisable: for a `!= nullptr` test on a raw pointer member,
   `.get() != nullptr` and `!.null()` are not interchangeable spellings in mwcceppc.**
   Measured and worse: both bools as named locals (73.72%), `useLights` local only (83.96%),
   `emission` local only (81.38%).

## `AcceptScriptMsg` 46.72% -> 48.56%: hoisting the two `msg` accessors

Retail's prologue loads `msg.GetUnk()` into `r9` and **stores it to `r1+100` immediately**
(`sth r9,100(r1)` at +0x24), and keeps `msg.GetMessage()` in `r28` across the whole switch
(`lwz r28,8(r25)`). We re-read both members at their use sites, which costs the stack slot
and the callee-saved register. Naming them (`const EScriptObjectMessage message =
msg.GetMessage(); const TUniqueId unk = msg.GetUnk();`) puts both in the prologue. Only the
second use sites change; the `kSM_ToggleActive` recursive call still reads them from `msg`,
which is what retail does too. Measured: `unk` alone 48.31%, `message` alone 47.03%, both
48.56% (kept).

Still 48.56%: the residue is 424 of 468 aligned instructions, and the frame is still 16 bytes
larger than retail's (352 vs 336) because retail reuses `r26`/`r28` where we need an extra
callee-saved register. The switch dispatch tree matches instruction for instruction from
+0x14 to +0x74; the divergence is register *numbering* plus the epilogue, not the dispatch.
Do not assume a big win here without a fresh look at the switch body order.

## Walls measured in THIS run (spellings and scores, so the next run skips them)

WALL: PreRenderAllViewports__13CScriptEffectFR13CStateManager 99.84% - instruction sequence is retail's; the CAABox temp's stack slot is allocated after the ternary-arm temporaries instead of before them, 17 stack displacements

**`PreRenderAllViewports`, 22 further spellings this run, all 99.84% or worse.** The previous
run recorded four; these are new. Retail allocates `position` at `r1+8`, `emptyBounds` at
`r1+20`, then the two ternary-arm temporaries at `r1+44`/`r1+72`, i.e. the else-block's two
named locals **before** the ternary's temporaries. We allocate `position`, then arm 1,
then arm 2, then `emptyBounds` at `r1+76`. Nothing tried moves `emptyBounds` ahead of the
arms. Measured, all 99.84% unless stated: `position` hoisted above the ternary; the whole
`if/else` wrapped in an extra `{}`; `CParticleGen* const system = mParticleSystem.get()`
then the ternary on `system`; a nested `{}` around the valid arm; `const CAABox& box =
*bounds` in the valid arm; `emptyBounds` as a non-`const` object; `const CVector3f& p =
GetTranslation()`; `const auto& bounds = <conditional>` (build fails — `auto&` is not
accepted by this mwcceppc); `rstl::optional_object<CAABox> bounds; if (...) bounds = ...;`
(75.31% — `operator=` drops the payload copy, 396 B); a `static` helper returning
`optional_object` (90.02%); a `static` helper returning `CAABox` (75.75%); `const
CAABox& emptyBounds(position, position)` (build fails — no such ctor); `bounds.get()`
(build fails); `SetOtherBounds(box)` from a hoisted `const CAABox box = *bounds` (80.51%);
the early-return shapes (49.44%, 85.43%, 53.93%); `if (!bounds.valid())` with the else body
first (64.24%); `bounds.valid()` moved to a trailing `mCanRender = bounds.valid();` (96.17%);
`UpdatePortalSystemState(mgr)` hoisted to the top (93.08%); `const CVector3f zero =
CVector3f::Zero()` used in both ternaries (72.22%); a default-constructed `CAABox emptyBounds;`
assigned in the else (build fails); `bounds` as a reference to the conditional (80.61%); two
named arms then a reference to select (73.70%). **The remaining 4 displacements are inside
the CAABox payload read-back, so the slot assignment is the whole gap.**

WALL: __ct__15CGameSplineDescFRC11CMayaSplineQ213CMotionSpline11ESplineTypefb 92.31% - the epilogue's `lwz r0,36(r1)` reload order; 8 more spellings, none moved it

Confirms the previous run's wall from a different direction. The single differing instruction
is that retail restores in the order it saved (`lwz r0 ; lfd f31 ; lwz r31 ; lwz r30 ;
lwz r29 ; mtlr`) and we restore `r0` last. Eight spellings measured this run, none better:
all four members assigned in the constructor **body** (68.08%), body with `mDuration` first
(59.96%), the mem-init list with `mDuration` moved ahead of `mType` (92.31%, no change),
`mSpline(spline)` in the init list and the other three in the body (92.31%), `mClosedLoop(closedLoop
!= false)` (76.73%), `mDuration(static_cast<float>(duration))` (92.31%), an unused
`(void)duration;` in the body (92.31%), `mDuration(duration)` in an otherwise-unchanged init
list (92.31%). The score is a plateau on the *spelling*, not on the source shape — do not
spend another item on permutations of this constructor.

`PreRender__13CScriptEffectFR13CStateManager` 79.95%, four spellings, no change: the baseline;
`else if (mgr.fn_800366e4(this) != 0)`; a named `const bool inFrustum = mgr.fn_800366e4(this)
!= 0;` in a restructured `else` block (77.65%); and hoisting the call into a `visibleToCamera`
local (68.02%). The previous run's analysis holds and this run re-measured it: retail's
`clrlwi. r0,r3,24` is the **bool** return idiom and `CStateManager::fn_800366e4` is declared
`int` at `include/MetroidPrime/CStateManager.hpp:275`. Every local spelling that avoids the
header still emits `cmpwi`. Fixing it needs the return type changed to `bool`, which moves
`CActor.cpp:297` and `CEnergyProjectile.cpp:137` too — its own item, not this one.

`UpdateGeneratorRate__13CScriptEffectFR13CStateManager` 85.62%, five spellings, no change:
the baseline; `for (uint i = 1; i < static_cast<uint>(mgr.GetNumPlayers()); ++i)` (85.27% —
gets `cmplw` but loses 12 bytes elsewhere); the count hoisted into `const uint numPlayers`
(78.46%); the unsigned loop plus a hoisted `const CVector3f position = GetTranslation()`
(70.62%); and the camera-0 distanceSq hoisted into a named local before the loop (85.27%).
The residue is still f30/f31 allocation and the peeled first iteration.

`__ct__13CScriptEffectF9TUniqueId...` (88.24%, 1100 B) was re-read but not attempted: its
first divergence is a **different callee**, `bl CModelDataNull__10CModelDataFv` where we emit
`bl __ct__10CModelDataFv`. `CModelDataNull()` is `static CModelData CModelDataNull() { return
CModelData(); }` at `include/MetroidPrime/CModelData.hpp:183` and mwcceppc inlines it; retail
carries it out of line at `0x80036184` as a one-instruction tail call to `__ct__10CModelDataFv`.
**NEW, worth a look: preventing that inline would fix the first divergence in this unit's
largest remaining named function, and it is a header change on a class eight other units
construct** — measure all of them before taking it.

## For the orchestrator — still true, still not mine to decide

The previous run's note on the 11 functions at 0.00% is re-measured and unchanged: our object
does not emit them at all (`nm` confirms). **`__dt__15CGameSplineDescFv` (84 B) is now known
to be writable and is NOT a claim question**: `build/G2ME01/src/MetroidPrime/ScriptObjects/
CScriptPlatform.o` already emits a **byte-identical** copy of it (`objdump` at 0xdf4), and
`CScriptCannonBall.o` pairs it at 100.00%. Retail's CScriptEffect object also has it. So the
definition already exists in this repo and is already correct; moving it into
`CScriptEffect.cpp` would pair it here at 100% and is a mechanical change. It is *not* in
this diff because moving it out of `CScriptPlatform.cpp` would change that unit's `.text`,
which this `progress` item must not do. That is the whole cost.

## Gates

`./tools/goal_check.sh build/goal/item.json` -> **`PASS`**:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12131 -> 12132   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.28% fuzzy, 27.50% matched, 12.89% linked (12132 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptEffect: 17 -> 18 / 35 functions
  ok    no asm added
```

`python3 tools/check_symbol_names.py` -> 0 missing names (525 units).
`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptEffect.cpp` ->
`ok: 0 unit(s) checked` (nothing to check; the unit is claimed in `splits.txt` but the tool
reports no out-of-order functions).

No `tools/`, no `config/`, no `docs/`, no `build/goal/` file was edited by hand; nothing
committed. (`docs/HANDOFF.md`'s state block shows the judge's own rewritten 12132 line — that
is `goal_check.sh` rewriting derived counts, not an edit of mine.)
