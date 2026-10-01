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