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

---

# progress-unit-cscripteffect — run 3 (`wt-mp2-goal-L7`): 18/35 -> 19/35

**Result: `__dt__15CGameSplineDescFv` taken from 0.00% to 100.00% (84 B, the first function this
unit that our object did not emit at all). `./tools/goal_check.sh build/goal/item.json` -> `PASS`
(matched 12205 -> 12206, target rose 18 -> 19, linked 5860 unchanged, no `asm` added).** The unit
stays `NonMatching`, as a `progress` item requires.

Three files changed: `include/Kyoto/Math/CGameSplineDesc.hpp` (one line, `~CGameSplineDesc()`
declared instead of defined in-class), and the out-of-line definition written into
`src/MetroidPrime/ScriptObjects/CScriptEffect.cpp` and
`src/MetroidPrime/ScriptObjects/CScriptCannonBall.cpp`. No `tools/`, no `config/`, no
`build/goal/` file edited. Nothing committed.

## Measured

| | before | after |
|---|---|---|
| unit `matched_functions` | 18 / 35 | **19 / 35** |
| unit `matched_code` | 3308 / 11284 (29.32%) | **3392 / 11284 (30.06%)** |
| unit `fuzzy_match_percent` | 58.73% | **59.47%** |
| `ScriptCannonBall/.../CScriptCannonBall` | 12 / 26, 65.84% fuzzy | **12 / 26, 65.84% fuzzy (unchanged)** |
| DOL `matched_functions` | 12205 / 28465 | **12206 / 28465** |
| DOL `linked` | 5860 | 5860 (unchanged) |
| `All:` | 34.46% fuzzy, 27.74% matched | 34.46% fuzzy, **27.75%** matched |

## The change, and why it is three files

`include/Kyoto/Math/CGameSplineDesc.hpp:12` had `~CGameSplineDesc() {}`. **An in-class body is
inlined at every call site**, so the out-of-line weak copy was only ever *emitted*, never called,
and `CScriptEffect.o` did not contain the symbol at all — which is why the function sat at 0.00%
in `build/report.json` even though `CScriptPlatform.o` and `CScriptCannonBall.o` each already
carry a **byte-identical** 84-byte copy (`objdump` of this run's
`build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptEffect.o` at 0x150 is the same instruction
sequence as retail's 0x80080A80). Declaring it in the header and writing the definition out of
line puts it in the object, and it pairs at 100.00%.

**Both** units need the copy, and that is not a workaround: retail's own `CScriptCannonBall.o`
defines it too — `build/report.json` lists `__dt__15CGameSplineDescFv` at 100.00% under
`ScriptCannonBall/MetroidPrime/ScriptObjects/CScriptCannonBall` (dtk gave it that object's range)
and this run measured it at 0.00% under `main/MetroidPrime/ScriptObjects/CScriptEffect`. One copy
is not enough, and that is a gate consequence rather than a style one: with the header declared
only, `CScriptCannonBall.o` stops defining the symbol, and `tools/report_diff.py` **cannot** pair
that loss as a move — its module-wide pass compares the first `/`-component, `main` for this unit
against `ScriptCannonBall` for that one — so a `GONE` at 100.00% would fail the gate outright.
Writing the copy in both places is what retail does.

### Two placement details that cost a build each

- **Declaration order, twice.** Appending the definition at the end of each file put it at the
  *lowest* `.text` offset in both objects (mwcceppc emits definitions in reverse source order) and
  `gate.sh` failed on `decl-order`:
  `ScriptCannonBall/.../CScriptCannonBall permuted and not in decl_order.md`,
  `main/.../CScriptEffect permuted and not in decl_order.md`.
  It belongs between `__ct__15CGameSplineDesc` (retail 0x80080AD4) and `~CScriptEffect`
  (0x800802D4), and in `CScriptCannonBall.cpp` between `~CScriptCannonBall` (retail offset 4180)
  and `AcceptScriptMsg` (1816). After the move, `python3 tools/check_decl_order.py` ->
  `ok: 981 unit(s) checked, 29 permuted, all 29 accounted for in decl_order.md`.
- **The port link is untouched, and that is worth recording.** `CScriptCannonBall.cpp` *is* in the
  port link and `src/MetroidPrime/ScriptObjects/CScriptEffect.cpp` **is not** (no object under
  `build-port-link/CMakeFiles/mp_game.dir/` for it — the port reaches the constructor through
  `PortReachStubs.cpp`). So the symbol did not become newly undefined anywhere: `probe_sources.sh`
  reports `751 files, 0 failed, 0 errors; link: LINKED (288 undefined, 0 duplicates)`, the same
  numbers as the baseline.

## Re-measured this run, so the next run does not repeat it

**`CModelDataNull` is always inlined; the previous run's `NEW` note about it stands.** Writing
`CModelData::CModelDataNull()` instead of `CModelData()` in the constructor's mem-init list leaves
`__ct__13CScriptEffect` at **88.24%** and the call still goes to `__ct__10CModelDataFv`. Taking
its address (`CModelData (*const kModelDataNull)() = CModelData::CModelDataNull;` in an anonymous
namespace) does **not** stop it either: `nm` then shows both `W CModelDataNull__10CModelDataFv`
(emitted, unused) and `U __ct__10CModelDataFv` (called), and the score is still 88.24%. Separately
and usefully: the **unqualified** `CModelDataNull()` in a mem-initializer is rejected by mwcceppc
as `undefined identifier` even with `#include "MetroidPrime/CModelData.hpp"` added — only the
`CModelData::`-qualified form compiles.

**`PreRenderAllViewports` 99.84% — one new spelling, and it rules out the remaining theory.**
Hoisting `position` *and* `emptyBounds` above the ternary (both built unconditionally) gives
**81.33%** and leaves the slot order **identical to the baseline** (`position`@8, arm temps@20/48,
`emptyBounds`@76, `bounds`@100). So the pre-pass allocation is not driven by declaration order and
the compiler does not sink the initialisation; retail's `position`@8 / `emptyBounds`@20 before the
arms at 44/72 is not reachable from this source shape. That is 30 spellings over three runs.

**`__ct__15CGameSplineDesc` 92.31% — five new spellings, all 92.31%.** `mClosedLoop` assigned in
the body rather than the init list; `mSpline, mClosedLoop, mType, mDuration`;
`mSpline, mDuration, mClosedLoop, mType`; an extra `(void)spline;` in the body; and
`mType, mDuration, mClosedLoop, mSpline` (constructor last). The residue is still only the
epilogue's `lwz r0,36(r1)`. **New, and it contradicts the previous runs' "nothing can reach it":
retail's order (`lwz r0 ; lfd f31 ; lwz r31 ; lwz r30 ; lwz r29 ; mtlr`) is reached by 22 of the
DOL's 100%-matched functions**, and only two of those also save a third integer callee-saved
register, which is this constructor's shape — `CFluidUVMotion::CalculateFluidTextureOffset`
(saves r30+r31) and `CABSFlinch::UpdateBody` (saves r29+r30+r31). So the epilogue order is
reachable by *something*; it is simply not reachable by this constructor's spelling, and the
search should look at what those two functions' bodies have in common rather than permute the
init list again.

**`UpdateGeneratorRate` 85.62% — re-read, not attempted.** Residue confirmed by disassembly rather
than recalled: retail tests the loop bound with `cmplw r30,r0` (0x80081bc4) where we emit
`cmpw r30,r0` (0x1010), and retail keeps the running max in **f30** while **f31** holds a hoisted
`1.0f` (`lfs f31,-30788(r2)` at 0x80081af0, first used at 0x80081c20-0x80081c28); we keep the max
in f31. Retail's frame is 80 bytes and saves f30 and f31, ours is 64 and saves f31 only.

`PreRender` 79.95%, `AcceptScriptMsg` 48.56% and `__ct__13CScriptEffect` 88.24% are unchanged; the
latter's first divergence is still the `CModelDataNull` call above. The 11 functions at 0.00% are
still the spline/stream members and were not touched — still a claim question, still not mine.

## Gates

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12205 -> 12206   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.46% fuzzy, 27.75% matched, 12.89% linked (12206 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptEffect: 18 -> 19 / 35 functions
  ok    no asm added
```

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` ->
`matched 12205 -> 12206, linked 5860 -> 5860, +1 functions at 100%, no regression`.
`python3 tools/check_docs_claims.py` -> `docs claims agree with the tree`.
(`docs/HANDOFF.md`'s state block shows the judge's own rewritten 12206/10658 lines — that is
`goal_check.sh` rewriting derived counts, not an edit of mine.)

---

# progress-unit-cscripteffect — run 4 (`wt-mp2-goal-L5`): 19/35 -> 20/35

**Result: `fn_80080BE8` identified and renamed to the function our object already emits byte-for-byte
(0.00% -> 100.00%, 464 bytes), and `PreRender` 79.95% -> 97.72% by implementing the render-queue block
the source carried as a `TODO`. `./tools/goal_check.sh build/goal/item.json` -> **`PASS`**
(matched 12227 -> 12228, target rose 19 -> 20, linked 5860 unchanged, no `asm` added).** The unit
stays `NonMatching`, as a `progress` item requires.

Two files changed: `config/G2ME01/symbols.txt` (one line, a rename) and
`src/MetroidPrime/ScriptObjects/CScriptEffect.cpp` (+21/-1). No header, no `tools/`, no
`build/goal/` file edited by hand. Nothing committed.

## Measured

| | before | after |
|---|---|---|
| unit `matched_functions` | 19 / 35 | **20 / 35** |
| unit `matched_code` | 3392 / 11284 (30.06%) | **3856 / 11284 (34.17%)** |
| unit `fuzzy_match_percent` | 59.47% | **64.43%** |
| DOL `matched_functions` | 12227 / 28465 | **12228 / 28465** |
| DOL `linked` | 5860 | 5860 (unchanged) |
| `All:` | 34.54% fuzzy, 27.87% matched | 34.54% fuzzy, **27.88%** matched |

| function | before | after |
|---|---|---|
| `fn_80080BE8` -> `__ct__Q24rstl52vector<15CMayaSplineKnot,...>FRCQ24rstl52vector<...>` | 0.00% | **100.00%** |
| `PreRender__13CScriptEffectFR13CStateManager` | 79.95% | **97.72%** |
| everything else in the unit | unchanged | unchanged |

## 1. The 0.00% functions were a **naming** problem, not a missing-code problem

This contradicts runs 1-3, which all recorded "our object does not emit them at all (`nm`
confirms)" and called it a claim question. **`nm` was asked the wrong question**: it was asked for
the retail *placeholder* name. Asked for real names and compared by **size**, our object already
emitted retail's `fn_80080BE8` exactly.

The measurement that settles it: `rstl::vector<CMayaSplineKnot>`'s copy constructor is emitted by
`CScriptEffect.o` as a weak symbol of **464 bytes**, and `fn_80080BE8` is
`size:0x1D0` = **464 bytes** in `symbols.txt`. Disassembling both ranges instruction-by-
instruction with branch targets normalised gives **9 differing instructions out of 116, and every
one is a `bl`/`b`/`beq`/`bne`/`bdnz`** — i.e. pure relocation, zero semantic difference:

```
  insn 20: retail 4827ce81 'bl 802fdab8 <allocate__Q24rstl17rmemory_allocatorFi>'
          ours   48000001 'bl 3b4 <...vector...FRCQ24rstl52vector...+0x50>'
```

The caller confirms it independently: retail's `__ct__11CMayaSplineFRC11CMayaSpline` (0x80080B3C)
calls 0x80080BE8 with `r3 = this+8`, `r4 = other.mKnots`, which is a vector copy construction of the
`mKnots` member at offset 8 — and our copy constructor at 0x2b8 makes exactly that call. Retail
just has no name for it, because dtk never resolved the COMDAT.

**So the fix is one line of `symbols.txt`, not a claim change.** `fn_80080BE8` becomes
`__ct__Q24rstl52vector<15CMayaSplineKnot,Q24rstl17rmemory_allocator>FRCQ24rstl52vector<15CMayaSplineKnot,Q24rstl17rmemory_allocator>`
with `scope:weak`. objdiff then pairs it and the function counts.

**Generalisable, and the thing three runs missed: when a `NonMatching` unit shows functions at
0.00%, ask whether the *code* is missing before concluding it is. `report.json`'s
`functions[].fuzzy_match_percent` is absent (not 0) for a function the object does not emit, and a
present-but-unpaired function at 0.00% is a **naming** result. The cheap test is `nm -S` on the
built object, compare symbol *sizes* against the `size:0x..` in `symbols.txt`, and disassemble the
matching pair.** The sizes here lined up exactly (464 = 0x1D0); that is not a coincidence to look
for twice.

Verified safe: `sha1sum build/G2ME01/main.dol` unchanged
(`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`), `check_symbol_names.py` -> 0 missing (525 units),
all 86 RELs still hash-matched, `report_diff.py` -> `no regression`.

### `fn_80080DB8` and `fn_80080E64` are the same story, and a `src/` change reaches them

Both are in range and unnamed. `fn_80080DB8` is 172 B (`0xAC`) and `fn_80080E64` is 224 B
(`0xE0`). Scanned against every symbol our object emits, `fn_80080DB8` scores **35/43
instructions** against `__ct__11CMayaSplineFRC11CMayaSpline` (172 B) — it is retail's **second,
assignment-shaped copy of `CMayaSpline`** (it calls the vector's `operator=` at 0x80080E64 where the
constructor calls the vector's copy constructor). `CMayaSpline` has **no declared `operator=`**, so
mwcceppc only ever synthesises one where an assignment needs it, and `CScriptEffect.cpp` has none.

Declaring `CMayaSpline& operator=(const CMayaSpline&);` in `include/Kyoto/Math/CMayaSpline.hpp` and
writing the definition into `CScriptEffect.cpp` emits **both** missing symbols
(`__as__11CMayaSpline` 0xAC, `__as__Q24rstl52vector<...>` 0xE0) and takes the unit to **22/35**.

**It regresses the build, and that is the whole reason it is not in this diff: declaring the
operator in the header changes three other units** (`CWorldTransManager`, `Tweaks`,
`CEnergyProjectile` each stop emitting their own implicit `operator=`, so five 100%-matched functions
across `Tweaks` and `CEnergyProjectile` drop to 0.00% and the global count falls 12228 -> 12223).
**Do not re-try the header declaration without a measured fix for the other three units.**

## 2. `PreRender` 79.95% -> 97.72%: the TODO was real missing work

The previous runs read `PreRender`'s residue as "retail's `clrlwi.` vs our `cmpwi`, plus register
numbering" and stopped. **`include/MetroidPrime/CStateManager.hpp:290` already declares
`fn_800366e4` as `bool`**, so the blocker recorded at runs 1-2 (`int` at line 275, needing a return
type change) is **stale** — that change has already landed. What is left is not codegen at all: it
is **22 instructions of source that was never written**.

Retail's tail, at 0x80081A40, dispatches on `mRenderOrder` (a 2-bit field at 0x2CA) and submits the
effect's id to one of two special render queues:

```
80081a40: lbz     r0,714(r28)        # mRenderOrder
80081a44: rlwinm  r0,r0,27,30,31
80081a48: cmpwi   r0,1
80081a4c: beq     80081a7c          # order == 1 -> queue A
80081a50: bge     80081a58
80081a54: b       80081a90          # order == 0 -> nothing
80081a58: cmpwi   r0,3
80081a5c: bge     80081a90
80081a60: lhz     r0,8(r28)         # GetUniqueId()
80081a64: mr      r3,r29
80081a68: addi    r4,r1,16
80081a6c: sth     r0,12(r1)
80081a70: sth     r0,16(r1)
80081a74: bl      80037984 <fn_80037984__13CStateManagerF9TUniqueId>
80081a7c: lhz     r0,8(r28)
80081a80: mr      r3,r29
80081a84: addi    r4,r1,8
80081a88: sth     r0,8(r1)
80081a8c: bl      80037a04 <fn_80037A04__13CStateManagerF9TUniqueId>
```

Both callees already exist and are declared (`CStateManager.hpp:300-302`), and `CScriptSkyRipple.cpp:107`
already calls `fn_80037A04` with exactly this `reinterpret_cast` id read — so this is the same
established idiom, not a new invention. Implementing it (79.95% -> 96.86%) plus hoisting
`const CAABox& bounds` to the top of the function (96.86% -> 97.72%) is the change.

**The `bounds` hoist is worth 2.96 points on its own and is the notes' own un-tried idea** — runs 1-3
all recorded that "retail computes `addi r31,r28,204` before the `fn_800366e4` call and keeps it in a
callee-saved register across the visor switch", and all three treated that as a *register-numbering*
problem needing the `const CAABox&` bound earlier. Binding it at the top of the function is exactly
that, and it moves the frame from 96 bytes to retail's shape. Measured separately: TODO block alone
96.86%, hoist alone (no TODO) 93.90%.

## Measured and rejected in THIS run

- **`rstl::vector<T,Alloc>::operator=` un-`inline`d in `include/rstl/vector.hpp`** (drop `inline`
  from the declaration and the out-of-class definition): **all 86 RELs fail to link**, 87 computed
  checksums mismatch. Retail's out-of-line `reserve`/`clear`/`uninitialized_copy` for
  `vector<CMayaSplineKnot>` are `scope:weak` COMDATs the REL linker expects to find. **Never do this.**
- **`CMayaSpline::operator=` declared in the header**: +3 to this unit but **-5 globally**
  (12228 -> 12223), five 100% functions in `Tweaks` and `CEnergyProjectile` drop to 0.00%.
- **`CAABox lightBounds(center, center)` as a named local** in `PreRender` (matching what run 1 found
  necessary in `PreRenderAllViewports`): 97.72% -> **80.95%**. The named `CAABox` is *not* what
  retail does here.
- **`const TUniqueId copy = id` on the Queue2 arm**: 97.72% -> 96.89%; non-`const` -> 96.24%. Only
  the **Queue1** arm wants the copy (97.72%); Queue2 wants the reference passed straight through
  (96.89% with a copy on both arms). Retail's own code agrees it is asymmetric — Queue1 stores to one
  slot, Queue2 stores to two.
- **`const CAABox& bounds` left where it was** (hoist reverted, TODO kept): 96.86%.
- **`mRenderOrder` read as anything but a 2-bit field**: not tried; retail's `rlwinm r0,r0,27,30,31`
  plus `cmpwi r0,1` / `cmpwi r0,3` is the switch-on-a-narrow-enum lowering, and `mRenderOrder` is
  already `uint : 2`.

## Residue left in `PreRender` (97.72%, 4 of 136 instructions)

Not re-tried, and the spellings above are the ones already spent:

| what | retail | ours |
|---|---|---|
| Queue2 arg setup | `addi r4,r1,16` then `sth r0,16(r1)` | `addi r4,r1,12` then `sth r0,16(r1)` |
| Queue2 arm | one fewer `b` | extra `b` before the arm |
| `addi r31,r28,204` | at insn 20 | at insn 7 (hoisted above the `fn_800366e4` call) |

Everything else matches instruction-for-instruction. The Queue2 arm wants a second stack slot at
`r1+16` in addition to the `r1+12` one the Queue1 arm uses, which is the same "one extra named
temporary per arm" shape that run 1 solved in `PreRenderAllViewports` with a `const` local — but
here `const` on Queue2 measured *worse*, so the fix is not a `const` local and I did not find it.

WALL: PreRender__13CScriptEffectFR13CStateManager 97.72% - the Queue2 arm's second stack temporary at r1+16 and one redundant b; the const-local spelling that fixed the same shape in PreRenderAllViewports measures worse here

**Still open, and the notes' claim about it is now measurably wrong:** the other `fn_` names in this
range are **not** a claim question in the way runs 1-3 said. `fn_80080BE8` was purely a rename.
`fn_80080DB8`/`fn_80080E64` are reachable but need the `CMayaSpline::operator=` declaration, which
costs five functions elsewhere. The remaining `fn_` (`fn_80080394` 1772 B, `fn_80080FCC` 248,
`fn_80080F50` 124, `fn_80082E74` 92, `fn_80082ED0` 24, `fn_80080F44` 12) do not match anything we
emit even by size, so those really are unwritten code — `fn_80080F44` is `li r0,0 ; stw r0,4(r3) ;
blr`, `fn_80082ED0` zeroes three floats at 0x8045A900, and `fn_80080F50` is an `SLdrEditorProperties`
deleting destructor.

**Still stale from earlier runs, re-measured here:** `__ct__15CGameSplineDesc` is **92.31%**, and I
found *why* the search stalled — retail's epilogue order (`lwz r0` before `lfd f31`) is emitted by
only **5 objects in the whole tree**, and one scan of all 2066 units shows **zero** objects emit the
`lwz r0`-last order that `__ct__15CGameSplineDesc` produces. The 5 that get it right
(`CRainSplashGenerator`, `CCameraFilter`, `CMorphBall` x2, `CGrappleArm`) all end their body with a
**call**; `__ct__CGameSplineDesc` ends with a plain store (`stb r0,76(r29)`) and is the only object
in the build with the bad order. That is a concrete lead the previous runs did not have, and it says
the trigger is what terminates the body, not the init-list spelling all 18 tried permutations varied.
`fn_800366e4` is `bool` at line 290, so runs 1-2's "`int`, needs its own item" is **STALE**.

## Gates

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12227 -> 12228   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.54% fuzzy, 27.88% matched, 12.89% linked (12228 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptEffect: 19 -> 20 / 35 functions
  ok    no asm added
goal_check: PASS progress-unit-cscripteffect
```

Also measured: `probe_sources.sh` -> `751 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0
duplicates)` — 287 is the baseline in `build/goal/judge/undef.base.count`, unchanged;
`check_symbol_names.py` -> 0 missing names (525 units); `check_decl_order.py --unit
MetroidPrime/ScriptObjects/CScriptEffect` -> `ok: 1 unit(s) checked, none emits its functions out of
retail order`. (`docs/HANDOFF.md`'s state block shows the judge's own rewritten 12228/10680 lines —
that is `goal_check.sh` rewriting derived counts, not an edit of mine.)
